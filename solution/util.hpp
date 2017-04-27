/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_UTIL
#define _CON_LIB_UTIL

#include <functional>

namespace con {

template <class T>
T copy_construct(const T& rhs) {
  return T(rhs);
}

template <class F>
class BindArgsMover {
 public:
  explicit BindArgsMover(F&& f) : f_(std::forward<F>(f)) {}

  template <class... Args>
  auto operator()(Args&&... args) {
    f_(std::move(args)...);
  }

 private:
  F f_;
};

template <class F, class... Args>
auto bind_simple(F&& f, Args&&... args) {
  return std::bind(BindArgsMover<F>(std::forward<F>(f)),
                   std::forward<Args>(args)...);
}

}

#endif // _CON_LIB_UTIL
