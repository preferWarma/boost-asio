#ifndef ASYNC_LOG_SYSTEM_HEAD_ONLY_H_
#define ASYNC_LOG_SYSTEM_HEAD_ONLY_H_

#include "ConfigManager.h"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <ostream>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::atomic;
using std::condition_variable;
using std::lock_guard;
using std::mutex;
using std::ofstream;
using std::ostream;
using std::ostream_iterator;
using std::queue;
using std::string;
using std::unique_lock;
using std::vector;

// 日志队列
class LogQueue {
public:
    void
    Push(const string& msg) {
        lock_guard<mutex> lock(_mutex);
        _que.push(msg);
        if (_que.size() == 1) {
            _cond.notify_one();
        }
    }

    bool
    Pop(string& msg) {
        unique_lock<mutex> lock(_mutex);
        _cond.wait(lock, [this] {
            return !_que.empty() || _isShutDown;
        });
        if (_isShutDown && _que.empty()) {
            return false;
        }
        msg = _que.front();
        _que.pop();
        return true;
    }

    void
    ShutDown() {
        _isShutDown = true;
        _cond.notify_all();
    }

private:
    queue<string> _que;       // 日志队列
    mutex _mutex;             // 互斥锁
    condition_variable _cond; // 条件变量
    atomic<bool> _isShutDown; // 是否关闭
};

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
};

enum LogMode {
    TO_FILE    = 0x1,
    TO_CONSOLE = 0x2,
};

class AsyncLogSystem {
public:
    AsyncLogSystem(const AsyncLogSystem&) = delete;
    AsyncLogSystem(AsyncLogSystem&&)      = delete;
    AsyncLogSystem&
    operator=(const AsyncLogSystem&)
        = delete;
    AsyncLogSystem&
    operator=(AsyncLogSystem&&)
        = delete;

    static AsyncLogSystem&
    GetInstance() {
        static AsyncLogSystem instance;
        return instance;
    }

private:
    AsyncLogSystem()
        : _console(std::cout), _isShutDown(false), _logMode(0) {
        auto logFilePath = ConfigManager::GetInstance().GetValue("Log", "Path");
        auto logModeStr  = ConfigManager::GetInstance().GetValue("Log", "Mode");
        bool toFile      = logModeStr.find("FILE") != string::npos;
        bool toConsole   = logModeStr.find("CONSOLE") != string::npos;
        if (toFile) {
            _logMode |= LogMode::TO_FILE;
        }
        if (toConsole) {
            _logMode |= LogMode::TO_CONSOLE;
        }

        _logFile.open(logFilePath, std::ios::out | std::ios::app);
        if (!_logFile.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logFilePath);
        }

        _worker = std::thread([this]() -> void {
            string msg;
            while (_logQue.Pop(msg)) {
                if (_logMode & LogMode::TO_CONSOLE) {
                    _console << msg << std::endl;
                }
                if (_logMode & LogMode::TO_FILE) {
                    _logFile << msg << std::endl;
                }
            }
        });
    }

public:
    ~AsyncLogSystem() {
        _isShutDown = true;
        _logQue.ShutDown();
        if (_worker.joinable()) {
            _worker.join();
        }
        if (_logFile.is_open()) {
            _logFile.close();
        }
    }

public:
    template<typename... Args>
    void
    Log(LogLevel level, const string& fmt, Args&&... args) {
        if (_isShutDown) {
            return;
        }
        _logQue.Push(FormatMessage(level, fmt, std::forward<Args>(args)...));
    }

private:
    inline string
    to_string(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG :
                return "DEBUG";
            case LogLevel::INFO :
                return "INFO";
            case LogLevel::WARN :
                return "WARN";
            case LogLevel::ERROR :
                return "ERROR";
            default :
                return "UNKNOWN";
        }
    }

    // 辅助函数, 将单个参数转化为字符串
    template<typename T>
    string
    to_string(T&& arg) {
        std::stringstream oss;
        oss << std::forward<T>(arg);
        return oss.str();
    }

    inline string
    getCurrentTime(const string& format = "%Y-%m-%d %H:%M:%S") {
        time_t now = time(nullptr);
        char buf[1024];
        strftime(buf, sizeof(buf), format.c_str(), localtime(&now));
        return buf;
    }

    // 使用模板折叠格式化日志消息，支持 "{}" 占位符
    template<typename... Args>
    string
    FormatMessage(const LogLevel level, const string& fmt, Args&&... args) {
        vector<string> argStr = {to_string(std::forward<Args>(args))...};
        std::ostringstream oss;
        oss << "[" << to_string(level) << "] " << getCurrentTime() << ": ";

        size_t argIndex    = 0;
        size_t pos         = 0;
        size_t placeholder = fmt.find("{}", pos);

        while (placeholder != string::npos) {
            oss << fmt.substr(pos, placeholder - pos);
            if (argIndex < argStr.size()) {
                oss << argStr[argIndex++];
            } else {
                // 没有足够的参数，保留 "{}"
                oss << "{}";
            }
            pos         = placeholder + 2; // 跳过 "{}"
            placeholder = fmt.find("{}", pos);
        }

        // 添加剩余的字符串
        oss << fmt.substr(pos);

        // 如果还有剩余的参数，按原方式拼接
        while (argIndex < argStr.size()) {
            oss << argStr[argIndex++];
        }

        return oss.str();
    }

private:
    LogQueue _logQue;         // 日志队列
    std::thread _worker;      // 工作线程
    ofstream _logFile;        // 日志输出文件
    ostream& _console;        // 日志输出流
    atomic<bool> _isShutDown; // 是否关闭
    int _logMode;             // 日志输出模式
};

// 全局的日志对象
inline AsyncLogSystem& logger = AsyncLogSystem::GetInstance();

#define LOG_DEBUG(...) logger.Log(LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  logger.Log(LogLevel::INFO, __VA_ARGS__)
#define LOG_WARN(...)  logger.Log(LogLevel::WARN, __VA_ARGS__)
#define LOG_ERROR(...) logger.Log(LogLevel::ERROR, __VA_ARGS__)

#endif // ASYNC_LOG_SYSTEM_HEAD_ONLY_H_
