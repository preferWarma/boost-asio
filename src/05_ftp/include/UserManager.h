#pragma once
#include "lyf.h"
#include <mutex>
#include <unordered_map>

using std::lock_guard;
using std::mutex;
using std::shared_ptr;
using std::unordered_map;

class Session;

class UserManager : public lyf::Singleton<UserManager> {
    friend class lyf::Singleton<UserManager>;

public:
    shared_ptr<Session>
    GetUserSession(int uid) {
        lock_guard<mutex> lock(_mutex);
        if (_uidToSession.find(uid) != _uidToSession.end()) {
            return _uidToSession[uid];
        }
        return nullptr;
    }

    void
    SetUserSession(int uid, shared_ptr<Session> session) {
        lock_guard<mutex> lock(_mutex);
        _uidToSession[uid] = session;
    }

    void
    RemoveUserSession(int uid) {
        lock_guard<mutex> lock(_mutex);
        _uidToSession.erase(uid);
    }

    ~UserManager() {
        _uidToSession.clear();
    }

private:
    UserManager() {}

private:
    mutex _mutex;                                          // 互斥锁
    unordered_map<int, shared_ptr<Session>> _uidToSession; // 用户到会话的映射
};
