//  Copyright 2016 Klemens Morgenstern
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_DLL_DETAIL_DEMANGLING_MSVC_HPP_
#define BOOST_DLL_DETAIL_DEMANGLING_MSVC_HPP_

#if !defined(BOOST_DLL_INTERFACE_UNIT)
#include <boost/core/detail/string_view.hpp>

#if !defined(BOOST_DLL_USE_STD_MODULE)
#include <iterator>
#include <algorithm>
#include <type_traits>
#endif // !defined(BOOST_DLL_USE_STD_MODULE)
#endif // !defined(BOOST_DLL_INTERFACE_UNIT)

#include <boost/dll/detail/demangling/mangled_storage_base.hpp>

namespace boost { namespace dll { namespace detail {

class mangled_storage_impl  : public mangled_storage_base
{
    template<typename T>
    struct dummy {};

    template<typename Return, typename ...Args>
    std::string get_return_type(dummy<Return(Args...)>) const
    {
        return get_name<Return>();
    }
    //function to remove preceding 'class ' or 'struct ' if the are given in this format.

    inline static void trim_typename(std::string & val);
public:
    using ctor_sym = std::string;
    using dtor_sym = std::string;

    using mangled_storage_base::mangled_storage_base;

    template<typename T>
    std::string get_variable(const std::string &name) const;

    template<typename Func>
    std::string get_function(const std::string &name) const;

    template<typename Class, typename Func>
    std::string get_mem_fn(const std::string &name) const;

    template<typename Signature>
    ctor_sym get_constructor() const;

    template<typename Class>
    dtor_sym get_destructor() const;

    template<typename T> //overload, does not need to virtual.
    std::string get_name() const
    {
        auto nm = mangled_storage_base::get_name<T>();
        trim_typename(nm);
        return nm;
    }

    template<typename T>
    std::string get_vtable() const;

    template<typename T>
    std::vector<std::string> get_related() const;

};

void mangled_storage_impl::trim_typename(std::string & val)
{
    //remove preceding class or struct, because you might want to use a struct as class, et vice versa
    if (val.size() >= 6)
    {
        using namespace std;
        static constexpr char class_ [7] = "class ";
        static constexpr char struct_[8] = "struct ";

        if (equal(begin(class_), end(class_)-1, val.begin()))
            val.erase(0, 6);
        else if (val.size() >= 7)
            if (equal(begin(struct_), end(struct_)-1, val.begin()))
                val.erase(0, 7);
    }
}

namespace parser {

    inline bool try_consume_prefix(boost::core::string_view& s, boost::core::string_view prefix) {
        const bool result = s.starts_with(prefix);
        if (result) {
            s.remove_prefix(prefix.size());
        }
        return result;
    }

    inline bool ignore_prefix(boost::core::string_view& s, boost::core::string_view prefix) {
        parser::try_consume_prefix(s, prefix);
        return true;
    }

    inline void consume_ptrs(boost::core::string_view& s) {
        do {
            while (parser::try_consume_prefix(s, " ")) {}
        } while (parser::try_consume_prefix(s, "__ptr32") || parser::try_consume_prefix(s, "__ptr64"));
    }

    inline bool ignore_ptrs(boost::core::string_view& s) {
        parser::consume_ptrs(s);
        return true;
    }

    inline bool try_consume_visibility(boost::core::string_view& s) {
        return parser::try_consume_prefix(s, "public:")
            || parser::try_consume_prefix(s, "protected:")
            || parser::try_consume_prefix(s, "private:");
    }

    template<typename T>
    bool try_consume_type(boost::core::string_view& s, const mangled_storage_impl& ms) {
        if (std::is_void<T>::value) {
            return parser::try_consume_prefix(s, "void");
        }

        parser::ignore_prefix(s, "class ");
        parser::ignore_prefix(s, "struct ");

        const auto& mangled_name = ms.get_name<T>();

        static_assert(
            !std::is_function<typename std::remove_pointer<T>::type>::value,
            "boost::dll::smart_library on Windows platform does not support "
            "functions that accept functions. If you wish to see such support "
            "- please provide a working PR on github with sufficient tests. "
            "Otherwise simplify the function. For example, use `void*` "
            "parameter instead of a function pointer. "
        );
        if (!parser::try_consume_prefix(s, mangled_name)) {
            return false;
        }

        if (std::is_const<typename std::remove_reference<T>::type>::value) {
            if (!parser::try_consume_prefix(s, " const")) {
                return false;
            }
        }

        if (std::is_volatile<typename std::remove_reference<T>::type>::value) {
            if (!parser::try_consume_prefix(s, " volatile")) {
                return false;
            }
        }

        if (std::is_rvalue_reference<T>::value) {
            if (!parser::try_consume_prefix(s, " &&")) {
                return false;
            }
        }

        if (std::is_lvalue_reference<T>::value) {
            if (!parser::try_consume_prefix(s, " &")) {
                return false;
            }
        }

        return parser::ignore_ptrs(s);
    }

    inline bool try_consume_thiscall(boost::core::string_view& s) {
        parser::try_consume_prefix(s, " ");
        return parser::try_consume_prefix(s, "__cdecl ")         // Win 64bit
            || parser::try_consume_prefix(s, "__thiscall ");     // Win 32bit
    }

    template<typename Return, typename Arg>
    bool try_consume_arg_list(boost::core::string_view& s, const mangled_storage_impl& ms, Return (*)(Arg)) {
        return parser::try_consume_type<Arg>(s, ms);
    }

    template<typename Return, typename First, typename Second, typename ...Args>
    bool try_consume_arg_list(boost::core::string_view& s, const mangled_storage_impl& ms, Return (*)(First, Second, Args...)) {
        using next_type = Return (*)(Second, Args...);
        return parser::try_consume_type<First>(s, ms)
            && parser::try_consume_prefix(s, ",")
            && parser::try_consume_arg_list(s, ms, next_type());
    }

    template<typename Return>
    bool try_consume_arg_list(boost::core::string_view& s, const mangled_storage_impl& ms, Return (*)()) {
        return parser::try_consume_type<void>(s, ms);
    }

    class is_destructor_with_name {
        const std::string& dtor_name_;

    public:
        explicit is_destructor_with_name(const std::string& dtor_name)
            : dtor_name_(dtor_name) {}

        inline bool operator()(boost::core::string_view s) const {
            return parser::try_consume_visibility(s)
                && parser::ignore_prefix(s, " virtual")
                && parser::try_consume_thiscall(s)
                && parser::try_consume_prefix(s, dtor_name_)
                && parser::ignore_ptrs(s)
                && s.empty();
        }

        inline bool operator()(const mangled_storage_base::entry& e) const {
            return (*this)(boost::core::string_view(e.demangled.data(), e.demangled.size()));
        }
    };

    template<typename T>
    class is_variable_with_name {
        const std::string& variable_name_;
        const mangled_storage_impl& ms_;

    public:
        is_variable_with_name(const std::string& variable_name, const mangled_storage_impl& ms)
            : variable_name_(variable_name), ms_(ms) {}

        inline bool operator()(boost::core::string_view s) const {
            if (parser::try_consume_visibility(s) && !parser::try_consume_prefix(s, " static ")) {
                return false;
            }

            return parser::try_consume_type<T>(s, ms_)
                && parser::try_consume_prefix(s, variable_name_)
                && s.empty();
        }

        inline bool operator()(const mangled_storage_base::entry& e) const {
            return (*this)(boost::core::string_view(e.demangled.data(), e.demangled.size()));
        }
    };

    template <class Signature>
    class is_constructor_with_name {
        const std::string& ctor_name_;
        const mangled_storage_impl& ms_;

    public:
        is_constructor_with_name(const std::string& ctor_name, const mangled_storage_impl& ms)
            : ctor_name_(ctor_name), ms_(ms) {}

        inline bool operator()(boost::core::string_view s) const {
            return parser::try_consume_visibility(s)
                && parser::try_consume_thiscall(s)
                && parser::try_consume_prefix(s, ctor_name_)
                && parser::try_consume_prefix(s, "(")
                && parser::try_consume_arg_list(s, ms_, Signature())
                && parser::try_consume_prefix(s, ")")
                && parser::ignore_ptrs(s)
                && s.empty();
        }

        inline bool operator()(const mangled_storage_base::entry& e) const {
            return (*this)(boost::core::string_view(e.demangled.data(), e.demangled.size()));
        }
    };

    template <class Signature>
    class is_function_with_name;

    template <class Result, class... Args>
    class is_function_with_name<Result(*)(Args...)> {
        const std::string& function_name_;
        const mangled_storage_impl& ms_;

    public:
        is_function_with_name(const std::string& function_name, const mangled_storage_impl& ms)
            : function_name_(function_name), ms_(ms) {}

        inline bool operator()(boost::core::string_view s) const {
            if (parser::try_consume_visibility(s) && !parser::try_consume_prefix(s, " static ")) {
                return false;
            }

            using Signature = Result(*)(Args...);
            return parser::try_consume_type<Result>(s,  ms_)
                && parser::ignore_prefix(s, " ")
                && parser::try_consume_prefix(s, "__cdecl ")
                && parser::try_consume_prefix(s, function_name_)
                && parser::try_consume_prefix(s, "(")
                && parser::try_consume_arg_list(s, ms_, Signature())
                && parser::try_consume_prefix(s, ")")
                && parser::ignore_ptrs(s)
                && s.empty();
        }

        inline bool operator()(const mangled_storage_base::entry& e) const {
            return (*this)(boost::core::string_view(e.demangled.data(), e.demangled.size()));
        }
    };

    template <typename Class, typename Func>
    class is_mem_fn_with_name;

    template <typename Class, class Result, class... Args>
    class is_mem_fn_with_name<Class, Result(*)(Args...)> {
        const std::string& function_name_;
        const mangled_storage_impl& ms_;

    public:
        is_mem_fn_with_name(const std::string& function_name, const mangled_storage_impl& ms)
            : function_name_(function_name), ms_(ms) {}

        inline bool operator()(boost::core::string_view s) const {
            using Signature = Result(*)(Args...);
            return parser::try_consume_visibility(s)
                && parser::ignore_prefix(s, " virtual")
                && parser::try_consume_prefix(s, " ")
                && parser::try_consume_type<Result>(s, ms_)
                && parser::try_consume_thiscall(s)
                && parser::try_consume_type<typename std::remove_cv<Class>::type>(s, ms_)
                && parser::try_consume_prefix(s, "::")
                && parser::try_consume_prefix(s, function_name_)
                && parser::try_consume_prefix(s, "(")
                && parser::try_consume_arg_list(s, ms_, Signature())
                && parser::try_consume_prefix(s, ")")
                && (!std::is_const<Class>::value || parser::try_consume_prefix(s, "const "))
                && (!std::is_volatile<Class>::value || parser::try_consume_prefix(s, "volatile "))
                && parser::ignore_ptrs(s)
                && s.empty();
        }

        inline bool operator()(const mangled_storage_base::entry& e) const {
            return (*this)(boost::core::string_view(e.demangled.data(), e.demangled.size()));
        }
    };
}  // namespace parser

template<typename T>
std::string mangled_storage_impl::get_variable(const std::string &name) const {
    const auto found = std::find_if(storage_.begin(), storage_.end(), parser::is_variable_with_name<T>(name, *this));

    if (found != storage_.end())
        return found->mangled;
    else
        return "";
}

template<typename Func>
std::string mangled_storage_impl::get_function(const std::string &name) const {
    const auto found = std::find_if(storage_.begin(), storage_.end(), parser::is_function_with_name<Func*>(name, *this));

    if (found != storage_.end())
        return found->mangled;
    else
        return "";
}

template<typename Class, typename Func>
std::string mangled_storage_impl::get_mem_fn(const std::string &name) const {
    const auto found = std::find_if(storage_.begin(), storage_.end(), parser::is_mem_fn_with_name<Class, Func*>(name, *this));

    if (found != storage_.end())
        return found->mangled;
    else
        return "";
}


template<typename Signature>
auto mangled_storage_impl::get_constructor() const -> ctor_sym {
    std::string ctor_name; // = class_name + "::" + name;
    std::string unscoped_cname; //the unscoped class-name
    {
        auto class_name = get_return_type(dummy<Signature>());
        auto pos = class_name.rfind("::");
        if (pos == std::string::npos)
        {
            ctor_name = class_name+ "::" + class_name ;
            unscoped_cname = class_name;
        }
        else
        {
            unscoped_cname = class_name.substr(pos+2) ;
            ctor_name = class_name+ "::" + unscoped_cname;
        }
    }

    const auto f = std::find_if(storage_.begin(), storage_.end(), parser::is_constructor_with_name<Signature*>(ctor_name, *this));

    if (f != storage_.end())
        return f->mangled;
    else
        return "";
}

template<typename Class>
auto mangled_storage_impl::get_destructor() const -> dtor_sym {
    std::string dtor_name; // = class_name + "::" + name;
    std::string unscoped_cname; //the unscoped class-name
    {
        auto class_name = get_name<Class>();
        auto pos = class_name.rfind("::");
        if (pos == std::string::npos)
        {
            dtor_name = class_name+ "::~" + class_name  + "(void)";
            unscoped_cname = class_name;
        }
        else
        {
            unscoped_cname = class_name.substr(pos+2) ;
            dtor_name = class_name+ "::~" + unscoped_cname + "(void)";
        }
    }

    const auto found = std::find_if(storage_.begin(), storage_.end(), parser::is_destructor_with_name(dtor_name));

    if (found != storage_.end())
        return found->mangled;
    else
        return "";
}

template<typename T>
std::string mangled_storage_impl::get_vtable() const {
    std::string id = "const " + get_name<T>() + "::`vftable'";

    auto predicate = [&](const mangled_storage_base::entry & e)
                {
                    return e.demangled == id;
                };

    auto found = std::find_if(storage_.begin(), storage_.end(), predicate);


    if (found != storage_.end())
        return found->mangled;
    else
        return "";
}

template<typename T>
std::vector<std::string> mangled_storage_impl::get_related() const {
    std::vector<std::string> ret;
    auto name = get_name<T>();

    for (auto & c : storage_)
    {
        if (c.demangled.find(name) != std::string::npos)
            ret.push_back(c.demangled);
    }

    return ret;
}

}}}

#endif /* BOOST_DLL_DETAIL_DEMANGLING_MSVC_HPP_ */
