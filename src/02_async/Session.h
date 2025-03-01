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
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <json/json.h>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>

using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::buffer;
using boost::asio::detail::socket_ops::network_to_host_short;
using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using lyf::PrintTool::blue;
using lyf::PrintTool::green;
using std::map;
using std::mutex;
using std::queue;
using std::shared_ptr;
using std::string;
using std::string_view;

constexpr int MAX_ID  = 1024 * 10; // 最大消息ID
constexpr int MAX_LEN = 1024 * 2;  // 最大消息体长度

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
    io_context& _ioc;                           // 上下文
    tcp::acceptor _acceptor;                    // 用于接受客户端连接的接受器
    map<string, shared_ptr<Session>> _sessions; // 此服务器占有的所有会话Session
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(io_context& ioc, Server* server)
        : _sock(ioc), _server(server) {
        // 对每个Session设置一个唯一的ID
        boost::uuids::uuid id = boost::uuids::random_generator()();
        _id                   = boost::uuids::to_string(id);
        // 初始化头部接收节点
        _recvHeadNode = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, -1);
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
        Clear();
        auto handler
            = std::bind(&Session::HandlerReadHead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        // 开始监听, 读取头部信息
        async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), handler);
    }

    void
    Send(const char* msg, int totalLen, short msgId) {
        // 加锁
        std::lock_guard<mutex> lock(_sendLock);
        bool pending = false; // 是否有数据正在发送
        if (!_sendQueue.empty()) {
            pending = true;
        }
        _sendQueue.push(std::make_shared<SendNode>(msg, totalLen, msgId));
        if (pending) { // 如果有数据正在发送, 就不发送了
            return;
        }
        // 没有数据正在发送, 就发送队列中的数据
        auto msgNode = _sendQueue.front();
        async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()),
                    std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1));
        std::cout << "server send: " << blue(msg) << std::endl;
    }

    void
    Send(string_view msg, short msgId) {
        Send(msg.data(), msg.size(), msgId);
    }

    void
    Close() {
        _sock.close();
    }

    void
    Clear() {
        if (_recvHeadNode) {
            _recvHeadNode->Clear();
        }
        if (_recvMsgNode) {
            _recvMsgNode->Clear();
        }
    }

private:
    void
    HandlerReadHead(const error_code& ec, size_t bytes_transferred) {
        if (ec) {
            std::cout << "read error: " << ec.message() << std::endl;
            Clear();
            _server->RemoveSession(_id);
            return;
        }
        assert(bytes_transferred == HEAD_TOTAL_LEN);
        // 此时头部接受完成, 解析消息头
        short MsgId = 0;
        memcpy(&MsgId, _recvHeadNode->Data(), HEAD_ID_LEN);
        // 将网络字节序转换为主机字节序
        MsgId = network_to_host_short(MsgId);
        if (MsgId > MAX_ID) {
            std::cout << "invalid msg id with " << MsgId << std::endl;
            Clear();
            _server->RemoveSession(_id);
            return;
        }
        short validDataLen = 0;
        memcpy(&validDataLen, _recvHeadNode->Data() + HEAD_ID_LEN, HEAD_DATA_LEN);
        // 将网络字节序转换为主机字节序
        validDataLen = network_to_host_short(validDataLen);
        if (validDataLen > MAX_LEN) {
            std::cout << "invalid data len with " << validDataLen << std::endl;
            Clear();
            _server->RemoveSession(_id);
            return;
        }
        // 继续监听消息体
        _recvMsgNode = std::make_shared<RecvNode>(validDataLen, MsgId);
        auto handler
            = std::bind(&Session::HandlerReadMsg, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        async_read(_sock, buffer(_recvMsgNode->Data(), _recvMsgNode->TotalLen()), handler);
    }

    void
    HandlerReadMsg(const error_code& ec, size_t bytes_transferred) {
        if (ec) {
            std::cout << "read error: " << ec.message() << std::endl;
            Clear();
            _server->RemoveSession(_id);
            return;
        }
        assert(bytes_transferred == _recvMsgNode->TotalLen());
        // 解析消息体
        _recvMsgNode->Data()[_recvMsgNode->TotalLen()] = '\0';
        Json::Reader reader;
        Json::Value root;
        if (!reader.parse(_recvMsgNode->Data(), root)) {
            std::cerr << "parse error: " << _recvMsgNode->Data() << std::endl;
        }
        assert(root["id"].asInt() == _recvMsgNode->MsgId());

        std::cout << "server received message[id: " << _recvMsgNode->MsgId() << "], size: " << _recvMsgNode->TotalLen()
                  << "B]: " << green(root.toStyledString()) << std::endl;
        Send(_recvMsgNode->Data(), _recvMsgNode->TotalLen(), _recvMsgNode->MsgId());
        // 重置状态，准备接收下一条消息
        Clear();
        auto handler
            = std::bind(&Session::HandlerReadHead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
        async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), handler);
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
    Server* _server;
    string _id;
    queue<shared_ptr<MsgNode>> _sendQueue; // 发送队列
    mutex _sendLock;                       // 发送队列的锁
    shared_ptr<MsgNode> _recvMsgNode;      // 收到的消息体结构
    shared_ptr<MsgNode> _recvHeadNode;     // 收到的消息头结构
};

// Server的实现
inline Server::Server(io_context& ioc, int port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "server start listen port: " << blue(std::to_string(port)) << std::endl;
    StartAccept();
}

inline void
Server::RemoveSession(const string& id) {
    _sessions.erase(id);
}

// 开始接受连接
inline void
Server::StartAccept() {
    // 对每个连接上的客户端连接都创建一个Session来处理该连接的回话请求
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
        // 获取客户端的 IP 地址和端口号并打印
        auto client_ep = newSession->Socket().remote_endpoint();
        std::cout << "client connected: " << green(client_ep.address().to_string()) << " : "
                  << blue(std::to_string(client_ep.port())) << std::endl;

        newSession->Start();                      // 开始会话
        _sessions[newSession->Id()] = newSession; // 保存会话
    }
    StartAccept();                                // 继续接受连接
}

#endif /* !SESSION_H_ */
