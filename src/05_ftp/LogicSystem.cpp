#include "LogicSystem.h"
#include "AsyncLogSystem.h"
#include "ConfigManager.h"
#include "MsgNode.h"
#include "base64.h"
#include "const.h"
#include <json/json.h>
#include <mutex>

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
LogicSystem::RegisterCallbacks() {
    // 注册回调函数
    _callbacksMap[MsgIDType::TEST_MSG_RECV]
        = std::bind(&LogicSystem::TestMsgCallback, this, std::placeholders::_1, std::placeholders::_2);
    _callbacksMap[MsgIDType::UPLOAD_FILE_RECV]
        = std::bind(&LogicSystem::UploadFileCallback, this, std::placeholders::_1, std::placeholders::_2);
}

void
LogicSystem::TestMsgCallback(shared_ptr<Session> session, shared_ptr<MsgNode> recvMsgNode) {
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(recvMsgNode->Data(), root)) {
        LOG_ERROR("parse error: {}", recvMsgNode->Data());
    }

    LOG_DEBUG("server received message[id: {}, size: {}B]", recvMsgNode->MsgId(), recvMsgNode->TotalLen());

    root["role"]  = "server"; // 服务器角色
    root["error"] = ErrorCodes::Success;
    // 发送响应消息
    session->Send(root.toStyledString(), MsgIDType::TEST_MSG_SEND);
    // 重置头部状态，准备接收下一条消息
    session->ClearHead();
}

void
LogicSystem::UploadFileCallback(shared_ptr<Session> session, shared_ptr<MsgNode> recvMsgNode) {
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(recvMsgNode->Data(), root)) {
        LOG_ERROR("parse error: {}", recvMsgNode->Data());
    }

    LOG_DEBUG("server received message[id: {}, size: {}B]", recvMsgNode->MsgId(), recvMsgNode->TotalLen());

    // 处理上传文件的逻辑
    string decodedData = base64_decode(root["data"].asString());
    auto seq           = root["seq"].asInt();        // 序号
    auto name          = root["name"].asString();    // 文件名
    auto totalSize     = root["total_size"].asInt(); // 总大小
    auto transSize     = root["trans_size"].asInt(); // 已传输大小
    auto fileDir       = ConfigManager::GetInstance().GetStaticPath();
    auto filePath      = (fileDir / name).string();
    LOG_INFO("file path is saved in: {}", filePath);
    // 打开文件并写入数据
    ofstream outFile;
    if (seq == 0) {
        outFile.open(filePath, std::ios::binary | std::ios::trunc); // 对于第一个包，使用截断模式
    } else {
        outFile.open(filePath, std::ios::binary | std::ios::app);   // 对于后续包，使用追加模式
    }
    if (!outFile.is_open()) {
        LOG_ERROR("Failed to open file: {}", filePath);
        return;
    }
    outFile.write(decodedData.data(), decodedData.size());

    // 构造响应消息
    Json::Value response;
    response["error"]      = ErrorCodes::Success;
    response["name"]       = name;
    response["seq"]        = seq;
    response["total_size"] = totalSize;
    response["trans_size"] = transSize;

    // 发送响应消息
    session->Send(response.toStyledString(), MsgIDType::UPLOAD_FILE_SEND);
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

            LOG_DEBUG("LogicSystem::DealMsg: (session id: {}, message id: {})", session->Id(), recvNode->MsgId());
            // 调用对应的回调函数
            auto it = _callbacksMap.find(recvNode->MsgId());
            if (it != _callbacksMap.end()) {
                it->second(session, recvNode);
            } else {
                LOG_ERROR("no callback for message id: {}", recvNode->MsgId());
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
    if (_msgQueue.size() >= MAX_RECV_QUEUE_LEN) {
        LOG_ERROR("recv queue is full, drop msg");
        return;
    }
    _msgQueue.push(logicNode);
    if (_msgQueue.size() >= 1) {
        lock.unlock();
        _consumerCond.notify_one();
    }
}
