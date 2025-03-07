#include "LogicSystem.h"
#include "MsgNode.h"
#include "lyf.h"
#include <json/json.h>
#include <mutex>

using lyf::PrintTool::green;
using lyf::PrintTool::red;
using std::unique_lock;

LogicSystem::~LogicSystem() {
    // 析构函数需要等待工作线程处理完再退出，但是工作线程可能处于挂起状态，所以要发送一个激活信号唤醒工作线程,
    // 并且将_b_stop标记设置为true
    _stop = true;
    _consumerCond.notify_one();
    if (_workerThread.joinable()) {
        _workerThread.join();
    }
}

void
LogicSystem::HelloWorldCallback(shared_ptr<Session> session, shared_ptr<MsgNode> recvMsgNode) {
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(recvMsgNode->Data(), root)) {
        std::cerr << red("parse error: ") << recvMsgNode->Data() << std::endl;
    }

    std::cout << "server received message[id: " << recvMsgNode->MsgId() << "], size: " << recvMsgNode->TotalLen()
              << "B]: " << green(root.toStyledString()) << std::endl;
    root["role"] = "server"; // 服务器角色
    session->Send(root.toStyledString(), recvMsgNode->MsgId());
    // 重置头部状态，准备接收下一条消息
    session->ClearHead();
}

void
LogicSystem::DealMsg() {
    while (true) {
        unique_lock<mutex> lock(_mutex);
        // 阻塞当前线程, 在消息队列 _msgQueue 不为空或者 _stop 标志被设置为 true 时，才会继续执行后续代码
        _consumerCond.wait(lock, [this] {
            return !_msgQueue.empty() || _stop;
        });

        auto doWork = [this]() {
            auto logicNode = _msgQueue.front();
            _msgQueue.pop();
            auto session  = logicNode->GetSession();
            auto recvNode = logicNode->GetRecvNode();

            std::cout << "LogicSystem::DealMsg: (session id: " << session->Id() << ", message id: " << recvNode->MsgId()
                      << ")" << std::endl;
            // 调用对应的回调函数
            auto it = _callbacksMap.find(recvNode->MsgId());
            if (it != _callbacksMap.end()) {
                it->second(session, recvNode);
            } else {
                std::cerr << "LogicSystem::DealMsg: no callback for message id: " << recvNode->MsgId() << std::endl;
            }
        };

        // 如果服务器关闭了, 则处理所有消息后退出
        if (_stop) {
            while (!_msgQueue.empty()) {
                doWork();
            }
            return;
        }

        // 如果没有关闭则处理一个消息后继续循环
        else {
            doWork();
            continue;
        }
    }
}

void
LogicSystem::PostMsgToQue(shared_ptr<LogicNode> logicNode) {
    unique_lock<mutex> lock(_mutex);
    _msgQueue.push(logicNode);
    if (_msgQueue.size() >= 1) {
        _consumerCond.notify_one();
    }
}
