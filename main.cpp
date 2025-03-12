#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <iostream>

using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::io_context;
using boost::asio::ip::tcp;
using boost::asio::signal_set;
using boost::asio::use_awaitable;
using boost::system::error_code;
using std::cout, std::endl;
using std::string;
namespace this_coro = boost::asio::this_coro;

awaitable<void>
handler(tcp::socket socket) {
    try {
        // 读取数据
        char data[1024];
        size_t n = co_await socket.async_read_some(buffer(data), use_awaitable);
        // 处理数据
        std::string message(data, n);
        cout << "Received: " << message << endl;
        // 发送响应
        std::string response = message;
        co_await async_write(socket, buffer(response), use_awaitable);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        socket.close();
    }
}

awaitable<void>
listener() {
    // 获取当前协程的执行器
    auto executor = co_await this_coro::executor;
    // 创建一个TCP acceptor
    tcp::acceptor acceptor(executor, {tcp::v4(), 8080});
    // 输出服务器地址
    cout << "Server listening on: " << acceptor.local_endpoint() << endl;
    // 循环等待客户端连接
    while (true) {
        // 等待客户端连接
        auto socket = co_await acceptor.async_accept(use_awaitable);
        // 输出客户端地址
        cout << "Client connected: " << socket.remote_endpoint() << endl;
        // 创建一个新的协程来处理客户端连接
        co_spawn(executor, handler(std::move(socket)), detached);
    }
}

int
main() {
    try {
        io_context ioc(1);
        signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](const error_code&, int) {
            ioc.stop();
        });
        co_spawn(ioc, listener(), detached);
        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
