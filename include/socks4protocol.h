#ifndef SOCKS4PROTOCOL_H_
#define SOCKS4PROTOCOL_H_

#include <string>

namespace SOCKS4
{
  constexpr unsigned char Version = 4;
  namespace Client
  {
    enum class Command
    {
      CONNECT = 1,
      BIND    = 2
    };

    class Request
    {
    public:
      Request(Command _cmd,  unsigned int _ip, unsigned short _port, const std::string _id):
        command((unsigned char)_cmd), port(_port), ip(_ip), id(_id){};

      Request(): command(0), port(0), ip(0), id(""){};

      inline unsigned char  GetVersion() const { return version; };
      inline Command        GetCommand() const { return (Command)command; };
      inline unsigned short GetPort()    const { return port; };
      inline unsigned int   GetIP()      const { return ip; }
      inline std::string    GetID()      const { return id; };

      std::string GetIPString() const 
      {
        std::string s = std::to_string(static_cast<char>((ip >> 24) & 0xff)) + "." +
                        std::to_string(static_cast<char>((ip >> 16) & 0xff)) + "." +
                        std::to_string(static_cast<char>((ip >> 8 ) & 0xff)) + "." +
                        std::to_string(static_cast<char>((ip >> 0 ) & 0xff));
        return s;
      };

      void SetCommand(Command cmd)          { command = (unsigned char)cmd; };
      void SetPort(unsigned short new_port) { port = new_port; };
      void SetIP(unsigned int new_ip)       { ip = new_ip; };
      void SetID(const std::string& new_id) { id = new_id; };

      static Request FromNetBytes(const std::string& byte_string)
      {
        if (byte_string.size() <= 8)
          throw std::runtime_error("Byte string incorrect length!");

        unsigned char version = byte_string.at(0);
        if (version != 4)
          throw std::runtime_error("Protocol version incorrect! Must be 4!");
        
        unsigned char command = byte_string.at(1);
        if (command != 1 && command != 2)
          throw std::runtime_error("Invalid command! Must be 1 or 2!");

        Command cmd = (command == 1)? Command::CONNECT : Command::BIND;

        unsigned short port;
        try 
        { 
          port = *reinterpret_cast<unsigned short const *>(&byte_string[2]);
          port = (port >> 8) | (port << 8);
        }
        catch (...) { throw std::runtime_error("Invalid port!"); }

        unsigned ip;
        try 
        { 
          ip = *reinterpret_cast<unsigned const *>(&byte_string[4]);
          ip = ((ip << 24) & 0xff000000) | ((ip << 8) & 0xff0000) | ((ip >> 8) & 0xff00) | ((ip >> 24) & 0xff);
        }
        catch (...) { throw std::runtime_error("Invalid ip!"); }

        std::string id = byte_string.substr(8); 
        return Request(cmd, ip, port, id);
      }

      const std::string ToNetBytes() const
      {
        std::string s{ version, command, 
          static_cast<char>((port >> 8) & 0xff), static_cast<char>((port >> 0) & 0xff),
          static_cast<char>((ip >> 24) & 0xff),  static_cast<char>((ip >> 16) & 0xff),
          static_cast<char>((ip >> 8) & 0xff),   static_cast<char>((ip >> 0) & 0xff)};
        s += id;
        return s;
      }

    private:
      unsigned char    version = SOCKS4::Version;
      unsigned char    command;
      unsigned short   port;
      unsigned int     ip;
      std::string      id;
    };
  };

  namespace Server
  {
    enum class Reply
    {
      GRANTED     = 0x5a,
      FAILED      = 0x5b,
      NO_IDENTD   = 0x5c,
      IDENTD_DENY = 0x5d
    };

    class Response
    {
    public:
      Response(Reply _reply, unsigned int _ip, unsigned short _port):
        reply((unsigned char)_reply), port(_port), ip(_ip){};

      Response(): reply((unsigned char)Reply::FAILED), port(0), ip(0){};

      inline unsigned char  GetVersion() const { return version; }
      inline Reply          GetReply()   const { return (Reply)reply; };
      inline unsigned short GetPort()    const { return port; }
      inline unsigned int   GetIP()      const { return ip; }

      void SetReply(Reply _reply)        { reply = (unsigned char)_reply; }
      void SetPort(unsigned short _port) { port = _port; }
      void SetIP(unsigned int _ip)       { ip = _ip; }

      const std::string GetIPString() const
      {
        std::string s = std::to_string(static_cast<char>((ip >> 24) & 0xff)) + "." +
                        std::to_string(static_cast<char>((ip >> 16) & 0xff)) + "." +
                        std::to_string(static_cast<char>((ip >> 8 ) & 0xff)) + "." +
                        std::to_string(static_cast<char>((ip >> 0 ) & 0xff));
        return s;
      }

      static Response FromNetBytes(const std::string& byte_string)
      {
        if (byte_string.size() != 8)
          throw std::runtime_error("Byte string not correct size! Must be 8!");

        unsigned char version = byte_string.at(0);
        if (version != 4)
          throw std::runtime_error("Protocol version incorrect! Must be 4!");

        unsigned char reply = byte_string.at(1);
        if (reply != 0x5a && reply != 0x5b && reply != 0x5c && reply != 0x5d)
          throw std::runtime_error("Reply code invalid!i Must be 0x5a, 0x5b, 0x5c or 0x5d");

        unsigned short port;
        try
        {
          port = *reinterpret_cast<unsigned short const *>(&byte_string[2]);
          port = (port << 8) | (port >> 8);
        }
        catch(...) { throw std::runtime_error("Invalid port!"); }

        unsigned int ip;
        try
        {
          ip = *reinterpret_cast<unsigned int const *>(&byte_string[4]);
          ip = ((ip << 24) & 0xff000000) | ((ip << 8) & 0xff0000) | ((ip >> 8) & 0xff00) | ((ip >> 24) & 0xff);
      }
        catch(...) { throw std::runtime_error("Invalid ip!"); }

        return Response((Reply)reply, ip, port);
      }


      const std::string ToNetBytes() const
      {
        std::string s{0, reply,
          static_cast<char>((port >> 8 ) & 0xff), static_cast<char>((port >> 0 ) & 0xff),
          static_cast<char>((  ip >> 24) & 0xff), static_cast<char>((  ip >> 16) & 0xff),
          static_cast<char>((  ip >> 8 ) & 0xff), static_cast<char>((  ip >> 0 ) & 0xff)};
        return s;
      }

    private:
      unsigned char   version = SOCKS4::Version;
      unsigned char   reply;
      unsigned short  port;
      unsigned int    ip;
    };
  };
}

#endif //SOCKS4PROTOCOL_H_
