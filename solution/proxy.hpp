/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the proxy library.
 *
 *  @file     wrapper.h
 *  @author   Mingxin Wang
 */

#ifndef PROXY_HPP_INCLUDED
#define PROXY_HPP_INCLUDED

#include "wrapper.hpp"
#include "requirements.hpp"

namespace poly {

template <class T>
concept bool Wrapper() {
  return requires(T t) {
    { t.get() } -> void*;
  };
}

template <class I, class W>
class proxy; // Implementation defined

template <class I> using SharedProxy = proxy<I, SharedWrapper>;
template <class I, std::size_t SOO_SIZE = 16u> using DeepProxy = proxy<I, DeepWrapper<SOO_SIZE>>;
template <class I> using DeferredProxy = proxy<I, DeferredWrapper>;
template <class I, std::size_t SIZE = sizeof(std::ptrdiff_t)> using TrivialProxy = proxy<I, TrivialWrapper<SIZE>>;

template <class T>
class Callable; // undefined

template <class R, class... Args>
class Callable<R(Args...)> {
 public:
  virtual R operator()(Args... args) = 0;
};

template <class T>
class LinearBuffer {
 public:
  virtual T fetch() = 0;
};

template <class R, class... Args, class W>
class proxy<Callable<R(Args...)>, W> {
 public:
  template <class T>
  proxy(T&& data) requires
      !std::is_same<RawType<T>, proxy>::value &&
      con::requirements::Callable<T, R, Args...>()
      { init(std::forward<T>(data)); }

  proxy() { init(); }
  proxy(proxy&& lhs) { lhs.move_init(*this); }
  proxy(const proxy& rhs) { rhs.copy_init(*this); }
  ~proxy() { deinit(); }

  proxy& operator=(const proxy& rhs) {
    deinit();
    rhs.copy_init(*this);
    return *this;
  }

  proxy& operator=(proxy&& lhs) {
    deinit();
    lhs.move_init(*this);
    return *this;
  }

  template <class T>
  proxy& operator=(T&& data) requires
      !std::is_same<RawType<T>, proxy>::value {
    deinit();
    init(std::forward<T>(data));
    return *this;
  }

  template <class... _Args>
  R operator()(_Args&&... args) {
    return (*reinterpret_cast<Abstraction*>(data_.get()))(std::forward<_Args>(args)...);
  }

 private:
  class Abstraction : public Callable<R(Args...)> {
   public:
    Abstraction() = default;

    template <class T>
    Abstraction(T&& data) : wrapper_(std::forward<T>(data)) {}

    void copy_init(void* mem) const {
      memcpy(mem, this, sizeof(Callable<R(Args...)>));
      new (&reinterpret_cast<Abstraction*>(mem)->wrapper_) W(wrapper_);
    }

    void move_init(void* mem) {
      memcpy(mem, this, sizeof(Callable<R(Args...)>));
      new (&reinterpret_cast<Abstraction*>(mem)->wrapper_) W(std::move(wrapper_));
    }

    W wrapper_;
  };

  class Uninitialized : public Abstraction {
   public:
    R operator()(Args...) override {
      throw std::runtime_error("Using uninitialized proxy");
    }
  };

  template <class T>
  class Implementation : public Abstraction {
   public:
    template <class U>
    Implementation(U&& data) : Abstraction(std::forward<U>(data)) {}

    R operator()(Args... args) override {
      return (*reinterpret_cast<T*>(this->wrapper_.get()))(std::forward<Args>(args)...);
    }
  };

  void init() {
    new (reinterpret_cast<Uninitialized*>(data_.get())) Uninitialized();
  }

  template <class T>
  void init(T&& data) {
    new (reinterpret_cast<Implementation<std::remove_reference_t<T>>*>(data_.get()))
        Implementation<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  void copy_init(proxy& rhs) const {
    reinterpret_cast<const Abstraction*>(data_.get())->copy_init(rhs.data_.get());
  }

  void move_init(proxy& rhs) {
    reinterpret_cast<Abstraction*>(data_.get())->move_init(rhs.data_.get());
  }

  void deinit() {
    reinterpret_cast<Abstraction*>(data_.get())->~Abstraction();
  }

  MemoryBlock<sizeof(Uninitialized)> data_;
};

template <class V, class W>
class proxy<LinearBuffer<V>, W> {
 public:
  template <class T>
  proxy(T&& data) requires
      !std::is_same<RawType<T>, proxy>::value &&
      con::requirements::LinearBuffer<T, V>()
      { init(std::forward<T>(data)); }

  proxy() { init(); }
  proxy(proxy&& lhs) { lhs.move_init(*this); }
  proxy(const proxy& rhs) { rhs.copy_init(*this); }
  ~proxy() { deinit(); }

  proxy& operator=(const proxy& rhs) {
    deinit();
    rhs.copy_init(*this);
    return *this;
  }

  proxy& operator=(proxy&& lhs) {
    deinit();
    lhs.move_init(*this);
    return *this;
  }

  template <class T>
  proxy& operator=(T&& data) requires
      !std::is_same<RawType<T>, proxy>::value {
    deinit();
    init(std::forward<T>(data));
    return *this;
  }

  V fetch() {
    return (*reinterpret_cast<Abstraction*>(data_.get())).fetch();
  }

 private:
  class Abstraction : public LinearBuffer<V> {
   public:
    Abstraction() = default;

    template <class T>
    Abstraction(T&& data) : wrapper_(std::forward<T>(data)) {}

    void copy_init(void* mem) const {
      memcpy(mem, this, sizeof(LinearBuffer<V>));
      new (&reinterpret_cast<Abstraction*>(mem)->wrapper_) W(wrapper_);
    }

    void move_init(void* mem) {
      memcpy(mem, this, sizeof(LinearBuffer<V>));
      new (&reinterpret_cast<Abstraction*>(mem)->wrapper_) W(std::move(wrapper_));
    }

    W wrapper_;
  };

  class Uninitialized : public Abstraction {
   public:
    V fetch() override {
      throw std::runtime_error("Using uninitialized proxy");
    }
  };

  template <class T>
  class Implementation : public Abstraction {
   public:
    template <class U>
    Implementation(U&& data) : Abstraction(std::forward<U>(data)) {}

    V fetch() override {
      return (*reinterpret_cast<T*>(this->wrapper_.get())).fetch();
    }
  };

  void init() {
    new (reinterpret_cast<Uninitialized*>(data_.get())) Uninitialized();
  }

  template <class T>
  void init(T&& data) {
    new (reinterpret_cast<Implementation<std::remove_reference_t<T>>*>(data_.get()))
        Implementation<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  void copy_init(proxy& rhs) const {
    reinterpret_cast<const Abstraction*>(data_.get())->copy_init(rhs.data_.get());
  }

  void move_init(proxy& rhs) {
    reinterpret_cast<Abstraction*>(data_.get())->move_init(rhs.data_.get());
  }

  void deinit() {
    reinterpret_cast<Abstraction*>(data_.get())->~Abstraction();
  }

  MemoryBlock<sizeof(Uninitialized)> data_;
};

class AtomicCounterModifier {
 public:
  virtual bool decrement() = 0;
  virtual DeepProxy<LinearBuffer<TrivialProxy<AtomicCounterModifier>>> increase(std::size_t) = 0;
};

template <class W>
class proxy<AtomicCounterModifier, W> {
 public:
  template <class T>
  proxy(T&& data) requires
      !std::is_same<RawType<T>, proxy>::value &&
      con::requirements::AtomicCounterModifier<T>() { init(std::forward<T>(data)); }

  proxy() { init(); }
  proxy(proxy&& lhs) { lhs.move_init(*this); }
  proxy(const proxy& rhs) { rhs.copy_init(*this); }
  ~proxy() { deinit(); }

  proxy& operator=(const proxy& rhs) {
    deinit();
    rhs.copy_init(*this);
    return *this;
  }

  proxy& operator=(proxy&& lhs) {
    deinit();
    lhs.move_init(*this);
    return *this;
  }

  template <class T>
  proxy& operator=(T&& data) requires
      !std::is_same<RawType<T>, proxy>::value {
    deinit();
    init(std::forward<T>(data));
    return *this;
  }

  bool decrement() {
    return (*reinterpret_cast<Abstraction*>(data_.get())).decrement();
  }

  template <class... _Args>
  DeepProxy<LinearBuffer<TrivialProxy<AtomicCounterModifier>>> increase(_Args&&... args) {
    return (*reinterpret_cast<Abstraction*>(data_.get())).increase(std::forward<_Args>(args)...);
  }

 private:
  class Abstraction : public AtomicCounterModifier {
   public:
    Abstraction() = default;

    template <class T>
    Abstraction(T&& data) : wrapper_(std::forward<T>(data)) {}

    void copy_init(void* mem) const {
      memcpy(mem, this, sizeof(AtomicCounterModifier));
      new (&reinterpret_cast<Abstraction*>(mem)->wrapper_) W(wrapper_);
    }

    void move_init(void* mem) {
      memcpy(mem, this, sizeof(AtomicCounterModifier));
      new (&reinterpret_cast<Abstraction*>(mem)->wrapper_) W(std::move(wrapper_));
    }

    W wrapper_;
  };

  class Uninitialized : public Abstraction {
   public:
    bool decrement() override {
      throw std::runtime_error("Using uninitialized proxy");
    }

    DeepProxy<LinearBuffer<TrivialProxy<AtomicCounterModifier>>> increase(std::size_t) override {
      throw std::runtime_error("Using uninitialized proxy");
    }
  };

  template <class T>
  class Implementation : public Abstraction {
   public:
    template <class U>
    Implementation(U&& data) : Abstraction(std::forward<U>(data)) {}

    bool decrement() override {
      return (*reinterpret_cast<T*>(this->wrapper_.get())).decrement();
    }

    DeepProxy<LinearBuffer<TrivialProxy<AtomicCounterModifier>>> increase(std::size_t i) override {
      return (*reinterpret_cast<T*>(this->wrapper_.get())).increase(std::forward<std::size_t>(i));
    }
  };

  void init() {
    new (reinterpret_cast<Uninitialized*>(data_.get())) Uninitialized();
  }

  template <class T>
  void init(T&& data) {
    new (reinterpret_cast<Implementation<std::remove_reference_t<T>>*>(data_.get()))
        Implementation<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  void copy_init(proxy& rhs) const {
    reinterpret_cast<const Abstraction*>(data_.get())->copy_init(rhs.data_.get());
  }

  void move_init(proxy& rhs) {
    reinterpret_cast<Abstraction*>(data_.get())->move_init(rhs.data_.get());
  }

  void deinit() {
    reinterpret_cast<Abstraction*>(data_.get())->~Abstraction();
  }

  MemoryBlock<sizeof(Uninitialized)> data_;
};

}

#endif // PROXY_HPP_INCLUDED
