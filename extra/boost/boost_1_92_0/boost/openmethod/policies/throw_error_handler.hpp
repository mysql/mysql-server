// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_THROW_ERROR_HPP
#define BOOST_OPENMETHOD_POLICY_THROW_ERROR_HPP

#include <boost/openmethod/preamble.hpp>

#include <sstream>
#include <stdexcept>
#include <string>

namespace boost::openmethod::policies {

//! Throws error as an exception.
//!
struct throw_error_handler : error_handler {
    //! A ErrorHandlerFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy.
    template<class Registry>
    class fn {
      public:
        //! Throws the error.
        //!
        //! Wraps the error in an object that can be caught either as an
        //! `Error`, or as a `std::runtime_error`, and throws it as an exception.
        //!
        //! @tparam Error A subclass of @ref openmethod_error.
        //! @param error The error object.
        template<class Error>
        [[noreturn]] static auto error(const Error& error) -> void {
            struct wrapper : Error, std::runtime_error {
                wrapper(const Error& error, std::string&& description)
                    : Error(error), std::runtime_error(description) {
                }
            };

            std::ostringstream os;
            error.template write<Registry>(os);

            throw wrapper(error, os.str());
        }
    };
};

} // namespace boost::openmethod::policies

#endif
