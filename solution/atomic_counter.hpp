/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_ATOMIC_COUNTER
#define _CON_LIB_ATOMIC_COUNTER

#include <functional>
#include <atomic>
#include <stack>

namespace con {

template <class T>
class SingleElementBuffer {
 public:
  template <class... Args>
  explicit SingleElementBuffer(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  T fetch() const { return value_; }

 private:
  const T value_;
};

template <class T, template <class, class...> class Container = std::stack>
class StackedLinearBuffer {
 public:
  void push(std::size_t num, T&& cur) {
    container_.emplace(num, std::forward<T>(cur));
  }

  T fetch() {
    T res = container_.top().second;
    if (--container_.top().first == 0u) {
      container_.pop();
    }
    return res;
  }

 private:
  Container<std::pair<std::size_t, T>> container_;
};

class BasicAtomicCounter {
 public:
  BasicAtomicCounter() = delete;

  class Modifier {
   public:
    explicit Modifier(std::atomic_size_t* count) : count_(count) {}

    Modifier() = default;

    Modifier(const Modifier&) = default;

    Modifier& operator=(const Modifier&) = default;

    SingleElementBuffer<Modifier> increase(std::size_t increase_count) const {
      count_->fetch_add(increase_count, std::memory_order_relaxed);
      return SingleElementBuffer<Modifier>(count_);
    }

    bool decrement() {
      if (count_->fetch_sub(1u, std::memory_order_release) == 0u) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete count_;
        return false;
      }
      return true;
    }

   private:
    std::atomic_size_t* count_;
  };

  class Initializer {
   public:
    SingleElementBuffer<Modifier> operator()(std::size_t init_count) const {
      return SingleElementBuffer<Modifier>(new std::atomic_size_t(init_count));
    }
  };
};

template <std::size_t MAX_COUNT>
class TreeAtomicCounter {
 private:
  struct Node {
    explicit Node(Node* parent, std::size_t init_count)
        : parent_(parent), count_(init_count) {}

    Node* const parent_;
    std::atomic_size_t count_;
  };

 public:
  TreeAtomicCounter() = delete;

  class Modifier {
   public:
    explicit Modifier(Node* node) : node_(node) {}

    Modifier() = default;

    Modifier(const Modifier&) = default;

    Modifier& operator=(const Modifier&) = default;

    bool decrement() {
      while (node_->count_.fetch_sub(1u, std::memory_order_release) == 0u) {
        std::atomic_thread_fence(std::memory_order_acquire);
        Node* parent = node_->parent_;
        delete node_;
        if (parent == nullptr) {
          return false;
        }
        node_ = parent;
      }
      return true;
    }

    StackedLinearBuffer<Modifier> increase(std::size_t increase_count) const {
      StackedLinearBuffer<Modifier> buffer;
      std::size_t increased,
                  current = node_->count_.load(std::memory_order_relaxed);
      do {
        if (current == MAX_COUNT) {
          init_node(node_, increase_count, buffer);
          *this = buffer.fetch();
          return buffer;
        }
        increased = std::min(increase_count, MAX_COUNT - current);
      } while (!node_->count_.compare_exchange_weak(
          current, current + increased, std::memory_order_relaxed));
      if (increased != increase_count) {
        if (increased != 1u) {
          buffer.push(increased - 1u, Modifier(node_));
        }
        init_node(node_, increase_count - increased, buffer);
      } else {
        buffer.push(increased, Modifier(node_));
      }
      return buffer;
    }

   private:
    Node* node_;
  };

  class Initializer {
   public:
    StackedLinearBuffer<Modifier> operator()(std::size_t init_count) const {
      StackedLinearBuffer<Modifier> buffer;
      init_node(nullptr, init_count, buffer);
      return buffer;
    }
  };

 private:
  static void init_node(Node* parent,
                       std::size_t init_count,
                       StackedLinearBuffer<Modifier>& buffer) {
    while (MAX_COUNT < init_count) {
      parent = new Node(parent, MAX_COUNT);
      buffer.push(MAX_COUNT, Modifier(parent));
      init_count -= MAX_COUNT;
    }
    buffer.push(init_count + 1u, Modifier(new Node(parent, init_count)));
  }
};

}

#endif // _CON_LIB_ATOMIC_COUNTER
