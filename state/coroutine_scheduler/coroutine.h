// Author: Ming Zhang
// Copyright (c) 2022

#pragma once

// Use symmetric_coroutine from boost::coroutine, not asymmetric_coroutine from boost::coroutine2
// symmetric_coroutine meets transaction processing, in which each coroutine can freely yield to another
#define BOOST_COROUTINES_NO_DEPRECATION_WARNING

#include <boost/coroutine/all.hpp>

#include "util/common.h"

using coro_call_t = boost::coroutines::symmetric_coroutine<void>::call_type;

using coro_yield_t = boost::coroutines::symmetric_coroutine<void>::yield_type;

// For coroutine scheduling
struct Coroutine {
  Coroutine() : is_wait_poll(false) {}

  // Wether I am waiting for polling network replies. If true, I leave the yield-able coroutine list
  bool is_wait_poll;

  // My coroutine ID
  coro_id_t coro_id;

  // Registered coroutine function
  coro_call_t func;

  // Use pointer to accelerate yield. Otherwise, one needs a while loop
  // to yield the next coroutine that does not wait for network replies
  Coroutine* prev_coro;

  Coroutine* next_coro;
};