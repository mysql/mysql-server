// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_UNIQUE_PTR_HPP
#define BOOST_OPENMETHOD_INTEROP_UNIQUE_PTR_HPP

#include <boost/openmethod/core.hpp>

#include <memory>

namespace boost::openmethod {

//! Specialize virtual_traits for std::unique_ptr by value.
//!
//! @tparam Class A class type, possibly cv-qualified.
//! @tparam Registry A @ref registry.
template<class Class, class Registry>
struct virtual_traits<std::unique_ptr<Class>, Registry> {
    //! `Class`, stripped from cv-qualifiers.
    using virtual_type = std::remove_cv_t<Class>;

    //! Return a reference to a non-modifiable `Class` object.
    //! @param arg A reference to a `std::unique_ptr<Class>`.
    //! @return A reference to the object pointed to.
    static auto peek(const std::unique_ptr<Class>& arg) -> const Class& {
        return *arg;
    }

    //! Cast to a type.
    //!
    //! Cast a reference to the managed object, using `static_cast` if possible,
    //! and `Registry::rtti::dynamic_cast_ref` otherwise. If the cast succeeds,
    //! transfer ownership to a `std::unique_ptr` to the target type, and return
    //! it.
    //!
    //! @tparam Derived A xvalue reference to a `std::unique_ptr`.
    //! @param obj A xvalue reference to a `std::unique_ptr`.
    //! @return A `std::unique_ptr<Derived::element_type>`.
    template<typename Derived>
    static auto cast(std::unique_ptr<Class>&& ptr) {
        if constexpr (detail::requires_dynamic_cast<Class&, Derived&>) {
            auto p = &Registry::rtti::template dynamic_cast_ref<
                typename Derived::element_type&>(*ptr);
            // coverity[alloc_fn]
            ptr.release();
            return Derived(p);
        } else {
            auto p = &static_cast<typename Derived::element_type&>(*ptr);
            // coverity[alloc_fn]
            ptr.release();
            return Derived(p);
        }
    }

    //! Rebind to a different element type.
    //!
    //! @tparam Other The new element type.
    template<class Other>
    using rebind = std::unique_ptr<Other>;
};

//! Alias for a `virtual_ptr<std::unique_ptr<T>>`.
template<class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY>
using unique_virtual_ptr = virtual_ptr<std::unique_ptr<Class>, Registry>;

//! Create a new object and return a `unique_virtual_ptr` to it.
//!
//! Create an object using `std::make_unique`, and return a @ref
//! unique_virtual_ptr pointing to it. Since the exact class of the object is
//! known, the `virtual_ptr` is created using @ref final_virtual_ptr.
//!
//! `Class` is _not_ required to be a polymorphic class.
//!
//! @tparam Class The class of the object to create.
//! @tparam Registry A @ref registry.
//! @tparam T Types of the arguments to pass to the constructor of `Class`.
//! @param args Arguments to pass to the constructor of `Class`.
//! @return A `unique_virtual_ptr<Class, Registry>` pointing to a newly
//! created object of type `Class`.
template<
    class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY,
    typename... T>
inline auto make_unique_virtual(T&&... args) {
    return final_virtual_ptr<Registry>(
        std::make_unique<Class>(std::forward<T>(args)...));
}

namespace aliases {
using boost::openmethod::make_unique_virtual;
using boost::openmethod::unique_virtual_ptr;
} // namespace aliases

} // namespace boost::openmethod

#endif
