#include "Session.h"
#include "const.h"
#include "lyf.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using lyf::PrintTool::blue;
using lyf::PrintTool::green;

Session::Session(io_context& ioc, Server* server)
    : _sock(ioc), _server(server) {
    // 对每个Session设置一个唯一的ID
    boost::uuids::uuid id = boost::uuids::random_generator()();
    _id                   = boost::uuids::to_string(id);
    // 初始化头部接收节点
    _recvHeadNode = std::make_shared<RecvNode>(HEAD_TOTAL_LEN, -1);
}

void
Session::Start() {
    Clear();
    auto handler
        = std::bind(&Session::HandlerReadHead, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
    // 开始监听, 读取头部信息
    async_read(_sock, buffer(_recvHeadNode->Data(), HEAD_TOTAL_LEN), handler);
}

void
Session::Send(const char* msg, int totalLen, short msgId) {
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
Session::Send(string_view msg, short msgId) {
    Send(msg.data(), msg.size(), msgId);
}

void
Session::HandlerReadHead(const error_code& ec, size_t bytes_transferred) {
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
Session::HandlerReadMsg(const error_code& ec, size_t bytes_transferred) {
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
        async_write(_sock, buffer(msgNode->Data(), msgNode->TotalLen()),
                    std::bind(&Session::HandlerWrite, shared_from_this(), std::placeholders::_1));
    }
}
