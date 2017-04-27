/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ source file, and is also an example for using
 *  the Concurrent Support Library.
 *
 *  @file     example_5_application_concurrent_copy.cc
 *  @author   Mingxin Wang
 */

#include <algorithm>
#include <iostream>

#include "../solution/concurrent.h"

template <class Portal, class T>
auto make_callable(const Portal& portal, T* dest, T* src, std::size_t n) {      /// Make a concurrent callable unit that does the copy
  return con::make_concurrent_callable(
      con::copy_construct(portal),
      con::make_concurrent_procedure([=]() mutable {                            /// Wrap a lambda into a concurrent procedure
        for (std::size_t i = 0u; i < n; ++i) {
          *dest++ = *src++;
        }
      }));
}

template <class T, class Portal>
void concurrent_copy(T* dest, T* src, std::size_t n, Portal&& portal,
    std::size_t concurrency = std::thread::hardware_concurrency()) {
  con::ConcurrentCaller1D<decltype(make_callable(portal, dest, src, n))> caller;/// Construct a caller that sequentially call the callable units
  std::size_t remainder = n % concurrency, task_size = n / concurrency;         /// Divide the array into "concurrency" blocks
  for (std::size_t i = 0u; i < remainder; ++i) {                                /// Have "n % concurrency" blocks, the size of each block is "n / concurrency + 1"
    caller.emplace(make_callable(portal, dest, src, task_size + 1u));
    src += task_size + 1u;
    dest += task_size + 1u;
  }
  for (std::size_t i = remainder; i < concurrency; ++i) {                       /// Have other "concurrency - n % concurrency" blocks, the size of each block is "n / concurrency"
    caller.emplace(make_callable(portal, dest, src, task_size));
    src += task_size;
    dest += task_size;
  }
  con::sync_concurrent_invoke([] {}, caller);                                   /// The "Sync Concurrent Invoke" model
}

int main() {
  int a[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  int b[10] = {0};
  concurrent_copy(b, a, 10, con::ThreadPortal<true>());                         /// Copy a[] to b[]
  std::for_each(b, b + 10, [](int x) { std::cout << x << std::endl; });         /// Print b[]
  return 0;
}
