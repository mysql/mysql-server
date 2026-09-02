#ifndef C4_YML_STD_MAP_HPP_
#define C4_YML_STD_MAP_HPP_

/** @file map.hpp write/read std::map to/from a YAML tree. */

#include "c4/yml/node.hpp"
#include <map>

namespace c4 {
namespace yml {

// std::map requires child nodes in the data
// tree hierarchy (a MAP node in ryml parlance).
// So it should be serialized via write()/read().

/** serialize a map to a node: implementation for @ref Tree */
template<class K, class V, class Less, class Alloc>
void write(c4::yml::Tree *tree, c4::yml::id_type id, std::map<K, V, Less, Alloc> const& m)
{
    tree->set_map(id);
    for(auto const& C4_RESTRICT p : m)
    {
        id_type child = tree->append_child(id);
        tree->set_key_serialized(child, p.first);
        tree->set_serialized(child, p.second);
    }
}

/** serialize a map to a node: implementation for @ref NodeRef */
template<class K, class V, class Less, class Alloc>
C4_ALWAYS_INLINE void write(c4::yml::NodeRef *n, std::map<K, V, Less, Alloc> const& m)
{
    n->set_map();
    for(auto const& C4_RESTRICT p : m)
    {
        NodeRef ch = n->append_child();
        ch.set_key_serialized(p.first);
        ch.set_serialized(p.second);
    }
}


/** deserialize a map from a node: implementation for @ref Tree .
 * Read the node members, assigning into the existing map. If a key is
 * already present in the map, then its value will be
 * move-assigned. The map */
template<class K, class V, class Less, class Alloc>
ReadResult read(c4::yml::Tree const* tree, c4::yml::id_type id, std::map<K, V, Less, Alloc> * m)
{
    if C4_UNLIKELY(!tree->is_map(id))
        return ReadResult(id);
    for(id_type child = tree->first_child(id); child != NONE; child = tree->next_sibling(child))
    {
        K k{};
        ReadResult result = tree->deserialize_key(child, &k);
        if C4_UNLIKELY(!result)
            return result;
        result = tree->deserialize(child, &(*m)[std::move(k)]);
        if C4_UNLIKELY(!result)
            return result;
    }
    return ReadResult();
}

/** deserialize a map from a node: implementation for @ref ConstNodeRef . read
 * the node members, assigning into the existing map. If a key is
 * already present in the map, then its value will be
 * move-assigned. */
template<class K, class V, class Less, class Alloc>
ReadResult read(c4::yml::ConstNodeRef const& n, std::map<K, V, Less, Alloc> * m)
{
    if C4_UNLIKELY(!n.is_map())
        return ReadResult(n.id());
    for(ConstNodeRef const& C4_RESTRICT ch : n)
    {
        K k{};
        ReadResult result = ch.deserialize_key(&k);
        if C4_UNLIKELY(!result)
            return result;
        result = ch.deserialize(&(*m)[std::move(k)]);
        if C4_UNLIKELY(!result)
            return result;
    }
    return ReadResult();
}

} // namespace yml
} // namespace c4

#endif // C4_YML_STD_MAP_HPP_
