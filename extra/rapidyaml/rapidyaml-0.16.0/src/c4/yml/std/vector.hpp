#ifndef C4_YML_STD_VECTOR_HPP_
#define C4_YML_STD_VECTOR_HPP_

#include "c4/yml/node.hpp"
#include <c4/std/vector.hpp>
#include <vector>

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wuseless-cast")

// vector<T> is a sequence-like type, and it requires child nodes
// in the data tree hierarchy (a SEQ node in ryml parlance).
// So it should be serialized via write()/read().

/** serialize a vector to a node: implementation for @ref Tree */
template<class T, class Alloc>
void write(Tree *tree, id_type id, std::vector<T, Alloc> const& vec)
{
    tree->set_seq(id);
    for(T const& val : vec)
        tree->set_serialized(tree->append_child(id), val);
}

/** serialize a vector to a node: implementation for @ref NodeRef */
template<class T, class Alloc>
void write(NodeRef *n, std::vector<T, Alloc> const& vec)
{
    // don't defer to the tree impl here (if that were the case, we
    // wouldn't even need to implement this function, as ryml ends up
    // calling the tree impl by default). It may be the case that the
    // serialization of T uses a serialization of a nested type that
    // is not available in the tree implementation, so we need to
    // provide this as a hook for that. If that's not required, then
    // the tree implementation will get called anyway.
    n->set_seq();
    for(T const& val : vec)
    {
        NodeRef ch = n->append_child();
        // call write() directly. ch.set_serialized() would also work,
        // but does an additional check (to create the node if it is a
        // seed). That check is not needed here because we used
        // .append_child() to create the node.
        write(&ch, val);
    }
}


/** deserialize from a @ref Tree node, overwriting existing vector entries. */
template<class T, class Alloc>
ReadResult read(Tree const *tree, id_type id, std::vector<T, Alloc> *vec)
{
    if C4_UNLIKELY(!tree->is_seq(id))
        return ReadResult(id);
    vec->clear();
    #if C4_CPP < 17 // prior to C++17, emplace_back() does not return a reference
    size_t pos = 0;
    #endif
    for(id_type child = tree->first_child(id); child != NONE; child = tree->next_sibling(child))
    {
        #if C4_CPP >= 17
        T &val = vec->emplace_back();
        #else
        vec->emplace_back();
        T &val = (*vec)[pos++];
        #endif
        // do not call read(tree, child, &val) here. we do not know
        // that the YAML is valid, so we need to do the checks in
        // child.deserialize()
        if(!tree->deserialize(child, &val))
            return ReadResult(id);
    }
    return ReadResult{};
}

/** deserialize from a @ref ConstNodeRef node, overwriting existing vector entries. */
template<class T, class Alloc>
ReadResult read(ConstNodeRef const& node, std::vector<T, Alloc> *vec)
{
    // don't defer to the tree impl here (if that were the case, we
    // wouldn't even need to implement this function, as ryml ends up
    // calling the tree impl by default). It may be the case that the
    // serialization of T uses a serialization of a nested type that
    // is not available in the tree implementation, so we need to
    // provide this as a hook for that. If that's not required, then
    // the tree implementation will get called anyway.
    if C4_UNLIKELY(!node.is_seq())
        return ReadResult(node.id());
    vec->clear();
    #if C4_CPP < 17 // prior to C++17, emplace_back() does not return a reference
    size_t pos = 0;
    #endif
    for(ConstNodeRef const& child : node.children())
    {
        #if C4_CPP >= 17
        T &val = vec->emplace_back();
        #else
        vec->emplace_back();
        T &val = (*vec)[pos++];
        #endif
        // do not call read(child, &val) here. we do not know that the
        // YAML is valid, so we need to do the checks in
        // child.deserialize()
        if(!child.deserialize(&val))
            return ReadResult(child.id());
    }
    return ReadResult();
}



//-----------------------------------------------------
// specialization for std::vector<bool>. needed because the return
// type of its operator[] is not bool, but std::vector<bool>::reference

/** deserialize from a @ref Tree node, overwriting existing vector entries. */
template<class Alloc>
ReadResult read(Tree const* tree, id_type id, std::vector<bool, Alloc> *vec)
{
    if C4_UNLIKELY(!tree->is_seq(id))
        return ReadResult(id);
    vec->clear();
    bool tmp = {};
    for(id_type child = tree->first_child(id); child != NONE; child = tree->next_sibling(child))
    {
        if(!from_chars(tree->val(child), &tmp))
            return ReadResult(child);
        vec->push_back(tmp); // leave this. gcc4.8 does not have std::vector<bool>::emplace_back()
    }
    return ReadResult();
}

/** deserialize from a @ref ConstNodeRef node, overwriting existing vector entries. */
template<class Alloc>
ReadResult read(ConstNodeRef const& n, std::vector<bool, Alloc> *vec)
{
    // call the tree implementation, saving the node readability
    // checks. we can do that here because bool (the value type) is a
    // final type, so it can't be overrided.
    return read(n.tree(), n.id(), vec);
}

C4_SUPPRESS_WARNING_GCC_POP

} // namespace yml
} // namespace c4

#endif // C4_YML_STD_VECTOR_HPP_
