#include "IOThreadPool.h"

IOThreadPool::IOThreadPool(size_t size) {
    _work = std::make_unique<Work>(_ioc.get_executor());
    for (size_t i = 0; i < size; ++i) {
        _threads.emplace_back([this] {
            _ioc.run();
        });
    }
}

io_context&
IOThreadPool::GetIOService() {
    return _ioc;
}

void
IOThreadPool::Stop() {
    _ioc.stop();
    _work->reset();
    for (auto& t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}
