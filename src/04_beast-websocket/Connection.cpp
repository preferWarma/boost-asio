#include "Connection.h"
#include "ConnectionManager.h"
#include "lyf.h"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <mutex>

using beast::buffers_to_string;
using boost::asio::buffer;
using boost::asio::make_strand;
using boost::beast::get_lowest_layer;
using boost::system::error_code;
using boost::uuids::random_generator;
using boost::uuids::to_string;
using lyf::PrintTool::green;
using std::lock_guard;
using std::make_unique;
using std::mutex;

Connection::Connection(io_context& ioc)
    : _ioc(ioc), _wsPtr(make_unique<stream<tcp_stream>>(make_strand(ioc))), _uid(to_string(random_generator{}())) {}

string
Connection::Uid() const {
    return _uid;
}

tcp::socket&
Connection::Socket() const {
    return get_lowest_layer(*_wsPtr).socket();
}

void
Connection::AsyncAccept() {
    auto self = shared_from_this();
    _wsPtr->async_accept([self](error_code ec) {
        try {
            if (!ec) {
                std::cout << "Connection accept success from " << self->Socket().remote_endpoint() << std::endl;
                ConnectionManager::GetInstance().AddConnection(self);
                self->Start();
            } else {
                std::cerr << "Connection accept failed: " << ec.message() << std::endl;
            }
        } catch (std::exception& e) {
            std::cerr << "Connection::AsyncAccept Error: " << e.what() << std::endl;
        }
    });
}

void
Connection::AsyncSend(const string& sendMsg) {
    lock_guard<mutex> lock(_sendMutex);
    _sendQueue.push(string(sendMsg));
    if (_sendQueue.size() > 1) {
        return; // 队列中有其他消息，不立即发送
    } else {
        SendCallback(sendMsg);
    }
}

void
Connection::SendCallback(const string& message) {
    auto self = shared_from_this();
    _wsPtr->async_write(buffer(message), [self](error_code ec, size_t bytes) {
        try {
            if (!ec) {
                lock_guard<mutex> lock(self->_sendMutex);
                self->_sendQueue.pop();                           // 对应当前已经发送的message变量
                if (!self->_sendQueue.empty()) {
                    self->SendCallback(self->_sendQueue.front()); // 发送下一个消息
                }
            } else {
                std::cerr << "Send failed: " << ec.message() << std::endl;
                ConnectionManager::GetInstance().RemoveConnection(self);
            }
        } catch (std::exception& e) {
            std::cerr << "Connection::SendCallback Error: " << e.what() << std::endl;
        }
    });
}

void
Connection::Start() {
    auto self = shared_from_this();
    _wsPtr->async_read(_recvBuf, [self](error_code ec, size_t bytes) {
        try {
            if (!ec) {
                std::cout << "Received " << bytes << " bytes from " << self->Socket().remote_endpoint() << std::endl;
                self->_wsPtr->text(self->_wsPtr->got_text());
                auto recvData = buffers_to_string(self->_recvBuf.data());
                std::cout << "data is: " << green(recvData) << std::endl;
                self->_recvBuf.consume(self->_recvBuf.size()); // 清空接收缓冲区
                self->AsyncSend(recvData);
                self->Start();                                 // 继续接收下一个消息
            } else {
                std::cerr << "Receive failed: " << ec.message() << std::endl;
                ConnectionManager::GetInstance().RemoveConnection(self);
            }
        } catch (std::exception& e) {
            ConnectionManager::GetInstance().RemoveConnection(self);
            std::cerr << "Connection::Start Error: " << e.what() << std::endl;
        }
    });
}
