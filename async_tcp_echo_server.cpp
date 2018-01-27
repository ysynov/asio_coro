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

namespace mycoro {


template<typename T>
struct coreturn {
    struct promise;
    friend struct promise;
    using handle_type = std::experimental::coroutine_handle<promise>;
    coreturn(const coreturn &) = delete;
    coreturn(coreturn &&s)
    : coro(s.coro) {
        std::cout << "Coreturn wrapper moved" << std::endl;
        s.coro = nullptr;
    }
    ~coreturn() {
        std::cout << "Coreturn wrapper gone" << std::endl;
        //if ( coro && coro.done()) coro.destroy();
    }
    coreturn &operator = (const coreturn &) = delete;
    coreturn &operator = (coreturn &&s) {
        coro = s.coro;
        s.coro = nullptr;
        return *this;
    }
    struct promise {
        friend struct coreturn;
        promise() {
            std::cout << "Promise created" << std::endl;
        }
        ~promise() {
            std::cout << "Promise died" << std::endl;
        }

        /*
        auto return_value(T v) {
            std::cout << "Got an answer of " << v << std::endl;
            value = v;
            return std::experimental::suspend_never{};
        }
        */
        auto return_void() {
            std::cout << "return_void" << std::endl;
            return std::experimental::suspend_never{};
        }

        auto final_suspend() {
            std::cout << "Finished the coro" << std::endl;
            return std::experimental::suspend_never{};
        }
        void unhandled_exception() {
            std::exit(1);
        }
    private:
        //T value;
    };
protected:
    /*
    T get() {
        //return coro.promise().value;
    }
    */
    coreturn(handle_type h)
    : coro(h) {
        std::cout << "Created a coreturn wrapper object" << std::endl;
    }
    handle_type coro;
};

template<typename T>
struct sync : public coreturn<T> {
    using coreturn<T>::coreturn;
    using handle_type = typename coreturn<T>::handle_type;
    /*
    T get() {
        std::cout << "We got asked for the return value..." << std::endl;
        if ( not this->coro.done() ) this->coro.resume();
        return coreturn<T>::get();
    }
    */
    struct promise_type : public coreturn<T>::promise {
        auto get_return_object() {
            std::cout << "Send back a sync" << std::endl;
            return sync<T>{handle_type::from_promise(*this)};
        }
        auto initial_suspend() {
            std::cout << "Started the coroutine, don't stop now!" << std::endl;
            return std::experimental::suspend_never{};
        }
    };
};

}

template <typename SyncReadStream, typename DynamicBuffer>
auto async_read_some(SyncReadStream &s, DynamicBuffer &&buffers) {
  struct Awaiter {
    SyncReadStream &s;
    DynamicBuffer &&buffers;

    std::error_code ec;
    size_t sz;

    bool await_ready() { return false; }
    auto await_resume() {
      if (ec)
        throw std::system_error(ec);
      return sz;
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
  mycoro::sync<void> do_read()
  {
    auto self(shared_from_this());
    while (true)
    {
        auto res = co_await async_read_some(socket_, boost::asio::buffer(data_, max_length));
        std::cout << "after await" << std::endl;
        do_write(res);
    }

    /*
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            do_write(length);
          }
        });
    */
    //co_return 0;
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            //do_read();
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
