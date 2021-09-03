#ifndef CONNECTION_H_
#define CONNECTION_H_


#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/array.hpp>

#include "socks4protocol.h"

class Connection : public boost::enable_shared_from_this<Connection>
{
public:
  typedef boost::shared_ptr<boost::asio::ip::tcp::socket> Socket;
  typedef boost::asio::strand<boost::asio::thread_pool::executor_type> Strand;

  explicit Connection(boost::asio::thread_pool&);
  ~Connection();

  void Start();
  void DoBind(SOCKS4::Client::Request&);
  void DoConnect(SOCKS4::Client::Request&);
  void StartDuplex(bool*);

  Socket& GetSocket();

private:
  unsigned long long               total_received          = 0;
  unsigned long long               total_sent              = 0;
  Socket                           client_socket;
  Socket                           remote_socket;
  std::string                      client_request_packet;
  Strand                           client_strand;
  Strand                           remote_strand;
  boost::array<char, 8192>         client_buffer;
  boost::array<char, 8192>         remote_buffer;

}; // Connection

#endif // CONNECTION_H_
