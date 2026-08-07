//---------------------------------------------------------------------------//
// Copyright (c) 2019 Anthony Chang <ac.chang@outlook.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_EXCEPTION_SET_DEFAULT_QUEUE_ERROR_HPP
#define BOOST_COMPUTE_EXCEPTION_SET_DEFAULT_QUEUE_ERROR_HPP

#include <exception>

namespace boost {
namespace compute {

/// \class set_default_queue_error
/// \brief Exception thrown when failure to set default command queue 
///
/// This exception is thrown when Boost.Compute fails to set up user-provided 
/// default command queue for the system. 
class set_default_queue_error : public std::exception
{
public:
    /// Creates a new set_default_queue_error exception object.
    set_default_queue_error() throw()
    {
    }

    /// Destroys the set_default_queue_error object.
    ~set_default_queue_error() throw()
    {
    }

    /// Returns a string with a description of the error.
    const char* what() const throw()
    {
        return "User command queue mismatches default device and/or context";
    }
};

} // end compute namespace
} // end boost namespace

#endif // BOOST_COMPUTE_EXCEPTION_SET_DEFAULT_QUEUE_ERROR_HPP
