// Copyright David Abrahams 2002.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef MODULE_INIT_DWA20020722_HPP
# define MODULE_INIT_DWA20020722_HPP

# include <boost/python/detail/prefix.hpp>
# include <boost/preprocessor/cat.hpp>
# include <boost/preprocessor/stringize.hpp>

# if PY_VERSION_HEX >= 0x03050000
#  include <boost/static_assert.hpp>
#  include <boost/type_traits/is_pod.hpp>
# endif

# ifndef BOOST_PYTHON_MODULE_INIT

namespace boost { namespace python {

#  ifdef HAS_CXX11
// Use to activate the Py_MOD_GIL_NOT_USED flag.
class mod_gil_not_used {
public:
    explicit mod_gil_not_used(bool flag = true) : flag_(flag) {}
    bool flag() const { return flag_; }

private:
    bool flag_;
};

namespace detail {

inline bool gil_not_used_option() { return false; }
template <typename F, typename... O>
bool gil_not_used_option(F &&, O &&...o);
template <typename... O>
inline bool gil_not_used_option(mod_gil_not_used f, O &&...o) {
    return f.flag() || gil_not_used_option(o...);
}
template <typename F, typename... O>
inline bool gil_not_used_option(F &&, O &&...o) {
    return gil_not_used_option(o...);
}

}
#  endif // HAS_CXX11

namespace detail {

#  if PY_VERSION_HEX >= 0x03000000

BOOST_PYTHON_DECL PyObject* init_module(PyModuleDef&, void(*)(), bool gil_not_used = false);

#   if PY_VERSION_HEX >= 0x03050000

BOOST_PYTHON_DECL int exec_module(PyObject*, void(*)());

BOOST_PYTHON_DECL int exec_module_with_state(PyObject*, void(*)(void*));

#   endif // PY_VERSION_HEX >= 0x03050000

#  else // PY_VERSION_HEX >= 0x03000000

BOOST_PYTHON_DECL PyObject* init_module(char const* name, void(*)());

#  endif // PY_VERSION_HEX >= 0x03000000

}}}

#  if PY_VERSION_HEX >= 0x03000000

#   ifdef HAS_CXX11
#    define _BOOST_PYTHON_MODULE_INIT(name, ...) \
  PyObject* BOOST_PP_CAT(PyInit_, name)()  \
  { \
    static PyModuleDef_Base initial_m_base = { \
        PyObject_HEAD_INIT(NULL) \
        0, /* m_init */ \
        0, /* m_index */ \
        0 /* m_copy */ };  \
    static PyMethodDef initial_methods[] = { { 0, 0, 0, 0 } }; \
 \
    static struct PyModuleDef moduledef = { \
        initial_m_base, \
        BOOST_PP_STRINGIZE(name), \
        0, /* m_doc */ \
        -1, /* m_size */ \
        initial_methods, \
        0,  /* m_reload */ \
        0, /* m_traverse */ \
        0, /* m_clear */ \
        0,  /* m_free */ \
    }; \
 \
    return boost::python::detail::init_module( \
        moduledef, BOOST_PP_CAT(init_module_, name), \
        boost::python::detail::gil_not_used_option(__VA_ARGS__) ); \
  } \
  void BOOST_PP_CAT(init_module_, name)()

#   else // !HAS_CXX11
#    define _BOOST_PYTHON_MODULE_INIT(name) \
  PyObject* BOOST_PP_CAT(PyInit_, name)()  \
  { \
    static PyModuleDef_Base initial_m_base = { \
        PyObject_HEAD_INIT(NULL) \
        0, /* m_init */ \
        0, /* m_index */ \
        0 /* m_copy */ };  \
    static PyMethodDef initial_methods[] = { { 0, 0, 0, 0 } }; \
 \
    static struct PyModuleDef moduledef = { \
        initial_m_base, \
        BOOST_PP_STRINGIZE(name), \
        0, /* m_doc */ \
        -1, /* m_size */ \
        initial_methods, \
        0,  /* m_reload */ \
        0, /* m_traverse */ \
        0, /* m_clear */ \
        0,  /* m_free */ \
    }; \
 \
    return boost::python::detail::init_module( \
        moduledef, BOOST_PP_CAT(init_module_, name) ); \
  } \
  void BOOST_PP_CAT(init_module_, name)()
#   endif // HAS_CXX11

#   if PY_VERSION_HEX >= 0x03050000

#    if defined(HAS_CXX11) && (PY_VERSION_HEX >= 0x030D0000)
#     define _BOOST_PYTHON_MODULE_MULTI_PHASE_INIT(name, ...) \
  int BOOST_PP_CAT(exec_module_,name)(PyObject* mod) \
  { \
    return boost::python::detail::exec_module( \
        mod, BOOST_PP_CAT(init_module_, name) ); \
  } \
  extern "C" BOOST_SYMBOL_EXPORT PyObject* BOOST_PP_CAT(PyInit_, name)()  \
  { \
    static PyModuleDef_Base initial_m_base = { \
        PyObject_HEAD_INIT(NULL) \
        0, /* m_init */ \
        0, /* m_index */ \
        0 /* m_copy */ };  \
    static PyMethodDef initial_methods[] = { { 0, 0, 0, 0 } }; \
 \
    static PyModuleDef_Slot slots[] = { \
        {Py_mod_exec, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(BOOST_PP_CAT(exec_module_, name)))}, \
        {Py_mod_gil, boost::python::detail::gil_not_used_option(__VA_ARGS__) ? Py_MOD_GIL_NOT_USED : Py_MOD_GIL_USED}, \
        {0, NULL} \
    }; \
 \
    static struct PyModuleDef moduledef = { \
        initial_m_base, \
        BOOST_PP_STRINGIZE(name), \
        0, /* m_doc */ \
        0, /* m_size */ \
        initial_methods, \
        slots,  /* m_slots */ \
        0, /* m_traverse */ \
        0, /* m_clear */ \
        0,  /* m_free */ \
    }; \
 \
    return PyModuleDef_Init(&moduledef); \
  } \
  void BOOST_PP_CAT(init_module_, name)()
#     define _BOOST_PYTHON_MODULE_WITH_STATE_INIT(name, StateType, ...) \
  int BOOST_PP_CAT(exec_module_,name)(PyObject* mod) \
  { \
    return boost::python::detail::exec_module_with_state( \
        mod, reinterpret_cast<void(*)(void*)>(BOOST_PP_CAT(init_module_, name)) ); \
  } \
  extern "C" BOOST_SYMBOL_EXPORT PyObject* BOOST_PP_CAT(PyInit_, name)()  \
  { \
    BOOST_STATIC_ASSERT_MSG(boost::is_pod<StateType>::value, \
        "Module State MUST be a plain POD structure!"); \
    static PyModuleDef_Base initial_m_base = { \
        PyObject_HEAD_INIT(NULL) \
        0, /* m_init */ \
        0, /* m_index */ \
        0 /* m_copy */ };  \
    static PyMethodDef initial_methods[] = { { 0, 0, 0, 0 } }; \
 \
    static PyModuleDef_Slot slots[] = { \
        {Py_mod_exec, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(BOOST_PP_CAT(exec_module_, name)))}, \
        {Py_mod_gil, boost::python::detail::gil_not_used_option(__VA_ARGS__) ? Py_MOD_GIL_NOT_USED : Py_MOD_GIL_USED}, \
        {0, NULL} \
    }; \
 \
    static struct PyModuleDef moduledef = { \
        initial_m_base, \
        BOOST_PP_STRINGIZE(name), \
        0, /* m_doc */ \
        sizeof(StateType), /* m_size */ \
        initial_methods, \
        slots,  /* m_slots */ \
        0, /* m_traverse */ \
        0, /* m_clear */ \
        0,  /* m_free */ \
    }; \
 \
    return PyModuleDef_Init(&moduledef); \
  } \
  void BOOST_PP_CAT(init_module_, name)(StateType* state)
#    else // ! HAS_CXX11 && Python 3.13+
#     define _BOOST_PYTHON_MODULE_MULTI_PHASE_INIT(name) \
  int BOOST_PP_CAT(exec_module_,name)(PyObject* mod) \
  { \
    return boost::python::detail::exec_module( \
        mod, BOOST_PP_CAT(init_module_, name) ); \
  } \
  extern "C" BOOST_SYMBOL_EXPORT PyObject* BOOST_PP_CAT(PyInit_, name)()  \
  { \
    static PyModuleDef_Base initial_m_base = { \
        PyObject_HEAD_INIT(NULL) \
        0, /* m_init */ \
        0, /* m_index */ \
        0 /* m_copy */ };  \
    static PyMethodDef initial_methods[] = { { 0, 0, 0, 0 } }; \
 \
    static PyModuleDef_Slot slots[] = { \
        {Py_mod_exec, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(BOOST_PP_CAT(exec_module_, name)))}, \
        {0, NULL} \
    }; \
 \
    static struct PyModuleDef moduledef = { \
        initial_m_base, \
        BOOST_PP_STRINGIZE(name), \
        0, /* m_doc */ \
        0, /* m_size */ \
        initial_methods, \
        slots,  /* m_slots */ \
        0, /* m_traverse */ \
        0, /* m_clear */ \
        0,  /* m_free */ \
    }; \
 \
    return PyModuleDef_Init(&moduledef); \
  } \
  void BOOST_PP_CAT(init_module_, name)()
#     define _BOOST_PYTHON_MODULE_WITH_STATE_INIT(name, StateType) \
  int BOOST_PP_CAT(exec_module_,name)(PyObject* mod) \
  { \
    return boost::python::detail::exec_module_with_state( \
        mod, reinterpret_cast<void(*)(void*)>(BOOST_PP_CAT(init_module_, name)) ); \
  } \
  extern "C" BOOST_SYMBOL_EXPORT PyObject* BOOST_PP_CAT(PyInit_, name)()  \
  { \
    BOOST_STATIC_ASSERT_MSG(boost::is_pod<StateType>::value, \
        "Module State MUST be a plain POD structure!"); \
    static PyModuleDef_Base initial_m_base = { \
        PyObject_HEAD_INIT(NULL) \
        0, /* m_init */ \
        0, /* m_index */ \
        0 /* m_copy */ };  \
    static PyMethodDef initial_methods[] = { { 0, 0, 0, 0 } }; \
 \
    static PyModuleDef_Slot slots[] = { \
        {Py_mod_exec, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(BOOST_PP_CAT(exec_module_, name)))}, \
        {0, NULL} \
    }; \
 \
    static struct PyModuleDef moduledef = { \
        initial_m_base, \
        BOOST_PP_STRINGIZE(name), \
        0, /* m_doc */ \
        sizeof(StateType), /* m_size */ \
        initial_methods, \
        slots,  /* m_slots */ \
        0, /* m_traverse */ \
        0, /* m_clear */ \
        0,  /* m_free */ \
    }; \
 \
    return PyModuleDef_Init(&moduledef); \
  } \
  void BOOST_PP_CAT(init_module_, name)(StateType* state)
#    endif // HAS_CXX11 && Python 3.13+

#   endif // PY_VERSION_HEX >= 0x03050000

#  else // ! PY_VERSION_HEX >= 0x03000000

#   define _BOOST_PYTHON_MODULE_INIT(name)              \
  void BOOST_PP_CAT(init,name)()                        \
{                                                       \
    boost::python::detail::init_module(                 \
        BOOST_PP_STRINGIZE(name),&BOOST_PP_CAT(init_module_,name)); \
}                                                       \
  void BOOST_PP_CAT(init_module_,name)()

#  endif // PY_VERSION_HEX >= 0x03000000

#  if defined(HAS_CXX11) && (PY_VERSION_HEX >= 0x03000000)
#   define BOOST_PYTHON_MODULE_INIT(name, ...)                  \
  void BOOST_PP_CAT(init_module_,name)();                      \
extern "C" BOOST_SYMBOL_EXPORT _BOOST_PYTHON_MODULE_INIT(name, __VA_ARGS__)
#  else
#   define BOOST_PYTHON_MODULE_INIT(name)                       \
  void BOOST_PP_CAT(init_module_,name)();                      \
extern "C" BOOST_SYMBOL_EXPORT _BOOST_PYTHON_MODULE_INIT(name)
#  endif // HAS_CXX11 && Python 3

#  if PY_VERSION_HEX >= 0x03050000
#   if defined(HAS_CXX11) && (PY_VERSION_HEX >= 0x030D0000)
#    define BOOST_PYTHON_MODULE_MULTI_PHASE_INIT(name, ...)          \
  void BOOST_PP_CAT(init_module_,name)();                      \
extern "C" BOOST_SYMBOL_EXPORT _BOOST_PYTHON_MODULE_MULTI_PHASE_INIT(name, __VA_ARGS__)
#    define BOOST_PYTHON_MODULE_WITH_STATE_INIT(name, StateType, ...)          \
  void BOOST_PP_CAT(init_module_,name)(StateType* state);                      \
extern "C" BOOST_SYMBOL_EXPORT _BOOST_PYTHON_MODULE_WITH_STATE_INIT(name, StateType, __VA_ARGS__)
#   else
#    define BOOST_PYTHON_MODULE_MULTI_PHASE_INIT(name)          \
  void BOOST_PP_CAT(init_module_,name)();                      \
extern "C" BOOST_SYMBOL_EXPORT _BOOST_PYTHON_MODULE_MULTI_PHASE_INIT(name)
#    define BOOST_PYTHON_MODULE_WITH_STATE_INIT(name, StateType)          \
  void BOOST_PP_CAT(init_module_,name)(StateType* state);                      \
extern "C" BOOST_SYMBOL_EXPORT _BOOST_PYTHON_MODULE_WITH_STATE_INIT(name, StateType)
#   endif // HAS_CXX11 && Python 3.13+
#  endif // PY_VERSION_HEX >= 0x03050000

# endif // BOOST_PYTHON_MODULE_INIT

#endif // MODULE_INIT_DWA20020722_HPP
