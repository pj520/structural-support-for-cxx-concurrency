/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_CONCURRENT_CALLABLE
#define _CON_LIB_CONCURRENT_CALLABLE

#include <vector>
#include <functional>
#include <queue>

#include "core.hpp"
#include "abstraction.hpp"

namespace con {

template <class Portal = abstraction::SharedProxy<abstraction::ConcurrentCallablePortal>,
          class ConcurrentProcedure = abstraction::SharedProxy<abstraction::ConcurrentProcedure>>
class SinglePhaseConcurrentCallable {
 private:
  class Callable {
   public:
    explicit Callable(ConcurrentProcedure&& procedure)
        : procedure_(std::forward<ConcurrentProcedure>(procedure)) {}

    template <class AtomicCounterModifier, class Callback>
    void operator()(AtomicCounterModifier&& modifier, Callback&& callback) {
      procedure_(modifier, callback);
      concurrent_join(modifier, callback);
    }

   private:
    ConcurrentProcedure procedure_;
  };

 public:
  template <class T, class U>
  explicit SinglePhaseConcurrentCallable(T&& portal, U&& procedure)
      : portal_(std::forward<T>(portal)),
        callable_(std::forward<U>(procedure)) {}

  template <class AtomicCounterModifier, class Callback>
  void operator()(AtomicCounterModifier&& modifier,
                  const Callback& callback) requires
      requirements::Callable<
          ConcurrentProcedure, void, AtomicCounterModifier, Callback>() {
    portal_(std::move(callable_),
            std::forward<AtomicCounterModifier>(modifier),
            copy_construct(callback));
  }

 private:
  Portal portal_;
  Callable callable_;
};

template <class Portal = abstraction::SharedProxy<abstraction::ConcurrentCallablePortal>,
          class ConcurrentProcedure = abstraction::SharedProxy<abstraction::ConcurrentProcedure>,
          class Container = std::queue<std::pair<Portal, ConcurrentProcedure>>>
class MultiPhaseConcurrentCallable {
 private:
  class Callable {
   public:
    explicit Callable(ConcurrentProcedure&& procedure, Container&& rest)
        : procedure_(std::forward<ConcurrentProcedure>(procedure)),
          rest_(std::forward<Container>(rest)) {}

    template <class AtomicCounterModifier, class Callback>
    void operator()(AtomicCounterModifier&& modifier, Callback&& callback) {
      procedure_(modifier, callback);
      execute(std::forward<AtomicCounterModifier>(modifier),
              std::forward<Callback>(callback),
              rest_);
    }

   private:
    ConcurrentProcedure procedure_;
    Container rest_;
  };

 public:
  template <class T, class U>
  void append_phase(T&& portal, U&& procedure) {
    data_.emplace(std::forward<T>(portal), std::forward<U>(procedure));
  }

  template <class AtomicCounterModifier, class Callback>
  void operator()(AtomicCounterModifier&& modifier,
                  const Callback& callback) requires
      requirements::Callable<
          ConcurrentProcedure, void, AtomicCounterModifier, Callback>() {
    execute(std::forward<AtomicCounterModifier>(modifier),
            copy_construct(callback), data_);
  }

 private:
  template <class AtomicCounterModifier, class Callback>
  static void execute(AtomicCounterModifier&& modifier,
                      Callback&& callback, Container& data) {
    if (data.empty()) {
      concurrent_join(modifier, callback);
    } else {
      auto current = data.front();
      data.pop();
      current.first(Callable(std::move(current.second), std::move(data)),
                    std::forward<AtomicCounterModifier>(modifier),
                    std::forward<Callback>(callback));
    }
  }

  Container data_;
};

template <class Portal, class ConcurrentProcedure>
auto make_concurrent_callable(Portal&& portal,
                              ConcurrentProcedure&& procedure) {
  return SinglePhaseConcurrentCallable<Portal, ConcurrentProcedure>(
      std::forward<Portal>(portal),
      std::forward<ConcurrentProcedure>(procedure));
}

}

#endif // _CON_LIB_CONCURRENT_CALLABLE
