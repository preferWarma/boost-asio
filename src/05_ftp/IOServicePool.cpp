#include "IOServicePool.h"
#include "AsyncLogSystem.h"
#include <cstddef>
#include <memory>

using boost::asio::make_work_guard;
using std::make_unique;

IOServicePool::IOServicePool(size_t size)
    : _ioServices(size), _works(size) {
    // 创建工作对象
    for (size_t i = 0; i < size; ++i) {
        _works[i] = make_unique<Work>((make_work_guard(_ioServices[i].get_executor())));
    }
    // 创建线程
    for (size_t i = 0; i < size; ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
        });
    }
    LOG_DEBUG("ioc池初始化完成");
}

io_context&
IOServicePool::GetIOService() {
    static size_t next = 0;
    auto& ioService    = _ioServices[next];
    next               = (next + 1) % _ioServices.size();
    return ioService;
}

void
IOServicePool::Stop() {
    // 停止所有 io_context
    for (auto& work : _works) {
        work->get_executor().context().stop();
        work.reset();
    }
    // 等待所有线程结束
    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    LOG_DEBUG("ioc池停止");
}
