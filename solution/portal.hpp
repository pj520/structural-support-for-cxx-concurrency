/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_PORTAL
#define _CON_LIB_PORTAL

#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>
#include <memory>

#include "requirements.hpp"

namespace con {

class SerialPortal {
 public:
  template <class F, class... Args>
  void operator()(F&& f, Args&&... args) const
      requires requirements::Callable<F, void, Args...>() {
    f(std::forward<Args>(args)...);
  }
};

template <bool DAEMON>
class ThreadPortal;

template <>
class ThreadPortal<true> {
 public:
  template <class F, class... Args>
  void operator()(F&& f, Args&&... args) const requires
      requirements::Callable<F, void, Args...>() {
    std::thread(std::forward<F>(f), std::forward<Args>(args)...).detach();
  }
};

class ThreadManager {
 public:
  ~ThreadManager() {
    mtx_.lock();
    while (!threads_.empty()) {
      std::deque<std::thread> threads = std::move(threads_);
      mtx_.unlock();
      do {
        threads.back().join();
        threads.pop_back();
      } while (!threads.empty());
      mtx_.lock();
    }
    mtx_.unlock();
  }

  void emplace(std::thread&& th) {
    std::lock_guard<std::mutex> lk(mtx_);
    threads_.emplace_back(std::forward<std::thread>(th));
  }

  static ThreadManager& instance() {
    static ThreadManager manager;
    return manager;
  }

 private:
  explicit ThreadManager() = default;

  std::mutex mtx_;
  std::deque<std::thread> threads_;
};

template <>
class ThreadPortal<false> {
 public:
  template <class F, class... Args>
  void operator()(F&& f, Args&&... args) const requires
      requirements::Callable<F, void, Args...>() {
    ThreadManager::instance().emplace(
        std::thread(std::forward<F>(f), std::forward<Args>(args)...));
  }
};

template <class Task, class Queue>
requires requirements::Runnable<Task>()
class ThreadPool {
 public:
  explicit ThreadPool() : is_shutdown_(false) {}

  void execute() {
    std::unique_lock<std::mutex> lk(mtx_);
    for (;;) {
      while (!tasks_.empty()) {
        Task current = std::move(tasks_.front());
        tasks_.pop();
        mtx_.unlock();
        current();
        mtx_.lock();
      }
      if (is_shutdown_) {
        break;
      }
      cond_.wait(lk);
    }
  }

  void shutdown() {
    {
      std::lock_guard<std::mutex> lk(mtx_);
      is_shutdown_ = true;
    }
    cond_.notify_all();
  }

  template <class... Args>
  void emplace(Args&&... args) {
    {
      std::lock_guard<std::mutex> lk(mtx_);
      tasks_.emplace(std::forward<Args>(args)...);
    }
    cond_.notify_one();
  }

 private:
  std::mutex mtx_;
  std::condition_variable cond_;
  bool is_shutdown_;
  Queue tasks_;
};

template <class Task = abstraction::SharedProxy<abstraction::Callable<void()>>,
          class Queue = std::queue<Task>>
class ThreadPoolPortal {
 public:
  template <class Portal = class ThreadPortal<false>>
  explicit ThreadPoolPortal(
      std::size_t concurrency, const Portal& portal = Portal())
      : pool_(std::make_shared<ThreadPool<Task, Queue>>()) {
    for (std::size_t i = 0; i < concurrency; ++i) {
      portal([pool = pool_] { pool->execute(); });
    }
  }

  ThreadPoolPortal(ThreadPoolPortal&&) = default;

  ThreadPoolPortal(const ThreadPoolPortal&) = delete;

  ~ThreadPoolPortal() { if ((bool)pool_) pool_->shutdown(); }

  template <class F, class... Args>
  void operator()(F&& f, Args&&... args) const requires
      requirements::Callable<F, void, Args...>() {
    pool_->emplace(bind_simple(std::forward<F>(f), std::forward<Args>(args)...));
  }

 private:
  std::shared_ptr<ThreadPool<Task, Queue>> pool_;
};

}

#endif // _CON_LIB_PORTAL
