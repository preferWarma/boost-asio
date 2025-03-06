#include "MsgNode.h"
#include "Session.h"
#include "const.h"
#include "lyf.h"
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <cstring>
#include <iostream>
#include <json/json.h>
#include <string>

using boost::asio::io_context;
using boost::system::error_code;
using endpoint = boost::asio::ip::tcp::endpoint;
using lyf::PrintTool::blue;
using lyf::PrintTool::green;
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std::chrono_literals;

void
BaseSend(tcp::socket& sock, string_view data, short msgId) {
    Json::Value root;
    root["id"]   = msgId;
    root["data"] = data.data();
    string msg   = root.toStyledString();

    // 构造消息头
    short requestId        = msgId;
    short networkRequestId = host_to_network_short(requestId);
    short requestLen       = msg.length();
    short networkRequest   = host_to_network_short(requestLen);

    char sendMsg[MAX_LEN] = {0};
    memcpy(sendMsg, &networkRequestId, HEAD_ID_LEN);               // 消息ID字段
    memcpy(sendMsg + HEAD_ID_LEN, &networkRequest, HEAD_DATA_LEN); // 消息长度字段

    // 构造消息体
    memcpy(sendMsg + HEAD_TOTAL_LEN, msg.c_str(), requestLen);
    // 发送消息
    boost::asio::write(sock, buffer(sendMsg, requestLen + HEAD_TOTAL_LEN));
}

void
testSend(tcp::socket& sock) {
    std::this_thread::sleep_for(2ms);
    string msg = "hello world";
    BaseSend(sock, msg, 408);
}

void
userInputSend(tcp::socket& sock) {
    std::this_thread::sleep_for(2ms);
    string msg;
    std::cout << "input message: \n";
    std::getline(std::cin, msg);
    BaseSend(sock, msg, 408);
}

void
testRecv(tcp::socket& sock) {
    std::this_thread::sleep_for(2ms);
    char receiveHead[HEAD_TOTAL_LEN] = {0};
    // 接收消息头
    boost::asio::read(sock, buffer(receiveHead, HEAD_TOTAL_LEN));
    // 消息ID
    short responseId = 0;
    memcpy(&responseId, receiveHead, HEAD_ID_LEN);
    responseId = network_to_host_short(responseId); // 转换为主机字节序
    // 消息长度
    short responseLen = 0;
    memcpy(&responseLen, receiveHead + HEAD_ID_LEN, HEAD_DATA_LEN);
    responseLen = network_to_host_short(responseLen); // 转换为主机字节序

    // 接收消息体
    char receive_buf[MAX_LEN] = {0};
    boost::asio::read(sock, buffer(receive_buf, responseLen));
    std::cout << "received message[id: " << responseId << "], size: " << responseLen << "B]: " << green(receive_buf)
              << std::endl;
}

int
main() {
    try {
        io_context ioc;                                           // 上下文服务
        endpoint remote_ep(make_address(SERVER_IP), SERVER_PORT); // 构造端点, 包含地址和端口
        tcp::socket sock(ioc);                                    // 构造socket
        error_code ec = error::host_not_found;                    // 错误码
        auto _        = sock.connect(remote_ep, ec);              // 连接到服务器
        if (ec) {
            std::cerr << ec.message() << '\n';
            return ec.value();
        }
        std::cout << "connected to server(ip: " << green(SERVER_IP) << ", port: " << blue(std::to_string(SERVER_PORT))
                  << ")\n";

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
