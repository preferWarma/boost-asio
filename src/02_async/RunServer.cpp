#include "Session.h"
#include "const.h"
#include <exception>
#include <iostream>

int
main(int argc, const char** argv) {
    try {
        io_context ioc;
        Server server(ioc, SERVER_PORT);
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
