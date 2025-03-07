#ifndef LOGICSYSTEM_H_
#define LOGICSYSTEM_H_

#include "MsgNode.h"
#include "Session.h"
#include "const.h"
#include "lyf.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>

using std::condition_variable;
using std::mutex;
using std::shared_ptr;
using std::thread;
using std::unordered_map;

// 回调函数类型别名定义
using callback_t = std::function<void(shared_ptr<Session>, shared_ptr<MsgNode>)>;

class LogicNode {
public:
    LogicNode(shared_ptr<Session> session, shared_ptr<MsgNode> msgNode)
        : _session(session), _recvNode(msgNode) {}

    shared_ptr<Session>&
    GetSession() {
        return _session;
    }

    shared_ptr<MsgNode>&
    GetRecvNode() {
        return _recvNode;
    }

private:
    shared_ptr<Session> _session;
    shared_ptr<MsgNode> _recvNode;
};

class LogicSystem : public lyf::Singleton<LogicSystem> {
    friend class lyf::Singleton<LogicSystem>;

public:
    ~LogicSystem();

    void
    PostMsgToQue(shared_ptr<LogicNode> logicNode);

private:
    LogicSystem()
        : _stop(false) {
        RegisterCallbacks();
        _workerThread = thread(&LogicSystem::DealMsg, this);
    }

    void
    RegisterCallbacks() {
        // 注册回调函数
        _callbacksMap[MsgIDType::HelloWorld]
            = std::bind(&LogicSystem::HelloWorldCallback, this, std::placeholders::_1, std::placeholders::_2);
    }

    void
    DealMsg();

    void
    HelloWorldCallback(shared_ptr<Session> session, shared_ptr<MsgNode> recvMsgNode);

private:
    thread _workerThread;                           // 处理消息的线程
    queue<shared_ptr<LogicNode>> _msgQueue;         // 消息队列
    mutex _mutex;                                   // 消息队列的互斥锁
    condition_variable _consumerCond;               // 消费者条件变量
    bool _stop;                                     // 停止标志
    unordered_map<short, callback_t> _callbacksMap; // 回调函数映射表
};

#endif
