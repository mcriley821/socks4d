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


#define SEND_ERROR_AND_RETURN_IF(err) \
  if (err) { \
    if (err != error::eof && err != error::operation_aborted) \
      co_await send_response(Response::ERROR, request); \
    BOOST_LOG_TRIVIAL(error) << client_str_ << " bad request: " << err.message(); \
    co_return; \
  }


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
  if (client_str_.size())
    BOOST_LOG_TRIVIAL(info) << client_str_ << " closing connection";
  close(client_socket_);
}


void Connection::timeout(const error_code& err) noexcept
{
  if (err) {
    if (err != error::operation_aborted)
      BOOST_LOG_TRIVIAL(error) << client_str_ << " timer error: " << err.message();
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
        << socket.local_endpoint() << " close error: " << err.message();
    }
  }
}


awaitable<void> Connection::start() noexcept
{
  if (!set_client_string())
    co_return;

  Request request;

  error_code err_req = co_await read_request_begin(request);
  SEND_ERROR_AND_RETURN_IF(err_req);

  std::string buff;
  auto [err, n] = co_await async_read_until(
      client_socket_, dynamic_buffer(buff, IDENT_MAX_LEN), '\0', default_token());
  SEND_ERROR_AND_RETURN_IF(err);

  request.ident = buff.substr(0, n);
  buff.erase(0, n);

  request.port = ntohs(request.port);
  request.ipv4 = ntohl(request.ipv4);

  if (request.ipv4 < 256) { // socks4a request == 0.0.0.x
    std::tie(err, request.ipv4) = co_await read_socks4a_domain(buff);
    SEND_ERROR_AND_RETURN_IF(err);
  }
 
  if (request.command == Request::CONNECT)
    co_await connect(std::move(request));
  else
    co_await bind(std::move(request));
}


bool Connection::set_client_string() noexcept
{
  error_code ec;
  tcp::endpoint endpoint = client_socket_.remote_endpoint(ec);
  if (ec) {
    BOOST_LOG_TRIVIAL(error) << "client closed connection";
    return false;
  }

  std::ostringstream oss;
  oss << endpoint;
  client_str_ = oss.str();
  return true;
}


awaitable<error_code> Connection::read_request_begin(Request& request) noexcept
{
  BOOST_LOG_TRIVIAL(debug) << client_str_ << " reading request";

  auto [err, n] = co_await async_read(
      client_socket_, request.buffers<mutable_buffer>(), 
      transfer_exactly(8), default_token());

  if (err)
    co_return err;
  
  if (request.version != 4)
    err = socks4::error::bad_version;
  else if (request.command != Request::CONNECT && request.command != Request::BIND)
    err = socks4::error::bad_command;
  co_return err;
}


awaitable<std::tuple<error_code, uint32_t>> Connection::read_socks4a_domain(std::string& buff) noexcept
{
  BOOST_LOG_TRIVIAL(debug) << client_str_ << " socks4a request";

  auto [err, n] = co_await async_read_until(
      client_socket_, dynamic_buffer(buff, DOMAIN_MAX_LEN), '\0', default_token());

  if (err)
    co_return std::tuple{err, 0};

  std::string domain = buff.substr(0, n);
  
  using resolver_t = default_token::as_default_on_t<tcp::resolver>;
  resolver_t resolver(co_await this_coro::executor);

  auto [err_resolve, results] = co_await resolver.async_resolve(domain, "");
  if (err_resolve)
    co_return std::tuple{err_resolve, 0};

  for (const auto& entry: results)
    if (entry.endpoint().address().is_v4())
      co_return std::tuple{error_code(), entry.endpoint().address().to_v4().to_uint()};

  co_return std::tuple{error::host_not_found, 0};
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
