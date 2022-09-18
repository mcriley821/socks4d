#ifndef CONNECTION_H_
#define CONNECTION_H_

#include <stdint.h>

#include <memory>

#include <boost/asio.hpp>

#include "socks4.h"


class Connection
{
  using error_code = boost::system::error_code;

  using default_token = boost::asio::as_tuple_t<boost::asio::use_awaitable_t<>>;

  using socket_t = default_token::as_default_on_t<boost::asio::ip::tcp::socket>;
  using strand_t = boost::asio::strand<boost::asio::io_context::executor_type>;

public:
  explicit Connection(boost::asio::io_context& context, socket_t socket) noexcept;
  ~Connection() noexcept;

  boost::asio::awaitable<void> start() noexcept;

private:
  void timeout(const error_code& err) noexcept;

  bool set_client_string() noexcept;
  boost::asio::awaitable<error_code> read_request_begin(Request& request) noexcept;
  boost::asio::awaitable<std::tuple<error_code, uint32_t>> read_socks4a_domain(std::string& buff) noexcept;

  boost::asio::awaitable<void> bind(Request request) noexcept;
  boost::asio::awaitable<void> connect(Request request) noexcept;
  boost::asio::awaitable<bool> send_response(Response::Code code, const Request& request) noexcept;
  boost::asio::awaitable<void> duplex(socket_t remote_socket) noexcept;
  boost::asio::awaitable<void> transfer(socket_t& from, socket_t& to) noexcept;

  static void close(socket_t& socket) noexcept;

  socket_t client_socket_;
  boost::asio::deadline_timer timer_;
  std::string client_str_;
}; // Connection

#endif // CONNECTION_H_
