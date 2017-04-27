/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_BINARY_SEMAPHORE
#define _CON_LIB_BINARY_SEMAPHORE

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <thread>
#include <system_error>
#include <future>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

#ifdef _POSIX_SOURCE
#include <semaphore.h>
#endif // _POSIX_SOURCE

#if defined(_GLIBCXX_HAVE_LINUX_FUTEX) && ATOMIC_INT_LOCK_FREE > 1
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif // defined(_GLIBCXX_HAVE_LINUX_FUTEX) && ATOMIC_INT_LOCK_FREE > 1

namespace con {

class SpinBinarySemaphore {
 public:
  explicit SpinBinarySemaphore() {
    flag_.test_and_set(std::memory_order_relaxed);
  }

  void wait() {
    while (flag_.test_and_set(std::memory_order_relaxed)) {}
    std::atomic_thread_fence(std::memory_order_acquire);
  }

  void release() { flag_.clear(std::memory_order_release); }

 private:
  std::atomic_flag flag_;
};

class BlockingBinarySemaphore {
 public:
  BlockingBinarySemaphore() : ready_(false), notified_(false) {}

  void wait() {
    {
      std::unique_lock<std::mutex> lk(mtx_);
      cond_.wait(lk, [&] { return ready_; });
    }
    while (!notified_.load(std::memory_order_relaxed)) {
      std::this_thread::yield();
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    ready_ = false;
    notified_.store(false, std::memory_order_relaxed);
  }

  void release() {
    {
      std::lock_guard<std::mutex> lk(mtx_);
      ready_ = true;
    }
    cond_.notify_one();
    notified_.store(true, std::memory_order_release);
  }

 private:
  std::condition_variable cond_;
  std::mutex mtx_;
  bool ready_;
  std::atomic_bool notified_;
};

#ifdef _WIN32
class WinEventBinarySemaphore {
 public:
  WinEventBinarySemaphore() : handle_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
    if (handle_ == NULL) {
      throw std::system_error((int)GetLastError(), std::system_category());
    }
  }

  ~WinEventBinarySemaphore() { CloseHandle(handle_); }

  void wait() { WaitForSingleObject(handle_, INFINITE); }

  void release() { SetEvent(handle_); }

 private:
  const HANDLE handle_;
};
#endif // _WIN32

#ifdef _POSIX_SOURCE
class PosixBinarySemaphore {
 public:
  explicit PosixBinarySemaphore() {
    if (sem_init(&sem_, 0, 0u) == -1) {
      throw std::system_error(std::make_error_code(
          errno == ENOSPC ? std::errc::no_space_on_device
                          : std::errc::operation_not_permitted));
    }
  }

  ~PosixBinarySemaphore() {
    sem_destroy(&sem_);
  }

  void wait() {
    sem_wait(&sem_);
  }

  void release() {
    sem_post(&sem_);
  }

 private:
  sem_t sem_;
};
#endif // _POSIX_SOURCE

#if defined(_GLIBCXX_HAVE_LINUX_FUTEX) && ATOMIC_INT_LOCK_FREE > 1
class LinuxFutexBinarySemaphore {
 public:
  explicit LinuxFutexBinarySemaphore() : futex_(0) {}

  void wait() {
    futex(&futex_, FUTEX_WAIT, 0, NULL, NULL, 0);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
  }

  void release() {
    __atomic_store_n(&futex_, 1, __ATOMIC_RELEASE);
    futex(&futex_, FUTEX_WAKE, 1, NULL, NULL, 0);
  }

 private:
  static inline int futex(int *uaddr, int futex_op, int val,
                          const struct timespec *timeout,
                          int *uaddr2, int val3) {
    return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr, val3);
  }

  int futex_;
};
#endif // defined

class DisposableBinarySemaphore {
 public:
  void wait() { prom_.get_future().wait(); }

  void release() { prom_.set_value(); }

 private:
  std::promise<void> prom_;
};

}

#endif // _CON_LIB_BINARY_SEMAPHORE
