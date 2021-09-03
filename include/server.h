#ifndef SERVER_H_
#define SERVER_H_

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include "connection.h"

class Server
{
public:
  Server(const boost::asio::ip::address_v4& ip, const unsigned short port, const unsigned short thread_count);

  void Run();
  void Stop();

private:
  void StartAccepting();
  
  boost::asio::thread_pool          pool;
  boost::asio::signal_set           signals;
  boost::asio::ip::tcp::acceptor    acceptor;
  boost::shared_ptr<Connection>     conn;
};

#endif // SERVER_H_
