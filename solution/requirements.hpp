/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_REQUIREMENTS
#define _CON_LIB_REQUIREMENTS

#include <functional>

namespace con {

namespace requirements {

template <class T>
concept bool BinarySemaphore() {
  return requires(T semaphore) {
    { semaphore.wait() };
    { semaphore.release() };
  };
}

template <class T, class U>
concept bool LinearBuffer() {
  return requires(T buffer) {
    { buffer.fetch() } -> U;
  };
}

template <class T>
concept bool AtomicCounterModifier() {
  return requires(T modifier) {
    { modifier.decrement() } -> bool;
  } && (requires(T modifier) {
    { modifier.increase(0u) } -> LinearBuffer<T>;
  } || requires(T modifier) {
    { modifier.increase(0u).fetch() } -> AtomicCounterModifier;
  });
}

template <class T>
concept bool AtomicCounterInitializer() {
  return requires(T initializer) {
    { initializer(0u).fetch() } -> AtomicCounterModifier;
  };
}

template <class F>
concept bool Runnable() {
  return requires(F f) {
    { f() };
  };
}

template <class F, class R, class... Args>
concept bool Callable() {
  return requires(F f, Args&&... args) {
    { f(std::forward<Args>(args)...) } -> R;
  };
}

template <class T, class U, class V>
concept bool ConcurrentProcedure() {
  return requires(T procedure, U&& modifier, V&& callback) {
    { procedure(std::forward<U>(modifier), std::forward<V>(callback)) }
        -> AtomicCounterModifier;
  };
}

template <class T, class U, class V>
concept bool ConcurrentCaller() {
  return requires(const T c_caller, T caller, U& buffer, const V& callback) {
    { c_caller.size() } -> size_t;
    { caller.call(buffer, callback) };
  };
}

template <class T, class U, class V>
constexpr bool concurrent_caller_all(T&, const U&, V&) {
  return ConcurrentCaller<V, T, U>();
}

template <class T, class U, class V, class... W>
constexpr bool concurrent_caller_all(T& buffer, const U& callback,
                                     V& caller, W&... callers) {
  return concurrent_caller_all(buffer, callback, caller) &&
      concurrent_caller_all(buffer, callback, callers...);
}

template <class T, class U, class... V>
concept bool ConcurrentCallerAll() {
  return requires(T& buffer, const U& callback, V&... callers) {
    requires concurrent_caller_all(buffer, callback, callers...);
  };
}

}

}

#endif // _CON_LIB_REQUIREMENTS
