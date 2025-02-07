#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <thread>

// 作用域声明
using boost::asio::io_context;
using boost::system::error_code;
using namespace boost::asio::ip;
using namespace boost::asio;

// 别名声明
using endpoint   = boost::asio::ip::tcp::endpoint;
using socket_ptr = std::shared_ptr<tcp::socket>;

// 常量声明
constexpr int MAX_LEN = 1024;        // 最大长度
const std::string IP  = "127.0.0.1"; // IP地址
constexpr int PORT    = 8080;        // 端口号

// 全局变量
std::set<std::shared_ptr<std::thread>> threads_set; // 线程池

void
session(socket_ptr sock) { // 会话
    try {
        while (true) {
            char data[MAX_LEN] = {0};
            error_code ec;
            size_t read_len = sock->read_some(buffer(data), ec);
            if (ec == error::eof) {
                std::cout << lyf::PrintTool::red("client disconnected") << "\n";
                break;
            } else if (ec) {
                throw boost::system::system_error(ec);
            }
            std::cout << "received from : " << lyf::PrintTool::green(sock->remote_endpoint().address().to_string())
                      << '\n';
            std::cout << "message: " << lyf::PrintTool::green(data) << '\n';
            // 回显消息
            std::string msg(data);
            sock->write_some(buffer(msg), ec);
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
}

void
server(io_context& ioc, unsigned short port) { // 接收连接
    // 构造用于接收连接的acceptor
    tcp::acceptor acceptor(ioc, endpoint(tcp::v4(), port));
    while (true) {
        socket_ptr sock = std::make_shared<tcp::socket>(ioc); // 对每个连接上的客户端创建用于通信的socket
        acceptor.accept(*sock);                               // 等待连接
        auto t = std::make_shared<std::thread>(session, sock); // 创建线程处理会话
        threads_set.insert(t);                                 // 将线程加入线程池
    }
}

int
main() {
    try {
        io_context ioc;
        server(ioc, PORT); // 启动服务器接收连接
        for (auto& t : threads_set) {
            t->join();     // 等待线程结束
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}
