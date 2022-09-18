#ifndef SOCKS4_H_
#define SOCKS4_H_

#include <stdint.h>

#include <string>
#include <array>

#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>

namespace socks4 
{
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


  enum class error {
    success = 0,

    bad_version,
    bad_command,
    
  };

  class socks4_category_impl : public boost::system::error_category
  {
    const char* name() const noexcept; 

    std::string message(int ev) const; 

    const char* message(int ev, char* buffer, size_t len) const noexcept;
  };

  const boost::system::error_category& socks4_category();
  
  boost::system::error_code make_error_code(error e);
}

namespace boost 
{
  namespace system
  {
    template<> struct is_error_code_enum<::socks4::error>: std::true_type {};
  }
}

using Request = socks4::Packet<socks4::RequestToken>;
using Response = socks4::Packet<socks4::ResponseToken>;


#endif // SOCKS4_H_
