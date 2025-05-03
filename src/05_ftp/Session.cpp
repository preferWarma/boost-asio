#include "Session.h"
#include "AsyncLogSystem.h"
#include "LogicSystem.h"
#include "const.h"
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
using boost::asio::detail::socket_ops::network_to_host_long;
using boost::asio::detail::socket_ops::network_to_host_short;
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

Session::~Session() {
    LOG_DEBUG("Session destructor called");
    Clear();

    if (!_ioc.stopped()) {
        _ioc.stop();
    }
    if (_sock.is_open()) {
        _sock.close(); // 关闭socket, 防止资源泄漏
    }
    // 如果Session被析构, 就从Server中移除
    if (_server) {
        _server->RemoveSession(_id);
    }
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
                    LOG_ERROR("client disconnected");
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
                    LOG_ERROR("client disconnected");
                    ClearHead();
                    _server->RemoveSession(_id);
                    co_return;
                }
                // 解析消息体并处理消息
                PraseMsg();
            }
        } catch (const std::exception& e) {
            LOG_ERROR("co_handler error: {}", e.what());
            Clear();
            _server->RemoveSession(_id);
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
        LOG_ERROR("send queue is full, drop msg");
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
    LOG_DEBUG("server send: {}", msg);
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
    MsgId                     = network_to_host_short(MsgId);
    unsigned int validDataLen = 0;
    memcpy(&validDataLen, _recvHeadNode->Data() + HEAD_ID_LEN, HEAD_DATA_LEN);
    // 将网络字节序转换为主机字节序
    validDataLen = network_to_host_long(validDataLen);
    if (validDataLen > MAX_LEN) {
        LOG_ERROR("invalid data len with {}", validDataLen);
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
        LOG_ERROR("head read error: {}", ec.message());
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
        LOG_ERROR("message read error: {}", ec.message());
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
        LOG_ERROR("write error: {}", ec.message());
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
