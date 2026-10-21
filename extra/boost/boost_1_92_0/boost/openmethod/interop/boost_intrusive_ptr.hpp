// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_BOOST_INTRUSIVE_PTR_HPP
#define BOOST_OPENMETHOD_INTEROP_BOOST_INTRUSIVE_PTR_HPP

#include <boost/openmethod/core.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <memory>

namespace boost::openmethod {

//! Specialize virtual_traits for boost::intrusive_ptr.
//!
//! @tparam Class A class type, possibly cv-qualified.
//! @tparam Registry A @ref registry.
template<typename Class, class Registry>
struct virtual_traits<boost::intrusive_ptr<Class>, Registry> {
    //! Rebind to a different element type.
    //!
    //! @tparam Other The new element type.
    template<class Other>
    using rebind = boost::intrusive_ptr<Other>;

    //! `Class`, stripped from cv-qualifiers.
    using virtual_type = std::remove_cv_t<Class>;

    //! Return a reference to a non-modifiable `Class` object.
    //! @param arg A reference to a `boost::intrusive_ptr<Class>`.
    //! @return A reference to the object pointed to.
    static auto peek(const boost::intrusive_ptr<Class>& arg) -> const Class& {
        return *arg;
    }

    //! Cast method argument to overrider argument.
    //!
    //! Cast a `boost::intrusive_ptr` to a `boost::intrusive_ptr` to a derived
    //! class, using a static cast if possible, and a dynamic cast otherwise.
    //!
    //! @tparam OverriderType The type required by the overrider (a
    //! `boost::intrusive_ptr`).
    //! @param obj The method's argument..
    //! @return A `boost::intrusive_ptr` _value_.
    template<class OverriderType>
    static auto cast(const boost::intrusive_ptr<Class>& obj) {
        using element_type = typename OverriderType::element_type;

        if constexpr (detail::requires_dynamic_cast<Class*, element_type*>) {
            // make it work with custom RTTI
            return OverriderType(
                &Registry::rtti::template dynamic_cast_ref<element_type&>(
                    *obj));
        } else {
            return boost::static_pointer_cast<element_type>(obj);
        }
    }
};

//! Specialize virtual_traits for const boost::intrusive_ptr&.
//!
//! @tparam Class A class type, possibly cv-qualified.
//! @tparam Registry A @ref registry.
template<class Class, class Registry>
struct virtual_traits<const boost::intrusive_ptr<Class>&, Registry> {
  public:
    //! Rebind to a different element type.
    //!
    //! @tparam Other The new element type.
    template<class Other>
    using rebind = boost::intrusive_ptr<Other>;

    //! `Class`, stripped from cv-qualifiers.
    using virtual_type = std::remove_cv_t<Class>;

    //! Return a reference to a non-modifiable `Class` object.
    //! @param arg A reference to a `boost::intrusive_ptr<Class>`.
    //! @return A reference to the object pointed to.
    static auto peek(const boost::intrusive_ptr<Class>& arg) -> const Class& {
        return *arg;
    }

    //! Cast method argument to overrider argument.
    //!
    //! Cast a `boost::intrusive_ptr` to a `boost::intrusive_ptr` to a derived
    //! class, using a static cast if possible, and a dynamic cast otherwise.
    //!
    //! @tparam OverriderType The type required by the overrider (a `const
    //! boost::intrusive_ptr&`).
    //! @param obj The method's argument..
    //! @return A `boost::intrusive_ptr` _value_.
    template<class OverriderType>
    static decltype(auto) cast(const boost::intrusive_ptr<Class>& obj) {
        if constexpr (std::is_same_v<
                          OverriderType, const boost::intrusive_ptr<Class>&>) {
            return obj;
        } else {
            using element_type =
                typename std::remove_reference_t<OverriderType>::element_type;

            if constexpr (detail::requires_dynamic_cast<
                              Class*, element_type*>) {
                // make it work with custom RTTI
                return std::remove_const_t<
                    std::remove_reference_t<OverriderType>>(
                    &Registry::rtti::template dynamic_cast_ref<element_type&>(
                        *obj));
            } else {
                return boost::static_pointer_cast<element_type>(obj);
            }
        }
    }
};

//! Alias for a `virtual_ptr<boost::intrusive_ptr<T>>`.
template<class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY>
using boost_intrusive_virtual_ptr =
    virtual_ptr<boost::intrusive_ptr<Class>, Registry>;

//! Create a new object and return a `boost_intrusive_virtual_ptr` to it.
//!
//! Create an object using `std::make_shared`, and return a @ref
//! boost_intrusive_virtual_ptr pointing to it. Since the exact class of the
//! object is known, the `virtual_ptr` is created using @ref final_virtual_ptr.
//!
//! `Class` is _not_ required to be a polymorphic class.
//!
//! @tparam Class The class of the object to create.
//! @tparam Registry A @ref registry.
//! @tparam T Types of the arguments to pass to the constructor of `Class`.
//! @param args Arguments to pass to the constructor of `Class`.
//! @return A `boost_intrusive_virtual_ptr<Class, Registry>` pointing to a newly
//! created object of type `Class`.
template<
    class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY,
    typename... T>
inline auto make_boost_intrusive_virtual(T&&... args) {
    return final_virtual_ptr<Registry>(intrusive_ptr<Class>(
        new std::remove_cv_t<Class>(std::forward<T>(args)...)));
}

namespace aliases {
using boost::openmethod::boost_intrusive_virtual_ptr;
using boost::openmethod::make_boost_intrusive_virtual;
} // namespace aliases

} // namespace boost::openmethod

#endif
