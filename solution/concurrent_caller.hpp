/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_CONCURRENT_CALLER
#define _CON_LIB_CONCURRENT_CALLER

#include <vector>
#include <functional>
#include <queue>

#include "core.hpp"
#include "util.hpp"
#include "abstraction.hpp"

namespace con {

template <class ConcurrentCallable = abstraction::SharedProxy<abstraction::ConcurrentCallable>>
class ConcurrentCaller0D {
 public:
  template <class T>
  explicit ConcurrentCaller0D(T&& callable)
      : callable_(std::forward<T>(callable)) {}

  constexpr std::size_t size() const { return 1u; }

  template <class LinearBuffer, class Callback>
  void call(LinearBuffer& buffer, const Callback& callback) requires
      requirements::Callable<ConcurrentCallable,
                             void,
                             decltype(buffer.fetch()),
                             Callback>() {
    callable_(buffer.fetch(), callback);
  }

 private:
  ConcurrentCallable callable_;
};

template <class ConcurrentCallable>
auto make_concurrent_caller(ConcurrentCallable&& callable) {
  return ConcurrentCaller0D<ConcurrentCallable>(
      std::forward<ConcurrentCallable>(callable));
}

template <class ConcurrentCallable = abstraction::SharedProxy<abstraction::ConcurrentCallable>,
          class Container = std::vector<ConcurrentCallable>>
class ConcurrentCaller1D {
 public:
  template <class... Args>
  void emplace(Args&&... args) {
    data_.emplace_back(std::forward<Args>(args)...);
  }

  std::size_t size() const { return data_.size(); }

  template <class LinearBuffer, class Callback>
  void call(LinearBuffer& buffer, const Callback& callback) requires
      requirements::Callable<ConcurrentCallable,
                             void,
                             decltype(buffer.fetch()),
                             Callback>() {
    for (auto& callable : data_) {
      callable(buffer.fetch(), callback);
    }
  }

 private:
  Container data_;
};

template <class ConcurrentCallable>
auto make_concurrent_caller(std::size_t count,
                            const ConcurrentCallable& callable) {
  ConcurrentCaller1D<ConcurrentCallable> res;
  while (count-- > 0u) {
    res.emplace(callable);
  }
  return res;
}

template <class ExecutionAgentPortal = abstraction::SharedProxy<abstraction::ConcurrentCallablePortal>,
          class ConcurrentCallable = abstraction::SharedProxy<abstraction::ConcurrentCallable>,
          class Container = std::vector<ConcurrentCallable>>
class ConcurrentCaller2D {
 public:
  template <class T>
  explicit ConcurrentCaller2D(
      T&& portal, std::size_t concurrency = std::thread::hardware_concurrency())
      : portal_(std::forward<T>(portal)), concurrency_(concurrency) {}

  template <class... Args>
  void emplace(Args&&... args) {
    data_.emplace_back(std::forward<Args>(args)...);
  }

  std::size_t size() const { return data_.size(); }

  template <class LinearBuffer, class Callback>
  void call(LinearBuffer& buffer, const Callback& callback) requires
      requirements::Callable<ConcurrentCallable,
                             void,
                             decltype(buffer.fetch()),
                             Callback>() {
    std::vector<decltype(buffer.fetch())> modifiers;
    modifiers.reserve(data_.size());
    for (std::size_t i = 0; i < data_.size(); ++i) {
      modifiers.emplace_back(buffer.fetch());
    }
    auto task = [&, &data = data_](std::size_t first, std::size_t last) mutable {
      for (std::size_t i = first; i < last; ++i) {
        data[i](std::move(modifiers[i]), callback);
      }
    };
    ConcurrentCaller1D<decltype(
        make_concurrent_callable(
            copy_construct(portal_),
            make_concurrent_procedure(
                copy_construct(task),
                std::declval<std::size_t>(),
                std::declval<std::size_t>())))> caller;
    std::size_t remainder = data_.size() % concurrency_,
                task_size = data_.size() / concurrency_,
                first = 0u;
    for (std::size_t i = 0u; i < remainder; ++i) {
      caller.emplace(make_concurrent_callable(
          copy_construct(portal_),
          make_concurrent_procedure(
            copy_construct(task), copy_construct(first), first + task_size + 1)));
      first += task_size + 1u;
    }
    for (std::size_t i = remainder; i < concurrency_; ++i) {
      caller.emplace(make_concurrent_callable(
          copy_construct(portal_),
          make_concurrent_procedure(
            copy_construct(task), copy_construct(first), first + task_size)));
      first += task_size;
    }
    sync_concurrent_invoke([] {}, caller);
  }

 private:
  Container data_;
  ExecutionAgentPortal portal_;
  const std::size_t concurrency_;
};

}

#endif // _CON_LIB_CONCURRENT_CALLER
