/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ source file, and is also an example for using
 *  the Concurrent Support Library.
 *
 *  @file     example_3_concurrent_fork.cc
 *  @author   Mingxin Wang
 */

#include <atomic>
#include <iostream>
#include <random>
#include <limits>

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

std::atomic_size_t exit_count, worker_id;
constexpr std::size_t INIT_COUNT = 3u;

bool check() {                                                                  /// Returns weather the current worker is allowed to continue running
  std::size_t cur = exit_count.load(std::memory_order_relaxed);                 /// Equivalent to:
  do {                                                                          ///   [[atomic]] {
    if (cur == 0u) {                                                            ///     if (exit_count > 0) {
      return true;                                                              ///       --exit_count;
    }                                                                           ///       return false;
  } while (!exit_count.compare_exchange_weak(cur,                               ///     }
                                             cur - 1u,                          ///     return true;
                                             std::memory_order_relaxed));       ///   }
  return false;
}

void work() {                                                                   /// The entry for the workers
  std::size_t id = worker_id.fetch_add(1u, std::memory_order_relaxed);          /// Get an id
  std::cout << "Worker " << id << " is started." << std::endl;
  while (check()) {
    std::cout << "Worker " << id << " is working." << std::endl;
    do_something();                                                             /// Do something non-trivial
  }
  std::cout << "Worker " << id << " is stopped." << std::endl;
}

auto make_callable() {                                                          /// Make a concurrent callable unit that starts a worker
  return con::make_concurrent_callable(
      con::ThreadPortal<true>(),                                                /// Specifies the method to submit tasks (with daemon threads)
      con::make_concurrent_procedure(work));                                    /// Wrap a pointer of function into a concurrent procedure
}

class MainProcedure : public con::ConcurrentProcedureTemplate {                 /// The well-known "Template Pattern"
 public:
  void run() override {
    std::string instruction;
    for (;;) {
      std::cin >> instruction;                                                  /// Input an instruction
      if (instruction == "+") {                                                 /// Add a worker at runtime
        fork(con::make_concurrent_caller(make_callable()));                     /// Call the protected template function
      } else if (instruction == "-") {                                          /// Stop one worker (suppose there is at least one)
        exit_count.fetch_add(1u, std::memory_order_relaxed);
      } else if (instruction == "x") {                                          /// Stop all workers
        exit_count.store(std::numeric_limits<std::size_t>::max(),
                         std::memory_order_relaxed);
        break;
      }
    }
  }
};

int main() {
  std::cout << "There are " << INIT_COUNT << " workers initially." << std::endl;
  std::cout << "You may enter '+', '-' or 'x' to control:" << std::endl;
  std::cout << "  enter '+' to start a new worker," << std::endl;
  std::cout << "  enter '-' to remove a worker," << std::endl;
  std::cout << "  enter 'x' to remove all the workers." << std::endl;
  std::cout << std::endl;
  con::sync_concurrent_invoke(                                                  /// The "Sync Concurrent Invoke" model
      [] {},
      con::make_concurrent_caller(INIT_COUNT, make_callable()),                 /// Make a initial concurrent caller
      con::make_concurrent_caller(
          con::make_concurrent_callable(con::SerialPortal(), MainProcedure())));/// Sequentially execute the MainProcedure
  std::cout << "Done." << std::endl;
  return 0;
}
