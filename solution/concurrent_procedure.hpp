/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_CONCURRENT_PROCEDURE
#define _CON_LIB_CONCURRENT_PROCEDURE

#include <functional>

#include "requirements.hpp"
#include "abstraction.hpp"
#include "util.hpp"

namespace con {

template <class F, class... Args>
auto make_concurrent_procedure(F&& f, Args&&... args) requires
    requirements::Callable<F, void, Args...>() {
  return [fun = bind_simple(std::forward<F>(f), std::forward<Args>(args)...)](
      auto&& modifier, auto&&) mutable {
    fun();
    return std::move(modifier);
  };
}

auto make_concurrent_procedure() {
  return [](auto&& modifier, auto&&) {
    return std::move(modifier);
  };
}

class ConcurrentProcedureTemplate {
 public:
  template <class Modifier, class Callback>
  auto operator()(Modifier&& modifier, Callback&& callback) {
    modifier_ = std::forward<Modifier>(modifier);
    callback_ = std::forward<Callback>(callback);
    this->run();
    return std::move(modifier_);
  }

 protected:
  ConcurrentProcedureTemplate() = default;

  template <class... ConcurrentCallers>
  void fork(ConcurrentCallers&&... callers) {
    modifier_ = concurrent_fork(std::move(modifier_), callback_, callers...);
  }

  virtual void run() = 0;

 private:
  abstraction::AtomicCounterModifier modifier_;
  abstraction::Callable<void()> callback_;
};

}

#endif // _CON_LIB_CONCURRENT_PROCEDURE
