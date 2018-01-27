//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <experimental/coroutine>

#include <boost/asio.hpp>

using boost::asio::ip::tcp;

template <typename... Args>
struct std::experimental::coroutine_traits<void, Args...> {
    struct promise_type {
        void get_return_object() { }
        std::experimental::suspend_never initial_suspend() { return {}; }
        std::experimental::suspend_never final_suspend() { return {}; }
        void return_void() { }
        void unhandled_exception() {
         std::exit(1);
        }
    };
};

template <typename SyncReadStream, typename DynamicBuffer>
auto async_read_some(SyncReadStream &s, DynamicBuffer &&buffers) {
  struct Awaiter {
    SyncReadStream &s;
    DynamicBuffer &&buffers;

    std::error_code ec;
    size_t sz;

    bool await_ready() { return false; }
    auto await_resume() {
      return std::make_pair(ec, sz);
    }
    void await_suspend(std::experimental::coroutine_handle<> coro) {
      s.async_read_some(std::move(buffers),
                            [this, coro](auto ec, auto sz) mutable {
                              this->ec = ec;
                              this->sz = sz;
                              coro.resume();
                            });
    }
  };
  return Awaiter{ s, std::forward<DynamicBuffer>(buffers) };
}

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    while (true)
    {
        const auto [ec, sz] = co_await async_read_some(socket_, boost::asio::buffer(data_, max_length));
        if (!ec)
        {
            do_write(sz);
        }
        else
        {
            std::cout << "Error: " << ec << std::endl;
            break;
        }
    }
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (ec)
          {
            std::cout << "Error: " << ec << std::endl;
          }
        });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};


int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
