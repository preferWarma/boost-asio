#ifndef SESSION_H_
#define SESSION_H_

#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::buffer;
using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using lyf::PrintTool::green;
using std::map;
using std::shared_ptr;
using std::string;

constexpr int MAX_LENGTH = 1024;

// 前置声明, 避免循环引用
class Session;

class Server {
public:
    Server(io_context& ioc, int port);

    void
    RemoveSession(const string& id);

private:
    // 接受连接
    void
    start_accept();

    // 接受连接的回调
    void
    handler_accept(shared_ptr<Session> session, const error_code& ec);

private:
    io_context& _ioc;
    tcp::acceptor _acceptor;
    map<string, shared_ptr<Session>> _sessions;
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(io_context& ioc, Server* server)
        : _sock(ioc), _server(server) {
        // 对每个Session设置一个唯一的ID
        boost::uuids::uuid id = boost::uuids::random_generator()();
        _id                   = boost::uuids::to_string(id);
    }

    const string&
    Id() {
        return _id;
    }

    tcp::socket&
    Socket() {
        return _sock;
    }

    void
    Start() {
        memset(_data, 0, sizeof(_data));
        auto handler
            = std::bind(&Session::handler_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        _sock.async_read_some(buffer(_data, MAX_LENGTH), handler);
    }

private:
    // 读的时候多次读, 采用async_read_some而不是async_read，所以需要bytes_transferred参数来判断是否读完
    void
    handler_read(const error_code& ec, size_t bytes_transferred) {
        if (ec) {
            std::cout << "read error: " << ec.message() << std::endl;
            _server->RemoveSession(_id);
            return;
        }
        // 打印读到的数据
        std::cout << "server has receive: " << green(_data) << std::endl;
        // 读完发数据(echo原封不动发送), 所以是async_write
        async_write(_sock, buffer(_data, bytes_transferred),
                    std::bind(&Session::handler_write, shared_from_this(), std::placeholders::_1));
    }

    // 写的时候一次性写完, 采用async_send而不是async_write_some，所以不需要bytes_transferred参数
    void
    handler_write(const error_code& ec) {
        if (ec) {
            std::cout << "write error: " << ec.message() << std::endl;
            _server->RemoveSession(_id);
            return;
        }
        memset(_data, 0, sizeof(_data));
        // 写完再读, 所以是async_read_some
        auto handler
            = std::bind(&Session::handler_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        _sock.async_read_some(buffer(_data, MAX_LENGTH), handler);
    }

private:
    tcp::socket _sock;
    char _data[MAX_LENGTH];
    Server* _server;
    string _id;
};

/// Server的实现

inline Server::Server(io_context& ioc, int port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "server start listen port: " << port << std::endl;
    start_accept();
}

inline void
Server::RemoveSession(const string& id) {
    _sessions.erase(id);
}

// 开始接受连接
inline void
Server::start_accept() {
    auto newSession = std::make_shared<Session>(_ioc, this);
    _acceptor.async_accept(newSession->Socket(),
                           std::bind(&Server::handler_accept, this, newSession, std::placeholders::_1));
}

// 接受连接完成的回调函数(使用shared_ptr)
inline void
Server::handler_accept(shared_ptr<Session> newSession, const error_code& ec) {
    if (ec) {
        std::cout << "accept error: " << ec.message() << std::endl;
    } else {
        newSession->Start();                      // 开始会话
        _sessions[newSession->Id()] = newSession; // 保存会话
    }
    start_accept();                               // 继续接受连接
}

#endif /* !SESSION_H_ */
