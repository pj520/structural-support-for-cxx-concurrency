/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the Concurrent Support Library.
 *
 *  @file     concurrent.h
 *  @author   Mingxin Wang
 */

#ifndef _CON_LIB_ABSTRACTION
#define _CON_LIB_ABSTRACTION

#include <functional>
#include <memory>
#include <atomic>

#include "requirements.hpp"

namespace con {

namespace abstraction {

template <std::size_t SIZE>
class MemoryBlock final {
 public:
  MemoryBlock() = default;
  MemoryBlock(MemoryBlock&&) = default;
  MemoryBlock(const MemoryBlock&) = default;

  MemoryBlock& operator=(MemoryBlock&&) = default;
  MemoryBlock& operator=(const MemoryBlock&) = default;

  void* get() { return data_; }
  const void* get() const { return data_; }

 private:
  char data_[SIZE];
};

class DefferedWrapper {
 public:
  template <class T>
  DefferedWrapper(T&& data) requires
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, DefferedWrapper>::value
      { data_ = (void*)&data; }
  DefferedWrapper() = default;
  DefferedWrapper(const DefferedWrapper&) = default;
  DefferedWrapper(DefferedWrapper&&) = default;

  DefferedWrapper& operator=(const DefferedWrapper&) = default;
  DefferedWrapper& operator=(DefferedWrapper&&) = default;

  void* get() { return data_; }

 private:
  void* data_;
};

template <std::size_t SOO_SIZE>
class DeepWrapper {
 public:
  template <class T>
  DeepWrapper(T&& data) requires
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, DeepWrapper>::value
      { init(std::forward<T>(data)); }
  DeepWrapper() { init(); }
  DeepWrapper(const DeepWrapper& rhs) { rhs.copy_init(*this); }
  DeepWrapper(DeepWrapper&& lhs) { lhs.move_init(*this); }

  ~DeepWrapper() { deinit(); }

  DeepWrapper& operator=(const DeepWrapper& rhs) {
    deinit();
    rhs.copy_init(*this);
    return *this;
  }

  DeepWrapper& operator=(DeepWrapper&& lhs) {
    deinit();
    lhs.move_init(*this);
    return *this;
  }

  void* get() {
    return holder_ == nullptr ? nullptr : reinterpret_cast<char*>(holder_) + sizeof(AbstractHolder);
  }

 private:
  class AbstractHolder {
   public:
    virtual ~AbstractHolder() {}
    virtual void copy_init(DeepWrapper&) = 0;
    virtual void move_init(DeepWrapper&) = 0;
  };

  template <class T>
  class ConcreteHolder : public AbstractHolder {
   public:
    template <class U>
    ConcreteHolder(U&& data) : data_(std::forward<U>(data)) {}

    void copy_init(DeepWrapper& rhs) override { rhs.init(data_); }
    void move_init(DeepWrapper& rhs) override { rhs.init(std::move(data_)); }

   private:
    T data_;
  };

  void init() { holder_ = nullptr; }

  template <class T>
  void init(T&& data) requires (sizeof(T) <= SOO_SIZE) {
    holder_ = reinterpret_cast<AbstractHolder*>(soo_block_.get());
    new (reinterpret_cast<
        ConcreteHolder<std::remove_reference_t<T>>*>(soo_block_.get()))
        ConcreteHolder<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  template <class T>
  void init(T&& data) requires (sizeof(T) > SOO_SIZE) {
    holder_ = new ConcreteHolder<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  void copy_init(DeepWrapper& rhs) const {
    if (holder_ == nullptr) {
      rhs.init();
    } else {
      holder_->copy_init(rhs);
    }
  }

  void move_init(DeepWrapper& rhs) {
    if (holder_ == nullptr) {
      rhs.init();
    } else if (holder_ == soo_block_.get()) {
      holder_->move_init(rhs);
    } else {
      rhs.holder_ = holder_;
      holder_ = nullptr;
    }
  }

  void deinit() {
    if (holder_ == soo_block_.get()) {
      holder_->~AbstractHolder();
    } else {
      delete holder_;
    }
  }

  AbstractHolder* holder_;

  MemoryBlock<sizeof(ConcreteHolder<MemoryBlock<SOO_SIZE>>)> soo_block_;
};

class SharedWrapper {
 public:
  template <class T>
  SharedWrapper(T&& data) requires
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, SharedWrapper>::value
      { init(std::forward<T>(data)); }
  SharedWrapper() { init(); }
  SharedWrapper(const SharedWrapper& rhs) { rhs.copy_init(*this); }
  SharedWrapper(SharedWrapper&& lhs) { lhs.move_init(*this); }

  ~SharedWrapper() { deinit(); }

  SharedWrapper& operator=(const SharedWrapper& rhs) {
    deinit();
    rhs.copy_init(*this);
    return *this;
  }

  SharedWrapper& operator=(SharedWrapper&& lhs) {
    deinit();
    lhs.move_init(*this);
    return *this;
  }

  void* get() {
    return holder_ == nullptr ? nullptr : reinterpret_cast<char*>(holder_) + sizeof(AbstractHolder);
  }

 private:
  class AbstractHolder {
   public:
    AbstractHolder() : count_(0u) {}

    virtual ~AbstractHolder() {}

    mutable std::atomic_size_t count_;
  };

  template <class T>
  class ConcreteHolder : public AbstractHolder {
   public:
    template <class U>
    ConcreteHolder(U&& data) : data_(std::forward<U>(data)) {}

   private:
    T data_;
  };

  void init() { holder_ = nullptr; }

  template <class T>
  void init(T&& data) {
    holder_ = new ConcreteHolder<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  void copy_init(SharedWrapper& rhs) const {
    rhs.holder_ = holder_;
    holder_->count_.fetch_add(1u, std::memory_order_relaxed);
  }

  void move_init(SharedWrapper& rhs) {
    rhs.holder_ = holder_;
    holder_ = nullptr;
  }

  void deinit() {
    if (holder_ != nullptr &&
        holder_->count_.fetch_sub(1u, std::memory_order_release) == 0u) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete holder_;
    }
  }

  AbstractHolder* holder_;
};

template <class I, class W>
class proxy; // Implementation defined

template <class I> using SharedProxy = proxy<I, SharedWrapper>;
template <class I, std::size_t SOO_SIZE = 16u> using DeepProxy = proxy<I, DeepWrapper<SOO_SIZE>>;
template <class I> using DefferedProxy = proxy<I, DefferedWrapper>;

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
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, proxy>::value &&
      requirements::Callable<T, R, Args...>()
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
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, proxy>::value {
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
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, proxy>::value &&
      requirements::LinearBuffer<T, V>()
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
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, proxy>::value {
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
  virtual SharedProxy<LinearBuffer<SharedProxy<AtomicCounterModifier>>> increase(std::size_t) = 0;
};

template <class W>
class proxy<AtomicCounterModifier, W> {
 public:
  template <class T>
  proxy(T&& data) requires
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, proxy>::value &&
      requirements::AtomicCounterModifier<T>() { init(std::forward<T>(data)); }

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
      !std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, proxy>::value {
    deinit();
    init(std::forward<T>(data));
    return *this;
  }

  bool decrement() {
    return (*reinterpret_cast<Abstraction*>(data_.get())).decrement();
  }

  template <class... _Args>
  DeepProxy<LinearBuffer<DeepProxy<AtomicCounterModifier>>> increase(_Args&&... args) {
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

    SharedProxy<LinearBuffer<SharedProxy<AtomicCounterModifier>>> increase(std::size_t) override {
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

    SharedProxy<LinearBuffer<SharedProxy<AtomicCounterModifier>>> increase(std::size_t i) override {
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

using ConcurrentCallback = Callable<void()>;

using ConcurrentProcedure = Callable<void(SharedProxy<AtomicCounterModifier>, SharedProxy<ConcurrentCallback>)>;

using ConcurrentCallable = Callable<void(SharedProxy<AtomicCounterModifier>, SharedProxy<ConcurrentCallback>)>;

using ConcurrentCallablePortal = Callable<void(SharedProxy<ConcurrentCallable>, SharedProxy<AtomicCounterModifier>, SharedProxy<ConcurrentCallback>)>;

}

}

#endif // _WANG_CON_ABSTRACTION
