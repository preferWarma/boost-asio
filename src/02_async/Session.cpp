#include "Session.h"
#include "LogicSystem.h"
#include "config.h"
#include "const.h"
#include "lyf.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <json/json.h>
#include <memory>

#ifdef USE_COROUTINE
#include <boost/asio/detached.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
#endif

using boost::asio::async_read;
using boost::asio::async_write;
using boost::asio::buffer;
using boost::asio::detail::socket_ops::network_to_host_short;
using lyf::PrintTool::blue;
#ifndef USE_IOSERVICE_POOL
using boost::asio::bind_executor;
#endif

Session::Session(io_context& ioc, Server* server)
    : _sock(ioc),
      _server(server)
#ifndef USE_IOSERVICE_POOL
      ,
      _strand(ioc.get_executor())
#endif
#ifdef USE_COROUTINE
      ,
      _ioc(ioc)
#endif
{
    // 对每个Session设置一个唯一的ID
    boost::uuids::uuid id = boost::uuids::random_generator()();
    _id                   = boost::uuids::to_string(id);
    // 初始化头部接收节点
    _recvHeadNode = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, -1);
}

void
Session::Start() {
    Clear();
// 开始监听, 读取头部信息
#ifdef USE_COROUTINE
    // 协程版本
    auto shared_this = shared_from_this(); // 伪闭包，防止Session被析构
    auto co_handler  = [this, shared_this]() -> awaitable<void> {
        try {
            while (true) {
                // 读取头部信息
                size_t bytes_transferred
                    = co_await async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), use_awaitable);
                if (bytes_transferred == 0) {
                    std::cout << "client disconnected" << std::endl;
                    ClearHead();
                    _server->RemoveSession(_id);
                    co_return;
                }
                // 解析头部信息并处理头部
                PraseHead();

                // 读取消息体
                bytes_transferred
                    = co_await async_read(_sock, buffer(_recvMsgNode->Data(), _recvMsgNode->TotalLen()), use_awaitable);
                if (bytes_transferred == 0) {
                    std::cout << "client disconnected" << std::endl;
                    ClearHead();
                    _server->RemoveSession(_id);
                    co_return;
                }
                // 解析消息体并处理消息
                PraseMsg();
            }
        } catch (const std::exception& e) {
            std::cerr << "co_handler error: " << e.what() << std::endl;
        }
    };
    co_spawn(_ioc, co_handler, detached);
#else
    // 非协程版本
    auto handler
        = std::bind(&Session::HandlerReadHead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
#ifndef USE_IOSERVICE_POOL
    async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), bind_executor(_strand, handler));
#else
    async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), handler);
#endif
#endif
}

void
Session::Send(const char* msg, int totalLen, short msgId) {
    // 加锁
    std::lock_guard<mutex> lock(_sendLock);
    bool pending = false; // 是否有数据正在发送
    if (!_sendQueue.empty()) {
        pending = true;
    }
    if (_sendQueue.size() >= MAX_SEND_QUEUE_LEN) {
        std::cout << "send queue is full, drop msg" << std::endl;
        return;
    }
    _sendQueue.push(std::make_shared<SendNode>(msg, totalLen, msgId));
    if (pending) { // 如果有数据正在发送, 就不发送了
        return;
    }
    // 没有数据正在发送, 就发送队列中的数据
    auto msgNode = _sendQueue.front();
    auto handler = std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1);
#ifndef USE_IOSERVICE_POOL
    async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()), bind_executor(_strand, handler));
#else
    async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()), handler);
#endif
    std::cout << "server send: " << blue(msg) << std::endl;
}

void
Session::Send(string_view msg, short msgId) {
    Send(msg.data(), msg.size(), msgId);
}

void
Session::PraseHead() {
    short MsgId = 0;
    memcpy(&MsgId, _recvHeadNode->Data(), HEAD_ID_LEN);
    // 将网络字节序转换为主机字节序
    MsgId = network_to_host_short(MsgId);
    if (MsgId > MAX_ID) {
        std::cout << "invalid msg id with " << MsgId << std::endl;
        ClearHead();
        _server->RemoveSession(_id);
        return;
    }

    short validDataLen = 0;
    memcpy(&validDataLen, _recvHeadNode->Data() + HEAD_ID_LEN, HEAD_DATA_LEN);
    // 将网络字节序转换为主机字节序
    validDataLen = network_to_host_short(validDataLen);
    if (validDataLen > MAX_LEN) {
        std::cout << "invalid data len with " << validDataLen << std::endl;
        ClearHead();
        _server->RemoveSession(_id);
        return;
    }
    // 继续监听消息体
    _recvMsgNode = std::make_shared<RecvNode>(validDataLen, MsgId);
}

void
Session::HandlerReadHead(const error_code& ec, size_t bytes_transferred) {
    if (ec) {
        std::cout << "head read error: " << ec.message() << std::endl;
        ClearHead();
        _server->RemoveSession(_id);
        return;
    }
    assert(bytes_transferred == HEAD_TOTAL_LEN);

    // 此时头部接受完成, 解析消息头
    PraseHead();

    auto handler
        = std::bind(&Session::HandlerReadMsg, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
#ifndef USE_IOSERVICE_POOL
    async_read(_sock, buffer(_recvMsgNode->Data(), _recvMsgNode->TotalLen()), bind_executor(_strand, handler));
#else
    async_read(_sock, buffer(_recvMsgNode->Data(), _recvMsgNode->TotalLen()), handler);
#endif
}

void
Session::PraseMsg() {
    // 解析消息体
    _recvMsgNode->Data()[_recvMsgNode->TotalLen()] = '\0';
    // 调用逻辑系统处理消息
    LogicSystem::GetInstance().PostMsgToQue(std::make_shared<LogicNode>(shared_from_this(), _recvMsgNode));
}

void
Session::HandlerReadMsg(const error_code& ec, size_t bytes_transferred) {
    if (ec) {
        std::cout << "message read error: " << ec.message() << std::endl;
        ClearMsg();
        _server->RemoveSession(_id);
        return;
    }
    assert(bytes_transferred == _recvMsgNode->TotalLen());
    // 解析消息体
    PraseMsg();

    auto handler
        = std::bind(&Session::HandlerReadHead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
#ifndef USE_IOSERVICE_POOL
    async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), bind_executor(_strand, handler));
#else
    async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), handler);
#endif
}

// 写的时候一次性写完, 采用async_send而不是async_write_some，所以不需要bytes_transferred参数
void
Session::HandlerWrite(const error_code& ec) {
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
        auto handler  = std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1);
#ifndef USE_IOSERVICE_POOL
        async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()), bind_executor(_strand, handler));
#else
        async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()), handler);
#endif
    }
}
