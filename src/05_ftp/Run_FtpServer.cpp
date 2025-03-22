#include "AsyncLogSystem.h"
#include "ConfigManager.h"
#include "Server.h"
#include "const.h"
#include <exception>
#include <iostream>
#include <string>

#ifndef USE_IOSERVICE_POOL
#include "IOThreadPool.h"
#else
#include "IOServicePool.h"
#endif

using boost::asio::io_context;
using boost::asio::signal_set;

int
main(int argc, const char** argv) {
    // 配置文件初始化
    auto& config = ConfigManager::GetInstance();
    // 服务池初始化
#ifndef USE_IOSERVICE_POOL
    auto& pool = IOThreadPool::GetInstance();
#else
    auto& pool = IOServicePool::GetInstance();
#endif
    try {
        // 这里的io_context主要用于绑定server的acceptor，对于每个处理连接上对话的session绑定的则是服务池的io_context
        io_context ioc;
        auto SERVER_PORT = stoi(config["SelfServer"]["Port"]);
        Server server(ioc, SERVER_PORT);

        signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const error_code& ec, int signal) {
            if (!ec) {
                LOG_INFO("\nReceived signal: {}\nserver stop\n", strsignal(signal));
                ioc.stop();
                pool.Stop();
            }
        });

        ioc.run(); // 启动负责监听的线程
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
