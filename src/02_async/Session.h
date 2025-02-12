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
using lyf::PrintTool::blue;
using lyf::PrintTool::green;
using std::map;
using std::mutex;
using std::queue;
using std::shared_ptr;
using std::string;

constexpr int MAX_LEN = 1024 * 2;

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
        _recvHeadNode         = std::make_shared<MsgNode>(HEAD_LEN);
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
        _sock.async_read_some(buffer(_data, MAX_LEN), handler);
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
        auto msgNode = _sendQueue.front();
        async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()),
                    std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1));
        std::cout << "server send: " << blue(msg) << std::endl;
    }

    void
    Close() {
        _sock.close();
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
        int copyLen = 0;        // 已经复制的长度
        while (bytes_transferred > 0) {
            if (!_headParsed) { // 头部还没有解析完
                // 接受的数据不足一个消息头大小
                if (bytes_transferred + _recvHeadNode->CurLen() < HEAD_LEN) {
                    memcpy(_recvHeadNode->Data() + _recvHeadNode->CurLen(), _data + copyLen, bytes_transferred);
                    _recvHeadNode->SetCurLen(_recvHeadNode->CurLen() + bytes_transferred);
                    memset(_data, 0, sizeof(_data));
                    auto handler = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1,
                                             std::placeholders::_2);
                    _sock.async_read_some(buffer(_data, MAX_LEN), handler);
                    return;
                }
                // 接受的数据足够一个消息头大小
                int headRemain = HEAD_LEN - _recvHeadNode->CurLen();
                memcpy(_recvHeadNode->Data() + _recvHeadNode->CurLen(), _data + copyLen, headRemain); // 复制消息头
                // 更新已经处理的data长度和剩余未处理的长度
                copyLen += headRemain;
                bytes_transferred -= headRemain;
                // 解析消息头
                short validDataLen = 0;
                memcpy(&validDataLen, _recvHeadNode->Data(), HEAD_LEN);
                if (validDataLen > MAX_LEN) { // 头部长度非法（比如发送了一个很大的消息导致消息长度大于MAX_LEN）
                    std::cout << "invalid data len with " << _recvHeadNode->Data() << std::endl;
                    _server->RemoveSession(_id);
                    return;
                }
                _recvMsgNode = std::make_shared<MsgNode>(validDataLen);
                // 消息的长度小于头部标记的长度, 则数据没有接收完, 先将部分消息放到接受节点中
                if (bytes_transferred < validDataLen) {
                    memcpy(_recvMsgNode->Data() + _recvMsgNode->CurLen(), _data + copyLen, bytes_transferred);
                    _recvMsgNode->SetCurLen(_recvMsgNode->CurLen() + bytes_transferred);
                    memset(_data, 0, sizeof(_data));
                    _headParsed  = true; // 标记消息头已经解析完成
                    auto handler = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1,
                                             std::placeholders::_2);
                    _sock.async_read_some(buffer(_data, MAX_LEN), handler);
                    return;
                } else { // 消息的长度大于等于头部标记的长度, 则数据可以接收完, 直接处理
                    memcpy(_recvMsgNode->Data() + _recvMsgNode->CurLen(), _data + copyLen, validDataLen);
                    _recvMsgNode->SetCurLen(_recvMsgNode->CurLen() + validDataLen);
                    copyLen += validDataLen;
                    bytes_transferred -= validDataLen;
                    _recvMsgNode->Data()[_recvMsgNode->TotalLen()] = '\0';
                    // 处理接受完的消息
                    std::cout << "server has receive: " << green(_recvMsgNode->Data()) << std::endl;
                    Send(_recvMsgNode->Data(), _recvMsgNode->TotalLen());
                    // 重置状态, 准备接收下一条消息(或者一个数据包含有多个数据，执行切包操作)
                    _headParsed = false;
                    _recvMsgNode->Clear();
                    if (bytes_transferred == 0) { // 没有数据了, 继续监听
                        memset(_data, 0, sizeof(_data));
                        auto handler = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1,
                                                 std::placeholders::_2);
                        _sock.async_read_some(buffer(_data, MAX_LEN), handler);
                        return;
                    } else { // 还数据了, 继续接收和解析
                        continue;
                    }
                }
            } else { // 头部已经解析完成, 数据还没读完, 继续接收上次没有接收完的数据
                int remainLen = _recvMsgNode->TotalLen() - _recvMsgNode->CurLen();
                // 本次接收的数据长度还是小于剩余的数据长度, 则数据没有接收完, 先将部分消息放到接受节点中
                if (bytes_transferred < remainLen) {
                    memcpy(_recvMsgNode->Data() + _recvMsgNode->CurLen(), _data + copyLen, bytes_transferred);
                    _recvMsgNode->SetCurLen(_recvMsgNode->CurLen() + bytes_transferred);
                    memset(_data, 0, sizeof(_data));
                    auto handler = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1,
                                             std::placeholders::_2);
                    _sock.async_read_some(buffer(_data, MAX_LEN), handler);
                    return;
                } else { // 本次接收的数据长度大于等于剩余的数据长度, 则数据可以接收完, 直接处理
                    memcpy(_recvMsgNode->Data() + _recvMsgNode->CurLen(), _data + copyLen, remainLen);
                    _recvMsgNode->SetCurLen(_recvMsgNode->CurLen() + remainLen);
                    copyLen += remainLen;
                    bytes_transferred -= remainLen;
                    _recvMsgNode->Data()[_recvMsgNode->TotalLen()] = '\0';
                    // 处理接受完的消息
                    std::cout << "server has receive: " << green(_recvMsgNode->Data()) << std::endl;
                    Send(_recvMsgNode->Data(), _recvMsgNode->TotalLen());
                    std::cout << "server has send: " << lyf::PrintTool::red(_recvMsgNode->Data()) << std::endl;
                    // 重置状态, 准备接收下一条消息(或者一个数据包含有多个数据，执行切包操作)
                    _recvMsgNode->Clear();
                    _headParsed = false;
                    if (bytes_transferred == 0) { // 没有数据了, 继续监听
                        memset(_data, 0, sizeof(_data));
                        auto handler = std::bind(&Session::HandlerRead, shared_from_this(), std::placeholders::_1,
                                                 std::placeholders::_2);
                        _sock.async_read_some(buffer(_data, MAX_LEN), handler);
                        return;
                    } else { // 还有数据了, 继续接收和解析
                        continue;
                    }
                }
            }
        }
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
    char _data[MAX_LEN];
    Server* _server;
    string _id;
    queue<shared_ptr<MsgNode>> _sendQueue; // 发送队列
    mutex _sendLock;                       // 发送队列的锁
    shared_ptr<MsgNode> _recvMsgNode;      // 收到的消息结构
    shared_ptr<MsgNode> _recvHeadNode;     // 收到的消息头结构
    bool _headParsed = false;              // 是否解析了消息头
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
