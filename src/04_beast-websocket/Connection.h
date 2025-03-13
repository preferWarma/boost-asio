#ifndef CONNECTION_H_
#define CONNECTION_H_

#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <queue>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
using beast::flat_buffer;
using beast::tcp_stream;
using boost::asio::io_context;
using boost::asio::ip::tcp;
using std::queue;
using std::string;
using std::unique_ptr;
using websocket::stream;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(io_context& ioc);

    string
    Uid() const;

    tcp::socket&
    Socket() const;

    void
    AsyncAccept();

    void
    AsyncSend(const string& message);

    void
    SendCallback(const string& message);

    void
    Start();

private:
    io_context& _ioc;                      // IO Context
    unique_ptr<stream<tcp_stream>> _wsPtr; // WebSocket stream
    string _uid;                           // 连接的唯一标识符
    flat_buffer _recvBuf;                  // 接收缓冲区
    queue<string> _sendQueue;              // 发送队列
    std::mutex _sendMutex;                 // 发送队列的互斥锁
};

#endif // CONNECTION_H_
