#ifndef SERVER_H_
#define SERVER_H_

#include <vector>
#include <thread>

#include <boost/asio.hpp>


class Server
{
  using tcp = boost::asio::ip::tcp;
  using default_token = boost::asio::as_tuple_t<boost::asio::use_awaitable_t<>>;
  using acceptor_t = default_token::as_default_on_t<tcp::acceptor>;
  using socket_t = default_token::as_default_on_t<tcp::socket>;
public:
  Server(const tcp::endpoint endpoint, const unsigned short thread_count);

  void run();
  void stop();

private:
  void handle_signal(const boost::system::error_code& error, int signum);
  boost::asio::awaitable<void> accept();
  boost::asio::awaitable<void> handle_connection(socket_t client_socket);
  
  boost::asio::io_context io_context_;
  boost::asio::signal_set signals_;
  acceptor_t              acceptor_;

  std::vector<std::thread> thread_pool_;
  const unsigned short     thread_count_;

};

#endif // SERVER_H_
