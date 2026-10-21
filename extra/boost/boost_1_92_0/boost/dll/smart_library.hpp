//  Copyright 2016 Klemens Morgenstern
//  Copyright Antony Polukhin, 2019-2026
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file boost/dll/smart_library.hpp
/// \warning Experimental feature that relies on an incomplete implementation of platform specific C++
///          mangling. In case of an issue provide a PR with a fix and tests to https://github.com/boostorg/dll .
///          boost/dll/smart_library.hpp is not included in boost/dll.hpp
/// \brief Contains the boost::dll::experimental::smart_library class for loading mangled symbols.

#ifndef BOOST_DLL_SMART_LIBRARY_HPP_
#define BOOST_DLL_SMART_LIBRARY_HPP_

#include <boost/dll/detail/config.hpp>

#if !defined(BOOST_USE_MODULES) || defined(BOOST_DLL_INTERFACE_UNIT)

#ifdef BOOST_HAS_PRAGMA_ONCE
# pragma once
#endif

#include <boost/dll/config.hpp>

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <type_traits>
#include <utility>  // std::move
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

#if (__cplusplus < 201103L) && (!defined(_MSVC_LANG) || _MSVC_LANG < 201103L)
#  error This file requires C++11 at least!
#endif

#if defined(_MSC_VER) // MSVC, Clang-cl, and ICC on Windows
#   include <boost/dll/detail/demangling/msvc.hpp>
#else
#   include <boost/dll/detail/demangling/itanium.hpp>
#endif

#include <boost/dll/shared_library.hpp>
#include <boost/dll/detail/get_mem_fn_type.hpp>
#include <boost/dll/detail/ctor_dtor.hpp>
#include <boost/dll/detail/type_info.hpp>


namespace boost {
namespace dll {
namespace experimental {

using boost::dll::detail::constructor;
using boost::dll::detail::destructor;

BOOST_DLL_BEGIN_MODULE_EXPORT

/*!
* \brief This class is an extension of \ref shared_library, which allows to load C++ symbols.
*
* This class allows type safe loading of overloaded functions, member-functions, constructors and variables.
* It also allows to overwrite classes so they can be loaded, while being declared with different names.
*
* \warning Experimental feature that relies on an incomplete implementation of platform specific C++
*          mangling. In case of an issue provide a PR with a fix and tests to https://github.com/boostorg/dll
*
* Currently known limitations:
*
* Member functions must be defined outside of the class to be exported. That is:
* \code
* //not exported:
* struct BOOST_SYMBOL_EXPORT my_class { void func() {} };
* //exported
* struct BOOST_SYMBOL_EXPORT my_class { void func(); };
* void my_class::func() {}
* \endcode
*
* With the current analysis, the first version does get exported in MSVC.
* MinGW also does export it, BOOST_SYMBOL_EXPORT is written before it. To allow this on windows one can use
* BOOST_DLL_MEMBER_EXPORT for this, so that MinGW and MSVC can provide those functions. This does however not work with gcc on linux.
*
* Direct initialization of members.
* On linux the following member variable i will not be initialized when using the allocating constructor:
* \code
* struct BOOST_SYMBOL_EXPORT my_class { int i; my_class() : i(42) {} };
* \endcode
*
* This does however not happen when the value is set inside the constructor function.
*/
class smart_library {
    shared_library lib_;
    detail::mangled_storage_impl storage_;

public:
    /*!
     * Get the underlying shared_library
     */
    const shared_library &shared_lib() const noexcept { return lib_;}

    using mangled_storage = detail::mangled_storage_impl;
    /*!
    * Access to the mangled storage, which is created on construction.
    *
    * \throw Nothing.
    */
    const mangled_storage &symbol_storage() const noexcept { return storage_; }

    ///Overload, for current development.
    mangled_storage &symbol_storage() noexcept { return storage_; }

    //! \copydoc shared_library::shared_library()
    smart_library() = default;

    //! \copydoc shared_library::shared_library(const boost::dll::fs::path& lib_path, load_mode::type mode = load_mode::default_mode)
    smart_library(const boost::dll::fs::path& lib_path, load_mode::type mode = load_mode::default_mode) {
        lib_.load(lib_path, mode);
        storage_.load(lib_path);
    }

    //! \copydoc shared_library::shared_library(const boost::dll::fs::path& lib_path, boost::dll::fs::error_code& ec, load_mode::type mode = load_mode::default_mode)
    smart_library(const boost::dll::fs::path& lib_path, boost::dll::fs::error_code& ec, load_mode::type mode = load_mode::default_mode) {
        load(lib_path, mode, ec);
    }

    //! \copydoc shared_library::shared_library(const boost::dll::fs::path& lib_path, load_mode::type mode, boost::dll::fs::error_code& ec)
    smart_library(const boost::dll::fs::path& lib_path, load_mode::type mode, boost::dll::fs::error_code& ec) {
        load(lib_path, mode, ec);
    }
    /*!
     * copy a smart_library object.
     *
     * \param lib A smart_library to move from.
     *
     * \throw Nothing.
     */
     smart_library(const smart_library & lib) = default;
   /*!
    * Move a smart_library object.
    *
    * \param lib A smart_library to move from.
    *
    * \throw Nothing.
    */
    smart_library(smart_library&& lib)  = default;

    /*!
      * Construct from a shared_library object.
      *
      * \param lib A shared_library to move from.
      *
      * \throw Nothing.
      */
    explicit smart_library(const shared_library & lib) noexcept
        : lib_(lib) {
        if (lib_.is_loaded()) {
            storage_.load(lib_.location());
        }
    }
    /*!
    * Construct from a shared_library object.
    *
    * \param lib A shared_library to move from.
    *
    * \throw Nothing.
    */
    explicit smart_library(shared_library&& lib) noexcept
        : lib_(std::move(lib))
    {
        if (lib_.is_loaded()) {
            storage_.load(lib_.location());
        }
    }

    /*!
    * Destroys the smart_library.
    * `unload()` is called if the DLL/DSO was loaded. If library was loaded multiple times
    * by different instances of shared_library, the actual DLL/DSO won't be unloaded until
    * there is at least one instance of shared_library.
    *
    * \throw Nothing.
    */
    ~smart_library() = default;

    //! \copydoc shared_library::load(const boost::dll::fs::path& lib_path, load_mode::type mode = load_mode::default_mode)
    void load(const boost::dll::fs::path& lib_path, load_mode::type mode = load_mode::default_mode) {
        boost::dll::fs::error_code ec;
        storage_.load(lib_path);
        lib_.load(lib_path, mode, ec);

        if (ec) {
            boost::dll::detail::report_error(ec, "load() failed");
        }
    }

    //! \copydoc shared_library::load(const boost::dll::fs::path& lib_path, boost::dll::fs::error_code& ec, load_mode::type mode = load_mode::default_mode)
    void load(const boost::dll::fs::path& lib_path, boost::dll::fs::error_code& ec, load_mode::type mode = load_mode::default_mode) {
        ec.clear();
        storage_.load(lib_path);
        lib_.load(lib_path, mode, ec);
    }

    //! \copydoc shared_library::load(const boost::dll::fs::path& lib_path, load_mode::type mode, boost::dll::fs::error_code& ec)
    void load(const boost::dll::fs::path& lib_path, load_mode::type mode, boost::dll::fs::error_code& ec) {
        ec.clear();
        storage_.load(lib_path);
        lib_.load(lib_path, mode, ec);
    }

    /*!
     * Load a variable from the referenced library.
     *
     * Unlinke shared_library::get this function will also load scoped variables, which also includes static class members.
     *
     * \note When mangled, MSVC will also check the type.
     *
     * \param name Name of the variable
     * \tparam T Type of the variable
     * \return A reference to the variable of type T.
     *
     * \throw \forcedlinkfs{system_error} if symbol does not exist or if the DLL/DSO was not loaded.
     */
    template<typename T>
    T& get_variable(const std::string &name) const {
        return lib_.get<T>(storage_.get_variable<T>(name));
    }

    /*!
     * Load a function from the referenced library.
     *
     * \b Example:
     *
     * \code
     * smart_library lib("test_lib.so");
     * typedef int      (&add_ints)(int, int);
     * typedef double (&add_doubles)(double, double);
     * add_ints     f1 = lib.get_function<int(int, int)>         ("func_name");
     * add_doubles  f2 = lib.get_function<double(double, double)>("func_name");
     * \endcode
     *
     * \note When mangled, MSVC will also check the return type.
     *
     * \param name Name of the function.
     * \tparam Func Type of the function, required for determining the overload
     * \return A reference to the function of type F.
     *
     * \throw \forcedlinkfs{system_error} if symbol does not exist or if the DLL/DSO was not loaded.
     */
    template<typename Func>
    Func& get_function(const std::string &name) const {
        return lib_.get<Func>(storage_.get_function<Func>(name));
    }

    /*!
     * Load a member-function from the referenced library.
     *
     * \b Example (import class is MyClass, which is available inside the library and the host):
     *
     * \code
     * smart_library lib("test_lib.so");
     *
     * typedef int      MyClass(*func)(int);
     * typedef int   MyClass(*func_const)(int) const;
     *
     * add_ints     f1 = lib.get_mem_fn<MyClass, int(int)>              ("MyClass::function");
     * add_doubles  f2 = lib.get_mem_fn<const MyClass, double(double)>("MyClass::function");
     * \endcode
     *
     * \note When mangled, MSVC will also check the return type.
     *
     * \param name Name of the function.
     * \tparam Class The class the function is a member of. If Class is const, the function will be assumed as taking a const this-pointer. The same applies for volatile.
     * \tparam Func Signature of the function, required for determining the overload
     * \return A pointer to the member-function with the signature provided
     *
     * \throw \forcedlinkfs{system_error} if symbol does not exist or if the DLL/DSO was not loaded.
     */
    template<typename Class, typename Func>
    typename boost::dll::detail::get_mem_fn_type<Class, Func>::mem_fn get_mem_fn(const std::string& name) const {
        return lib_.get<typename boost::dll::detail::get_mem_fn_type<Class, Func>::mem_fn>(
                storage_.get_mem_fn<Class, Func>(name)
        );
    }

    /*!
     * Load a constructor from the referenced library.
     *
     * \b Example (import class is MyClass, which is available inside the library and the host):
     *
     * \code
     * smart_library lib("test_lib.so");
     *
     * constructor<MyClass(int)    f1 = lib.get_mem_fn<MyClass(int)>();
     * \endcode
     *
     * \tparam Signature Signature of the function, required for determining the overload. The return type is the class which this is the constructor of.
     * \return A constructor object.
     *
     * \throw \forcedlinkfs{system_error} if symbol does not exist or if the DLL/DSO was not loaded.
     */
    template<typename Signature>
    constructor<Signature> get_constructor() const {
        return boost::dll::detail::load_ctor<Signature>(lib_, storage_.get_constructor<Signature>());
    }

    /*!
     * Load a destructor from the referenced library.
     *
     * \b Example (import class is MyClass, which is available inside the library and the host):
     *
     * \code
     * smart_library lib("test_lib.so");
     *
     * destructor<MyClass>     f1 = lib.get_mem_fn<MyClass>();
     * \endcode
     *
     * \tparam Class The class whose destructor shall be loaded
     * \return A destructor object.
     *
     * \throw \forcedlinkfs{system_error} if symbol does not exist or if the DLL/DSO was not loaded.
     *
     */
    template<typename Class>
    destructor<Class> get_destructor() const {
        return boost::dll::detail::load_dtor<Class>(lib_, storage_.get_destructor<Class>());
    }
    /*!
     * Load the typeinfo of the given type.
     *
     * \b Example (import class is MyClass, which is available inside the library and the host):
     *
     * \code
     * smart_library lib("test_lib.so");
     *
     * std::type_info &ti = lib.get_Type_info<MyClass>();
     * \endcode
     *
     * \tparam Class The class whose typeinfo shall be loaded
     * \return A reference to a type_info object.
     *
     * \throw \forcedlinkfs{system_error} if symbol does not exist or if the DLL/DSO was not loaded.
     *
     */
    template<typename Class>
    const std::type_info& get_type_info() const
    {
        return boost::dll::detail::load_type_info<Class>(lib_, storage_);
    }
    /**
     * This function can be used to add a type alias.
     *
     * This is to be used, when a class shall be imported, which is not declared on the host side.
     *
     * Example:
     * \code
     * smart_library lib("test_lib.so");
     *
     * lib.add_type_alias<MyAlias>("MyClass"); //when using MyAlias, the library will look for MyClass
     *
     * //get the destructor of MyClass
     * destructor<MyAlias> dtor = lib.get_destructor<MyAlias>();
     * \endcode
     *
     *
     * \param name Name of the class the alias is for.
     *
     * \attention If the alias-type is not large enough for the imported class, it will result in undefined behaviour.
     * \warning The alias will only be applied for the type signature, it will not replace the token in the scoped name.
     */
    template<typename Alias> void add_type_alias(const std::string& name) {
        this->storage_.add_alias<Alias>(name);
    }

    //! \copydoc shared_library::unload()
    void unload() noexcept {
        storage_.clear();
        lib_.unload();
    }

    //! \copydoc shared_library::is_loaded() const
    bool is_loaded() const noexcept {
        return lib_.is_loaded();
    }

    //! \copydoc shared_library::operator bool() const
    explicit operator bool() const noexcept {
        return is_loaded();
    }

    //! \copydoc shared_library::has(const char* symbol_name) const
    bool has(const char* symbol_name) const noexcept {
        return lib_.has(symbol_name);
    }

    //! \copydoc shared_library::has(const std::string& symbol_name) const
    bool has(const std::string& symbol_name) const noexcept {
        return lib_.has(symbol_name);
    }

    //! \copydoc shared_library::assign(const shared_library& lib)
    smart_library& assign(const smart_library& lib) {
       lib_.assign(lib.lib_);
       storage_.assign(lib.storage_);
       return *this;
    }

    //! \copydoc shared_library::swap(shared_library& rhs)
    void swap(smart_library& rhs) noexcept {
        lib_.swap(rhs.lib_);
        storage_.swap(rhs.storage_);
    }
};

/// Very fast equality check that compares the actual DLL/DSO objects. Throws nothing.
inline bool operator==(const smart_library& lhs, const smart_library& rhs) noexcept {
    return lhs.shared_lib().native() == rhs.shared_lib().native();
}

/// Very fast inequality check that compares the actual DLL/DSO objects. Throws nothing.
inline bool operator!=(const smart_library& lhs, const smart_library& rhs) noexcept {
    return lhs.shared_lib().native() != rhs.shared_lib().native();
}

/// Compare the actual DLL/DSO objects without any guarantee to be stable between runs. Throws nothing.
inline bool operator<(const smart_library& lhs, const smart_library& rhs) noexcept {
    return lhs.shared_lib().native() < rhs.shared_lib().native();
}

/// Swaps two shared libraries. Does not invalidate symbols and functions loaded from libraries. Throws nothing.
inline void swap(smart_library& lhs, smart_library& rhs) noexcept {
    lhs.swap(rhs);
}


#ifdef BOOST_DLL_DOXYGEN
/** Helper functions for overloads.
 *
 * Gets either a variable, function or member-function, depending on the signature.
 *
 * @code
 * smart_library sm("lib.so");
 * get<int>(sm, "space::value"); //import a variable
 * get<void(int)>(sm, "space::func"); //import a function
 * get<some_class, void(int)>(sm, "space::class_::mem_fn"); //import a member function
 * @endcode
 *
 * @param sm A reference to the @ref smart_library
 * @param name The name of the entity to import
 */
template<class T, class T2>
void get(const smart_library& sm, const std::string &name);
#endif

template<class T>
typename std::enable_if<std::is_object<T>::value, T&>::type get(const smart_library& sm, const std::string &name)

{
    return sm.get_variable<T>(name);
}

template<class T>
typename std::enable_if<std::is_function<T>::value, T&>::type get(const smart_library& sm, const std::string &name)
{
    return sm.get_function<T>(name);
}

template<class Class, class Signature>
auto get(const smart_library& sm, const std::string &name) -> typename detail::get_mem_fn_type<Class, Signature>::mem_fn
{
    return sm.get_mem_fn<Class, Signature>(name);
}

BOOST_DLL_END_MODULE_EXPORT


} /* namespace experimental */
} /* namespace dll */
} /* namespace boost */

#endif // !defined(BOOST_USE_MODULES) || defined(BOOST_DLL_INTERFACE_UNIT)

#endif /* BOOST_DLL_SMART_LIBRARY_HPP_ */
