// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_POLICY_STD_RTTI_HPP
#define BOOST_OPENMETHOD_POLICY_STD_RTTI_HPP

#include <boost/openmethod/preamble.hpp>

#ifndef BOOST_NO_RTTI
#include <typeindex>
#include <typeinfo>
#include <boost/core/demangle.hpp>
#endif

namespace boost::openmethod::policies {

//! Implements the @ref rtti policy using standard RTTI.
//!
//! `std_rtti` implements the `rtti` policy using the standard C++ RTTI system.
//! It is the default RTTI policy.
struct std_rtti : rtti {
    //! A RttiFn metafunction.
    //!
    //! @tparam Registry The registry containing this policy.
    template<class Registry>
    struct fn {
#ifndef BOOST_NO_RTTI
        //! Tests if a class is polymorphic.
        //!
        //! Evaluates to `true` if `Class` is a polymorphic class, as defined by
        //! the C++ standard, i.e. a class that contains at least one virtual
        //! function.
        //!
        //! @tparam Class A class.
        template<class Class>
        static constexpr bool is_polymorphic = std::is_polymorphic_v<Class>;

        //! Returns the static @ref type_id of a type.
        //!
        //! Returns `&typeid(Class)`, cast to `type_id`.
        //!
        //! @tparam Class A class.
        //! @return The static type_id of Class.
        template<class Class>
        static auto static_type() -> type_id {
            return &typeid(Class);
        }

        //! Returns the dynamic @ref type_id of an object.
        //!
        //! Returns `&typeid(obj)`, cast to `type_id`.
        //!
        //! @tparam Class A registered class.
        //! @param obj A reference to an instance of `Class`.
        //! @return The type_id of `obj`'s class.
        template<class Class>
        static auto dynamic_type(const Class& obj) -> type_id {
            return &typeid(obj);
        }

        //! Writes a representation of a @ref type_id to a stream.
        //!
        //! Writes the demangled name of the class identified by `type` to
        //! `stream`.
        //!
        //! @tparam Stream A SimpleOutputStream.
        //! @param type The `type_id` to write.
        //! @param stream The stream to write to.
        template<typename Stream>
        static auto type_name(type_id type, Stream& stream) -> void {
            stream << boost::core::demangle(
                reinterpret_cast<const std::type_info*>(type)->name());
        }

        //! Returns a key that uniquely identifies a class.
        //!
        //! C++ does *not* guarantee that there is a single instance of
        //! `std::type_info` per type. `std_rtti` uses the addresses of
        //! `std::type_index` objects as `type_id`s. Thus, the same class may
        //! have multiple corresponding `type_id`s. `std::type_index` objects,
        //! on the other hand, are guaranteed to compare as equal iff they
        //! correspond to the same class, and they can be used to identify the
        //! `type_id`s pertaining to the same class.
        //!
        //! @param type A `type_id`.
        //! @return A `std::type_index` for `type` (cast to a `const std::type_info&`).
        static auto type_index(type_id type) -> std::type_index {
            return std::type_index(
                *reinterpret_cast<const std::type_info*>(type));
        }

        //! Casts an object to a type.
        //!
        //! Casts `obj` to a reference to an instance of `D`, using
        //! `dynamic_cast`.
        //!
        //! @tparam D A reference to a subclass of `B`.
        //! @tparam B A registered class.
        //! @param obj A reference to an instance of `B`.
        template<typename D, typename B>
        static auto dynamic_cast_ref(B&& obj) -> D {
            return dynamic_cast<D>(obj);
        }
#endif
    };
};

} // namespace boost::openmethod::policies

#endif
