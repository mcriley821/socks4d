#include "connection.h"
#include <iostream>
#include <boost/make_shared.hpp>

using namespace boost;
using namespace boost::asio;
using namespace boost::system;
using namespace boost::asio::ip;

Connection::Connection(thread_pool& pool):
  client_strand(make_strand(pool)),
  remote_strand(make_strand(pool))
{
  client_socket = boost::make_shared<tcp::socket>(client_strand);
  remote_socket = boost::make_shared<tcp::socket>(remote_strand);
}


Connection::~Connection()
{
  try
  {
    if (client_socket)
      client_socket->close();
    if (remote_socket)
      remote_socket->close();
  }
  catch (...) { }
}


Connection::Socket& Connection::GetSocket()
{
  return client_socket;
}


void Connection::Start()
{
  async_read(*client_socket, dynamic_buffer(client_request_packet),
    [self=shared_from_this()](const error_code& err, size_t total)
    {
      if (!err)
        return (total > 8 && *(self->client_request_packet.end() - 1) == '\0')? 0ul : (total > 8)? 1 : 9 - total;
      else
        post(self->client_socket->get_executor(), [self2=self->shared_from_this()]() { try { self2->client_socket->close(); } catch(...) { } });
      return 0ul;
    },
    [self=shared_from_this()](const error_code& err, size_t total)
    {
      if (!err)
      {
        auto req = SOCKS4::Client::Request::FromNetBytes(self->client_request_packet);
        if (req.GetCommand() == SOCKS4::Client::Command::CONNECT)
          self->DoConnect(req);
        else
          self->DoBind(req);
      }
      else
        post(self->client_socket->get_executor(), [self2=self->shared_from_this()](){ try { self2->client_socket->close(); } catch(...) { } });
    });
}


void Connection::DoConnect(SOCKS4::Client::Request& req)
{
  auto remote = tcp::endpoint(address_v4(req.GetIP()), req.GetPort());
  remote_socket->async_connect(remote,
    [self=shared_from_this(), req2=req](const error_code& err)
    {
      if (!err)
      {
        auto reply = SOCKS4::Server::Response(SOCKS4::Server::Reply::GRANTED, req2.GetIP(), req2.GetPort());
        try { write(*(self->client_socket), buffer(reply.ToNetBytes()), transfer_exactly(8)); } catch(...) { return; }
        bool *b = nullptr;
        self->StartDuplex(b);
      }
      else
      {
        auto reply = SOCKS4::Server::Response(SOCKS4::Server::Reply::FAILED, req2.GetIP(), req2.GetPort());
        try { write(*(self->client_socket), buffer(reply.ToNetBytes()), transfer_exactly(8)); } catch(...) { return; }
        post(self->client_socket->get_executor(), [self2=self->shared_from_this()](){ try { self2->client_socket->close(); } catch(...) { } });
        post(self->remote_socket->get_executor(), [self2=self->shared_from_this()](){ try { self2->remote_socket->close(); } catch(...) { } });
      }
    });
}


void Connection::DoBind(SOCKS4::Client::Request& req)
{
  std::cerr << "BIND: NOT YET IMPLEMENTED" << std::endl;
  auto reply = SOCKS4::Server::Response(SOCKS4::Server::Reply::FAILED, req.GetIP(), req.GetPort());
  try { write(*client_socket, buffer(reply.ToNetBytes()), transfer_exactly(8)); } catch (...) { return; }
}


void Connection::StartDuplex(bool* which)
{
  if (!which || *which)
  {
    client_socket->async_read_some(buffer(client_buffer), 
      [self=shared_from_this()](const error_code& err, size_t N)
      {
        if (!err)
        {
          try{ write(*(self->remote_socket), buffer(self->client_buffer), transfer_exactly(N)); } catch(...) { return; }
          post(self->client_socket->get_executor(), [self2=self->shared_from_this()](){ self2->StartDuplex(new bool(true)); });
        }
        else
          post(self->client_socket->get_executor(), [self2=self->shared_from_this()](){ try { self2->client_socket->close(); } catch(...) { } });
      });
  }
  if (!which || !*which)
  {
    remote_socket->async_read_some(buffer(remote_buffer),
      [self=shared_from_this()](const error_code& err, size_t N)
      {
        if (!err)
        {
          try { write(*(self->client_socket), buffer(self->remote_buffer), transfer_exactly(N)); } catch(...) { return; }
          post(self->remote_socket->get_executor(), [self2=self->shared_from_this()](){ self2->StartDuplex(new bool(false)); });
        }
        else
          post(self->remote_socket->get_executor(), [self2=self->shared_from_this()](){ try { self2->remote_socket->close(); } catch(...) { } });
      });
  }
  if (which) delete which;
}
