#ifndef WEBSOCKET_SERVER_H_
#define WEBSOCKET_SERVER_H_

#include "Connection.h"
#include <boost/asio.hpp>
#include <iostream>

using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using std::make_shared;
using std::shared_ptr;

class WebSocketServer {
public:
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer&
    operator=(const WebSocketServer&)
        = delete;
    WebSocketServer(WebSocketServer&&) = delete;
    WebSocketServer&
    operator=(WebSocketServer&&)
        = delete;

public:
    explicit WebSocketServer(io_context& ioc, short port)
        : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "WebSocketServer is running on port " << port << std::endl;
    }

    void
    StartAccept() { // tcp层的accept
        auto connection = make_shared<Connection>(_ioc);
        _acceptor.async_accept(connection->Socket(), [this, connection](error_code ec) {
            try {
                if (!ec) {
                    connection->AsyncAccept(); // websocket层的accept
                } else {
                    std::cerr << "WebSocketServer::StartAccept Error: " << ec.message() << std::endl;
                }
                StartAccept();
            } catch (std::exception& e) {
                std::cerr << "WebSocketServer::StartAccept Error: " << e.what() << std::endl;
            }
        });
    }

private:
    io_context& _ioc;
    tcp::acceptor _acceptor;
};

#endif // WEBSOCKET_SERVER_H_
