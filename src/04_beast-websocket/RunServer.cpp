#include "WebSocketServer.h"

int
main() {
    io_context ioc;
    WebSocketServer server(ioc, 8080);
    server.StartAccept();
    ioc.run();
    return 0;
}
