#include "Session.h"
#include <exception>

constexpr int PORT = 8080;

int
main(int argc, const char** argv) {
    try {
        io_context ioc;
        Server server(ioc, PORT);
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
