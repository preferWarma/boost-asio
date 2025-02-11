#ifndef SESSION_H_
#define SESSION_H_

#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
#include <cstring>
#include <iostream>

using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::buffer;
using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using lyf::PrintTool::green;

constexpr int MAX_LENGTH = 1024;

class Session {
public:
    Session(io_context& ioc)
        : _sock(ioc) {}

    tcp::socket&
    Socket() {
        return _sock;
    }

    void
    Start() {
        memset(_data, 0, sizeof(_data));
        _sock.async_read_some(buffer(_data, MAX_LENGTH),
                              std::bind(&Session::handler_read, this, std::placeholders::_1, std::placeholders::_2));
    }

private:
    // 读的时候多次读, 采用async_read_some而不是async_read，所以需要bytes_transferred参数来判断是否读完
    void
    handler_read(const error_code& ec, size_t bytes_transferred) {
        if (ec) {
            std::cout << "read error: " << ec.message() << std::endl;
            // TODO: 存在隐患
            delete this;
            return;
        }
        // 打印读到的数据
        std::cout << "server has receive: " << green(_data) << std::endl;
        // 读完发数据(echo原封不动发送), 所以是async_write
        async_write(_sock, buffer(_data, bytes_transferred),
                    std::bind(&Session::handler_write, this, std::placeholders::_1));
    }

    // 写的时候一次性写完, 采用async_send而不是async_write_some，所以不需要bytes_transferred参数
    void
    handler_write(const error_code& ec) {
        if (ec) {
            std::cout << "write error: " << ec.message() << std::endl;
            // TODO: 存在隐患
            delete this;
            return;
        }
        memset(_data, 0, sizeof(_data));
        // 写完再读, 所以是async_read_some
        _sock.async_read_some(buffer(_data, MAX_LENGTH),
                              std::bind(&Session::handler_read, this, std::placeholders::_1, std::placeholders::_2));
    }

private:
    tcp::socket _sock;
    char _data[MAX_LENGTH];
};

class Server {
public:
    Server(io_context& ioc, int port)
        : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "server start listen port: " << port << std::endl;
        start_accept();
    }

private:
    // 开始接受连接
    void
    start_accept() {
        auto newSession = new Session(_ioc);
        _acceptor.async_accept(newSession->Socket(),
                               std::bind(&Server::handler_accept, this, newSession, std::placeholders::_1));
    }

    // 接受连接完成的回调函数
    void
    handler_accept(Session* newSession, const error_code& ec) {
        if (ec) {
            std::cout << "accept error: " << ec.message() << std::endl;
            delete newSession;
            return;
        } else {
            newSession->Start(); // 开始会话
        }
        start_accept();          // 继续接受连接
    }

private:
    io_context& _ioc;
    tcp::acceptor _acceptor;
};

#endif /* !SESSION_H_ */
