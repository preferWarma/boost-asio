#ifndef MSGNODE_H_
#define MSGNODE_H_

#include "const.h"
#include <boost/asio.hpp>
#include <cstring>

using boost::asio::detail::socket_ops::host_to_network_long;
using boost::asio::detail::socket_ops::host_to_network_short;

class MsgNode {
public:
    MsgNode(unsigned int totalLen, short msgId)
        : _curLen(0), _totalLen(totalLen), _msgId(msgId) {
        _data = new char[_totalLen + 1];
    }

    ~MsgNode() {
        delete[] _data;
    }

    void
    Clear() {
        if (_data) {
            // memset(_data, 0, _totalLen);
            _curLen = 0;
        }
    }

    char*
    Data() {
        return _data;
    }

    short
    MsgId() {
        return _msgId;
    }

    unsigned int
    TotalLen() {
        return _totalLen;
    }

    unsigned int
    CurLen() {
        return _curLen;
    }

    void
    SetCurLen(unsigned int len) {
        _curLen = len;
    }

protected:
    unsigned int _curLen;   // 当前已经发送的长度
    unsigned int _totalLen; // 总长度
    char* _data;            // 数据
    short _msgId;           // 消息ID
};

class RecvNode : public MsgNode {
public:
    RecvNode(unsigned int totalLen, short msgId)
        : MsgNode(totalLen, msgId) {}
};

class SendNode : public MsgNode {
public:
    SendNode(const char* msg, unsigned int totalLen, short msgId)
        : MsgNode(totalLen + HEAD_TOTAL_LEN, msgId) {
        short networkId = host_to_network_short(msgId);           // 将消息ID转换为网络字节序
        memcpy(_data, &networkId, HEAD_ID_LEN);                   // 填充头部的消息ID字段
        unsigned int networkLen = host_to_network_long(totalLen); // 将消息长度转换为网络字节序
        memcpy(_data + HEAD_ID_LEN, &networkLen, HEAD_DATA_LEN);  // 填充头部的消息长度字段
        memcpy(_data + HEAD_TOTAL_LEN, msg, totalLen);            // 后面是消息的内容
        _data[_totalLen] = '\0';
    }
};

#endif /* !MSGNODE_H_ */
