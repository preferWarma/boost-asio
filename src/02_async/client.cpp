#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <iostream>
#include <string>

using boost::asio::io_context;
using boost::system::error_code;
using endpoint = boost::asio::ip::tcp::endpoint;
using namespace boost::asio::ip;
using namespace boost::asio;

constexpr int MAX_LEN = 1024;        // 最大长度
const std::string IP  = "127.0.0.1"; // IP地址
constexpr int PORT    = 8080;        // 端口号

int
main() {
    try {
        io_context ioc;                              // 上下文服务
        endpoint remote_ep(make_address(IP), PORT);  // 构造端点, 包含地址和端口
        tcp::socket sock(ioc);                       // 构造socket
        error_code ec = error::host_not_found;       // 错误码
        auto _        = sock.connect(remote_ep, ec); // 连接到服务器
        if (ec) {
            std::cerr << ec.message() << '\n';
            return ec.value();
        }
        std::cout << "connected to server\n";
        std::cout << "please input message to send: ";
        // 发送消息
        std::string msg;
        std::getline(std::cin, msg); // 按行读取输入
        sock.send(buffer(msg));

        // 接收消息
        char receive_buf[MAX_LEN] = {0};
        size_t receive_len        = sock.receive(buffer(receive_buf));
        std::cout << "received message: " << lyf::PrintTool::green(receive_buf) << '\n';

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}
