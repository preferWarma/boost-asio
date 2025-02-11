#ifndef MSGNODE_H_
#define MSGNODE_H_

#include <cstring>

class MsgNode {
public:
    MsgNode(char* msg, int totalLen)
        : _curLen(0), _totalLen(totalLen) {
        _data = new char[_totalLen];
        memcpy(_data, msg, _totalLen);
    }

    ~MsgNode() {
        delete[] _data;
    }

    char*
    Data() {
        return _data;
    }

    int
    TotalLen() {
        return _totalLen;
    }

private:
    int _curLen;   // 当前已经发送的长度
    int _totalLen; // 总长度
    char* _data;   // 数据
};

#endif /* !MSGNODE_H_ */
