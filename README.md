# SOCKS4
Boost::ASIO SOCKS4 Server in C++


This project implements a multithreaded SOCKS4 server in C++ using the boost::asio library.

<h2>Building</h2>
<ol>
  <li>Clone/Download the repo wherever you like (following steps assume you place in your user directory)</li>
  <li><code>cd ~/SOCKS4/</code></li>
  <li><code>make</code></li>
  <li><code>./Server -h</code> for usage or continue reading</li>
</ol>

<h2>Usage</h2>

You can run `./Server -h` to print the usage of the server. 

    Usage: Server <ip> <port> <thread_count>
              ip: string, ipv4 address to bind the server on
            port: ushort, port (>1000) of the ip address to bind the server on
    thread_count: ushort, number of threads to use in the thread pool
    
<h2>Dependencies</h2>
Tested with boost 1.77.0
<ul>
  <li>boost (specifically libboost_system.so and asio headers)</li>
  <li>libpthread.so</li>
</ul>
