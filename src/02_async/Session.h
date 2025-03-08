#ifndef SESSION_H_
#define SESSION_H_

#include "MsgNode.h"
#include "Server.h"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>

using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using std::map;
using std::mutex;
using std::queue;
using std::shared_ptr;
using std::string;
using std::string_view;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(io_context& ioc, Server* server);

    const string&
    Id() {
        return _id;
    }

    tcp::socket&
    Socket() {
        return _sock;
    }

    shared_ptr<MsgNode>&
    RecvMsgNode() {
        return _recvMsgNode;
    }

    void
    Start();

    void
    Send(const char* msg, int totalLen, short msgId);

    void
    Send(string_view msg, short msgId);

    void
    Stop() {
        _sock.cancel();
        _sock.close();
    }

    void
    ClearHead() {
        if (_recvHeadNode) {
            _recvHeadNode->Clear();
        }
    }

    void
    ClearMsg() {
        if (_recvMsgNode) {
            _recvMsgNode->Clear();
        }
    }

    void
    Clear() {
        ClearHead();
        ClearMsg();
    }

private:
    void
    HandlerReadHead(const error_code& ec, size_t bytes_transferred);

    void
    HandlerReadMsg(const error_code& ec, size_t bytes_transferred);

    // 写的时候一次性写完, 采用async_send而不是async_write_some，所以不需要bytes_transferred参数
    void
    HandlerWrite(const error_code& ec);

private:
    tcp::socket _sock;
    Server* _server;
    string _id;
    queue<shared_ptr<MsgNode>> _sendQueue; // 发送队列
    mutex _sendLock;                       // 发送队列的锁
    shared_ptr<MsgNode> _recvMsgNode;      // 收到的消息体结构
    shared_ptr<MsgNode> _recvHeadNode;     // 收到的消息头结构
};

#endif /* !SESSION_H_ */
