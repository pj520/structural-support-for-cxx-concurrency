/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ source file, and is also an example for using
 *  the Concurrent Support Library.
 *
 *  @file     example_2_async_concurrent_invoke.cc
 *  @author   Mingxin Wang
 */

#include <iostream>

#include "../solution/concurrent.h"

int main() {
  con::async_concurrent_invoke(                                                 /// The "Async Concurrent Invoke" model
      [] { std::cout << "Done." << std::endl; },                                /// Specifies the callback
      con::make_concurrent_caller(                                              /// Make a temporary concurrent caller
          10u,                                                                  /// Specifies the number of tasks
          con::make_concurrent_callable(                                        /// Make a temporary concurrent callable
              con::ThreadPortal<false>(),                                       /// Specifies the method to submit tasks (with non-daemon threads)
              con::make_concurrent_procedure([] {                               /// Wrap a lambda into a concurrent procedure
                std::cout << "Hello world!" << std::endl;
              }))));
  return 0;
}
