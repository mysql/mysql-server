// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_FAST_PERFECT_HASH_HPP
#define BOOST_OPENMETHOD_POLICY_FAST_PERFECT_HASH_HPP

#include <boost/openmethod/preamble.hpp>

#include <limits>
#include <random>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code
#endif

namespace boost::openmethod {

namespace detail {

#if defined(UINTPTR_MAX)
using uintptr = std::uintptr_t;
constexpr uintptr uintptr_max = UINTPTR_MAX;
#else
static_assert(
    sizeof(std::size_t) == sizeof(void*),
    "This implementation requires that size_t and void* have the same size.");
using uintptr = std::size_t;
constexpr uintptr uintptr_max = (std::numeric_limits<std::size_t>::max)();
#endif

template<class Registry>
std::vector<type_id> fast_perfect_hash_control;

} // namespace detail

namespace policies {

//! Hash type ids using a fast, perfect hash function.
//!
//! `fast_perfect_hash` implements the @ref type_hash policy using a hash
//! function in the form `H(x)=(M*x)>>S`. It attempts to determine values for
//! `M` and `S` that do not result in collisions for the set of registered
//! type_ids. This may fail for certain sets of inputs, although it is very
//! likely to succeed for addresses of `std::type_info` objects.
//!
//! There is no guarantee that every value in the codomain of the function
//! corresponds to a value in the domain, or even that the codomain is a dense
//! range of integers. In other words, a lot of space may be wasted in presence
//! of large sets of type_ids.
struct fast_perfect_hash : type_hash {

    //! Cannot find hash factors
    struct search_error : openmethod_error {
        //! Number of attempts to find hash factors
        std::size_t attempts;
        //! Number of buckets used in the last attempt
        std::size_t buckets;

        //! Write a short description to an output stream
        //! @param os The output stream
        //! @tparam Registry The registry
        //! @tparam Stream A @ref LightweightOutputStream
        template<class Registry, class Stream>
        auto write(Stream& os) const -> void;
    };

    using errors = std::variant<search_error>;

    //! A TypeHashFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy
    template<class Registry>
    class fn {
        static std::size_t mult;
        static std::size_t shift;
        static std::size_t min_value;
        static std::size_t max_value;

        static void check(std::size_t index, type_id type);

        template<class InitializeContext, class... Options>
        static void initialize(
            const InitializeContext& ctx, std::vector<type_id>& buckets,
            const std::tuple<Options...>& options);

      public:
        //! Find the hash factors
        //!
        //! Attempts to find suitable values for the multiplication factor `M`
        //! and the shift amount `S` to that do not result in collisions for the
        //! specified input values.
        //!
        //! If no suitable values are found, calls the error handler with
        //! a @ref hash_error object then calls `abort`.
        //!
        //! @tparam Context An @ref InitializeContext.
        //! @param ctx A Context object.
        //! @return A pair containing the minimum and maximum hash values.
        template<class Context, class... Options>
        static auto
        initialize(const Context& ctx, const std::tuple<Options...>& options) {
            if constexpr (Registry::has_runtime_checks) {
                initialize(
                    ctx, detail::fast_perfect_hash_control<Registry>, options);
            } else {
                std::vector<type_id> buckets;
                initialize(ctx, buckets, options);
            }

            return std::pair{min_value, max_value};
        }

        //! Hash a type id
        //!
        //! Hash a type id.
        //!
        //! If `Registry` contains the @ref runtime_checks policy, checks that
        //! the type id is valid, i.e. if it was present in the set passed to
        //! @ref initialize. Its absence indicates that a class involved in a
        //! method definition, method overrider, or method call was not
        //! registered. In this case, signal a @ref missing_class using
        //! the registry's @ref error_handler if present; then calls `abort`.
        //!
        //! @param type The type_id to hash
        //! @return The hash value
        BOOST_FORCEINLINE
        static auto hash(type_id type) -> std::size_t {
            auto index =
                (mult * reinterpret_cast<detail::uintptr>(type)) >> shift;

            if constexpr (Registry::has_runtime_checks) {
                check(index, type);
            }

            return index;
        }

        //! Releases the memory allocated by `initialize`.
        //!
        //! @tparam Options... Zero or more option types, deduced from the function
        //! arguments.
        //! @param options Zero or more option objects.
        template<class... Options>
        static auto finalize(const std::tuple<Options...>&) -> void {
            detail::fast_perfect_hash_control<Registry>.clear();
        }
    };
};

template<class Registry>
std::size_t fast_perfect_hash::fn<Registry>::mult;

template<class Registry>
std::size_t fast_perfect_hash::fn<Registry>::shift;

template<class Registry>
std::size_t fast_perfect_hash::fn<Registry>::min_value;

template<class Registry>
std::size_t fast_perfect_hash::fn<Registry>::max_value;

template<class Registry>
template<class InitializeContext, class... Options>
void fast_perfect_hash::fn<Registry>::initialize(
    const InitializeContext& ctx, std::vector<type_id>& buckets,
    const std::tuple<Options...>& options) {
    (void)options;

    const auto N = std::distance(ctx.classes_begin(), ctx.classes_end());

    if constexpr (mp11::mp_contains<mp11::mp_list<Options...>, trace>::value) {
        Registry::output::os << "Finding hash factor for " << N << " types\n";
    }

    std::default_random_engine rnd(13081963);
    std::size_t total_attempts = 0;
    std::size_t M = 1;

    for (auto size = N * 5 / 4; size >>= 1;) {
        ++M;
    }

    std::uniform_int_distribution<std::size_t> uniform_dist;

    for (std::size_t pass = 0; pass < 4; ++pass, ++M) {
        shift = 8 * sizeof(type_id) - M;
        auto hash_size = 1 << M;
        min_value = (std::numeric_limits<std::size_t>::max)();
        max_value = (std::numeric_limits<std::size_t>::min)();

        if constexpr (InitializeContext::template has_option<trace>) {
            ctx.tr << "  trying with M = " << M << ", " << hash_size
                   << " buckets\n";
        }

        std::size_t attempts = 0;
        buckets.resize(hash_size);

        while (attempts < 100000) {
            std::fill(
                buckets.begin(), buckets.end(), type_id(detail::uintptr_max));
            ++attempts;
            ++total_attempts;
            mult = uniform_dist(rnd) | 1;

            for (auto iter = ctx.classes_begin(); iter != ctx.classes_end();
                 ++iter) {
                for (auto type_iter = iter->type_id_begin();
                     type_iter != iter->type_id_end(); ++type_iter) {
                    auto type = *type_iter;
                    auto index = (detail::uintptr(type) * mult) >> shift;
                    min_value = (std::min)(min_value, index);
                    max_value = (std::max)(max_value, index);

                    if (detail::uintptr(buckets[index]) !=
                        detail::uintptr_max) {
                        goto collision;
                    }

                    buckets[index] = type;
                }
            }

            if constexpr (InitializeContext::template has_option<trace>) {
                ctx.tr << "  found " << mult << " after " << total_attempts
                       << " attempts; span = [" << min_value << ", "
                       << max_value << "]\n";
            }

            return;

        collision: {}
        }
    }

    search_error error;
    error.attempts = total_attempts;
    error.buckets = std::size_t(1) << M;

    if constexpr (Registry::has_error_handler) {
        Registry::error_handler::error(error);
    }

    abort();
}

template<class Registry>
void fast_perfect_hash::fn<Registry>::check(std::size_t index, type_id type) {
    if (index < min_value || index > max_value ||
        detail::fast_perfect_hash_control<Registry>[index] != type) {

        if constexpr (Registry::has_error_handler) {
            missing_class error;
            error.type = type;
            Registry::error_handler::error(error);
        }

        abort();
    }
}

template<class Registry, class Stream>
auto fast_perfect_hash::search_error::write(Stream& os) const -> void {
    os << "could not find hash factors after " << attempts << "s using "
       << buckets << " buckets\n";
}

} // namespace policies
} // namespace boost::openmethod

#endif
