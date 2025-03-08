#ifndef IOSERVICEPOOL_H_
#define IOSERVICEPOOL_H_

#include "lyf.h"
#include <boost/asio.hpp>
#include <mutex>
#include <vector>

using boost::asio::io_context;
using std::mutex;
using std::thread;
using std::vector;

class IOServicePool : public lyf::Singleton<IOServicePool> {
    friend class lyf::Singleton<IOServicePool>;

public:
    using Work    = boost::asio::executor_work_guard<io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

public:
    ~IOServicePool() {}

    io_context&
    GetIOService();

    void
    Stop();

private:
    // 构造函数默认线程数量为CPU核心数量
    IOServicePool(size_t size = std::thread::hardware_concurrency());

private:
    vector<io_context> _ioServices; // io_context 容器
    vector<WorkPtr> _works;         // 工作容器
    vector<thread> _threads;        // 线程容器
};

#endif // IOSERVICEPOOL_H_
