//  (C) Copyright Joel de Guzman 2003.
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef VECTOR_INDEXING_SUITE_JDG20036_HPP
# define VECTOR_INDEXING_SUITE_JDG20036_HPP

# include <boost/python/suite/indexing/indexing_suite.hpp>
# include <boost/python/suite/indexing/container_utils.hpp>
# include <boost/python/iterator.hpp>

namespace boost { namespace python {
            
    // Forward declaration
    template <class Container, bool NoProxy, class DerivedPolicies>
    class vector_indexing_suite;
    
    namespace detail
    {
        template <class Container, bool NoProxy>
        class final_vector_derived_policies 
            : public vector_indexing_suite<Container, 
                NoProxy, final_vector_derived_policies<Container, NoProxy> > {};
    }

    // The vector_indexing_suite class is a predefined indexing_suite derived 
    // class for wrapping std::vector (and std::vector like) classes. It provides
    // all the policies required by the indexing_suite (see indexing_suite).
    // Example usage:
    //
    //  class X {...};
    //
    //  ...
    //
    //      class_<std::vector<X> >("XVec")
    //          .def(vector_indexing_suite<std::vector<X> >())
    //      ;
    //
    // By default indexed elements are returned by proxy. This can be
    // disabled by supplying *true* in the NoProxy template parameter.
    //
    template <
        class Container, 
        bool NoProxy = false,
        class DerivedPolicies 
            = detail::final_vector_derived_policies<Container, NoProxy> >
    class vector_indexing_suite 
        : public indexing_suite<Container, DerivedPolicies, NoProxy>
    {
    public:
    
        typedef typename Container::value_type data_type;
        typedef typename Container::value_type key_type;
        typedef typename Container::size_type index_type;
        typedef typename Container::size_type size_type;
        typedef typename Container::difference_type difference_type;
        
        template <class Class>
        static void 
        extension_def(Class& cl)
        {
            cl
                .def("__iadd__", &base_iadd)
                .def("append", &base_append)
                .def("count", &base_count)
                .def("extend", &base_extend)
                .def("remove", &base_remove)
                .def("reverse", &base_reverse)
            ;
        }
        
        static 
        typename mpl::if_<
            is_class<data_type>
          , data_type&
          , data_type
        >::type
        get_item(Container& container, index_type i)
        { 
            return container[i];
        }

        static object 
        get_slice(Container& container, index_type from, index_type to)
        { 
            if (from > to)
                return object(Container());
            return object(Container(container.begin()+from, container.begin()+to));
        }

        static void 
        set_item(Container& container, index_type i, data_type const& v)
        { 
            container[i] = v;
        }

        static void 
        set_slice(Container& container, index_type from, 
            index_type to, data_type const& v)
        { 
            if (from > to) {
                return;
            }
            else {
                container.erase(container.begin()+from, container.begin()+to);
                container.insert(container.begin()+from, v);
            }
        }

        template <class Iter>
        static void 
        set_slice(Container& container, index_type from, 
            index_type to, Iter first, Iter last)
        { 
            if (from > to) {
                container.insert(container.begin()+from, first, last);
            }
            else {
                container.erase(container.begin()+from, container.begin()+to);
                container.insert(container.begin()+from, first, last);
            }
        }

        static void 
        delete_item(Container& container, index_type i)
        { 
            container.erase(container.begin()+i);
        }
        
        static void 
        delete_slice(Container& container, index_type from, index_type to)
        { 
            if (from > to) {
                // A null-op.
                return;
            }
            container.erase(container.begin()+from, container.begin()+to);
        }
        
        static void
        clear(Container& container)
        {
            container.clear();
        }
        
        static size_t
        size(Container& container)
        {
            return container.size();
        }
        
        static bool
        contains(Container& container, key_type const& key)
        {
            return std::find(container.begin(), container.end(), key)
                != container.end();
        }
        
        static index_type
        get_min_index(Container& /*container*/)
        { 
            return 0;
        }

        static index_type
        get_max_index(Container& container)
        { 
            return container.size();
        }
      
        static bool 
        compare_index(Container& /*container*/, index_type a, index_type b)
        {
            return a < b;
        }
        
        static index_type
        convert_index(Container& container, PyObject* i_)
        { 
            extract<long> i(i_);
            if (i.check())
            {
                long index = i();
                if (index < 0)
                    index += DerivedPolicies::size(container);
                if (index >= long(container.size()) || index < 0)
                {
                    PyErr_SetString(PyExc_IndexError, "Index out of range");
                    throw_error_already_set();
                }
                return index;
            }
            
            PyErr_SetString(PyExc_TypeError, "Invalid index type");
            throw_error_already_set();
            return index_type();
        }
      
        static void 
        append(Container& container, data_type const& v)
        { 
            container.push_back(v);
        }
        
        template <class Iter>
        static void 
        extend(Container& container, Iter first, Iter last)
        { 
            container.insert(container.end(), first, last);
        }
        
    private:

        static size_t
        base_count(Container& container, object v)
        {
            extract<data_type&> elem(v);
            if (elem.check()) {
                return std::count(container.begin(), container.end(), elem());
            } else {
                extract<data_type> elem(v);
                if (!elem.check()) {
                    return 0;
                }
                return std::count(container.begin(), container.end(), elem());
            }
        }
    
        static void
        base_append(Container& container, object v)
        {
            extract<data_type&> elem(v);
            // try if elem is an exact Data
            if (elem.check())
            {
                DerivedPolicies::append(container, elem());
            }
            else
            {
                //  try to convert elem to data_type
                extract<data_type> elem(v);
                if (elem.check())
                {
                    DerivedPolicies::append(container, elem());
                }
                else
                {
                    PyErr_SetString(PyExc_TypeError, 
                        "Attempting to append an invalid type");
                    throw_error_already_set();
                }
            }
        }
        
        static void
        base_extend(Container& container, object v)
        {
            std::vector<data_type> temp;
            container_utils::extend_container(temp, v);
            DerivedPolicies::extend(container, temp.begin(), temp.end());
        }

        static object
        base_iadd(Container& container, object v)
        {
            base_extend(container, v);
            return object(container);
        }

        static void
        base_remove(Container& container, object v)
        {
            extract<data_type&> key(v);
            if (key.check())
            {
                auto i = std::find(container.begin(), container.end(), key());
                if (i == container.end())
                {
                    PyErr_SetString(PyExc_ValueError, "remove(x): x not in vector_indexing_suite");
                    throw_error_already_set();
                }
                container.erase(i);
            }
            else
            {
                extract<data_type> key(v);
                if (key.check())
                {
                    auto i = std::find(container.begin(), container.end(), key());
                    if (i == container.end())
                    {
                        PyErr_SetString(PyExc_ValueError, "remove(x): x not in vector_indexing_suite");
                        throw_error_already_set();
                    }
                    container.erase(i);
                }
                else
                {
                    PyErr_SetString(PyExc_TypeError,
                        "Attempting to remove an invalid type");
                    throw_error_already_set();
                }
            }
        }

        static void
        base_reverse(Container& container)
        {
            using std::swap;
            const unsigned n = size(container);
            for (unsigned i = 0; i < n / 2; i++)
                swap(container[i], container[n - i - 1]);
        }
    };
       
}} // namespace boost::python 

#endif // VECTOR_INDEXING_SUITE_JDG20036_HPP
