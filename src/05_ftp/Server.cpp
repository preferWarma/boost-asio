#include "Server.h"
#include "AsyncLogSystem.h"
#include "Session.h"
#include "UserManager.h"
#include "const.h"

#ifndef USE_IOSERVICE_POOL
#include "IOThreadPool.h"
#else
#include "IOServicePool.h"
#endif

using std::string;
using std::string_view;

Server::Server(io_context& ioc, int port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {
    LOG_INFO("server start listen port: {}", port);
    StartAccept();
}

// 开始接受连接
void
Server::StartAccept() {
// 对每个连接上的客户端连接都创建一个Session来处理该连接的回话请求
#ifndef USE_IOSERVICE_POOL
    auto& iocFromPool = IOThreadPool::GetInstance().GetIOService();
#else
    auto& iocFromPool = IOServicePool::GetInstance().GetIOService();
#endif
    auto newSession = std::make_shared<Session>(iocFromPool, this);
    _acceptor.async_accept(newSession->Socket(),
                           std::bind(&Server::HandlerAccept, this, newSession, std::placeholders::_1));
}

// 接受连接完成的回调函数(使用shared_ptr)
void
Server::HandlerAccept(shared_ptr<Session> newSession, const error_code& ec) {
    if (ec) {
        LOG_ERROR("accept error: {}", ec.message());
    } else {
        // 获取客户端的 IP 地址和端口号并打印
        auto client_ep = newSession->Socket().remote_endpoint();
        LOG_INFO("client connected: {} : {}", client_ep.address().to_string(), client_ep.port());

        newSession->Start();                      // 开始会话
        _sessions[newSession->Id()] = newSession; // 保存会话
    }
    StartAccept();                                // 继续接受连接
}

void
Server::Stop() {
    for (auto& session : _sessions) {
        session.second->Stop();
    }
    _sessions.clear();
}
