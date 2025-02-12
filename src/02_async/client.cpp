#include "MsgNode.h"
#include "Session.h"
#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <string>

using boost::asio::io_context;
using boost::system::error_code;
using endpoint = boost::asio::ip::tcp::endpoint;
using lyf::PrintTool::green;
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std::chrono_literals;

const std::string IP = "127.0.0.1"; // IP地址
constexpr int PORT   = 8080;        // 端口号

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
        std::cout << "connected to server(ip: " << green(IP) << ", port: " << green(std::to_string(PORT)) << ")\n";

        // 发送线程
        std::thread sendThread([&sock]() {
            while (true) {
                std::this_thread::sleep_for(2ms);
                string msg            = "hello world";
                short requestLen      = msg.length();
                char sendMsg[MAX_LEN] = {0};
                memcpy(sendMsg, &requestLen, HEAD_LEN);
                memcpy(sendMsg + HEAD_LEN, msg.c_str(), requestLen);
                boost::asio::write(sock, buffer(sendMsg, requestLen + HEAD_LEN));
            }
        });

        // 接收线程
        std::thread recvThread([&sock]() {
            while (true) {
                std::this_thread::sleep_for(2ms);
                char receiveHead[HEAD_LEN] = {0}; // 接收消息头
                boost::asio::read(sock, buffer(receiveHead, HEAD_LEN));
                short responseLen = 0;            // 消息长度
                memcpy(&responseLen, receiveHead, HEAD_LEN);
                char receive_buf[MAX_LEN] = {0};  // 接收消息
                boost::asio::read(sock, buffer(receive_buf, responseLen));
                std::cout << "received message[size: " << responseLen << "B]: " << green(receive_buf) << '\n';
            }
        });

        // 启动线程
        sendThread.join();
        recvThread.join();

    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}
