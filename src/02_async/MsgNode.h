#ifndef MSGNODE_H_
#define MSGNODE_H_

#include <boost/asio.hpp>
#include <cstring>

using boost::asio::detail::socket_ops::host_to_network_short;

constexpr int HEAD_LEN = 2; // 消息头的长度

class MsgNode {
public:
    // 发送时的构造函数
    MsgNode(char* msg, short totalLen)
        : _curLen(0), _totalLen(totalLen + HEAD_LEN) {
        _data          = new char[_totalLen + 1];
        int networkLen = host_to_network_short(totalLen); // 将消息长度转换为网络字节序
        memcpy(_data, &networkLen, HEAD_LEN);             // 前HEAD_LENGTH个字节表示消息的长度
        memcpy(_data + HEAD_LEN, msg, totalLen);          // 后面是消息的内容
        _data[_totalLen] = '\0';
    }

    // 接收时的构造函数
    MsgNode(short totalLen)
        : _curLen(0), _totalLen(totalLen) {
        _data = new char[_totalLen + 1];
    }

    ~MsgNode() {
        delete[] _data;
    }

    void
    Clear() {
        if (_data) {
            memset(_data, 0, _totalLen);
            _curLen = 0;
        }
    }

    char*
    Data() {
        return _data;
    }

    int
    TotalLen() {
        return _totalLen;
    }

    int
    CurLen() {
        return _curLen;
    }

    void
    SetCurLen(int len) {
        _curLen = len;
    }

private:
    int _curLen;   // 当前已经发送的长度
    int _totalLen; // 总长度
    char* _data;   // 数据
};

#endif /* !MSGNODE_H_ */
