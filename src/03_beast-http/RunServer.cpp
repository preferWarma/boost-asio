#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>

using namespace std::chrono_literals;
namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = boost::asio::ip::tcp;
using boost::asio::steady_timer;
using boost::system::error_code;
using std::string;

namespace lyf {
size_t
RequestCount() { // 统计请求次数
    static size_t count = 0;
    return ++count;
}

time_t
GetTimeStamp() { // 获取时间戳
    return time(nullptr);
}

class httpConnection : public std::enable_shared_from_this<httpConnection> {
    static constexpr size_t MAX_BUFFER_SIZE = 8192;

public:
    httpConnection(tcp::socket socket)
        : _socket(std::move(socket)), _buffer(MAX_BUFFER_SIZE), _deadline(_socket.get_executor(), 60s) {}

    void
    Start() {
        ReadRequest();
        CheckDeadline();
    }

private:
    void
    ReadRequest() {
        auto self = shared_from_this();
        http::async_read(_socket, _buffer, _request, [self](error_code ec, size_t bytes_transferred) {
            if (!ec) {
                self->ProcessRequest();
            }
        });
    }

    void
    CheckDeadline() {
        auto self = shared_from_this();
        _deadline.async_wait([self](error_code ec) {
            if (!ec) {
                self->_socket.close();
            }
        });
    }

    void
    ProcessRequest() {
        _response.version(_request.version());
        _response.keep_alive(false);
        switch (_request.method()) {
            case http::verb::get :

                HandleGet();
                break;
            case http::verb::post :
                HandlePost();
                break;
            default :
                _response.result(http::status::bad_request);
                _response.set(http::field::server, "lyf");
                _response.set(http::field::content_type, "text/plain");
                beast::ostream(_response.body())
                    << "Invalid request-method '" << string(_request.method_string()) << "'";
                break;
        }
        WriteResponse();
    }

    void
    WriteResponse() {
        auto self = shared_from_this();
        _response.content_length(_response.body().size());
        http::async_write(_socket, _response, [self](error_code ec, size_t bytes_transferred) {
            if (!ec) {
                if (self->_socket.shutdown(tcp::socket::shutdown_send, ec)) {
                    self->_deadline.cancel();
                }
            }
        });
    }

    void
    HandleGet() {
        _response.result(http::status::ok);
        _response.set(http::field::server, "lyf");
        if (_request.target() == "/count") {
            _response.set(http::field::content_type, "text/html");
            beast::ostream(_response.body()) << "<html>\n"
                                             << "<head><title>Request count</title></head>\n"
                                             << "<body>\n"
                                             << "<h1>Request count</h1>\n"
                                             << "<p>There have been " << RequestCount() << " requests so far.</p>\n"
                                             << "</body>\n"
                                             << "</html>\n";
        } else if (_request.target() == "/time") {
            _response.set(http::field::content_type, "text/html");
            // 将时间戳转为hh:mm:ss格式
            time_t t      = GetTimeStamp();
            auto timeinfo = localtime(&t);
            char time_str[80];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
            // 输出HTML页面
            beast::ostream(_response.body()) << "<html>\n"
                                             << "<head><title>Current time</title></head>\n"
                                             << "<body>\n"
                                             << "<h1>Current time</h1>\n"
                                             << "<p>The current time is " << time_str << "\n"
                                             << "</body>\n"
                                             << "</html>\n";
        } else {
            _response.result(http::status::not_found);
            _response.set(http::field::content_type, "text/plain");
            beast::ostream(_response.body()) << "GET request received";
        }
    }

    void
    HandlePost() {
        // 处理post请求
        if (_request.target() == "/email") {
            auto& body   = _request.body();
            auto bodyStr = beast::buffers_to_string(body.data());
            std::cout << "Received body is: " << bodyStr << std::endl;
            _response.set(http::field::content_type, "text/json");
            Json::Value root;
            Json::Reader reader;
            Json::Value src;
            if (reader.parse(bodyStr, src)) {
                auto email = src["email"].asString();
                std::cout << "Email: " << email << std::endl;
                root["message"] = "Email received successfully";
                root["email"]   = src["email"];
                root["error"]   = "0";
                beast::ostream(_response.body()) << root.toStyledString();
            } else {
                std::cout << "Failed to parse JSON" << std::endl;
                root["error"] = "1001";
                beast::ostream(_response.body()) << root.toStyledString();
                return;
            }
        }
    }

private:
    tcp::socket _socket;
    beast::flat_buffer _buffer;
    http::request<http::dynamic_body> _request;
    http::response<http::dynamic_body> _response;
    boost::asio::steady_timer _deadline;
};

void
httpSever(tcp::acceptor& acceptor, tcp::socket& sock) {
    acceptor.async_accept(sock, [&](error_code ec) {
        if (!ec) {
            // 打印客户端IP地址
            std::cout << "New request from " << sock.remote_endpoint().address().to_string() << std::endl;
            std::make_shared<httpConnection>(std::move(sock))->Start();
        }
        httpSever(acceptor, sock);
    });
}

} // namespace lyf

int
main() {
    using namespace lyf;
    using namespace boost::asio;
    try {
        io_context ioc;
        string IP = "127.0.0.1";
        tcp::acceptor acceptor(ioc, {ip::make_address_v4(IP), 8080});
        tcp::socket sock(ioc);
        httpSever(acceptor, sock);
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
