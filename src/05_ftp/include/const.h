#pragma once
#include <climits>

// 采用IO Service Pool 服务池
#define USE_IOSERVICE_POOL
// 采用协程(C++20)
#if __cplusplus >= 202002L
#define USE_COROUTINE
#endif

enum ErrorCodes {
    Success = 0,
};

constexpr int MAX_LEN = INT_MAX; // 最大消息体长度
// 头部字段常量
constexpr int HEAD_ID_LEN    = 2;                           // 头部中ID字段的长度
constexpr int HEAD_DATA_LEN  = 4;                           // 头部中数据长度字段的长度
constexpr int HEAD_TOTAL_LEN = HEAD_ID_LEN + HEAD_DATA_LEN; // 头部的总长度
// 队列长度限制
constexpr int MAX_SEND_QUEUE_LEN = 2000000; // 最大发送队列长度
constexpr int MAX_RECV_QUEUE_LEN = 2000000; // 最大接收队列长度

// 消息ID常量
enum MsgIDType {
    TEST_MSG_RECV    = 1001, // 测试消息请求
    TEST_MSG_SEND    = 1002, // 测试消息回复
    UPLOAD_FILE_RECV = 1003, // 发送文件请求
    UPLOAD_FILE_SEND = 1004, // 发送文件回复
};
