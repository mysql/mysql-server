#ifndef BOOST_OPENMETHOD_INPLACE_VPTR_HPP
#define BOOST_OPENMETHOD_INPLACE_VPTR_HPP

#include <boost/openmethod/core.hpp>

// =============================================================================
// inplace_vptr

namespace boost::openmethod {

namespace detail {

void boost_openmethod_registry(...);
void boost_openmethod_bases(...);

template<class Class>
using inplace_vptr_registry =
    decltype(boost_openmethod_registry(std::declval<Class*>()));

template<class>
struct update_vptr_bases;

template<class To, class Class>
void boost_openmethod_update_vptr(Class* obj);

template<class... Bases>
struct update_vptr_bases<mp11::mp_list<Bases...>> {
    template<class To, class Class>
    static void fn(Class* obj) {
        (boost_openmethod_update_vptr<To>(static_cast<Bases*>(obj)), ...);
    }
};

template<class To, class Class>
void boost_openmethod_update_vptr(Class* obj) {
    using registry = inplace_vptr_registry<Class>;
    using bases = decltype(boost_openmethod_bases(obj));

    if constexpr (mp11::mp_size<bases>::value == 0) {
        if constexpr (registry::has_indirect_vptr) {
            obj->boost_openmethod_vptr = &registry::template static_vptr<To>;
        } else {
            obj->boost_openmethod_vptr = registry::template static_vptr<To>;
        }
    } else {
        update_vptr_bases<bases>::template fn<To, Class>(obj);
    }
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif

template<class... Classes>
inline use_classes<Classes...> inplace_vptr_use_classes;

class inplace_vptr_base_tag {};

} // namespace detail

//! Embed a v-table pointer in a class.
//!
//! `inplace_vptr_base` is a [CRTP
//! mixin](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
//! that embeds a v-table pointer at the root of a class hierarchy. It also
//! declares a @ref boost_openmethod_vptr free function that returns the v-table
//! pointer stored in the object.
//!
//! `inplace_vptr_base` registers the class in `Registry`. It is not necessary
//! to register the class with @ref use_class or
//! {{BOOST_OPENMETHOD_REGISTER}}
//!
//! The v-table pointer is obtained directly from the `Registry`\'s @ref
//! static_vptr variable. No hashing is involved. If all the classes in
//! `Registry` inherit from `inplace_vptr_base`, the registry needs not have a
//! @ref policies::vptr policy, nor any policy it depends on (like @ref
//! policies::type_hash).
//!
//! If `Registry` contains the @ref has_indirect_vptr policy, the v-table
//! pointer is stored as a pointer to a pointer, and remains valid after a call
//! to @ref initialize.
//!
//! The default value of `Registry` can be changed by defining
//! {{BOOST_OPENMETHOD_DEFAULT_REGISTRY}}
//!
//! @tparam Class The class in which to embed the v-table pointer.
//! @tparam Registry The @ref registry in which `Class` and its derived classes
//! are registered.
//!
//! @par Example
//! @code
//! #include <boost/openmethod.hpp>
//! #include <boost/openmethod/inplace_vptr.hpp>
//! #include <boost/openmethod/initialize.hpp>
//!
//! using namespace boost::openmethod;
//!
//! struct Animal : inplace_vptr_base<Animal> {};
//!
//! struct Cat : Animal, inplace_vptr_derived<Cat, Animal> {};
//!
//! struct Dog : Animal, inplace_vptr_derived<Dog, Animal> {};
//!
//! BOOST_OPENMETHOD(
//!     poke, (virtual_<Animal&> animal, std::ostream& os), void);
//!
//! BOOST_OPENMETHOD_OVERRIDE(poke, (Cat&, std::ostream& os), void) {
//!     os << "hiss\n";
//! }
//!
//! BOOST_OPENMETHOD_OVERRIDE(poke, (Dog&, std::ostream& os), void) {
//!     os << "bark\n";
//! }
//!
//! int main() {
//!     initialize();
//!
//!     std::unique_ptr<Animal> a = std::make_unique<Cat>();
//!     std::unique_ptr<Animal> b = std::make_unique<Dog>();
//!
//!     poke(*a, std::cout); // hiss
//!     poke(*b, std::cout); // bark
//!
//!     return 0;
//! }
//! @endcode
template<class Class, class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY>
class inplace_vptr_base : protected detail::inplace_vptr_base_tag {
    template<class To, class Other>
    friend void detail::boost_openmethod_update_vptr(Other*);
    friend auto boost_openmethod_registry(Class*) -> Registry;
    friend auto boost_openmethod_bases(Class*) -> mp11::mp_list<>;

    std::conditional_t<Registry::has_indirect_vptr, const vptr_type*, vptr_type>
        boost_openmethod_vptr = nullptr;

    friend auto
    boost_openmethod_vptr(const Class& obj, Registry*) noexcept -> vptr_type {
        if constexpr (Registry::has_indirect_vptr) {
            return *obj.boost_openmethod_vptr;
        } else {
            return obj.boost_openmethod_vptr;
        }
    }

  protected:
    //! Set the vptr to `Class`\'s v-table.
    inplace_vptr_base() noexcept {
        (void)&detail::inplace_vptr_use_classes<Class, Registry>;
        detail::boost_openmethod_update_vptr<Class>(static_cast<Class*>(this));
    }

    //! Set the vptr to `nullptr`.
    ~inplace_vptr_base() noexcept {
        boost_openmethod_vptr = nullptr;
    }
};

#ifdef __MRDOCS__
//! Adjust the v-table pointer embedded in a class.
//!
//! `inplace_vptr_derived` is a [CRTP
//! mixin](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
//! that adjusts the v-table pointer in a @ref inplace_vptr_base. It can be used
//! only with classes that have @ref inplace_vptr_base as a direct or indirect
//! base class.
//!
//! `inplace_vptr_derived` registers the class and its bases in `Registry`. It
//! is not necessary to register them with @ref use_class or
//! {{BOOST_OPENMETHOD_REGISTER}}
//!
//! The v-table pointer is obtained directly from the `Registry`\'s @ref
//! static_vptr variable. No hashing is involved. If all the classes in
//! `Registry` inherit from `inplace_vptr_derived`, the registry needs not have a
//! @ref policies::vptr policy, nor any policy it depends on (like @ref
//! policies::type_hash).
//!
//! @see @ref inplace_vptr_base for an example.
//!
//! @tparam Class The class in which to embed the v-table pointer.
//! @tparam Base A direct base class of `Class`.
//! @tparam MoreBases More direct base classes of `Class`.
template<class Class, class Base, class... MoreBases>
class inplace_vptr_derived {
  protected:
    //! Set the vptr to `Class`\'s v-table.
    inplace_vptr_derived() noexcept;

    //! Set the vptr in each base class.
    //!
    //! For each base, set its vptr to the base's v-table.
    ~inplace_vptr_derived() noexcept;
};

#else
template<class Class, class Base, class... MoreBases>
class inplace_vptr_derived;
#endif

//! Specialization for a single base class.
//!
//!
//! @see The main template for documentation.
template<class Class, class Base>
class inplace_vptr_derived<Class, Base> {
    static_assert(
        !detail::is_registry<Base>,
        "registry can be specified only for root classes");

    static_assert(
        std::is_base_of_v<detail::inplace_vptr_base_tag, Base>,
        "class must inherit from inplace_vptr_base");

    template<class To, class Other>
    friend void detail::boost_openmethod_update_vptr(Other*);
    friend auto boost_openmethod_bases(Class*) -> mp11::mp_list<Base>;

  protected:
    //! Set the vptr to `Class`\'s v-table.
    inplace_vptr_derived() noexcept {
        using namespace detail;
        (void)&detail::inplace_vptr_use_classes<
            Class, Base, detail::inplace_vptr_registry<Class>>;
        boost_openmethod_update_vptr<Class>(static_cast<Class*>(this));
    }

    //! Set the vptr in base class.
    ~inplace_vptr_derived() noexcept {
        detail::boost_openmethod_update_vptr<Base>(
            static_cast<Base*>(static_cast<Class*>(this)));
    }
};

//! Specialization for multiple base classes.
//!
//! @see The main template for documentation.
template<class Class, class Base1, class Base2, class... MoreBases>
class inplace_vptr_derived<Class, Base1, Base2, MoreBases...> {
    static_assert(
        !detail::is_registry<Base1> && !detail::is_registry<Base2> &&
            (!detail::is_registry<MoreBases> && ...),
        "registry can be specified only for root classes");

    friend auto
    boost_openmethod_registry(Class*) -> detail::inplace_vptr_registry<Base1>;
    friend auto
    boost_openmethod_bases(Class*) -> mp11::mp_list<Base1, Base2, MoreBases...>;
    friend auto boost_openmethod_vptr(
        const Class& obj,
        detail::inplace_vptr_registry<Base1>* registry) -> vptr_type {
        return boost_openmethod_vptr(static_cast<const Base1&>(obj), registry);
    }

  protected:
    //! Set the vptr to `Class`\'s v-table.
    inplace_vptr_derived() noexcept {
        (void)&detail::inplace_vptr_use_classes<
            Class, Base1, Base2, MoreBases...,
            detail::inplace_vptr_registry<Base1>>;
        detail::boost_openmethod_update_vptr<Class>(static_cast<Class*>(this));
    }

    //! Set the vptr in each base class.
    //!
    //! For each base, set its vptr to the base's v-table.
    ~inplace_vptr_derived() noexcept {
        auto obj = static_cast<Class*>(this);
        detail::boost_openmethod_update_vptr<Base1>(static_cast<Base1*>(obj));
        detail::boost_openmethod_update_vptr<Base2>(static_cast<Base2*>(obj));
        (detail::boost_openmethod_update_vptr<MoreBases>(
             static_cast<MoreBases*>(obj)),
         ...);
    }
};

namespace aliases {
using boost::openmethod::inplace_vptr_base;
using boost::openmethod::inplace_vptr_derived;
} // namespace aliases

} // namespace boost::openmethod

#endif // BOOST_OPENMETHOD_inplace_vptr_HPP
