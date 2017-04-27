/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ source file, and is also an example for using
 *  the Concurrent Support Library.
 *
 *  @file     example_1_sync_concurrent_invoke.cc
 *  @author   Mingxin Wang
 */

#include <iostream>

#include "../solution/concurrent.h"

void solve(std::size_t n) {
  con::sync_concurrent_invoke(
      [] {},
      con::make_concurrent_caller(
          n,
          con::make_concurrent_callable(
              con::ThreadPortal<true>(),
              con::make_concurrent_procedure(do_something))));
}

void solve(std::size_t n) {
  con::sync_concurrent_invoke_explicit(
      con::TreeAtomicCounter<10u>::Initializer(),
      con::DisposableBinarySemaphore(),
      [] {},
      con::make_concurrent_caller(
          n,
          con::make_concurrent_callable(
              con::ThreadPortal<true>(),
              con::make_concurrent_procedure(do_something))));
}

int main() {
  con::sync_concurrent_invoke(                                                  /// The "Sync Concurrent Invoke" model
      [] {},                                                                    /// Main thread does nothing but wait for the completion of other tasks.
      con::make_concurrent_caller(                                              /// Make a temporary concurrent caller
          10u,                                                                  /// Specifies the number of tasks
          con::make_concurrent_callable(                                        /// Make a temporary concurrent callable
              con::ThreadPortal<true>(),                                        /// Specifies the method to submit tasks (with daemon threads)
              con::make_concurrent_procedure([] {                               /// Wrap a lambda into a concurrent procedure
                std::cout << "Hello world!" << std::endl;
              }))));
  std::cout << "Done." << std::endl;
  return 0;
}
