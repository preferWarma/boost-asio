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
#include <json/json.h>
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

void
testSend(tcp::socket& sock) {
    std::this_thread::sleep_for(2ms);
    Json::Value root;
    root["id"]            = std::to_string(lyf::getCurrentTimeStamp());
    root["data"]          = "hello world";
    string msg            = root.toStyledString();
    short requestLen      = msg.length();
    short networkRequest  = host_to_network_short(requestLen);
    char sendMsg[MAX_LEN] = {0};
    memcpy(sendMsg, &networkRequest, HEAD_LEN);
    memcpy(sendMsg + HEAD_LEN, msg.c_str(), requestLen);
    boost::asio::write(sock, buffer(sendMsg, requestLen + HEAD_LEN));
}

void
userInputSend(tcp::socket& sock) {
    std::this_thread::sleep_for(2ms);
    string msg;
    std::cout << "input message: \n";
    std::getline(std::cin, msg);
    Json::Value root;
    root["id"]            = std::to_string(lyf::getCurrentTimeStamp());
    root["data"]          = msg;
    msg                   = root.toStyledString();
    short requestLen      = msg.length();
    short networkRequest  = host_to_network_short(requestLen);
    char sendMsg[MAX_LEN] = {0};
    memcpy(sendMsg, &networkRequest, HEAD_LEN);
    memcpy(sendMsg + HEAD_LEN, msg.c_str(), requestLen);
    boost::asio::write(sock, buffer(sendMsg, requestLen + HEAD_LEN));
}

void
testRecv(tcp::socket& sock) {
    std::this_thread::sleep_for(2ms);
    char receiveHead[HEAD_LEN] = {0};                               // 接收消息头
    boost::asio::read(sock, buffer(receiveHead, HEAD_LEN));
    short responseLen = 0;                                          // 消息长度
    memcpy(&responseLen, receiveHead, HEAD_LEN);
    responseLen               = network_to_host_short(responseLen); // 转换为主机字节序
    char receive_buf[MAX_LEN] = {0};                                // 接收消息
    boost::asio::read(sock, buffer(receive_buf, responseLen));
    std::cout << "received message[size: " << responseLen << "B]: " << green(receive_buf) << std::endl;
}

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
        std::cout << "connected to server(ip: " << green(IP) << ", port: " << blue(std::to_string(PORT)) << ")\n";

        // 发送线程
        std::thread sendThread([&sock]() {
            while (true) {
                userInputSend(sock);
            }
        });

        // 接收线程
        std::thread recvThread([&sock]() {
            while (true) {
                testRecv(sock);
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
