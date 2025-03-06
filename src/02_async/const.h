#ifndef _CONST_H_
#define _CONST_H_

#include <string>

// 服务器配置信息常量
constexpr short SERVER_PORT = 8080;        // 服务器端口号
const std::string SERVER_IP = "127.0.0.1"; // 服务器IP地址

// 头部字段常量
constexpr int HEAD_ID_LEN    = 2;                           // 头部中ID字段的长度
constexpr int HEAD_DATA_LEN  = 2;                           // 头部中数据长度字段的长度
constexpr int HEAD_TOTAL_LEN = HEAD_ID_LEN + HEAD_DATA_LEN; // 头部的总长度

// 消息体常量
constexpr int MAX_ID  = 1024 * 10; // 最大消息ID
constexpr int MAX_LEN = 1024 * 2;  // 最大消息体长度

#endif
