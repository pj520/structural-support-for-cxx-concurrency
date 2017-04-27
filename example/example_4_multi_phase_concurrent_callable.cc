/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ source file, and is also an example for using
 *  the Concurrent Support Library.
 *
 *  @file     example_4_multi_phase_concurrent_callable.cc
 *  @author   Mingxin Wang
 */

#include <iostream>
#include <random>

#include "../solution/concurrent.h"

void do_something() {                                                           /// This function is meaningless, only for demonstration
  static std::random_device rd;                                                 /// A uniformly-distributed integer random number generator (for the seed)
  static std::uniform_int_distribution<> dis(500, 3000);                        /// Produces random integers
  static std::mutex mtx;
  int t;
  {
    std::lock_guard<std::mutex> lk(mtx);
    t = dis(rd);                                                                /// Generate a random number
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(t));                    /// Sleep for a random period
}

int main() {
  con::ConcurrentCaller1D<con::MultiPhaseConcurrentCallable<>> caller;          /// Construct a caller that sequentially call the callable units
  con::abstraction::ConcurrentCallablePortal thread_pool_portal(                /// Construct a con::ThreadPoolPortal and wrap it into an abstraction
      con::ThreadPoolPortal<>(1u));                                             /// Note that con::ThreadPoolPortal is NOT CopyConstructible
  for (int i = 1; i <= 10; ++i) {
    con::MultiPhaseConcurrentCallable<> callable;                               /// Each task has 2 phases
    callable.append_phase(                                                      /// Append the first phase and allow the tasks run concurrently with independent thread
        con::ThreadPortal<true>(),                                              /// Move construct
        con::make_concurrent_procedure([i] {                                    /// Wrap a lambda into a concurrent procedure
          std::cout << "Task " << i << " phase 1 started." << std::endl;
          do_something();                                                       /// Do something non-trivial
          std::cout << "Task " << i << " phase 1 finished." << std::endl;
        }));
    callable.append_phase(                                                      /// [Append the second phase and allow the tasks run sequentially with a specific threadpool]
        thread_pool_portal,                                                     /// Copy construct
        con::make_concurrent_procedure([i] {                                    /// Wrap a lambda into a concurrent procedure
          std::cout << "Task " << i << " phase 2 started." << std::endl;
          do_something();                                                       /// Do something non-trivial
          std::cout << "Task " << i << " phase 2 finished." << std::endl;
        }));
    caller.emplace(std::move(callable));                                        /// Add the callable to the caller
  }
  con::sync_concurrent_invoke([] {}, caller);                                   /// The "Sync Concurrent Invoke" model
  std::cout << "Done." << std::endl;
  return 0;
}
