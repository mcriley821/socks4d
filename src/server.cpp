#include "server.h"

#include <boost/log/trivial.hpp>
#include <boost/bind.hpp>

#include "connection.h"


using namespace boost::asio;
using error_code = boost::system::error_code;
using tcp = boost::asio::ip::tcp;


Server::Server(const tcp::endpoint endpoint, const unsigned short thread_count):
  io_context_(thread_count),
  signals_(io_context_, SIGINT, SIGTERM, SIGABRT),
  acceptor_(io_context_, std::move(endpoint)),
  thread_count_(thread_count)
{
  signals_.async_wait(boost::bind(
    &Server::handle_signal, this, placeholders::error, placeholders::signal_number));

  thread_pool_.emplace_back([&](){ io_context_.run(); });

  acceptor_.listen();
}


void Server::run()
{
  co_spawn(io_context_, accept(), detached);
  for (int i = thread_count_ - 1; i > 0; i--)
    thread_pool_.emplace_back([&](){ io_context_.run(); });
  
  for (std::thread& t: thread_pool_)
    t.join();
}


void Server::handle_signal(const error_code& err, int signum)
{
  BOOST_LOG_TRIVIAL(info) << "received signal: " << signum << " " << strsignal(signum);
  if (err)
    BOOST_LOG_TRIVIAL(error) << "signal handler error: " << err.message();
  stop();
}


void Server::stop()
{
  io_context_.stop();
}


awaitable<void> Server::accept()
{
  BOOST_LOG_TRIVIAL(debug) << "Begin server accept coroutine";
  while (true) {
    auto [err, sock] = co_await acceptor_.async_accept();
    if (err) {
      if (err == error::operation_aborted)
        co_return;
      
      BOOST_LOG_TRIVIAL(warning) << "Server accept coroutine: " << err.message();
      continue;
    }
    BOOST_LOG_TRIVIAL(info) << "Starting connection with " << sock.remote_endpoint();

    co_spawn(io_context_, handle_connection(std::move(sock)), detached);
  }
}


awaitable<void> Server::handle_connection(socket_t client_socket)
{
  Connection conn(io_context_, std::move(client_socket));
  co_await conn.start();
}

