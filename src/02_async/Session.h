#ifndef SESSION_H_
#define SESSION_H_

#include "MsgNode.h"
#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::buffer;
using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using lyf::PrintTool::green;
using std::map;
using std::mutex;
using std::queue;
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
    StartAccept();

    // 接受连接的回调
    void
    HandlerAccept(shared_ptr<Session> session, const error_code& ec);

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
            = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        _sock.async_read_some(buffer(_data, MAX_LENGTH), handler);
    }

    void
    Send(char* msg, int totalLen) {
        // 加锁
        std::lock_guard<mutex> lock(_sendLock);
        bool pending = false; // 是否有数据正在发送
        if (!_sendQueue.empty()) {
            pending = true;
        }
        _sendQueue.push(std::make_shared<MsgNode>(msg, totalLen));
        if (pending) { // 如果有数据正在发送, 就不发送了
            return;
        }
        // 没有数据正在发送, 就发送队列中的数据
        async_write(_sock, buffer(_data, totalLen),
                    std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1));
    }

private:
    // 读的时候多次读, 采用async_read_some而不是async_read，所以需要bytes_transferred参数来判断是否读完
    void
    HandlerRead(const error_code& ec, size_t bytes_transferred) {
        if (ec) {
            std::cout << "read error: " << ec.message() << std::endl;
            _server->RemoveSession(_id);
            return;
        }
        // 打印读到的数据
        std::cout << "server has receive: " << green(_data) << std::endl;
        // 读完发数据(echo原封不动发送)
        Send(_data, bytes_transferred);
        memset(_data, 0, sizeof(_data));
        // 继续读
        auto handler
            = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        _sock.async_read_some(buffer(_data, MAX_LENGTH), handler);
    }

    // 写的时候一次性写完, 采用async_send而不是async_write_some，所以不需要bytes_transferred参数
    void
    HandlerWrite(const error_code& ec) {
        if (ec) {
            std::cout << "write error: " << ec.message() << std::endl;
            _server->RemoveSession(_id);
            return;
        }
        // 对发送队列操作前加锁
        std::lock_guard<mutex> lock(_sendLock);
        _sendQueue.pop(); // 当handler_write执行时说明有一个写操作已经完成了, 所以可以pop掉上次的数据
        if (!_sendQueue.empty()) { // 如果队列不为空, 就继续发送
            auto& msgNode = _sendQueue.front();
            async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()),
                        std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1));
        }
    }

private:
    tcp::socket _sock;
    char _data[MAX_LENGTH];
    Server* _server;
    string _id;
    queue<shared_ptr<MsgNode>> _sendQueue; // 发送队列
    mutex _sendLock;                       // 发送队列的锁
};

/// Server的实现

inline Server::Server(io_context& ioc, int port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "server start listen port: " << port << std::endl;
    StartAccept();
}

inline void
Server::RemoveSession(const string& id) {
    _sessions.erase(id);
}

// 开始接受连接
inline void
Server::StartAccept() {
    auto newSession = std::make_shared<Session>(_ioc, this);
    _acceptor.async_accept(newSession->Socket(),
                           std::bind(&Server::HandlerAccept, this, newSession, std::placeholders::_1));
}

// 接受连接完成的回调函数(使用shared_ptr)
inline void
Server::HandlerAccept(shared_ptr<Session> newSession, const error_code& ec) {
    if (ec) {
        std::cout << "accept error: " << ec.message() << std::endl;
    } else {
        newSession->Start();                      // 开始会话
        _sessions[newSession->Id()] = newSession; // 保存会话
    }
    StartAccept();                                // 继续接受连接
}

#endif /* !SESSION_H_ */
