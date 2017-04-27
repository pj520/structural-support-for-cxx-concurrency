/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_CORE
#define _CON_LIB_CORE

#include <cstddef>
#include <functional>

#include "atomic_counter.hpp"
#include "binary_semaphore.hpp"
#include "requirements.hpp"

namespace con {

using DefaultAtomicCounterInitializer = BasicAtomicCounter::Initializer;
using DefaultBinarySemaphore = DisposableBinarySemaphore;

template <class ConcurrentCaller>
inline std::size_t count_call(const ConcurrentCaller& caller) {
  return caller.size();
}

template <class FirstConcurrentCaller,
          class... OtherConcurrentCallers>
inline std::size_t count_call(
    const FirstConcurrentCaller& first_caller,
    const OtherConcurrentCallers&... other_callers) {
  return count_call(first_caller) + count_call(other_callers...);
}

template <class LinearBuffer,
          class SerialCallable,
          class ConcurrentCaller>
inline void concurrent_call(LinearBuffer&& buffer,
                            const SerialCallable& callback,
                            ConcurrentCaller& caller) {
  caller.call(buffer, callback);
}

template <class LinearBuffer,
          class SerialCallable,
          class FirstConcurrentCaller,
          class... OtherConcurrentCallers>
inline void concurrent_call(
    LinearBuffer&& buffer,
    const SerialCallable& callback,
    FirstConcurrentCaller& first_caller,
    OtherConcurrentCallers&... other_callers) {
  concurrent_call(buffer, callback, first_caller);
  concurrent_call(buffer, callback, other_callers...);
}

template <class AtomicCounterInitializer,
          class Callback,
          class... ConcurrentCallers>
void async_concurrent_invoke_explicit(AtomicCounterInitializer&& initializer,
                                      const Callback& callback,
                                      ConcurrentCallers&&... callers) requires
    requirements::AtomicCounterInitializer<AtomicCounterInitializer>() &&
    requirements::Callable<Callback, void>() &&
    requirements::ConcurrentCallerAll<
        decltype(initializer(0u)),
        Callback,
        ConcurrentCallers...>() {
  concurrent_call(initializer(count_call(callers...) - 1u),
                  callback,
                  callers...);
}

template <class BinarySemaphore>
class SyncConcurrentCallback {
 public:
  explicit SyncConcurrentCallback(BinarySemaphore& semaphore)
      : semaphore_(&semaphore) {}

  SyncConcurrentCallback(const SyncConcurrentCallback&) = default;

  SyncConcurrentCallback() = default;

  SyncConcurrentCallback& operator=(const SyncConcurrentCallback&) = default;

  void operator()() const {
    semaphore_->release();
  }

 private:
  BinarySemaphore* semaphore_;
};

template <class BinarySemaphore>
class SyncInvokeHelper {
 public:
  explicit SyncInvokeHelper(BinarySemaphore& semaphore)
      : semaphore_(semaphore) {}

  ~SyncInvokeHelper() { semaphore_.wait(); }

 private:
  BinarySemaphore& semaphore_;
};

template <class AtomicCounterInitializer,
          class BinarySemaphore,
          class Runnable,
          class... ConcurrentCallers>
auto sync_concurrent_invoke_explicit(AtomicCounterInitializer&& initializer,
                                     BinarySemaphore&& semaphore,
                                     Runnable&& runnable,
                                     ConcurrentCallers&&... callers) requires
    requirements::AtomicCounterInitializer<AtomicCounterInitializer>() &&
    requirements::BinarySemaphore<BinarySemaphore>() &&
    requirements::Runnable<Runnable>() &&
    requirements::ConcurrentCallerAll<
        decltype(initializer(0u)),
        SyncConcurrentCallback<std::remove_reference_t<BinarySemaphore>>,
        ConcurrentCallers...>() {
  async_concurrent_invoke_explicit(initializer, SyncConcurrentCallback<
      std::remove_reference_t<BinarySemaphore>>(semaphore),
      callers...);
  SyncInvokeHelper<std::remove_reference_t<BinarySemaphore>> blocker(semaphore);
  return runnable();
}

template <class Runnable, class... ConcurrentCallers>
auto sync_concurrent_invoke(Runnable&& runnable,
                            ConcurrentCallers&&... callers) {
  return sync_concurrent_invoke_explicit(DefaultAtomicCounterInitializer(),
                                         DefaultBinarySemaphore(),
                                         runnable,
                                         callers...);
}

template <class Callback,
          class... ConcurrentCallers>
void async_concurrent_invoke(const Callback& callback,
                             ConcurrentCallers&&... callers) {
  async_concurrent_invoke_explicit(DefaultAtomicCounterInitializer(),
                                   callback,
                                   callers...);
}

template <class AtomicCounterModifier,
          class Callback,
          class... ConcurrentCallers>
auto concurrent_fork(AtomicCounterModifier&& modifier,
                     const Callback& callback,
                     ConcurrentCallers&&... callers) requires
    requirements::AtomicCounterModifier<AtomicCounterModifier>() &&
    requirements::Callable<Callback, void>() &&
    requirements::ConcurrentCallerAll<
        decltype(modifier.increase(0u)),
        Callback,
        ConcurrentCallers...>() {
  auto buffer = modifier.increase(count_call(callers...));
  concurrent_call(buffer, callback, callers...);
  return buffer.fetch();
}

template <class AtomicCounterModifier,
          class Callback>
void concurrent_join(AtomicCounterModifier&& modifier,
                     Callback& callback) requires
    requirements::AtomicCounterModifier<AtomicCounterModifier>() &&
    requirements::Callable<Callback, void>() {
  if (!modifier.decrement()) {
    callback();
  }
}

}

#endif // _CON_LIB_CORE
