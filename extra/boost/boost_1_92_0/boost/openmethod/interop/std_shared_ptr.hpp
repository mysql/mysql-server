// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_INTEROP_SHARED_PTR_HPP
#define BOOST_OPENMETHOD_INTEROP_SHARED_PTR_HPP

#include <boost/openmethod/core.hpp>
#include <memory>

namespace boost::openmethod {
namespace detail {

template<class T>
struct shared_ptr_cast_traits {
    static_assert(false_t<T>, "cast only allowed to a shared_ptr");
};

template<class T>
struct shared_ptr_cast_traits<std::shared_ptr<T>> {
    using virtual_type = T;
};

template<class T>
struct shared_ptr_cast_traits<const std::shared_ptr<T>&> {
    using virtual_type = T;
};

template<class T>
struct shared_ptr_cast_traits<std::shared_ptr<T>&&> {
    using virtual_type = T;
};

template<typename T, class Registry>
struct validate_method_parameter<std::shared_ptr<T>&, Registry, void>
    : std::false_type {
    static_assert(
        false_t<T>,
        "std::shared_ptr cannot be passed by non-const lvalue reference");
};

template<typename T, class Registry>
struct validate_method_parameter<
    virtual_ptr<std::shared_ptr<T>, Registry>&, Registry, void>
    : std::false_type {
    static_assert(
        false_t<T>,
        "std::shared_ptr cannot be passed by non-const lvalue reference");
};

} // namespace detail

//! Specialize virtual_traits for std::shared_ptr by value.
//!
//! @tparam Class A class type, possibly cv-qualified.
//! @tparam Registry A @ref registry.
template<typename Class, class Registry>
struct virtual_traits<std::shared_ptr<Class>, Registry> {
    //! Rebind to a different element type.
    //!
    //! @tparam Other The new element type.
    template<class Other>
    using rebind = std::shared_ptr<Other>;

    //! `Class`, stripped from cv-qualifiers.
    using virtual_type = std::remove_cv_t<Class>;

    //! Return a reference to a non-modifiable `Class` object.
    //! @param arg A reference to a `std::shared_ptr<Class>`.
    //! @return A reference to the object pointed to.
    static auto peek(const std::shared_ptr<Class>& arg) -> const Class& {
        return *arg;
    }

    //! Cast to another type.
    //!
    //! Cast to a `std::shared_ptr` to another type. If possible, use
    //! `std::static_pointer_cast`. Otherwise, use `std::dynamic_pointer_cast`.
    //!
    //! @tparam Derived A lvalue reference type to a `std::shared_ptr`.
    //! @param obj A reference to a `const shared_ptr<Class>`.
    //! @return A `std::shared_ptr` to the same object, cast to
    //! `Derived::element_type`.
    template<class Derived>
    static auto cast(const std::shared_ptr<Class>& obj) -> decltype(auto) {
        using namespace boost::openmethod::detail;

        if constexpr (requires_dynamic_cast<
                          Class*, typename Derived::element_type*>) {
            return std::dynamic_pointer_cast<
                typename shared_ptr_cast_traits<Derived>::virtual_type>(obj);
        } else {
            return std::static_pointer_cast<
                typename shared_ptr_cast_traits<Derived>::virtual_type>(obj);
        }
    }

#if __cplusplus >= 202002L
    //! Move-cast to another type (since c++20)
    //!
    //! Cast to a `std::shared_ptr` xvalue reference to another type. If
    //! possible, use `std::static_pointer_cast`. Otherwise, use
    //! `std::dynamic_pointer_cast`.
    //!
    //! @note This overload is only available for C++20 and above, because
    //! rvalue references overloads of `std::static_pointer_cast` and
    //! `std::dynamic_pointer_cast` were not available before.
    //!
    //! @tparam Derived A xvalue reference to a `std::shared_ptr`.
    //! @param obj A xvalue reference to a `shared_ptr<Class>`.
    //! @return A `std::shared_ptr&&` to the same object, cast to
    //! `Derived::element_type`.
    template<class Derived>
    static auto cast(std::shared_ptr<Class>&& obj) -> decltype(auto) {
        using namespace boost::openmethod::detail;

        if constexpr (requires_dynamic_cast<
                          Class*, decltype(std::declval<Derived>().get())>) {
            return std::dynamic_pointer_cast<
                typename shared_ptr_cast_traits<Derived>::virtual_type>(
                std::move(obj));
        } else {
            return std::static_pointer_cast<
                typename shared_ptr_cast_traits<Derived>::virtual_type>(
                std::move(obj));
        }
    }
#endif
};

//! Specialize virtual_traits for std::shared_ptr by reference.
//!
//! @note Passing a `std::shared_ptr` in a method call by const reference
//! creates a temporary `std::shared_ptr` and passes it by const reference to
//! the overrider. This is necessary because virtual arguments need to be cast
//! to the type expected by the overrider.
//!
//! @tparam Class A class type, possibly cv-qualified.
//! @tparam Registry A @ref registry.
template<class Class, class Registry>
struct virtual_traits<const std::shared_ptr<Class>&, Registry> {
  public:
    //! Rebind to a different element type.
    //!
    //! @tparam Other The new element type.
    template<class Other>
    using rebind = std::shared_ptr<Other>;

    //! `Class`, stripped from cv-qualifiers.
    using virtual_type = std::remove_cv_t<Class>;

    //! Return a reference to a non-modifiable `Class` object.
    //! @param arg A reference to a `std::shared_ptr<Class>`.
    //! @return A reference to the object pointed to.
    static auto peek(const std::shared_ptr<Class>& arg) -> const Class& {
        return *arg;
    }

    //! Cast to another type.
    //!
    //! Cast to a `std::shared_ptr` to another type. If possible, use
    //! `std::static_pointer_cast`. Otherwise, use `std::dynamic_pointer_cast`.
    //!
    //! @tparam Derived A lvalue reference type to a `std::shared_ptr`.
    //! @param obj A reference to a `const shared_ptr<Class>`.
    //! @return A `std::shared_ptr` to the same object, cast to
    //! `Derived::element_type`.
    template<class Other>
    static decltype(auto) cast(const std::shared_ptr<Class>& obj) {
        if constexpr (std::is_same_v<Other, const std::shared_ptr<Class>&>) {
            // avoid unnecessary copy
            return obj;
        } else {
            using namespace boost::openmethod::detail;

            if constexpr (requires_dynamic_cast<Class*, Other>) {
                return std::dynamic_pointer_cast<
                    typename shared_ptr_cast_traits<Other>::virtual_type>(obj);
            } else {
                return std::static_pointer_cast<
                    typename shared_ptr_cast_traits<Other>::virtual_type>(obj);
            }
        }
    }
};

//! Alias for a `virtual_ptr<std::shared_ptr<T>>`.
template<class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY>
using shared_virtual_ptr = virtual_ptr<std::shared_ptr<Class>, Registry>;

//! Create a new object and return a `shared_virtual_ptr` to it.
//!
//! Create an object using `std::make_shared`, and return a @ref
//! shared_virtual_ptr pointing to it. Since the exact class of the object is
//! known, the `virtual_ptr` is created using @ref final_virtual_ptr.
//!
//! `Class` is _not_ required to be a polymorphic class.
//!
//! @tparam Class The class of the object to create.
//! @tparam Registry A @ref registry.
//! @tparam T Types of the arguments to pass to the constructor of `Class`.
//! @param args Arguments to pass to the constructor of `Class`.
//! @return A `shared_virtual_ptr<Class, Registry>` pointing to a newly
//! created object of type `Class`.
template<
    class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY,
    typename... T>
inline auto make_shared_virtual(T&&... args) {
    return final_virtual_ptr<Registry>(
        std::make_shared<Class>(std::forward<T>(args)...));
}

namespace aliases {
using boost::openmethod::make_shared_virtual;
using boost::openmethod::shared_virtual_ptr;
} // namespace aliases

} // namespace boost::openmethod

#endif
