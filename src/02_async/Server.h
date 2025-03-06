#ifndef SERVER_H_
#define SERVER_H_

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <unordered_map>

using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::system::error_code;
using std::shared_ptr;
using std::string;
using std::unordered_map;

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
    io_context& _ioc;                                     // 上下文
    tcp::acceptor _acceptor;                              // 用于接受客户端连接的接受器
    unordered_map<string, shared_ptr<Session>> _sessions; // 此服务器占有的所有会话Session
};

#endif /*!SERVER_H_ */
