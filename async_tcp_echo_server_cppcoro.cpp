//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio.hpp>
#include <cppcoro/task.hpp>
#include <cstdlib>
#include <experimental/coroutine>
#include <iostream>
#include <memory>
#include <utility>

using boost::asio::ip::tcp;

template <typename... Args>
struct std::experimental::coroutine_traits<void, Args...> {
    struct promise_type {
        void get_return_object() {}
        std::experimental::suspend_never initial_suspend() { return {}; }
        std::experimental::suspend_never final_suspend() { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

template <typename SyncReadStream, typename DynamicBuffer>
auto async_read_some(SyncReadStream &s, DynamicBuffer &&buffers) {
    struct Awaiter {
        SyncReadStream &s;
        DynamicBuffer buffers;

        std::error_code ec;
        size_t sz;

        bool await_ready() { return false; }
        auto await_resume() { return std::make_pair(ec, sz); }
        void await_suspend(std::experimental::coroutine_handle<> coro) {
            s.async_read_some(std::move(buffers),
                              [this, coro](auto ec, auto sz) mutable {
                                  this->ec = ec;
                                  this->sz = sz;
                                  std::cout << "read completed" << std::endl;
                                  coro.resume();
                              });
        }
    };
    return Awaiter{s, std::forward<DynamicBuffer>(buffers)};
}

template <typename SyncReadStream, typename DynamicBuffer>
auto async_write(SyncReadStream &s, DynamicBuffer &&buffers) {
    struct Awaiter {
        SyncReadStream &s;
        DynamicBuffer buffers;

        std::error_code ec;
        size_t sz;

        bool await_ready() { return false; }
        auto await_resume() { return std::make_pair(ec, sz); }
        void await_suspend(std::experimental::coroutine_handle<> coro) {
            boost::asio::async_write(
                s, std::move(buffers), [this, coro](auto ec, auto sz) mutable {
                    this->ec = ec;
                    this->sz = sz;
                    std::cout << "write completed" << std::endl;
                    coro.resume();
                });
        }
    };
    return Awaiter{s, std::forward<DynamicBuffer>(buffers)};
}

template <typename Acceptor> auto async_accept(Acceptor &acceptor) {
    struct Awaiter {
        Acceptor &acceptor;

        tcp::socket socket;
        std::error_code ec;

        bool await_ready() { return false; }
        auto await_resume() { return std::make_pair(ec, std::move(socket)); }
        void await_suspend(std::experimental::coroutine_handle<> coro) {
            acceptor.async_accept([this, coro](auto ec, auto socket) mutable {
                this->ec = ec;
                this->socket = std::move(socket);
                coro.resume();
            });
        }
    };
    return Awaiter{acceptor, tcp::socket(acceptor.get_executor().context())};
}

class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { do_read(); }

  private:
    cppcoro::task<std::error_code> do_write(std::size_t length) {
        auto self(shared_from_this());

        const auto[ec, sz] =
            co_await async_write(socket_, boost::asio::buffer(data_, length));
        std::cout << "do_write completed" << std::endl;
        co_return ec;
    }

    void do_read() {
        auto self(shared_from_this());
        while (true) {
            std::cout << "before read" << std::endl;
            const auto[ec, sz] = co_await async_read_some(
                socket_, boost::asio::buffer(data_, max_length));
            std::cout << "after read" << std::endl;
            if (!ec) {
                std::cout << "before write" << std::endl;
                auto ec = co_await do_write(sz);
                std::cout << "after write" << std::endl;
                if (ec) {
                    std::cout << "Error writing to socket: " << ec << std::endl;
                    break;
                }
            } else {
                std::cout << "Error reading from socket: " << ec << std::endl;
                break;
            }
        }
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class server {
  public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

  private:
    void do_accept() {
        while (true) {
            auto[ec, socket] = co_await async_accept(acceptor_);
            if (!ec) {
                std::make_shared<session>(std::move(socket))->start();
            } else {
                std::cout << "Error accepting connection: " << ec << std::endl;
            }
        }
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
