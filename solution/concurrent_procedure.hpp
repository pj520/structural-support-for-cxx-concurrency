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
      auto&&, const auto&) mutable { fun(); };
}

auto make_concurrent_procedure() {
  return [](auto&&, const auto&) {};
}

class ConcurrentProcedureTemplate {
 public:
  template <class Modifier, class Callback>
  void operator()(Modifier&& modifier, Callback&& callback) {
    modifier_ = modifier;
    callback_ = std::forward<Callback>(callback);
    this->run();
  }

 protected:
  ConcurrentProcedureTemplate() = default;

  template <class... ConcurrentCallers>
  void fork(ConcurrentCallers&&... callers) {
    concurrent_fork(modifier_, callback_, callers...);
  }

  virtual void run() = 0;

 private:
  abstraction::AtomicCounterModifierReference modifier_;
  abstraction::ConcurrentCallback callback_;
};

}

#endif // _CON_LIB_CONCURRENT_PROCEDURE
