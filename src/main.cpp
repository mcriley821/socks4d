#include <iostream>
#include <string>

#include "server.h"

int PrintUsage()
{
    std::cout << std::endl
        << "Usage: SOCKS4Server <ip> <port> <thread_count>                         " << std::endl
        << "              ip: string, ipv4 address to bind the server on           " << std::endl
        << "            port: ushort, port (>1000) of the ip address to bind the server on " << std::endl
        << "    thread_count: ushort, number of threads to use in the thread pool  " << std::endl
        << std::endl;
    return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
    if (argc != 4) return PrintUsage();
    boost::asio::ip::address_v4 ip;
    unsigned short port, thread_count;
    try
    {
        ip = boost::asio::ip::address_v4::from_string(argv[1]);
        port = (unsigned short)std::stoul(argv[2]);
        thread_count = (unsigned short)std::stoul(argv[3]);
        if (port < 1000)
          throw std::runtime_error("Port number must be above 1000!");
    }
    catch (std::exception& e)
    {
        std::cerr << std::endl << e.what()  << std::endl;
        return PrintUsage();
    }

    try
    {
      Server server(ip, port, thread_count);
      server.Run();
    }
    catch (std::exception& e)
    {
      std::cerr << std::endl << e.what() << std::endl;
      return EXIT_FAILURE;
    }

    return 0;
}
