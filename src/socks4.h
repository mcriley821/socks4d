#ifndef SOCKS4_H_
#define SOCKS4_H_

#include <stdint.h>

#include <string>
#include <array>

#include <boost/asio/buffer.hpp>


struct RequestToken 
{
  enum Command : uint8_t { CONNECT = 1, BIND };
  std::string ident; 
  std::string domain_name;
};

struct ResponseToken
{
  enum Code : uint8_t { OK = 0x5a, ERROR };
};

template<class T>
concept RequestOrResponse = 
    std::is_same_v<T, RequestToken> || std::is_same_v<T, ResponseToken>;

template<class T>
concept MutableOrConstBuffer = 
       boost::asio::is_mutable_buffer_sequence<T>::value
    || boost::asio::is_const_buffer_sequence<T>::value;


template<RequestOrResponse T>
struct Packet : public T
{
  template<MutableOrConstBuffer B>
  std::array<B, 4> buffers() 
  {
    return 
    {
      boost::asio::buffer(&version, sizeof(version)),
      boost::asio::buffer(&command, sizeof(command)),
      boost::asio::buffer(&port,    sizeof(port)),
      boost::asio::buffer(&ipv4,    sizeof(ipv4)),
    };
  }

  uint8_t     version;
  uint8_t     command;
  uint16_t    port;
  uint32_t    ipv4;
};


using Request = Packet<RequestToken>;
using Response = Packet<ResponseToken>;


#endif // SOCKS4_H_
