#include <sys/types.h>  // for pid_t
#include <unistd.h>     // for fork, setsid

#include <iostream>
#include <string>
#include <thread>
#include <cerrno>
#include <cstring>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <boost/filesystem.hpp>

#include "server.h"


namespace ip = boost::asio::ip;
namespace po = boost::program_options;
namespace log = boost::log;
namespace expr = boost::log::expressions;
namespace fs = boost::filesystem;

using tcp = boost::asio::ip::tcp;


void print_usage(const char* const prog, const po::options_description& options)
{
  std::cout
    << "Usage: " << prog << " [OPTION]... ipv4\n"
    << "\n"
    << options
    << std::endl;
}


int main(int argc, char* argv[])
{
  std::string ipv4_str;
  ip::address_v4 ipv4;
  unsigned threads;
  unsigned short port;

  std::string log_directory;
  log::trivial::severity_level log_level;

  std::string prefix = std::getenv("PREFIX");

  // Setup options
  po::options_description visible_opts("Options");
  visible_opts.add_options()
    (
        "port,p"
      , po::value<unsigned short>(&port)->default_value(1080)
      , "port to bind"
    )
    (
        "threads,t"
      , po::value<unsigned>(&threads)->default_value(std::thread::hardware_concurrency())
      , "number of threads"
    )
    (
        "log-directory,o"
      , po::value<std::string>(&log_directory)->default_value(prefix + "/var/log/socks4")
      , "specify log directory"
    )
    (
        "log-level,l"
      , po::value<log::trivial::severity_level>(&log_level)->default_value(log::trivial::info)
      , "specify log level"
    )
    ("help,h", "print this help and exit")
  ;

  po::options_description hidden_opts("Hidden Options");
  hidden_opts.add_options()
    (
        "ipv4"
      , po::value<std::string>(&ipv4_str)
      , "ipv4 address to bind"
    )
  ;

  po::options_description all_opts;
  all_opts.add(visible_opts).add(hidden_opts);

  po::positional_options_description positional_opts;
  positional_opts.add("ipv4", 1);

  po::variables_map opt_map;
  po::command_line_parser parser(argc, argv);
  po::store(parser.options(all_opts).positional(positional_opts).run(), opt_map);

  // Parse options
  try {
    po::notify(opt_map);
  }
  catch (po::error& error) {
    std::cerr << error.what() << std::endl;
    print_usage(argv[0], visible_opts);
    return EXIT_FAILURE;
  }

  if (opt_map.count("help")) {
    print_usage(argv[0], visible_opts);
    return EXIT_SUCCESS;
  }

  boost::system::error_code error;
  ipv4 = ip::make_address_v4(ipv4_str, error);
  if (error) {
    std::cerr << error.message() << std::endl;
    print_usage(argv[0], visible_opts);
    return EXIT_FAILURE;
  }

  // Create log directory
  if (!fs::exists(log_directory)) {
    if (!fs::create_directories(log_directory, error)) {
      std::cerr << "Could not create log directory: " << error.message() << std::endl;
      return EXIT_FAILURE;
    }
  }

  // fork off from parent
  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "Fork failed: " << std::strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }
  else if (pid > 0) 
    return EXIT_SUCCESS;
  
  // start a new session
  pid_t sid = setsid();
  if (sid == -1) {
    std::cerr << "setsid failed: " << std::strerror(errno) << std::endl;
    return EXIT_FAILURE;
  }

  // close std fds
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // Setup logger
  log::add_file_log(
      log::keywords::file_name = fs::path(log_directory) / "socks4_%N.log",
      log::keywords::target = log_directory,
      log::keywords::target_file_name = "socks4_%N.log",
      log::keywords::max_files = 10,
      log::keywords::rotation_size = 10 * 1024 * 1024,
      log::keywords::format = (
        expr::format("[%1%] %2% %3%")
          % expr::format_date_time<boost::posix_time::ptime>(
              "TimeStamp", "%Y-%m-%d %H:%M:%S")
          % log::trivial::severity
          % expr::smessage
      ),
      log::keywords::auto_flush = true
  );
  log::core::get()->set_filter(log::trivial::severity >= log_level);
  log::add_common_attributes();  // enables timestamps

  // Launch server
  BOOST_LOG_TRIVIAL(info) << "Launching on " << ipv4_str << ":" << port;
  try {
    Server server(tcp::endpoint(ipv4, port), threads);
    server.run();
  }
  catch (std::exception& e) {
    BOOST_LOG_TRIVIAL(fatal) << e.what();
    return EXIT_FAILURE;
  }

  BOOST_LOG_TRIVIAL(info) << "exiting";
  return EXIT_SUCCESS;
}
