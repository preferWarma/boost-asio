#include "Session.h"
#include "const.h"
#include <exception>
#include <iostream>

using boost::asio::signal_set;
using boost::system::error_code;

int
main(int argc, const char** argv) {
    try {
        io_context ioc;
        signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const error_code& ec, int signal) {
            if (!ec) {
                std::cout << "\nReceived signal: " << signal << "\nserver stop\n";
                ioc.stop();
            }
        });
        Server server(ioc, SERVER_PORT);
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
