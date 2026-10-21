// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_VECTORED_ERROR_HPP
#define BOOST_OPENMETHOD_POLICY_VECTORED_ERROR_HPP

#include <boost/openmethod/preamble.hpp>

#include <functional>
#include <variant>

namespace boost::openmethod::policies {

//! Calls a std::function with the error.
//!
//! Wraps the error in a @ref std::variant, and calls a `std::function` with it.
//! The function object is initialized to a function (@ref default_handler) that
//! writes a description of the error, using the @ref output policy, if it is
//! available in the registry.
//!
//! This is the error handler used by the default registry. In debug variants,
//! it writes an error message to `stderr`, then returns. In release variants,
//! no message is emitted. Any call by the library to the error policy is
//! immediately followed by a call to @ref abort.
//!
//! By default, the library is exception-agnostic: it is exception-safe, but it
//! does not throw exceptions by itself. The program may replace the default
//! handler with a function that throws an exception, possibly preventing
//! program termination. The @ref throw_error_handler policy can also be used to
//! enable exception throwing on a registry basis.

struct default_error_handler : error_handler {
    //! A ErrorHandlerFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy.
    template<class Registry>
    class fn {
        template<typename, typename, typename>
        struct error_variant_aux;

        template<
            typename T, class... Errors, class Policy, class... MorePolicies>
        struct error_variant_aux<
            T, std::variant<Errors...>, mp11::mp_list<Policy, MorePolicies...>>
            : error_variant_aux<
                  void, std::variant<Errors...>,
                  mp11::mp_list<MorePolicies...>> {};

        template<class... Errors, class Policy, class... MorePolicies>
        struct error_variant_aux<
            std::void_t<typename Policy::errors>, std::variant<Errors...>,
            mp11::mp_list<Policy, MorePolicies...>>
            : error_variant_aux<
                  void,
                  mp11::mp_append<
                      std::variant<Errors...>, typename Policy::errors>,
                  mp11::mp_list<MorePolicies...>> {};

        template<class... Errors>
        struct error_variant_aux<
            void, std::variant<Errors...>, mp11::mp_list<>> {
            using type = std::variant<Errors...>;
        };

      public:
        //! A @ref std::variant containing an instance of a subclass of @ref
        //! openmethod_error.
        using error_variant = typename error_variant_aux<
            void,
            std::variant<
                not_initialized, no_overrider, ambiguous_call, missing_class,
                missing_base, odr_violation, final_error>,
            typename Registry::policy_list>::type;

        //! The type of the error handler function object.
        using function_type = std::function<void(const error_variant& error)>;

        //! Calls a function with the error object, wrapped in an @ref
        //! error_variant.
        //!
        //! @tparam Error A subclass of @ref openmethod_error.
        //! @param error The error object.
        template<class Error>
        static auto error(const Error& error) -> void {
            handler(error_variant(error));
        }

        //! Sets the function to be called to handle errors.
        //!
        //! Sets the error handler function to a new value, and returns the
        //! previous function.
        //!
        //! @param new_handler the new function.
        //! @return The previous function.
        // coverity[auto_causes_copy]
        static auto set(function_type new_handler) -> function_type {
            return std::exchange(handler, std::move(new_handler));
        }

        //! The default error handler function.
        //!
        //! @param error A variant containing the error.
        //!
        //! If `Registry` contains an @ref output policy, writes a description
        //! of the error; otherwise, does nothing.
        static auto default_handler(const error_variant& error) -> void {
            if constexpr (Registry::has_output) {
                std::visit(
                    [](auto&& error) {
                        error.template write<Registry>(Registry::output::os);
                    },
                    error);
                Registry::output::os << "\n";
            }
        }

      private:
        static function_type handler;
    };
};

template<class Registry>
typename default_error_handler::fn<Registry>::function_type
    default_error_handler::fn<Registry>::handler = default_handler;

} // namespace boost::openmethod::policies

#endif
