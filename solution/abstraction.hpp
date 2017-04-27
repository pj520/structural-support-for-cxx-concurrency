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

#include "requirements.hpp"

namespace con {

namespace abstraction {

class Object {
 public:
  virtual ~Object() {}
};

template <template <class, class...> class SmartPointer>
class WrapperBase {
 protected:
  WrapperBase() = default;

  WrapperBase(Object* object) : object_(object) {}

  WrapperBase(WrapperBase&&) = default;

  WrapperBase(const WrapperBase&) = default;

  WrapperBase& operator=(WrapperBase&&) = default;

  WrapperBase& operator=(const WrapperBase&) = default;

  template <class T>
  T* get() const { return static_cast<T*>(object_.get()); }

 private:
  SmartPointer<Object> object_;
};

template <bool SHARED>
class Wrapper;

template <>
class Wrapper<true> : public WrapperBase<std::shared_ptr> {
 public:
  Wrapper() = default;

  Wrapper(Object* object) : WrapperBase<std::shared_ptr>(object) {}

  Wrapper(const Wrapper&) = default;

  Wrapper(Wrapper&&) = default;

  Wrapper& operator=(Wrapper&&) = default;

  Wrapper& operator=(const Wrapper&) = default;
};

template <>
class Wrapper<false> : public WrapperBase<std::unique_ptr> {
 public:
  Wrapper() = default;

  Wrapper(Object* object) : WrapperBase<std::unique_ptr>(object) {}

  Wrapper(Wrapper&&) = default;

  Wrapper& operator=(Wrapper&&) = default;
};

template <class T>
class LinearBuffer : public Wrapper<false> {
 public:
  LinearBuffer() = default;

  template <class Data>
  LinearBuffer(Data data) requires requirements::LinearBuffer<Data, T>()
      : Wrapper<false>(new Implementation<Data>(std::move(data))) {}

  LinearBuffer(LinearBuffer&&) = default;

  T fetch() { return get<Abstraction>()->fetch(); }

 private:
  class Abstraction : public Object {
   public:
    virtual T fetch() = 0;
  };

  template <class Data>
  class Implementation : public Abstraction {
   public:
    explicit Implementation(Data&& data) : data_(std::forward<Data>(data)) {}

    T fetch() override { return data_.fetch(); }

   private:
    Data data_;
  };
};

class AtomicCounterModifier : public Wrapper<false> {
 public:
  AtomicCounterModifier() = default;

  template <class Data>
  AtomicCounterModifier(Data data) requires requirements::AtomicCounterModifier<Data>()
      : Wrapper<false>(new Implementation<Data>(std::move(data))) {}

  AtomicCounterModifier(AtomicCounterModifier&&) = default;

  AtomicCounterModifier& operator=(AtomicCounterModifier&&) = default;

  AtomicCounterModifier& operator=(const AtomicCounterModifier&) = default;

  bool decrement() { return get<Abstraction>()->decrement(); }

  LinearBuffer<AtomicCounterModifier> increase(std::size_t increase_count) {
    return get<Abstraction>()->increase(increase_count);
  }

 private:
  class Abstraction : public Object {
   public:
    virtual bool decrement() = 0;

    virtual LinearBuffer<AtomicCounterModifier> increase(
        std::size_t increase_count) = 0;
  };

  template <class Data>
  class Implementation : public Abstraction {
   public:
    explicit Implementation(Data&& data) : data_(std::forward<Data>(data)) {}

    bool decrement() override { return data_.decrement(); }

    LinearBuffer<AtomicCounterModifier> increase(std::size_t increase_count)
        override {
      return data_.increase(increase_count);
    }

   private:
    Data data_;
  };
};

class Runnable : public Wrapper<false> {
 public:
  Runnable() = default;

  template <class Data>
  Runnable(Data data) requires requirements::Runnable<Data>()
      : Wrapper<false>(new Implementation<Data>(std::move(data))) {}

  Runnable(Runnable&&) = default;

  void operator()() { (*get<Abstraction>())(); }

 private:
  class Abstraction : public Object {
   public:
    virtual void operator()() = 0;
  };

  template <class Data>
  class Implementation : public Abstraction {
   public:
    explicit Implementation(Data&& data) : data_(std::forward<Data>(data)) {}

    void operator()() override {
      data_();
    }

   private:
    Data data_;
  };
};

template <class T>
class Callable;

template <class R, class... Args>
class Callable<R(Args...)> : public Wrapper<true> {
 public:
  Callable() = default;

  template <class Data>
  Callable(Data data) requires requirements::Callable<Data, R, Args...>()
      : Wrapper<true>(new Implementation<Data>(std::move(data))) {}

  Callable(Callable&&) = default;

  Callable(const Callable&) = default;

  Callable& operator=(Callable&&) = default;

  Callable& operator=(const Callable&) = default;

  R operator()(Args... args) {
    return (*get<Abstraction>())(std::move(args)...);
  }

 private:
  class Abstraction : public Object {
   public:
    virtual R operator()(Args&&...) = 0;
  };

  template <class Data>
  class Implementation : public Abstraction {
   public:
    explicit Implementation(Data&& data) : data_(std::forward<Data>(data)) {}

    R operator()(Args&&... args) override {
      return data_(std::forward<Args>(args)...);
    }

   private:
    Data data_;
  };
};

typedef Callable<void()> ConcurrentCallback;

typedef Callable<AtomicCounterModifier(
    AtomicCounterModifier, ConcurrentCallback)> ConcurrentProcedure;

typedef Callable<void(
    AtomicCounterModifier, ConcurrentCallback)> ConcurrentCallable;

typedef Callable<void(
    ConcurrentCallable,
    AtomicCounterModifier,
    ConcurrentCallback)> ConcurrentCallablePortal;

}

}

#endif // _WANG_CON_ABSTRACTION
