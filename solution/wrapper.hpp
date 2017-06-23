/** Copyright (C) 2015-2017 Mingxin Wang - All Rights Reserved.
 *  This is a C++ header file, which is part of the implementation
 *  for the proxy library.
 *
 *  @file     wrapper.h
 *  @author   Mingxin Wang
 */

#ifndef WRAPPER_HPP_INCLUDED
#define WRAPPER_HPP_INCLUDED

#include <utility>
#include <type_traits>
#include <atomic>

namespace poly {

template <class T>
using RawType = std::remove_cv_t<std::remove_reference_t<T>>;

/* Holds a contiguous memory segment, sizeof(MemoryBlock<SIZE>) == SIZE */
template <std::size_t SIZE>
class MemoryBlock final {
 public:
  MemoryBlock() = default;
  MemoryBlock(MemoryBlock&&) = default;
  MemoryBlock(const MemoryBlock&) = default;

  MemoryBlock& operator=(MemoryBlock&&) = default;
  MemoryBlock& operator=(const MemoryBlock&) = default;

  /* Access to the memory segment */
  void* get() { return data_; }
  const void* get() const { return data_; }

 private:
  char data_[SIZE];
};

/* A deep-copy wrapper with SOO feature */
template <std::size_t SOO_SIZE>
class DeepWrapper {
 public:
  /* Constructors */
  template <class T>
  DeepWrapper(T&& data) requires
      !std::is_same<RawType<T>, DeepWrapper>::value
      { init(std::forward<T>(data)); }
  DeepWrapper() { init(); }
  DeepWrapper(const DeepWrapper& rhs) { rhs.copy_init(*this); }
  DeepWrapper(DeepWrapper&& lhs) { lhs.move_init(*this); }

  /* Destructor */
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

  /* Meets the Wrapper requirements */
  void* get() {
    // The address of the wrapped object is calculated with a constant offset
    return reinterpret_cast<char*>(holder_) + sizeof(AbstractHolder);
  }

 private:
  /* Pure virtual holder with virtual destructor */
  class AbstractHolder {
   public:
    virtual ~AbstractHolder() {}
    virtual void copy_init(DeepWrapper&) = 0;
    virtual void move_init(DeepWrapper&) = 0;
  };

  template <class T>
  class ConcreteHolder : public AbstractHolder {
   public:
    /* Constructor */
    template <class U>
    ConcreteHolder(U&& data) : data_(std::forward<U>(data)) {}

    void copy_init(DeepWrapper& rhs) override { rhs.init(data_); }
    void move_init(DeepWrapper& rhs) override { rhs.init(std::move(data_)); }

   private:
    /* Holds the concrete value */
    T data_;
  };

  /* Initialize *this to be invalid */
  void init() { holder_ = nullptr; }

  /* Overload for small object */
  template <class T>
  void init(T&& data) requires (sizeof(T) <= SOO_SIZE) {
    // Let holder_ point to the reserved SOO block, and SOO optimization is activated
    holder_ = reinterpret_cast<AbstractHolder*>(soo_block_.get());

    // Call the constructor of the ConcreteHolder without memory allocation
    new (reinterpret_cast<
        ConcreteHolder<std::remove_reference_t<T>>*>(soo_block_.get()))
        ConcreteHolder<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  /* Overload for large object */
  template <class T>
  void init(T&& data) requires (sizeof(T) > SOO_SIZE) {
    // Let holder_ point to a "new" object, and SOO optimization is inactivated
    holder_ = new ConcreteHolder<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  /* Copy semantics */
  void copy_init(DeepWrapper& rhs) const {
    // There are two situations:
    //   1. *this in invalid,
    //      rhs shall also be invalid.
    //   2. *this is valid, no matter whether SOO optimization is activated,
    //      rhs shall be initialized with *this.
    if (holder_ == nullptr) {
      rhs.init();
    } else {
      holder_->copy_init(rhs);
    }
  }

  /* Move semantics */
  void move_init(DeepWrapper& rhs) {
    // There are three situations:
    //   1. *this in invalid,
    //      rhs shall also be invalid.
    //   2. *this is valid and SSO optimization is activated,
    //      rhs shall be initialized with *this.
    //   3. *this in valid and SSO optimization is inactivated,
    //      The pointer of the holder can be simply moved from *this to rhs.
    if (holder_ == nullptr) {
      rhs.init();
    } else if (holder_ == soo_block_.get()) {
      holder_->move_init(rhs);
    } else {
      rhs.holder_ = holder_;
      holder_ = nullptr;
    }
  }

  /* Destroy semantics */
  void deinit() {
    // There are two situations:
    //   1. SOO optimization is activated,
    //      The destructor of the holder shall be called without release the memory.
    //   2. SOO optimization is inactivated,
    //      The pointer shall be deleted.
    //      If holder_ is a null pointer, this operation haves no side-effect.
    if (holder_ == soo_block_.get()) {
      holder_->~AbstractHolder();
    } else {
      delete holder_;
    }
  }

  /* Associates with the lifetime management strategy */
  AbstractHolder* holder_;

  /* A reserved block for SOO optimization */
  MemoryBlock<sizeof(ConcreteHolder<MemoryBlock<SOO_SIZE>>)> soo_block_;
};

/* A wrapper that has nothing to do with the lifetime management issue */
class DeferredWrapper {
 public:
  /* Constructors */
  template <class T>
  DeferredWrapper(T&& data) requires
      !std::is_same<RawType<T>, DeferredWrapper>::value
      { data_ = (void*)&data; }
  DeferredWrapper() = default;
  DeferredWrapper(const DeferredWrapper&) = default;
  DeferredWrapper(DeferredWrapper&&) = default;

  DeferredWrapper& operator=(const DeferredWrapper&) = default;
  DeferredWrapper& operator=(DeferredWrapper&&) = default;

  /* Meets the Wrapper requirements */
  void* get() { return data_; }

 private:
  void* data_;
};

/* A shared wrapper with "reference-counting" strategy for lifetime management */
class SharedWrapper {
 public:
  /* Constructors */
  template <class T>
  SharedWrapper(T&& data) requires
      !std::is_same<RawType<T>, SharedWrapper>::value
      { init(std::forward<T>(data)); }
  SharedWrapper() { init(); }
  SharedWrapper(const SharedWrapper& rhs) { rhs.copy_init(*this); }
  SharedWrapper(SharedWrapper&& lhs) { lhs.move_init(*this); }

  /* Destructor */
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

  /* Meets the Wrapper requirements */
  void* get() {
    // The address of the wrapped object is calculated with a constant offset
    return reinterpret_cast<char*>(holder_) + sizeof(AbstractHolder);
  }

 private:
  class AbstractHolder {
   public:
    AbstractHolder() : count_(0u) {}

    virtual ~AbstractHolder() {}

    /* Different from usual implementations for "std::shared_ptr", reference count equals to "count_ - 1" here */
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

  /* Initialize with a concrete type and value */
  template <class T>
  void init(T&& data) {
    // There is no requirement for extra synchronizations, so the memory order is relaxed
    holder_ = new ConcreteHolder<std::remove_reference_t<T>>(std::forward<T>(data));
  }

  /* Copy semantics */
  void copy_init(SharedWrapper& rhs) const {
    rhs.holder_ = holder_;
    holder_->count_.fetch_add(1u, std::memory_order_relaxed);
  }

  /* Move semantics */
  void move_init(SharedWrapper& rhs) {
    rhs.holder_ = holder_;
    holder_ = nullptr;
  }

  /* Destroy semantics */
  void deinit() {
    // A release-acquire synchronization
    // Every operation to the wrapped object happens before "delete holder_"
    // This operation is similar with the "Atomic Counter" defined in P0642R0
    if (holder_ != nullptr &&
        holder_->count_.fetch_sub(1u, std::memory_order_release) == 0u) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete holder_;
    }
  }

  AbstractHolder* holder_;
};

template <std::size_t SIZE>
class TrivialWrapper {
 public:
  /* Constructors */
  template <class T>
  TrivialWrapper(T&& data) requires
      !std::is_same<RawType<T>, TrivialWrapper>::value &&
      std::is_trivial<RawType<T>>::value &&
      (sizeof(RawType<T>) <= SIZE) {
    memcpy(data_.get(), &data, sizeof(RawType<T>));
  }
  TrivialWrapper() = default;
  TrivialWrapper(const TrivialWrapper&) = default;
  TrivialWrapper(TrivialWrapper&&) = default;

  TrivialWrapper& operator=(const TrivialWrapper& rhs) = default;
  TrivialWrapper& operator=(TrivialWrapper&&) = default;

  /* Meets the Wrapper requirements */
  void* get() {
    return data_.get();
  }

 private:
  MemoryBlock<SIZE> data_;
};

}

#endif // WRAPPER_HPP_INCLUDED
