#ifndef IOTHREADPOOL_H_
#define IOTHREADPOOL_H_
#include "lyf.h"
#include <boost/asio.hpp>
#include <cstddef>

using boost::asio::io_context;
using std::mutex;
using std::thread;
using std::unique_ptr;
using std::vector;

class IOThreadPool : public lyf::Singleton<IOThreadPool> {
    friend class lyf::Singleton<IOThreadPool>;
    using Work = boost::asio::executor_work_guard<io_context::executor_type>;

public:
    ~IOThreadPool() {}

    io_context&
    GetIOService();

    void
    Stop();

private:
    // 构造函数默认线程数量为CPU核心数量
    IOThreadPool(size_t size = std::thread::hardware_concurrency());

private:
    io_context _ioc;         // 主io_context
    unique_ptr<Work> _work;  // 管理ioc，让ioc不退出
    vector<thread> _threads; // 线程容器
};

#endif // !IOTHREADPOOL_H_
