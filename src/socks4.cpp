#include "socks4.h"

namespace socks4
{

  const char* socks4_category_impl::name() const noexcept 
  { 
    return "socks4";
  }
  
  
  std::string socks4_category_impl::message(int ev) const 
  {
    char buffer[64];
    return this->message(ev, buffer, sizeof(buffer));
  }
  
  
  const char* socks4_category_impl::message(int ev, char* buffer, size_t len) const noexcept
  {
    switch (static_cast<error>(ev)) {
    case error::success:
      return "No error";
    case error::bad_version:
      return "Bad request version";
    case error::bad_command:
      return "Bad request command";
    }
  
    snprintf(buffer, len, "Unknown socks4 error: %d", ev);
    return buffer;
  }
  
  
  const boost::system::error_category& socks4_category()
  {
    static const socks4_category_impl instance;
    return instance;
  }

  boost::system::error_code make_error_code(error e)
  {
    return boost::system::error_code(static_cast<int>(e), socks4_category());
  }
}
