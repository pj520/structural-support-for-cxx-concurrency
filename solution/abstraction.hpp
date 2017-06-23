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
#include "proxy.hpp"

namespace con {

namespace abstraction {

using ConcurrentCallback = poly::SharedProxy<poly::Callable<void()>>;
using AtomicCounterModifier = poly::TrivialProxy<poly::AtomicCounterModifier>;
using AtomicCounterModifierReference = poly::DeferredProxy<poly::AtomicCounterModifier>;
using ConcurrentProcedure = poly::DeepProxy<poly::Callable<void(
    AtomicCounterModifierReference,
    ConcurrentCallback)>>;
using ConcurrentCallable = poly::DeepProxy<poly::Callable<void(
    AtomicCounterModifier,
    ConcurrentCallback)>>;
using ConcurrentCallablePortal = poly::SharedProxy<poly::Callable<void(
    ConcurrentCallable,
    AtomicCounterModifier,
    ConcurrentCallback)>>;
using Runnable = poly::DeepProxy<poly::Callable<void()>>;

}

}

#endif // _WANG_CON_ABSTRACTION
