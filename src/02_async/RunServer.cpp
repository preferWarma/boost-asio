#include "IOThreadPool.h"
#include "Server.h"
#include "Session.h"
#include "const.h"
#include <exception>
#include <iostream>

using boost::asio::signal_set;
using boost::system::error_code;

int
main(int argc, const char** argv) {
    try {
        // 服务池初始化
        auto& iocPool = IOThreadPool::GetInstance();
        // 这里的io_context主要用于绑定server的acceptor，对于每个处理连接上对话的session绑定的则是服务池的io_context
        io_context ioc;
        Server server(ioc, SERVER_PORT);

        signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const error_code& ec, int signal) {
            if (!ec) {
                std::cout << "\nReceived signal: " << strsignal(signal) << "\nserver stop\n";
                server.Stop();
                ioc.stop();
                iocPool.Stop();
            }
        });

        ioc.run(); // 启动负责监听的线程
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
