#include "Server.h"
#include "IOServicePool.h"
#include "Session.h"
#include "lyf.h"

using lyf::PrintTool::blue;
using lyf::PrintTool::green;
using std::string;
using std::string_view;

Server::Server(io_context& ioc, int port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "server start listen port: " << blue(std::to_string(port)) << std::endl;
    StartAccept();
}

// 开始接受连接
void
Server::StartAccept() {
    // 对每个连接上的客户端连接都创建一个Session来处理该连接的回话请求
    auto& iocFromPool = IOServicePool::GetInstance().GetIOService();
    auto newSession   = std::make_shared<Session>(iocFromPool, this);
    _acceptor.async_accept(newSession->Socket(),
                           std::bind(&Server::HandlerAccept, this, newSession, std::placeholders::_1));
}

// 接受连接完成的回调函数(使用shared_ptr)
void
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
