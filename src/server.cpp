#include "server.h"

#include <boost/make_shared.hpp>

using namespace boost;
using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::ip;

Server::Server(const address_v4& ip, const unsigned short port, const unsigned short thread_count):
  pool(thread_count),
  signals(pool, SIGINT, SIGTERM, SIGABRT),
  acceptor(pool, tcp::endpoint(ip, port))
{
  signals.async_wait([&](const error_code& err, int sig){ this->Stop(); });
  acceptor.listen();
}

void Server::Run()
{
  post(pool, [&](){ this->StartAccepting(); });
  pool.join();
}

void Server::Stop()
{
  pool.stop();
}

void Server::StartAccepting()
{
  conn = boost::make_shared<Connection>(pool);
  acceptor.async_accept(*(conn->GetSocket()),
      [&](const error_code& err)
      {
        if (!err)
          conn->Start();
        StartAccepting();
      });
}

