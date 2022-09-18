#include "connection.h"

#include <arpa/inet.h>

#include <sstream>

#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#ifndef IDENT_MAX_LEN
#define IDENT_MAX_LEN 256
#endif

#ifndef TRANSFER_MAX_LEN
#define TRANSFER_MAX_LEN 4096
#endif

#ifndef DOMAIN_MAX_LEN
#define DOMAIN_MAX_LEN 256
#endif

#ifndef TRANSFER_TIMEOUT
#define TRANSFER_TIMEOUT 30
#endif

#ifndef REQUEST_TIMEOUT
#define REQUEST_TIMEOUT 120
#endif

using namespace boost::asio;
using namespace boost::asio::experimental::awaitable_operators;
namespace ip = boost::asio::ip;

using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;


Connection::Connection(io_context& context, socket_t socket) noexcept
  : client_socket_(std::move(socket))
  , timer_(context, boost::posix_time::seconds(REQUEST_TIMEOUT))
{
  timer_.async_wait(boost::bind(&Connection::timeout, this, placeholders::error));
}


Connection::~Connection() noexcept
{
  BOOST_LOG_TRIVIAL(info) << "closing connection with " << client_str_;
  close(client_socket_);
}


void Connection::timeout(const error_code& err) noexcept
{
  if (err) {
    if (err != error::operation_aborted) {
      BOOST_LOG_TRIVIAL(error)
        << client_str_ << " timer error: " << err.message();
    }
    return;
  }
  // expired
  error_code ec;
  client_socket_.shutdown(socket_base::shutdown_both, ec);

  if (ec)
    BOOST_LOG_TRIVIAL(error) << client_str_ << " shutdown error: " << ec.message();
  BOOST_LOG_TRIVIAL(info) << client_str_ << " timed out";
}


void Connection::close(socket_t& socket) noexcept
{
  if (socket.is_open()) {
    error_code err;
    socket.close(err);
    if (err) {
      BOOST_LOG_TRIVIAL(warning)
        << "Could not close " << socket.local_endpoint() << ": " << err.message();
    }
  }
}


awaitable<void> Connection::start() noexcept
{
  std::ostringstream oss;
  error_code ec;
  oss << client_socket_.remote_endpoint(ec);

  if (ec) {
    co_return;
  }

  client_str_ = oss.str();


  Request request;

  BOOST_LOG_TRIVIAL(trace) << "reading request from " << client_str_;
  auto [err, n] = co_await async_read(
      client_socket_, request.buffers<mutable_buffer>(), 
      transfer_exactly(8), default_token());

  if (err) {
    if (err != error::operation_aborted && err != error::eof) {
      BOOST_LOG_TRIVIAL(error) << client_str_ << " connection error: " << err.message();
      co_await send_response(Response::ERROR, request);
    }
    co_return;
  }
  else if (n != 8) {
    BOOST_LOG_TRIVIAL(error) << client_str_ << " bad request";
    co_await send_response(Response::ERROR, request);
    co_return;
  }
  else if (request.version != 4) {
    BOOST_LOG_TRIVIAL(error) << client_str_ << " bad version";
    co_await send_response(Response::ERROR, request);
    co_return;
  }
  else if (request.command != Request::CONNECT && request.command != Request::BIND) {
    BOOST_LOG_TRIVIAL(error) << client_str_ << " bad command";
    co_await send_response(Response::ERROR, request);
    co_return;
  }
  BOOST_LOG_TRIVIAL(trace) << "reading ident of " << client_str_;
  std::string string_buffer;
  std::tie(err, n) = co_await async_read_until(
      client_socket_, dynamic_buffer(string_buffer, IDENT_MAX_LEN), '\0', default_token());

  if (err || n == 0) {
    if (err && err != error::operation_aborted && err != error::eof) {
      BOOST_LOG_TRIVIAL(error) << client_str_ << " connection error: " << err.message();
      co_await send_response(Response::ERROR, request);
    }
    else if (n == 0) {
      BOOST_LOG_TRIVIAL(error) << client_str_ << " bad ident";
    }
    co_return;
  }
  request.ident = string_buffer.substr(0, n);
  string_buffer.erase(0, n);
  request.port = ntohs(request.port);
  request.ipv4 = ntohl(request.ipv4);

  BOOST_LOG_TRIVIAL(info) << client_str_ << " ident: " << request.ident;

  if (request.ipv4 < 256) { // socks4a request == 0.0.0.x
    BOOST_LOG_TRIVIAL(debug) << client_str_ << " socks4a request";

    std::tie(err, n) = co_await async_read_until(
        client_socket_, dynamic_buffer(string_buffer, DOMAIN_MAX_LEN), '\0', default_token());

    if (err || n == 0) {
      if (err && err != error::operation_aborted && err != error::eof) {
        BOOST_LOG_TRIVIAL(error) << client_str_ << "could not read domain: " << err.message();
       co_await send_response(Response::ERROR, request);       
      }
      else if (n == 0) {
        BOOST_LOG_TRIVIAL(error) << client_str_ << " bad domain";
      }
      co_return;
    }
    std::string domain = string_buffer.substr(0, n);
    
    using resolver_t = default_token::as_default_on_t<tcp::resolver>;
    resolver_t resolver(co_await this_coro::executor);

    auto [err, results] = co_await resolver.async_resolve(domain, "");
    for (const auto& entry: results) {
      if (entry.endpoint().address().is_v4()) {
        request.ipv4 = entry.endpoint().address().to_v4().to_uint();
        break;
      }
    }
    if (request.ipv4 < 256) {
      BOOST_LOG_TRIVIAL(error) << "could not resolve domain " << domain << " for " << client_str_;
      co_await send_response(Response::ERROR, request);
      co_return;
    }
  }
 
  if (request.command == Request::CONNECT)
    co_await connect(std::move(request));
  else
    co_await bind(std::move(request));
}


awaitable<void> Connection::connect(Request request) noexcept
{
  BOOST_LOG_TRIVIAL(debug) << client_str_ << " performing CONNECT";

  const auto executor = co_await this_coro::executor;
  const tcp::endpoint remote = tcp::endpoint(ip::address_v4(request.ipv4), request.port);

  socket_t remote_socket(executor, remote.protocol());

  BOOST_LOG_TRIVIAL(info) << client_str_ << " connecting to " << remote;
  auto [err] = co_await remote_socket.async_connect(remote);

  if (err) {
    BOOST_LOG_TRIVIAL(error) << "could not connect remote: " << remote << ": " << err.message();
    
    co_await send_response(Response::ERROR, request);
  
    close(remote_socket);
    co_return;
  }

  if (!co_await send_response(Response::OK, request)) {
    close(remote_socket);
    co_return;
  }

  co_await duplex(std::move(remote_socket));

  close(remote_socket);
}


awaitable<void> Connection::bind(Request /* request */) noexcept
{
  BOOST_LOG_TRIVIAL(debug) << client_str_ << " performing BIND"; 
  co_return;
}


awaitable<void> Connection::duplex(socket_t remote_socket) noexcept
{
  BOOST_LOG_TRIVIAL(debug) << client_str_ << " starting duplex";
  
  co_await (transfer(client_socket_, remote_socket)
         || transfer(remote_socket, client_socket_));
}


awaitable<void> Connection::transfer(socket_t& from, socket_t& to) noexcept
{
  std::array<char, TRANSFER_MAX_LEN> buff;
  auto timeout = boost::posix_time::seconds(TRANSFER_TIMEOUT);
  auto timeout_func = boost::bind(&Connection::timeout, this, placeholders::error);

  while (true) {
    timer_.expires_from_now(timeout);
    timer_.async_wait(timeout_func);

    auto [err, n] = co_await from.async_read_some(boost::asio::buffer(buff));
    if (err && err != error::eof) {  // send gathered data if eof
      if (err != error::operation_aborted)
        BOOST_LOG_TRIVIAL(error) << client_str_ << " duplex read error: " << err.message();
      break;
    }

    auto [errw, _] = co_await async_write(
        to, boost::asio::buffer(buff, n), default_token());

    if (err == error::eof || errw) {  // break if errw or eof
      if (errw && errw != error::operation_aborted)
        BOOST_LOG_TRIVIAL(error) << client_str_ << " duplex write error: " << errw.message();
      break;
    }
  }
}


awaitable<bool> Connection::send_response(Response::Code code, const Request& request) noexcept
{
  Response response;
  response.command = code;
  response.port = htons(request.port);
  response.ipv4 = htonl(request.ipv4);

  BOOST_LOG_TRIVIAL(debug) 
    << "sending " << ((code == Response::OK)? "ok" : "error") 
    << " response to " << client_str_;

  auto [err, n] = co_await async_write(
      client_socket_, response.buffers<const_buffer>(), default_token());
  if (err)
    BOOST_LOG_TRIVIAL(error) << client_str_ << " could not send response: " << err.message();
  co_return !bool(err);
}
