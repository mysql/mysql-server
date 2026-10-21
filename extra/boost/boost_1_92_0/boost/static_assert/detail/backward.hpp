// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// 
// This header is unused and is only present for backward compatibility.
// Without it, StaticAssert would be the only library without an include/
// directory, and this breaks third-party installation scripts and logic.

// Introduce an artificial dependency on Config, such that libraries that
// link to StaticAssert depend on the new location of boost/static_assert.hpp

#include <boost/config.hpp>
