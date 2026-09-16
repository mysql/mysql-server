#ifndef C4_YML_NODE_HPP_
#define C4_YML_NODE_HPP_

/** @file node.hpp Node classes */

#ifndef C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif

C4_SUPPRESS_WARNING_PUSH
C4_SUPPRESS_WARNING_GCC_CLANG("-Wtype-limits")
C4_SUPPRESS_WARNING_GCC_CLANG("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")
C4_SUPPRESS_WARNING_CLANG("-Wnull-dereference")
#if defined(__GNUC__) && __GNUC__ >= 6
C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
#endif
C4_SUPPRESS_WARNING_MSVC(4251/*needs to have dll-interface to be used by clients of struct*/)
C4_SUPPRESS_WARNING_MSVC(4296/*expression is always 'boolean_value'*/)
C4_SUPPRESS_WARNING_MSVC(4996/*deprecated*/)
// NOLINTBEGIN(modernize-avoid-c-style-cast)


namespace c4 {
namespace yml {

/** @cond dev */
#ifndef __DOXYGEN__
// forward decls
class NodeRef;
class ConstNodeRef;
template<class T> ReadResult read(ConstNodeRef const& C4_RESTRICT n, T *v);
template<class T> ReadResult read_key(ConstNodeRef const& C4_RESTRICT n, T *v);
template<class T> void write(NodeRef *n, T const& v);
template<class T> void write(NodeRef &n, T const& v);
template<class T> void write_key(NodeRef *n, T const& v);
template<class T> void write_key(NodeRef &n, T const& v);
#endif

namespace detail {

template<class NodeRefType>
struct child_iterator
{
    using value_type = NodeRefType;
    using tree_type = typename NodeRefType::tree_type;
    tree_type * C4_RESTRICT m_tree;
    id_type m_child_id;
    child_iterator(tree_type * t, id_type id) noexcept : m_tree(t), m_child_id(id) {}
    child_iterator& operator++ () RYML_NOEXCEPT { RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_child_id != NONE, m_tree, NONE); m_child_id = m_tree->next_sibling(m_child_id); return *this; }
    NodeRefType operator*  () const RYML_NOEXCEPT { return NodeRefType(m_tree, m_child_id); }
    bool operator!= (child_iterator that) const RYML_NOEXCEPT { RYML_ASSERT_VISIT_(m_tree == that.m_tree, m_tree, NONE); return m_child_id != that.m_child_id; }
};

template<class NodeRefType>
struct children_view
{
    using iterator = child_iterator<NodeRefType>;
    using tree_type = typename NodeRefType::tree_type;
    tree_type *tree;
    id_type b, e;
    children_view(tree_type *t, id_type b_, id_type e_) noexcept : tree(t), b(b_), e(e_) {}
    C4_ALWAYS_INLINE iterator begin() const noexcept { return {tree, b}; }
    C4_ALWAYS_INLINE iterator end  () const noexcept { return {tree, e}; }
};

template<class ViewType, class TreeType>
static ViewType make_children_view(TreeType *C4_RESTRICT tree, id_type id) RYML_NOEXCEPT
{
    return ViewType(tree, tree->get(id)->m_first_child, NONE);
}

template<class ViewType, class TreeType>
static ViewType make_siblings_view(TreeType *C4_RESTRICT tree, id_type id) RYML_NOEXCEPT
{
    NodeData const *nd = tree->get(id);
    id_type first = (nd->m_parent != NONE) ? tree->get(nd->m_parent)->m_first_child : NONE;
    return ViewType(tree, first, NONE);
}


// LCOV_EXCL_START
template<class NodeRefType, class Visitor>
RYML_DEPRECATED("") bool _visit(NodeRefType &node, Visitor fn, id_type indentation_level, bool skip_root=false)
{
    id_type increment = 0;
    if( ! (node.is_root() && skip_root))
    {
        if(fn(node, indentation_level))
            return true;
        ++increment;
    }
    if(node.has_children())
    {
        for(auto ch : node.children())
        {
            if(_visit(ch, fn, indentation_level + increment, false)) // no need to forward skip_root as it won't be root
            {
                return true;
            }
        }
    }
    return false;
}

template<class NodeRefType, class Visitor>
RYML_DEPRECATED("") bool _visit_stacked(NodeRefType &node, Visitor fn, id_type indentation_level, bool skip_root=false)
{
    id_type increment = 0;
    if( ! (node.is_root() && skip_root))
    {
        if(fn(node, indentation_level))
        {
            return true;
        }
        ++increment;
    }
    if(node.has_children())
    {
        fn.push(node, indentation_level);
        for(auto ch : node.children())
        {
            if(_visit_stacked(ch, fn, indentation_level + increment, false)) // no need to forward skip_root as it won't be root
            {
                fn.pop(node, indentation_level);
                return true;
            }
        }
        fn.pop(node, indentation_level);
    }
    return false;
}
// LCOV_EXCL_STOP

} // detail
/** @endcond */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @addtogroup doc_node_classes
 *
 * @{
 */


namespace detail {
/** a CRTP base providing read-only methods for @ref ConstNodeRef and @ref NodeRef */
template<class Impl, class ConstImpl>
struct RoNodeMethods // NOLINT
{
    /** @cond dev */
    // CRTP helper macros, undefined at the end
    #define mtree_ ((Impl const* C4_RESTRICT)this)->m_tree
    #define tree_ ((ConstImpl const* C4_RESTRICT)this)->m_tree
    #define id_ ((ConstImpl const* C4_RESTRICT)this)->m_id
    #define this_ ((Impl const* C4_RESTRICT)this)
    #define this_assert_readable_() ((Impl const* C4_RESTRICT)this)->assert_readable_()
    C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wcast-align") // the leading members are aligned
    /** @endcond */

public:

    /** @name node property getters */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE NodeType type()      const RYML_NOEXCEPT { this_assert_readable_(); return tree_->type(id_); }     /**< Forward to @ref Tree::type(). Node must be readable. */

    C4_ALWAYS_INLINE csubstr key()        const RYML_NOEXCEPT { this_assert_readable_(); return tree_->key(id_); }        /**< Forward to @ref Tree::key(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr key_tag()    const RYML_NOEXCEPT { this_assert_readable_(); return tree_->key_tag(id_); }    /**< Forward to @ref Tree::key_tag(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr key_ref()    const RYML_NOEXCEPT { this_assert_readable_(); return tree_->key_ref(id_); }    /**< Forward to @ref Tree::key_ref(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr key_anchor() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->key_anchor(id_); } /**< Forward to @ref Tree::key_anchor(). Node must be readable. */

    C4_ALWAYS_INLINE csubstr val()        const RYML_NOEXCEPT { this_assert_readable_(); return tree_->val(id_); }        /**< Forward to @ref Tree::val(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr val_tag()    const RYML_NOEXCEPT { this_assert_readable_(); return tree_->val_tag(id_); }    /**< Forward to @ref Tree::val_tag(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr val_ref()    const RYML_NOEXCEPT { this_assert_readable_(); return tree_->val_ref(id_); }    /**< Forward to @ref Tree::val_ref(). Node must be readable. */
    C4_ALWAYS_INLINE csubstr val_anchor() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->val_anchor(id_); } /**< Forward to @ref Tree::val_anchor(). Node must be readable. */

    C4_ALWAYS_INLINE NodeScalar const& keysc() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->keysc(id_); } /**< Forward to @ref Tree::keysc(). Node must be readable. */
    C4_ALWAYS_INLINE NodeScalar const& valsc() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->valsc(id_); } /**< Forward to @ref Tree::valsc(). Node must be readable. */

    C4_ALWAYS_INLINE bool key_is_null() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->key_is_null(id_); } /**< Forward to @ref Tree::key_is_null(). Node must be readable. */
    C4_ALWAYS_INLINE bool val_is_null() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->val_is_null(id_); } /**< Forward to @ref Tree::val_is_null(). Node must be readable. */

    C4_ALWAYS_INLINE bool is_key_unfiltered() const noexcept { this_assert_readable_(); return tree_->is_key_unfiltered(id_); } /**< Forward to @ref Tree::is_key_unfiltered(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_unfiltered() const noexcept { this_assert_readable_(); return tree_->is_val_unfiltered(id_); } /**< Forward to @ref Tree::is_val_unfiltered(). Node must be readable. */

    /** @} */

public:

    /** @name node type predicates */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE bool empty()            const RYML_NOEXCEPT { this_assert_readable_(); return tree_->empty(id_); } /**< Forward to @ref Tree::empty(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_stream()        const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_stream(id_); } /**< Forward to @ref Tree::is_stream(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_doc()           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_doc(id_); } /**< Forward to @ref Tree::is_doc(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_container()     const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_container(id_); } /**< Forward to @ref Tree::is_container(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_map()           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_map(id_); } /**< Forward to @ref Tree::is_map(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_seq()           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_seq(id_); } /**< Forward to @ref Tree::is_seq(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_val()          const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_val(id_); } /**< Forward to @ref Tree::has_val(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_key()          const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_key(id_); } /**< Forward to @ref Tree::has_key(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val()           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val(id_); } /**< Forward to @ref Tree::is_val(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_keyval()        const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_keyval(id_); } /**< Forward to @ref Tree::is_keyval(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_key_tag()      const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_key_tag(id_); } /**< Forward to @ref Tree::has_key_tag(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_val_tag()      const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_val_tag(id_); } /**< Forward to @ref Tree::has_val_tag(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_key_anchor()   const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_key_anchor(id_); } /**< Forward to @ref Tree::has_key_anchor(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_val_anchor()   const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_val_anchor(id_); } /**< Forward to @ref Tree::has_val_anchor(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_anchor()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_anchor(id_); } /**< Forward to @ref Tree::has_anchor(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_ref()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_ref(id_); } /**< Forward to @ref Tree::is_key_ref(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_ref()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_ref(id_); } /**< Forward to @ref Tree::is_val_ref(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_ref()           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_ref(id_); } /**< Forward to @ref Tree::is_ref(). Node must be readable. */
    C4_ALWAYS_INLINE bool parent_is_seq()    const RYML_NOEXCEPT { this_assert_readable_(); return tree_->parent_is_seq(id_); } /**< Forward to @ref Tree::parent_is_seq(). Node must be readable. */
    C4_ALWAYS_INLINE bool parent_is_map()    const RYML_NOEXCEPT { this_assert_readable_(); return tree_->parent_is_map(id_); } /**< Forward to @ref Tree::parent_is_map(). Node must be readable. */

    /** @} */

public:

    /** @name style predicates */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE bool type_has_any(type_bits bits)  const RYML_NOEXCEPT { this_assert_readable_(); return tree_->type_has_any(id_, bits); }  /**< Forward to @ref Tree::type_has_any(). Node must be readable. */
    C4_ALWAYS_INLINE bool type_has_all(type_bits bits)  const RYML_NOEXCEPT { this_assert_readable_(); return tree_->type_has_all(id_, bits); }  /**< Forward to @ref Tree::type_has_all(). Node must be readable. */
    C4_ALWAYS_INLINE bool type_has_none(type_bits bits) const RYML_NOEXCEPT { this_assert_readable_(); return tree_->type_has_none(id_, bits); } /**< Forward to @ref Tree::type_has_none(). Node must be readable. */

    C4_ALWAYS_INLINE NodeType key_style()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->key_style(id_); } /**< Forward to @ref Tree::key_style(). Node must be readable. */
    C4_ALWAYS_INLINE NodeType val_style()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->val_style(id_); } /**< Forward to @ref Tree::val_style(). Node must be readable. */

    C4_ALWAYS_INLINE bool is_container_styled() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_container_styled(id_); } /**< Forward to @ref Tree::is_container_styled(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_block()            const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_block(id_); }   /**< Forward to @ref Tree::is_block(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow()             const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_flow(id_); }     /**< Forward to @ref Tree::is_flow(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow_sl()          const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_flow_sl(id_); } /**< Forward to @ref Tree::is_flow_sl(). Node must be readable. */
    RYML_DEPRECATED("use one of .is_flow_ml{1,x,n}()")
    C4_ALWAYS_INLINE bool is_flow_ml()          const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_flow_ml1(id_); } /**< Forward to @ref Tree::is_flow_ml1(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow_ml1()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_flow_ml1(id_); } /**< Forward to @ref Tree::is_flow_ml1(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow_mln()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_flow_mln(id_); } /**< Forward to @ref Tree::is_flow_mln(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_flow_mlx()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_flow_mlx(id_); } /**< Forward to @ref Tree::is_flow_mlx(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_flow_space()      const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_flow_space(id_); }  /**< Forward to @ref Tree::has_flow_space(). Node must be readable. */

    C4_ALWAYS_INLINE bool is_key_styled()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_styled(id_); }  /**< Forward to @ref Tree::is_key_styled(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_styled()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_styled(id_); }  /**< Forward to @ref Tree::is_val_styled(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_literal()      const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_literal(id_); } /**< Forward to @ref Tree::is_key_literal(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_literal()      const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_literal(id_); } /**< Forward to @ref Tree::is_val_literal(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_folded()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_folded(id_); }  /**< Forward to @ref Tree::is_key_folded(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_folded()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_folded(id_); }  /**< Forward to @ref Tree::is_val_folded(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_squo()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_squo(id_); }    /**< Forward to @ref Tree::is_key_squo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_squo()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_squo(id_); }    /**< Forward to @ref Tree::is_val_squo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_dquo()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_dquo(id_); }    /**< Forward to @ref Tree::is_key_dquo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_dquo()         const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_dquo(id_); }    /**< Forward to @ref Tree::is_val_dquo(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_plain()        const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_plain(id_); }   /**< Forward to @ref Tree::is_key_plain(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_plain()        const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_plain(id_); }   /**< Forward to @ref Tree::is_val_plain(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_key_quoted()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_key_quoted(id_); }  /**< Forward to @ref Tree::is_key_quoted(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_val_quoted()       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_val_quoted(id_); }  /**< Forward to @ref Tree::is_val_quoted(). Node must be readable. */
    C4_ALWAYS_INLINE bool is_quoted()           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_quoted(id_); }      /**< Forward to @ref Tree::is_quoted(). Node must be readable. */

    /** @} */

public:

    /** @name hierarchy predicates */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE bool is_root()                              const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_root(id_); } /**< Forward to @ref Tree::is_root(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_parent()                           const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_parent(id_); } /**< Forward to @ref Tree::has_parent()  Node must be readable. */
    C4_ALWAYS_INLINE bool is_ancestor(ConstImpl const& ancestor) const RYML_NOEXCEPT { this_assert_readable_(); return tree_->is_ancestor(id_, ancestor.m_id); } /**< Forward to @ref Tree::is_ancestor()  Node must be readable. */

    C4_ALWAYS_INLINE bool has_child(ConstImpl const& n) const RYML_NOEXCEPT { this_assert_readable_(); return n.readable() ? tree_->has_child(id_, n.m_id) : false; } /**< Forward to @ref Tree::has_child(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_child(id_type node)       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_child(id_, node); } /**< Forward to @ref Tree::has_child(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_child(csubstr name)       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_child(id_, name); } /**< Forward to @ref Tree::has_child(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_children()                const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_children(id_); } /**< Forward to @ref Tree::has_children(). Node must be readable. */

    C4_ALWAYS_INLINE bool has_sibling(ConstImpl const& n) const RYML_NOEXCEPT { this_assert_readable_(); return n.readable() ? tree_->has_sibling(id_, n.m_id) : false; } /**< Forward to @ref Tree::has_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_sibling(id_type node)       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_sibling(id_, node); } /**< Forward to @ref Tree::has_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_sibling(csubstr name)       const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_sibling(id_, name); } /**< Forward to @ref Tree::has_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE bool has_other_siblings()            const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_other_siblings(id_); }  /**< Forward to @ref Tree::has_other_siblings(). Node must be readable. */

    /** @} */

public:

    /** @name hierarchy getters */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE id_type num_children()                  const RYML_NOEXCEPT { this_assert_readable_(); return tree_->num_children(id_); } /**< O(num_children). Forward to @ref Tree::num_children(). */
    C4_ALWAYS_INLINE id_type num_siblings()                  const RYML_NOEXCEPT { this_assert_readable_(); return tree_->num_siblings(id_); } /**< O(num_children). Forward to @ref Tree::num_siblings(). */
    C4_ALWAYS_INLINE id_type num_other_siblings()            const RYML_NOEXCEPT { this_assert_readable_(); return tree_->num_other_siblings(id_); } /**< O(num_siblings). Forward to @ref Tree::num_other_siblings(). */
    C4_ALWAYS_INLINE id_type child_pos(ConstImpl const& n)   const RYML_NOEXCEPT { this_assert_readable_(); RYML_ASSERT_VISIT_CB_(tree_->m_callbacks, n.readable(), n.tree(), n.id()); return tree_->child_pos(id_, n.m_id); } /**< O(num_children). Forward to @ref Tree::child_pos(). */
    C4_ALWAYS_INLINE id_type sibling_pos(ConstImpl const& n) const RYML_NOEXCEPT { this_assert_readable_(); RYML_ASSERT_VISIT_CB_(tree_->callbacks(), n.readable(), n.tree(), n.id()); return tree_->child_pos(tree_->parent(id_), n.m_id); } /**< O(num_siblings). Forward to @ref Tree::sibling_pos(). */

    C4_ALWAYS_INLINE id_type depth_asc()  const RYML_NOEXCEPT { this_assert_readable_(); return tree_->depth_asc(id_); } /** O(log(num_nodes)). Forward to Tree::depth_asc(). Node must be readable. */
    C4_ALWAYS_INLINE id_type depth_desc() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->depth_desc(id_); } /** O(num_nodes). Forward to Tree::depth_desc(). Node must be readable. */

    /** @} */

public:

    /** @name locations */
    /** @{ */

    Location location(Parser const& parser) const
    {
        this_assert_readable_();
        return tree_->location(parser, id_);
    }

    /** @} */

public:

    /** @name deserialization: fully checked for lazy code
     *
     * These methods raise an error if the deserialization failed or
     * optionally if the node is not readable.
     *
     */
    /** @{ */

    /** (1) deserialize the node's contents (val or container) to the
     * given variable, forwarding to the user-overrideable @ref read()
     * function, which can be for ConstNodeRef (see @ref
     * doc_serialization_node_read) or for tree+id (see @ref
     * doc_serialization_tree_read). This method differs from @ref
     * ConstNodeRef::deserialize() in that here the error callback is
     * called if the deserialization failed, or (optionally) the node
     * is not readable. */
    template<class T>
    void load(T *v, bool check_readable=true) const
    {
        if(check_readable)
            check_val_();
        else
            assert_val_(); // assert otherwise
        // we can call read() directly because we checked everything
        // (or the caller told us so)
        // use the adapter ctor to accomodate legacy read() implementations
        const ReadResult result(read((ConstImpl const&)*this, v), id_);
        if C4_UNLIKELY(!result)
            err_visit_(tree_, result.node, "could not deserialize node");
    }
    /** (2) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    void load(Wrapper const& wrapper, bool check_readable=true) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        if(check_readable)
            check_val_();
        else
            assert_val_(); // assert otherwise
        // we can call read() directly because we checked everything
        // (or the caller told us so)
        // use the adapter ctor to accomodate legacy read() implementations
        const ReadResult result(read((ConstImpl const&)*this, wrapper), id_);
        if C4_UNLIKELY(!result)
            err_visit_(tree_, result.node, "could not deserialize node");
    }

    /** (1) deserialize the node's key (necessarily a scalar) to the
     * given variable, forwarding to the user-overrideable @ref read_key()
     * function, which can be for ConstNodeRef (see @ref
     * doc_serialization_node_read) or for tree+id (see @ref
     * doc_serialization_tree_read). This method differs from @ref
     * ConstNodeRef::deserialize_key() in that here the error callback is
     * called if the deserialization failed, or (optionally) the node
     * is not readable. */
    template<class T>
    void load_key(T *k, bool check_readable=true) const
    {
        if(check_readable)
            check_key_();
        else
            assert_key_(); // assert otherwise
        // we can call read_key() directly because we checked
        // everything (or the caller told us so)
        // use the adapter ctor to accomodate legacy read_key() implementations
        const ReadResult result(read_key((ConstImpl const&)*this, k), id_);
        if C4_UNLIKELY(!result)
            err_visit_(tree_, result.node, "could not deserialize key");
    }
    /** (2) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    void load_key(Wrapper const& wrapper, bool check_readable=true) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        if(check_readable)
            check_key_();
        else
            assert_key_(); // assert otherwise
        // we can call read_key() directly because we checked
        // everything (or the caller told us so)
        // use the adapter ctor to accomodate legacy read_key() implementations
        const ReadResult result(read_key((ConstImpl const&)*this, wrapper), id_);
        if C4_UNLIKELY(!result)
            err_visit_(tree_, result.node, "could not deserialize key");
    }

    /** @} */

public:

    /** @name deserialization: asserts node readability, returns success status */
    /** @{ */

    /** (1) deserialize the node's contents (val or container) to the
     * given variable, forwarding to the user-overrideable @ref read()
     * function (see @ref doc_serialization_node_read).
     * @return a @ref ReadResult with the deserialization status. */
    template<class T>
    C4_NODISCARD ReadResult deserialize(T *v) const
    {
        assert_val_();
        // use the adapter ctor to accomodate legacy read() implementations
        return ReadResult(read((ConstImpl const&)*this, v), id_);
    }
    /** (2) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    C4_NODISCARD ReadResult deserialize(Wrapper const& wrapper) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        assert_val_();
        // use the adapter ctor to accomodate legacy read() implementations
        return ReadResult(read((ConstImpl const&)*this, wrapper), id_);
    }

    /** (1) deserialize the node's key (necessarily a scalar) to the
     * given variable, forwarding to the user-overrideable @ref
     * read_key() function (see @ref doc_serialization_node_read).
     *
     * @return a @ref ReadResult with the deserialization status. */
    template<class T>
    C4_NODISCARD ReadResult deserialize_key(T *v) const
    {
        assert_key_();
        // use the adapter ctor to accomodate legacy read_key() implementations
        return ReadResult(read_key((ConstImpl const&)*this, v), id_);
    }
    /** (2) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    C4_NODISCARD ReadResult deserialize_key(Wrapper const& wrapper) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        assert_key_();
        // use the adapter ctor to accomodate legacy read_key() implementations
        return ReadResult(read_key((ConstImpl const&)*this, wrapper), id_);
    }

    /** @} */

public:

    /** @name lookup and deserialize */
    /** @{ */

    /** (1) find a child by name and deserialize its contents to the
     * given variable (ie call .deserialize() on the child if it
     * exists). Otherwise, the variable is kept unchanged.
     *
     * @return a @ref ReadResult set with this node's id if no child
     * exists, or the ReadResult from the deserialization.
     *
     * @see see also @ref ConstNodeRef::find_child_r()
     */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(csubstr child_key, T *v) const
    {
        this_assert_readable_();
        ConstImpl ch;
        ReadResult r = this_->find_child_r(child_key, &ch);
        if(r)
            r = ch.deserialize(v);
        return r;
    }
    /** (2) like (1), but assign from fallback if no such child exists. */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(csubstr child_key, T *v, T const& fallback) const
    {
        this_assert_readable_();
        ConstImpl ch;
        if(this_->find_child_r(child_key, &ch))
            return ch.deserialize(v);
        *v = fallback;
        return ReadResult();
    }
    /** (3) like (1), but for wrapper tag types such as @ref c4::fmt::base64() */
    template<class Wrapper>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(csubstr child_key, Wrapper const& v) const
    {
        this_assert_readable_();
        ConstImpl ch;
        ReadResult r = this_->find_child_r(child_key, &ch);
        if(r)
            r = ch.deserialize(v);
        return r;
    }

    /** (1) find a child by position and deserialize its contents to
     * the given variable (ie call .deserialize() on the child if it
     * exists). Otherwise, the variable is kept unchanged.
     *
     * @return a @ref ReadResult set with this node's id if no child
     * exists, or the ReadResult from the deserialization.
     *
     * @see see also @ref ConstNodeRef::child_r() */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type child_pos, T *v) const
    {
        this_assert_readable_();
        ConstImpl ch;
        ReadResult r = this_->child_r(child_pos, &ch);
        if(r)
            r = ch.deserialize(v);
        return r;
    }
    /** (2) like (1), but assign from fallback if no such child exists */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type child_pos, T *v, T const& fallback) const
    {
        this_assert_readable_();
        ConstImpl ch;
        if(this_->child_r(child_pos, &ch))
            return ch.deserialize(v);
        *v = fallback;
        return ReadResult();
    }
    /** (3) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type child_pos, Wrapper const& wrapper) const
    {
        this_assert_readable_();
        ConstImpl ch;
        ReadResult r = this_->child_r(child_pos, &ch);
        if(r)
            r = ch.deserialize(wrapper);
        return r;
    }

    /** @} */

public:

    C4_SUPPRESS_WARNING_PUSH
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated")
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated-declarations")
    C4_SUPPRESS_WARNING_MSVC(4996) // deprecated

    /** @name legacy operators */
    /** @{ */

    template<class T>
    RYML_LEGACY_OPERATOR("use .load()")
    Impl const& operator>> (T &v) const { load(&v); return (Impl const&)*this; }

    template<class T>
    RYML_LEGACY_OPERATOR("use .load()")
    Impl const& operator>> (T const& wrapper) const { load(wrapper); return (Impl const&)*this; }

    template<class T>
    RYML_LEGACY_OPERATOR("use .load_key()")
    Impl const& operator>> (Key<T> const& v) const { load_key(&v.k); return (Impl const&)*this; }

    /** @} */

    C4_SUPPRESS_WARNING_POP

protected: // error helpers

    void check_val_() const
    {
        if C4_UNLIKELY(!tree_)
            err_basic_("node not readable");
        else if C4_UNLIKELY(!(((Impl const* C4_RESTRICT)this)->readable()))
            err_visit_(tree_, id_, "node not readable");
        else if C4_UNLIKELY(!(tree_->type(id_) & (VAL|MAP|SEQ)))
            err_visit_(tree_, id_, "node has no contents");
    }
    void check_key_() const
    {
        if C4_UNLIKELY(!tree_)
            err_basic_("node not readable");
        else if C4_UNLIKELY(!(((Impl const* C4_RESTRICT)this)->readable()))
            err_visit_(tree_, id_, "node not readable");
        else if C4_UNLIKELY(!(tree_->type(id_) & KEY))
            err_visit_(tree_, id_, "node has no key");
    }

    C4_NORETURN C4_NO_INLINE C4_COLD
    static void err_basic_(const char *msg)
    {
        RYML_ERR_BASIC_(msg);
    }

    C4_NORETURN C4_NO_INLINE C4_COLD
    static void err_visit_(Tree const *tree, id_type id, const char *msg)
    {
        RYML_ERR_VISIT_CB_(tree->m_callbacks, tree, id, msg);
    }

    #if RYML_USE_ASSERT
    void assert_val_() const { check_val_(); }
    void assert_key_() const { check_key_(); }
    #else
    C4_ALWAYS_INLINE void assert_val_() const noexcept {}
    C4_ALWAYS_INLINE void assert_key_() const noexcept {}
    #endif

public: // deprecated functions

    // these functions will be removed in future releases. If you
    // disagree with a particular function being deprecated, let us
    // know by opening a new issue at https://github.com/biojppm/rapidyaml/issues

    /** @cond dev */ // LCOV_EXCL_START
    C4_SUPPRESS_WARNING_PUSH
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated")
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated-declarations")
    C4_SUPPRESS_WARNING_MSVC(4996) // deprecated
    RYML_DEPRECATED("use .type().type_str(buf)") const char* type_str() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->type_str(id_); } /**< Forward to @ref Tree::type_str(). Node must be readable. */
    RYML_DEPRECATED("use has_other_siblings()") bool has_siblings() const RYML_NOEXCEPT { this_assert_readable_(); return tree_->has_siblings(id_); }
    RYML_DEPRECATED("use has_key_anchor()")  bool is_key_anchor() const noexcept { this_assert_readable_(); return tree_->has_key_anchor(id_); }
    RYML_DEPRECATED("use has_val_anchor()")  bool is_val_hanchor() const noexcept { this_assert_readable_(); return tree_->has_val_anchor(id_); }
    RYML_DEPRECATED("use has_anchor()")      bool is_anchor()     const noexcept { this_assert_readable_(); return tree_->has_anchor(id_); }
    RYML_DEPRECATED("use has_anchor() || is_ref()") bool is_anchor_or_ref() const noexcept { this_assert_readable_(); return tree_->is_anchor_or_ref(id_); }
    template<class T>
    RYML_DEPRECATED("use .deserialize_child()") bool get_if(csubstr name, T *var) const
    {
        this_assert_readable_();
        ConstImpl ch = ((ConstImpl const*)this)->find_child(name);
        if(ch.readable())
        {
            ch.load(var);
            return true;
        }
        return false;
    }
    template<class T>
    RYML_DEPRECATED("use .deserialize_child()") bool get_if(id_type pos, T *var) const
    {
        this_assert_readable_();
        ConstImpl ch = ((ConstImpl const*)this)->child(pos);
        if(ch.readable())
        {
            ch.load(var);
            return true;
        }
        return false;
    }
    template<class T>
    RYML_DEPRECATED("use .deserialize_child()")
    bool get_if(csubstr name, T *var, T const& fallback) const
    {
        if(get_if(name, var))
            return true;
        *var = fallback;
        return false;
    }
    template<class T>
    RYML_DEPRECATED("use .deserialize_child()")
    bool get_if(id_type pos, T *var, T const& fallback) const
    {
        if(get_if(pos, var))
            return true;
        *var = fallback;
        return false;
    }
    template<class Visitor>
    RYML_DEPRECATED("") bool visit(Visitor fn, id_type indentation_level=0, bool skip_root=true) const RYML_NOEXCEPT
    {
        this_assert_readable_();
        return detail::_visit(*(ConstImpl const*)this, fn, indentation_level, skip_root);
    }
    template <class Visitor, class U = Impl>
    RYML_DEPRECATED("")
    auto visit(Visitor fn, id_type indentation_level = 0, bool skip_root = true) RYML_NOEXCEPT
        -> typename std ::enable_if<!std ::is_same<U, ConstImpl>::value, bool>::type
    {
      this_assert_readable_();
      return detail::_visit(*(Impl *)this, fn, indentation_level, skip_root);
    }
    template<class Visitor>
    RYML_DEPRECATED("") bool visit_stacked(Visitor fn, id_type indentation_level=0, bool skip_root=true) const RYML_NOEXCEPT
    {
        this_assert_readable_();
        return detail::_visit_stacked(*(ConstImpl const*)this, fn, indentation_level, skip_root);
    }
    template<class Visitor, class U=Impl>
    RYML_DEPRECATED("") auto visit_stacked(Visitor fn, id_type indentation_level=0, bool skip_root=true) RYML_NOEXCEPT
        -> typename std ::enable_if<!std ::is_same<U, ConstImpl>::value, bool>::type
    {
        this_assert_readable_();
        return detail::_visit_stacked(*(Impl*)this, fn, indentation_level, skip_root);
    }
    /** @endcond */ // LCOV_EXCL_STOP
    C4_SUPPRESS_WARNING_POP

    #undef this_assert_readable_
    #undef this_
    #undef mtree_
    #undef tree_
    #undef id_

    C4_SUPPRESS_WARNING_GCC_CLANG_POP
};
} // detail


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/** Holds a pointer to an existing tree, and a node id. It can be used
 * only to read from the tree.
 *
 * @warning The lifetime of the tree must be larger than that of this
 * object. It is up to the user to ensure that this happens. */
class RYML_EXPORT ConstNodeRef : public detail::RoNodeMethods<ConstNodeRef, ConstNodeRef> // NOLINT
{
public:

    using tree_type = Tree const;

public:

    Tree const* C4_RESTRICT m_tree;
    id_type m_id;

private:

    friend NodeRef;
    friend struct detail::RoNodeMethods<ConstNodeRef, ConstNodeRef>;

public:

    /** @name construction */
    /** @{ */

    ConstNodeRef() noexcept : m_tree(nullptr), m_id(NONE) {}
    ConstNodeRef(Tree const &t) noexcept : m_tree(&t), m_id(t .root_id()) {}
    ConstNodeRef(Tree const *t) noexcept : m_tree(t ), m_id(t->root_id()) {}
    ConstNodeRef(Tree const *t, id_type id) noexcept : m_tree(t), m_id(id) {}

    ConstNodeRef(ConstNodeRef const&) noexcept = default;
    ConstNodeRef(ConstNodeRef     &&) noexcept = default;

    inline ConstNodeRef(NodeRef const&) noexcept;
    inline ConstNodeRef(NodeRef     &&) noexcept;

    /** @} */

public:

    /** @name assignment */
    /** @{ */

    ConstNodeRef& operator= (ConstNodeRef const&) noexcept = default;
    ConstNodeRef& operator= (ConstNodeRef     &&) noexcept = default;

    ConstNodeRef& operator= (NodeRef const&) noexcept;
    ConstNodeRef& operator= (NodeRef     &&) noexcept;

    /** @} */

public:

    /** @name state queries
     *
     * see @ref NodeRef for an explanation on what these states mean */
    /** @{ */

    C4_ALWAYS_INLINE bool invalid() const noexcept { return (!m_tree) || (m_id == NONE); }
    /** because a ConstNodeRef cannot be used to write to the tree,
     * readable() has the same meaning as !invalid() */
    C4_ALWAYS_INLINE bool readable() const noexcept { return m_tree != nullptr && m_id != NONE; }
    /** because a ConstNodeRef cannot be used to write to the tree, it can never be a seed.
     * This method is provided for API equivalence between ConstNodeRef and NodeRef. */
    constexpr static C4_ALWAYS_INLINE bool is_seed() noexcept { return false; }

    /** @} */

private:

    void assert_readable_() const
    {
        RYML_ASSERT_BASIC_(m_tree != nullptr);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_id != NONE && (m_id < m_tree->capacity()), m_tree, m_id);
    }

    void check_readable_() const
    {
        if C4_UNLIKELY(!m_tree)
            RYML_ERR_BASIC_("invalid node");
        if C4_UNLIKELY(!m_tree || m_id == NONE || (m_id > m_tree->capacity()))
            RYML_ERR_VISIT_CB_(m_tree->m_callbacks, m_tree, m_id, "invalid node");
    }

public:

    /** @name member getters */
    /** @{ */

    C4_ALWAYS_INLINE Tree const* tree() const noexcept { return m_tree; }
    C4_ALWAYS_INLINE id_type id() const noexcept { return m_id; }

    /** @} */

public:

    /** @name comparisons */
    /** @{ */

    C4_ALWAYS_INLINE bool operator== (ConstNodeRef const& that) const RYML_NOEXCEPT { return that.m_tree == m_tree && m_id == that.m_id; }
    C4_ALWAYS_INLINE bool operator!= (ConstNodeRef const& that) const RYML_NOEXCEPT { return ! this->operator== (that); }

    /** @} */

public:

    /** @name node property getters */
    /** @{ */

    C4_ALWAYS_INLINE NodeData const* get() const RYML_NOEXCEPT { return readable() ? m_tree->get(m_id) : nullptr; } /**< Forward to @ref Tree::type(). Node must be readable. */

    /** @} */

public:

    /** @name hierarchy getters */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE ConstNodeRef parent()                   const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->parent(m_id)}; }              /**< Forward to @ref Tree::parent(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef first_child()              const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->first_child(m_id)}; }         /**< Forward to @ref Tree::first_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef last_child()               const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->last_child (m_id)}; }         /**< Forward to @ref Tree::last_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef child(id_type pos)         const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->child(m_id, pos)}; }          /**< Forward to @ref Tree::child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef find_child(csubstr name)   const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->find_child(m_id, name)}; }    /**< Forward to @ref Tree::find_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef prev_sibling()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->prev_sibling(m_id)}; }        /**< Forward to @ref Tree::prev_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef next_sibling()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->next_sibling(m_id)}; }        /**< Forward to @ref Tree::next_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef first_sibling()            const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->first_sibling(m_id)}; }       /**< Forward to @ref Tree::first_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef last_sibling ()            const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->last_sibling(m_id)}; }        /**< Forward to @ref Tree::last_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef sibling(id_type pos)       const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->sibling(m_id, pos)}; }        /**< Forward to @ref Tree::sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef find_sibling(csubstr name) const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->find_sibling(m_id, name)}; }  /**< Forward to @ref Tree::find_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef ancestor_doc()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->ancestor_doc(m_id)}; }        /**< Forward to @ref Tree::ancestor_doc(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef doc(id_type i)             const RYML_NOEXCEPT { RYML_ASSERT_BASIC_(m_tree); return {m_tree, m_tree->doc(i)}; } /**< Forward to @ref Tree::doc(). Node must be readable. succeeds even when the node may have invalid or seed id */

    C4_NODISCARD C4_ALWAYS_INLINE ReadResult child_r(id_type pos, ConstNodeRef *child)       const RYML_NOEXCEPT { assert_readable_(); child->m_tree = m_tree; ReadResult result = m_tree->     child_r(m_id,  pos, &child->m_id); return result; }; /**< Forward to @ref Tree::child_r(). Node must be readable. */
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult find_child_r(csubstr name, ConstNodeRef *child) const RYML_NOEXCEPT { assert_readable_(); child->m_tree = m_tree; ReadResult result = m_tree->find_child_r(m_id, name, &child->m_id); return result; }; /**< Forward to @ref Tree::find_child_r(). Node must be readable. */

    C4_NODISCARD C4_ALWAYS_INLINE ReadResult sibling_r(id_type pos, ConstNodeRef *sibling)       const RYML_NOEXCEPT { assert_readable_(); sibling->m_tree = m_tree; ReadResult result = m_tree->     sibling_r(m_id,  pos, &sibling->m_id); return result; }; /**< Forward to @ref Tree::sibling_r(). Node must be readable. */
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult find_sibling_r(csubstr name, ConstNodeRef *sibling) const RYML_NOEXCEPT { assert_readable_(); sibling->m_tree = m_tree; ReadResult result = m_tree->find_sibling_r(m_id, name, &sibling->m_id); return result; }; /**< Forward to @ref Tree::find_sibling_r(). Node must be readable. */

    /** @} */

public:

    /** @name square_brackets
     * operator[] */
    /** @{ */

    /** Find a child by key; complexity is O(num_children).
     *
     * Behaves similar to the non-const overload, but further asserts
     * that the returned node is readable (because it can never be in
     * a seed state). The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to use the return value if it is not valid.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389  */
    C4_ALWAYS_INLINE ConstNodeRef operator[] (csubstr key) const RYML_NOEXCEPT
    {
        assert_readable_();
        id_type ch = m_tree->find_child(m_id, key);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** Find a child by position; complexity is O(pos).
     *
     * Behaves similar to the non-const overload, but further asserts
     * that the returned node is readable (because it can never be in
     * a seed state). This assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to use the return value if it is not valid.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389  */
    C4_ALWAYS_INLINE ConstNodeRef operator[] (id_type pos) const RYML_NOEXCEPT
    {
        assert_readable_();
        id_type ch = m_tree->child(m_id, pos);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** @} */

public:

    /** @name at
     *
     * These functions are the analogue to operator[], with the
     * difference that they emit an error instead of an
     * assertion. That is, if any of the pre or post conditions is
     * violated, an error is always emitted (resulting in a call to
     * the error callback).
     *
     * @{ */

    /** Get a child by name, with error checking; complexity is
     * O(num_children).
     *
     * Behaves as operator[](csubstr) const, but always raises an
     * error (even when RYML_USE_ASSERT is set to false) when the
     * returned node does not exist, or when this node is not
     * readable, or when it is not a map. This behaviour is similar to
     * std::vector::at(), but the error consists in calling the error
     * callback instead of directly raising an exception. */
    ConstNodeRef at(csubstr key) const
    {
        check_readable_();
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, m_tree->is_map(m_id), m_tree, m_id);
        id_type ch = m_tree->find_child(m_id, key);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** Get a child by position, with error checking; complexity is
     * O(pos).
     *
     * Behaves as operator[](id_type) const, but always raises an error
     * (even when RYML_USE_ASSERT is set to false) when the returned
     * node does not exist, or when this node is not readable, or when
     * it is not a container. This behaviour is similar to
     * std::vector::at(), but the error consists in calling the error
     * callback instead of directly raising an exception. */
    ConstNodeRef at(id_type pos) const
    {
        check_readable_();
        const id_type cap = m_tree->capacity();
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (pos >= 0 && pos < cap), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, m_tree->is_container(m_id), m_tree, m_id);
        const id_type ch = m_tree->child(m_id, pos);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** @} */

public:

    /** @name iteration */
    /** @{ */

    using iterator = detail::child_iterator<ConstNodeRef>;
    using const_iterator = detail::child_iterator<ConstNodeRef>;
    using children_view = detail::children_view<ConstNodeRef>;
    using const_children_view = detail::children_view<ConstNodeRef>;


    C4_ALWAYS_INLINE const_iterator begin()  const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, m_tree->first_child(m_id)); } /**< get an iterator to the first child */
    C4_ALWAYS_INLINE const_iterator cbegin() const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, m_tree->first_child(m_id)); } /**< get an iterator to the first child */

    C4_ALWAYS_INLINE const_iterator end()  const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, NONE); } /**< get an iterator to after the last child */
    C4_ALWAYS_INLINE const_iterator cend() const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, NONE); } /**< get an iterator to after the last child */


    C4_ALWAYS_INLINE const_children_view children()  const RYML_NOEXCEPT { assert_readable_(); return detail::make_children_view<const_children_view>(m_tree, m_id); } /**< get an iterable view over children */
    C4_ALWAYS_INLINE const_children_view cchildren() const RYML_NOEXCEPT { assert_readable_(); return detail::make_children_view<const_children_view>(m_tree, m_id); } /**< get an iterable view over children */

    C4_ALWAYS_INLINE const_children_view siblings()  const RYML_NOEXCEPT { assert_readable_(); return detail::make_siblings_view<const_children_view>(m_tree, m_id); } /** get an iterable view over all siblings (including the calling node) */
    C4_ALWAYS_INLINE const_children_view csiblings() const RYML_NOEXCEPT { assert_readable_(); return detail::make_siblings_view<const_children_view>(m_tree, m_id); } /** get an iterable view over all siblings (including the calling node) */

    /** @} */

public: // deprecated functions

    // these functions will be removed in future releases. If you
    // disagree with a particular function being deprecated, let us
    // know by opening a new issue at https://github.com/biojppm/rapidyaml/issues

    /** @cond dev */ // LCOV_EXCL_START
    C4_SUPPRESS_WARNING_PUSH
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated")
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated-declarations")
    C4_SUPPRESS_WARNING_MSVC(4996) // deprecated

    RYML_DEPRECATED("use ConstNodeRef()") ConstNodeRef(std::nullptr_t) noexcept : m_tree(nullptr), m_id(NONE) {}
    RYML_DEPRECATED("use node = {}") ConstNodeRef& operator= (std::nullptr_t) noexcept { m_tree = nullptr; m_id = NONE; return *this; }

    RYML_DEPRECATED("use one of readable(), is_seed() or !invalid()") bool valid() const noexcept { return m_tree != nullptr && m_id != NONE; }
    RYML_DEPRECATED("use invalid()")  bool operator== (std::nullptr_t) const noexcept { return m_tree == nullptr || m_id == NONE; }
    RYML_DEPRECATED("use !invalid()") bool operator!= (std::nullptr_t) const noexcept { return !(m_tree == nullptr || m_id == NONE); }
    RYML_DEPRECATED("use (this->val() == s)") bool operator== (csubstr s) const RYML_NOEXCEPT { RYML_ASSERT_BASIC_(m_tree); RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_id != NONE, m_tree, NONE); return m_tree->val(m_id) == s; }
    RYML_DEPRECATED("use (this->val() != s)") bool operator!= (csubstr s) const RYML_NOEXCEPT { RYML_ASSERT_BASIC_(m_tree); RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_id != NONE, m_tree, NONE); return m_tree->val(m_id) != s; }
    /** @endcond */ // LCOV_EXCL_STOP

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

// NOLINTBEGIN(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)

/** A reference to a node in an existing yaml tree, offering a more
 * convenient API than the index-based API used in the tree.
 *
 * Unlike its imutable ConstNodeRef peer, a NodeRef can be used to
 * mutate the tree, both by writing to existing nodes and by creating
 * new nodes to subsequently write to. Semantically, a NodeRef
 * object can be in one of three states:
 *
 * ```text
 * invalid  := not pointing at anything
 * readable := points at an existing tree/node
 * seed     := points at an existing tree, and the node
 *             may come to exist, if we write to it.
 * ```
 *
 * So both `readable` and `seed` are states where the node is also `valid`.
 *
 * ```cpp
 * Tree t = parse_in_arena("{a: b}");
 * NodeRef invalid; // not pointing at anything.
 * NodeRef readable = t["a"]; // also valid, because "a" exists
 * NodeRef seed = t["none"]; // also valid, but is seed because "none" is not in the map
 * ```
 *
 * When the object is in seed state, using it to read from the tree is
 * UB. The seed node can be used to write to the tree, provided that
 * its create() method is called prior to writing, which happens in
 * most modifying methods in NodeRef.
 *
 * It is the owners's responsibility to verify that an existing
 * node is readable before subsequently using it to read from the
 * tree.
 *
 * @warning The lifetime of the tree must be larger than that of this
 * object. It is up to the user to ensure that this happens.
 */
class RYML_EXPORT NodeRef : public detail::RoNodeMethods<NodeRef, ConstNodeRef> // NOLINT
{
public:

    using tree_type = Tree;
    using base_type = detail::RoNodeMethods<NodeRef, ConstNodeRef>;

private:

    Tree *C4_RESTRICT m_tree;
    id_type m_id;

    /** This member is used to enable lazy operator[] writing.
     *
     * When a child with a key or index is not found, m_id is set to
     * the id of the parent and the asked-for key or index are stored
     * in this member until a write does happen. Then it is given as
     * key or index for creating the child.
     *
     * When a key is used, the csubstr stores it (so the csubstr's
     * string is non-null and the csubstr's size is different from
     * NONE). When an index is used instead, the csubstr's string is
     * set to null, and only the csubstr's size is set to a value
     * different from NONE.
     *
     * Otherwise, when operator[] does find the child, then this member
     * is empty: m_seed.str is null and m_seed.len is NONE. */
    csubstr m_seed;

    C4_ALWAYS_INLINE void _clear_seed() noexcept
    {
        // this must be done manually or an assert is triggered:
        m_seed.str = nullptr;
        m_seed.len = NONE;
    }

    friend ConstNodeRef;
    friend struct detail::RoNodeMethods<NodeRef, ConstNodeRef>;

public:

    /** @name construction */
    /** @{ */

    NodeRef() noexcept : m_tree(nullptr), m_id(NONE), m_seed() { _clear_seed(); }
    NodeRef(Tree &t) noexcept : m_tree(&t), m_id(t .root_id()), m_seed() { _clear_seed(); }
    NodeRef(Tree *t) noexcept : m_tree(t ), m_id(t->root_id()), m_seed() { _clear_seed(); }
    NodeRef(Tree *t, id_type id) noexcept : m_tree(t), m_id(id), m_seed() { _clear_seed(); }
    NodeRef(Tree *t, id_type id, id_type seed_pos) noexcept : m_tree(t), m_id(id), m_seed() { m_seed.str = nullptr; m_seed.len = (size_t)seed_pos; }
    NodeRef(Tree *t, id_type id, csubstr seed_key) noexcept : m_tree(t), m_id(id), m_seed(seed_key) {}

    /** @} */

public:

    /** @name assignment */
    /** @{ */

    NodeRef(NodeRef const&) noexcept = default;
    NodeRef(NodeRef     &&) noexcept = default;

    NodeRef& operator= (NodeRef const&) noexcept = default;
    NodeRef& operator= (NodeRef     &&) noexcept = default;

    /** @} */

public:

    /** @name state_queries
     * @{ */

    /** true if the object is not referring to any existing or seed node. @see the doc for @ref NodeRef */
    bool invalid() const noexcept { return m_tree == nullptr || m_id == NONE; }
    /** true if the object is not invalid and in seed state. @see the doc for @ref NodeRef */
    bool is_seed() const noexcept { return (m_tree != nullptr && m_id != NONE) && has_seed_(); }
    /** true if the object is not invalid and not in seed state. @see the doc for @ref NodeRef */
    bool readable() const noexcept { return (m_tree != nullptr && m_id != NONE) && !has_seed_(); }

    /** @} */

private:

    C4_ALWAYS_INLINE bool valid_id_() const noexcept { return m_id != NONE && m_id < m_tree->capacity(); }
    C4_ALWAYS_INLINE bool has_seed_() const noexcept { return m_seed.str != nullptr || m_seed.len != (size_t)NONE; }

    void assert_readable_() const
    {
        RYML_ASSERT_BASIC_(m_tree != nullptr);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, valid_id_() && !has_seed_(), m_tree, m_id);
    }

    void check_readable_() const
    {
        if C4_UNLIKELY(!m_tree)
            err_basic_("invalid node");
        if C4_UNLIKELY((m_id == NONE || (m_id > m_tree->capacity()) ||
                       (m_seed.str != nullptr || m_seed.len != (size_t)NONE)))
            err_visit_(m_tree, m_id, "invalid node");
    }

    void check_writeable_() const
    {
        if C4_UNLIKELY(!m_tree)
            err_basic_("invalid node");
    }

    void assert_writeable_() const
    {
        RYML_ASSERT_BASIC_(m_tree != nullptr);
    }

public:

    /** @name comparisons */
    /** @{ */

    bool operator== (NodeRef const& that) const
    {
        if(m_tree == that.m_tree && m_id == that.m_id)
        {
            bool seed = is_seed();
            if(seed == that.is_seed())
            {
                if(seed)
                {
                    return (m_seed.len == that.m_seed.len)
                        && (m_seed.str == that.m_seed.str
                            || m_seed == that.m_seed); // do strcmp only in the last resort
                }
                return true;
            }
        }
        return false;
    }
    bool operator!= (NodeRef const& that) const { return ! this->operator==(that); }

    bool operator== (ConstNodeRef const& that) const { return m_tree == that.m_tree && m_id == that.m_id && !is_seed(); }
    bool operator!= (ConstNodeRef const& that) const { return ! this->operator==(that); }

    /** @} */

public:

    /** @name node_property_getters
     * @{ */

    C4_ALWAYS_INLINE id_type id() const noexcept { return m_id; }

    C4_ALWAYS_INLINE Tree const* tree() const noexcept { return m_tree; }
    C4_ALWAYS_INLINE Tree      * tree()       noexcept { return m_tree; }

    C4_ALWAYS_INLINE NodeData const* get() const RYML_NOEXCEPT { return readable() ? m_tree->get(m_id) : nullptr; } /**< Forward to @ref Tree::type(). Node must be readable. */
    C4_ALWAYS_INLINE NodeData      * get()       RYML_NOEXCEPT { return readable() ? m_tree->get(m_id) : nullptr; } /**< Forward to @ref Tree::type(). Node must be readable. */

    /** @} */


public:

    /** @name node_setters */
    /** @{ */

    /// if this node is in seed state, create the node in the tree
    void create()
    {
        RYML_ASSERT_BASIC_(m_tree != nullptr);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_id != NONE && m_id < m_tree->capacity(), m_tree, m_id);
        if(m_seed.str) // we have a seed key: use it to create the new map child
        {
            m_id = m_tree->append_child(m_id);
            m_tree->set_key(m_id, m_seed);
            m_seed.str = nullptr;
            m_seed.len = (size_t)NONE;
        }
        else if(m_seed.len != (size_t)NONE) // we have a seed index: create a seq child at that position
        {
            RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, (size_t)m_tree->num_children(m_id) == m_seed.len, m_tree, m_id);
            m_id = m_tree->append_child(m_id);
            m_seed.str = nullptr;
            m_seed.len = (size_t)NONE;
        }
    }

    void set_stream() { create(); m_tree->set_stream(m_id); }
    void set_doc() { create(); m_tree->set_doc(m_id); }

    void set_key(csubstr key) { create(); m_tree->set_key(m_id, key); }
    void set_key(csubstr key, NodeType more_flags) { create(); m_tree->set_key(m_id, key, more_flags); }

    void set_val(csubstr val) { create(); m_tree->set_val(m_id, val); }
    void set_val(csubstr val, NodeType more_flags) { create(); m_tree->set_val(m_id, val, more_flags); }

    void set_seq() { create(); m_tree->set_seq(m_id); }
    void set_seq(NodeType more_flags) { create(); m_tree->set_seq(m_id, more_flags); }

    void set_map() { create(); m_tree->set_map(m_id); }
    void set_map(NodeType more_flags) { create(); m_tree->set_map(m_id, more_flags); }

    void set_key_tag(csubstr key_tag) { create(); m_tree->set_key_tag(m_id, key_tag); }
    void set_val_tag(csubstr val_tag) { create(); m_tree->set_val_tag(m_id, val_tag); }
    void set_key_anchor(csubstr key_anchor) { create(); m_tree->set_key_anchor(m_id, key_anchor); }
    void set_val_anchor(csubstr val_anchor) { create(); m_tree->set_val_anchor(m_id, val_anchor); }
    void set_key_ref(csubstr key_ref) { create(); m_tree->set_key_ref(m_id, key_ref); }
    void set_val_ref(csubstr val_ref) { create(); m_tree->set_val_ref(m_id, val_ref); }

    void set_container_style(type_bits style) { create(); m_tree->set_container_style(m_id, style); }
    void set_key_style(type_bits style) { create(); m_tree->set_key_style(m_id, style); }
    void set_val_style(type_bits style) { create(); m_tree->set_val_style(m_id, style); }

    /** @} */

public:

    /** @name node_modifiers */
    /** @{ */

    void change_type(NodeType t)
    {
        assert_readable_();
        m_tree->change_type(m_id, t);
    }

    /** remove the KEY (flags included) from a scalar */
    void clear_key()
    {
        assert_readable_();
        m_tree->_clear_key(m_id);
    }

    /** remove the VAL (flags included) from a scalar */
    void clear_val()
    {
        assert_readable_();
        m_tree->_clear_val(m_id);
    }

    /** remove the children from a scalar */
    void clear_children()
    {
        assert_readable_();
        m_tree->remove_children(m_id);
    }

    void clear_style(bool recurse=false)
    {
        assert_readable_();
        m_tree->clear_style(m_id, recurse);
    }

    void set_style_conditionally(NodeType type_mask,
                                 NodeType rem_style_flags,
                                 NodeType add_style_flags,
                                 bool recurse=false)
    {
        assert_readable_();
        m_tree->set_style_conditionally(m_id, type_mask, rem_style_flags, add_style_flags, recurse);
    }

    /** @} */

public:

    /** @name serialization */
    /** @{ */

    /** forward to @ref Tree::to_arena() . Serializes a scalar type to
     * the tree's arena. The type must be serializeable as a scalar. */
    template<class T>
    csubstr to_arena(T const& C4_RESTRICT s)
    {
        RYML_ASSERT_BASIC_(m_tree); // no need for valid or readable
        return m_tree->to_arena(s);
    }

    template<class T>
    void save(T const& C4_RESTRICT k)
    {
        check_writeable_();
        create();
        write(*this, k);
    }
    template<class T>
    void save(T const& C4_RESTRICT k, NodeType style_flags)
    {
        check_writeable_();
        create();
        write(*this, k);
        RYML_ASSERT_BASIC_(!(style_flags & ~STYLE));
        _add_flags(style_flags);
    }

    template<class T>
    void save_key(T const& C4_RESTRICT k)
    {
        check_writeable_();
        create();
        write_key(*this, k);
    }
    template<class T>
    void save_key(T const& C4_RESTRICT k, NodeType style_flags)
    {
        check_writeable_();
        create();
        write_key(*this, k);
        RYML_ASSERT_BASIC_(!(style_flags & ~STYLE));
        _add_flags(style_flags);
    }

    /** serialize a variable to this node. If the variable is a
     * scalar, it is first serialized to the arena, and then assigned
     * to the node's val. Otherwise, the node will be made a
     * container, and its contents populated from the variable. */
    template<class T>
    void set_serialized(T const& C4_RESTRICT v)
    {
        assert_writeable_();
        create();
        write(*this, v);
    }
    template<class T>
    void set_serialized(T const& C4_RESTRICT v, NodeType style_flags)
    {
        assert_writeable_();
        create();
        write(*this, v);
        RYML_ASSERT_BASIC_(!(style_flags & ~STYLE));
        _add_flags(style_flags);
    }

    /** serialize a variable, then assign the result to the node's key
     *
     * @warning The variable must be serialized as a scalar, ie it
     * cannot be serialized as a container. */
    template<class T>
    void set_key_serialized(T const& C4_RESTRICT k)
    {
        assert_writeable_();
        create();
        write_key(*this, k);
    }
    template<class T>
    void set_key_serialized(T const& C4_RESTRICT k, NodeType style_flags)
    {
        assert_writeable_();
        create();
        write_key(*this, k);
        RYML_ASSERT_BASIC_(!(style_flags & ~STYLE));
        _add_flags(style_flags);
    }

    /** @} */

public:

    /** @name hierarchy getters */
    /** @{ */

    // vertically aligned to highlight differences.
    // documentation to the right -->

    C4_ALWAYS_INLINE NodeRef      parent()                         RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->parent(m_id)}; }              /**< Forward to @ref Tree::parent(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef parent()                   const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->parent(m_id)}; }              /**< Forward to @ref Tree::parent(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      first_child()                    RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->first_child(m_id)}; }         /**< Forward to @ref Tree::first_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef first_child()              const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->first_child(m_id)}; }         /**< Forward to @ref Tree::first_child(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      last_child()                     RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->last_child(m_id)}; }          /**< Forward to @ref Tree::last_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef last_child()               const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->last_child (m_id)}; }         /**< Forward to @ref Tree::last_child(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      child(id_type pos)               RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->child(m_id, pos)}; }          /**< Forward to @ref Tree::child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef child(id_type pos)         const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->child(m_id, pos)}; }          /**< Forward to @ref Tree::child(). Node must be readable. */

    C4_NODISCARD C4_ALWAYS_INLINE ReadResult child_r(id_type pos,      NodeRef *child)       RYML_NOEXCEPT { assert_readable_(); child->m_tree = m_tree; ReadResult result = m_tree->child_r(m_id,  pos, &child->m_id); return result; }; /**< Forward to @ref Tree::child_r(). Node must be readable. */
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult child_r(id_type pos, ConstNodeRef *child) const RYML_NOEXCEPT { assert_readable_(); child->m_tree = m_tree; ReadResult result = m_tree->child_r(m_id,  pos, &child->m_id); return result; }; /**< Forward to @ref Tree::child_r(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      find_child(csubstr name)         RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->find_child(m_id, name)}; }    /**< Forward to @ref Tree::find_child(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef find_child(csubstr name)   const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->find_child(m_id, name)}; }    /**< Forward to @ref Tree::find_child(). Node must be readable. */

    C4_NODISCARD C4_ALWAYS_INLINE ReadResult find_child_r(csubstr name,      NodeRef *child)       RYML_NOEXCEPT { assert_readable_(); child->m_tree = m_tree; ReadResult result = m_tree->find_child_r(m_id, name, &child->m_id); return result; }; /**< Forward to @ref Tree::find_child_r(). Node must be readable. */
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult find_child_r(csubstr name, ConstNodeRef *child) const RYML_NOEXCEPT { assert_readable_(); child->m_tree = m_tree; ReadResult result = m_tree->find_child_r(m_id, name, &child->m_id); return result; }; /**< Forward to @ref Tree::find_child_r(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      prev_sibling()                   RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->prev_sibling(m_id)}; }        /**< Forward to @ref Tree::prev_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef prev_sibling()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->prev_sibling(m_id)}; }        /**< Forward to @ref Tree::prev_sibling(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      next_sibling()                   RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->next_sibling(m_id)}; }        /**< Forward to @ref Tree::next_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef next_sibling()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->next_sibling(m_id)}; }        /**< Forward to @ref Tree::next_sibling(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      first_sibling()                  RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->first_sibling(m_id)}; }       /**< Forward to @ref Tree::first_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef first_sibling()            const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->first_sibling(m_id)}; }       /**< Forward to @ref Tree::first_sibling(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      last_sibling()                   RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->last_sibling(m_id)}; }        /**< Forward to @ref Tree::last_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef last_sibling()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->last_sibling(m_id)}; }        /**< Forward to @ref Tree::last_sibling(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      sibling(id_type pos)             RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->sibling(m_id, pos)}; }        /**< Forward to @ref Tree::sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef sibling(id_type pos)       const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->sibling(m_id, pos)}; }        /**< Forward to @ref Tree::sibling(). Node must be readable. */

    C4_ALWAYS_INLINE ReadResult   sibling_r(id_type pos,      NodeRef *sibling)       RYML_NOEXCEPT { assert_readable_(); sibling->m_tree = m_tree; ReadResult result = m_tree->sibling_r(m_id,  pos, &sibling->m_id); return result; }; /**< Forward to @ref Tree::sibling_r(). Node must be readable. */
    C4_ALWAYS_INLINE ReadResult   sibling_r(id_type pos, ConstNodeRef *sibling) const RYML_NOEXCEPT { assert_readable_(); sibling->m_tree = m_tree; ReadResult result = m_tree->sibling_r(m_id,  pos, &sibling->m_id); return result; }; /**< Forward to @ref Tree::sibling_r(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      find_sibling(csubstr name)       RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->find_sibling(m_id, name)}; }  /**< Forward to @ref Tree::find_sibling(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef find_sibling(csubstr name) const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->find_sibling(m_id, name)}; }  /**< Forward to @ref Tree::find_sibling(). Node must be readable. */

    C4_ALWAYS_INLINE ReadResult   find_sibling_r(csubstr name,      NodeRef *sibling)       RYML_NOEXCEPT { assert_readable_(); sibling->m_tree = m_tree; ReadResult result = m_tree->find_sibling_r(m_id, name, &sibling->m_id); return result; }; /**< Forward to @ref Tree::find_sibling_r(). Node must be readable. */
    C4_ALWAYS_INLINE ReadResult   find_sibling_r(csubstr name, ConstNodeRef *sibling) const RYML_NOEXCEPT { assert_readable_(); sibling->m_tree = m_tree; ReadResult result = m_tree->find_sibling_r(m_id, name, &sibling->m_id); return result; }; /**< Forward to @ref Tree::find_sibling_r(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      ancestor_doc()                   RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->ancestor_doc(m_id)}; }        /**< Forward to @ref Tree::ancestor_doc(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef ancestor_doc()             const RYML_NOEXCEPT { assert_readable_(); return {m_tree, m_tree->ancestor_doc(m_id)}; }        /**< Forward to @ref Tree::ancestor_doc(). Node must be readable. */

    C4_ALWAYS_INLINE NodeRef      doc(id_type i)                   RYML_NOEXCEPT { RYML_ASSERT_BASIC_(m_tree); return {m_tree, m_tree->doc(i)}; }            /**< Forward to @ref Tree::doc(). Node must be readable. */
    C4_ALWAYS_INLINE ConstNodeRef doc(id_type i)             const RYML_NOEXCEPT { RYML_ASSERT_BASIC_(m_tree); return {m_tree, m_tree->doc(i)}; }            /**< Forward to @ref Tree::doc(). Node must be readable. succeeds even when the node may have invalid or seed id */

    /** @} */

public:

    /** @name square_brackets
     * operator[] */
    /** @{ */

    /** Find child by key; complexity is O(num_children).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be used to write
     * to the tree provided that its create() method is called prior
     * to writing, which happens in most modifying methods in
     * NodeRef. It is the caller's responsibility to verify that the
     * returned node is readable before subsequently using it to read
     * from the tree.
     *
     * @warning the calling object must be readable. This precondition
     * is asserted. The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to call this method if the node is not readable.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389 */
    C4_ALWAYS_INLINE NodeRef operator[] (csubstr key) RYML_NOEXCEPT
    {
        assert_readable_();
        id_type ch = m_tree->find_child(m_id, key);
        return ch != NONE ? NodeRef(m_tree, ch) : NodeRef(m_tree, m_id, key);
    }

    /** Find child by position; complexity is O(pos).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be used to write
     * to the tree provided that its create() method is called prior
     * to writing, which happens in most modifying methods in
     * NodeRef. It is the caller's responsibility to verify that the
     * returned node is readable before subsequently using it to read
     * from the tree.
     *
     * @warning the calling object must be readable. This precondition
     * is asserted. The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to call this method if the node is not readable.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389 */
    C4_ALWAYS_INLINE NodeRef operator[] (id_type pos) RYML_NOEXCEPT
    {
        assert_readable_();
        id_type ch = m_tree->child(m_id, pos);
        return ch != NONE ? NodeRef(m_tree, ch) : NodeRef(m_tree, m_id, pos);
    }

    /** Find a child by key; complexity is O(num_children).
     *
     * Behaves similar to the non-const overload, but further asserts
     * that the returned node is readable (because it can never be in
     * a seed state). The assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to use the return value if it is not valid.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389  */
    C4_ALWAYS_INLINE ConstNodeRef operator[] (csubstr key) const RYML_NOEXCEPT
    {
        assert_readable_();
        id_type ch = m_tree->find_child(m_id, key);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** Find a child by position; complexity is O(pos).
     *
     * Behaves similar to the non-const overload, but further asserts
     * that the returned node is readable (because it can never be in
     * a seed state). This assertion is performed only if @ref
     * RYML_USE_ASSERT is set to true. As with the non-const overload,
     * it is UB to use the return value if it is not valid.
     *
     * @see https://github.com/biojppm/rapidyaml/issues/389  */
    C4_ALWAYS_INLINE ConstNodeRef operator[] (id_type pos) const RYML_NOEXCEPT
    {
        assert_readable_();
        id_type ch = m_tree->child(m_id, pos);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** @} */

public:

    /** @name at
     *
     * These functions are the analogue to operator[], with the
     * difference that they emit an error instead of an
     * assertion. That is, if any of the pre or post conditions is
     * violated, an error is always emitted (resulting in a call to
     * the error callback).
     *
     * @{ */

    /** Find child by key; complexity is O(num_children).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be subsequently
     * used to write to the tree provided that its create() method is
     * called prior to writing, which happens inside most mutating
     * methods in NodeRef. It is the caller's responsibility to verify
     * that the returned node is readable before subsequently using it
     * to read from the tree.
     *
     * @warning This method will call the error callback (regardless
     * of build type or of the value of RYML_USE_ASSERT) whenever any
     * of the following preconditions is violated: a) the object is
     * valid (points at a tree and a node), b) the calling object must
     * be readable (must not be in seed state), c) the calling object
     * must be pointing at a MAP node. The preconditions are similar
     * to the non-const operator[](csubstr), but instead of using
     * assertions, this function directly checks those conditions and
     * calls the error callback if any of the checks fail.
     *
     * @note since it is valid behavior for the returned node to be in
     * seed state, the error callback is not invoked when this
     * happens. */
    C4_ALWAYS_INLINE NodeRef at(csubstr key)
    {
        RYML_CHECK_BASIC_(m_tree != nullptr);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (m_id >= 0 && m_id < m_tree->capacity()), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, readable(), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, m_tree->is_map(m_id), m_tree, m_id);
        id_type ch = m_tree->find_child(m_id, key);
        return ch != NONE ? NodeRef(m_tree, ch) : NodeRef(m_tree, m_id, key);
    }

    /** Find child by position; complexity is O(pos).
     *
     * Returns the requested node, or an object in seed state if no
     * such child is found (see @ref NodeRef for an explanation of
     * what is seed state). When the object is in seed state, using it
     * to read from the tree is UB. The seed node can be used to write
     * to the tree provided that its create() method is called prior
     * to writing, which happens in most modifying methods in
     * NodeRef. It is the caller's responsibility to verify that the
     * returned node is readable before subsequently using it to read
     * from the tree.
     *
     * @warning This method will call the error callback (regardless
     * of build type or of the value of RYML_USE_ASSERT) whenever any
     * of the following preconditions is violated: a) the object is
     * valid (points at a tree and a node), b) the calling object must
     * be readable (must not be in seed state), c) the calling object
     * must be pointing at a MAP node. The preconditions are similar
     * to the non-const operator[](id_type), but instead of using
     * assertions, this function directly checks those conditions and
     * calls the error callback if any of the checks fail.
     *
     * @note since it is valid behavior for the returned node to be in
     * seed state, the error callback is not invoked when this
     * happens. */
    C4_ALWAYS_INLINE NodeRef at(id_type pos)
    {
        RYML_CHECK_BASIC_(m_tree != nullptr);
        const id_type cap = m_tree->capacity();
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (m_id >= 0 && m_id < cap), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (pos >= 0 && pos < cap), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, readable(), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, m_tree->is_container(m_id), m_tree, m_id);
        id_type ch = m_tree->child(m_id, pos);
        return ch != NONE ? NodeRef(m_tree, ch) : NodeRef(m_tree, m_id, pos);
    }

    /** Get a child by name, with error checking; complexity is
     * O(num_children).
     *
     * Behaves as operator[](csubstr) const, but always raises an
     * error (even when RYML_USE_ASSERT is set to false) when the
     * returned node does not exist, or when this node is not
     * readable, or when it is not a map. This behaviour is similar to
     * std::vector::at(), but the error consists in calling the error
     * callback instead of directly raising an exception. */
    ConstNodeRef at(csubstr key) const
    {
        RYML_CHECK_BASIC_(m_tree != nullptr);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (m_id >= 0 && m_id < m_tree->capacity()), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, readable(), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, m_tree->is_map(m_id), m_tree, m_id);
        id_type ch = m_tree->find_child(m_id, key);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** Get a child by position, with error checking; complexity is
     * O(pos).
     *
     * Behaves as operator[](id_type) const, but always raises an error
     * (even when RYML_USE_ASSERT is set to false) when the returned
     * node does not exist, or when this node is not readable, or when
     * it is not a container. This behaviour is similar to
     * std::vector::at(), but the error consists in calling the error
     * callback instead of directly raising an exception. */
    ConstNodeRef at(id_type pos) const
    {
        RYML_CHECK_BASIC_(m_tree != nullptr);
        const id_type cap = m_tree->capacity();
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (m_id >= 0 && m_id < cap), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, (pos >= 0 && pos < cap), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, readable(), m_tree, m_id);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, m_tree->is_container(m_id), m_tree, m_id);
        const id_type ch = m_tree->child(m_id, pos);
        RYML_CHECK_VISIT_CB_(m_tree->m_callbacks, ch != NONE, m_tree, m_id);
        return {m_tree, ch};
    }

    /** @} */

public:

    /** @name modification of hierarchy */
    /** @{ */

    NodeRef insert_child(NodeRef after)
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, after.m_tree == m_tree, m_tree, m_id);
        NodeRef r(m_tree, m_tree->insert_child(m_id, after.m_id));
        return r;
    }

    NodeRef prepend_child()
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->insert_child(m_id, NONE));
        return r;
    }

    NodeRef append_child()
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->append_child(m_id));
        return r;
    }

    NodeRef insert_sibling(ConstNodeRef const& after)
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, after.m_tree == m_tree, m_tree, m_id);
        NodeRef r(m_tree, m_tree->insert_sibling(m_id, after.m_id));
        return r;
    }

    NodeRef prepend_sibling()
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->prepend_sibling(m_id));
        return r;
    }

    NodeRef append_sibling()
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->append_sibling(m_id));
        return r;
    }

public:

    void remove_child(NodeRef & child)
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, has_child(child), m_tree, m_id);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, child.parent().id() == id(), m_tree, m_id);
        m_tree->remove(child.id());
        child = NodeRef{};
    }

    //! remove the nth child of this node
    void remove_child(id_type pos)
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, pos >= 0 && pos < num_children(), m_tree, m_id);
        id_type child = m_tree->child(m_id, pos);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, child != NONE, m_tree, m_id);
        m_tree->remove(child);
    }

    //! remove a child by name
    void remove_child(csubstr key)
    {
        assert_readable_();
        id_type child = m_tree->find_child(m_id, key);
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, child != NONE, m_tree, m_id);
        m_tree->remove(child);
    }

public:

    /** move the node to a different @p parent (which may belong to a
     * different tree), placing it after @p after. When the
     * destination parent is in a new tree, then this node's tree
     * pointer is reset to the tree of the parent node. */
    void move(NodeRef const& parent, ConstNodeRef const& after)
    {
        assert_readable_();
        parent.assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, parent.m_tree == after.m_tree || after.m_id == NONE, m_tree, m_id);
        if(parent.m_tree == m_tree)
        {
            m_tree->move(m_id, parent.m_id, after.m_id);
        }
        else
        {
            parent.m_tree->move(m_tree, m_id, parent.m_id, after.m_id);
            m_tree = parent.m_tree;
        }
    }

    /** duplicate the current node somewhere into a different @p parent
     * (possibly from a different tree), and place it after the node
     * @p after. To place into the first position of the parent,
     * simply pass an empty or default-constructed reference like
     * this: `n.move({})`. */
    NodeRef duplicate(NodeRef const& parent, ConstNodeRef const& after) const
    {
        assert_readable_();
        parent.assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, parent.m_tree == after.m_tree || after.m_id == NONE, m_tree, m_id);
        id_type dup = parent.m_tree->duplicate(m_tree, m_id, parent.m_id, after.m_id);
        NodeRef r(parent.m_tree, dup);
        return r;
    }

    NodeRef duplicate_children(NodeRef const& parent, ConstNodeRef const& after) const
    {
        assert_readable_();
        parent.assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, parent.m_tree == after.m_tree || after.m_id == NONE, m_tree, m_id);
        id_type last_dup = parent.m_tree->duplicate_children(m_tree, m_id, parent.m_id, after.m_id);
        NodeRef r(parent.m_tree, last_dup);
        return r;
    }

    /** @} */

public:

    /** @name iteration */
    /** @{ */

    using iterator = detail::child_iterator<NodeRef>;
    using const_iterator = detail::child_iterator<ConstNodeRef>;
    using children_view = detail::children_view<NodeRef>;
    using const_children_view = detail::children_view<ConstNodeRef>;


    C4_ALWAYS_INLINE       iterator begin()        RYML_NOEXCEPT { assert_readable_(); return iterator(m_tree, m_tree->first_child(m_id)); }       /**< get a mutable iterator to the first child. NOT AVAILABLE for ConstNodeRef. */
    C4_ALWAYS_INLINE const_iterator begin()  const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, m_tree->first_child(m_id)); } /**< get an iterator to the first child */
    C4_ALWAYS_INLINE const_iterator cbegin() const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, m_tree->first_child(m_id)); } /**< get an iterator to the first child */

    C4_ALWAYS_INLINE       iterator end()        RYML_NOEXCEPT { assert_readable_(); return iterator(m_tree, NONE); }       /**< get an iterator to after the last child. NOT AVAILABLE for ConstNodeRef. */
    C4_ALWAYS_INLINE const_iterator end()  const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, NONE); } /**< get an iterator to after the last child */
    C4_ALWAYS_INLINE const_iterator cend() const RYML_NOEXCEPT { assert_readable_(); return const_iterator(m_tree, NONE); } /**< get an iterator to after the last child */


    C4_ALWAYS_INLINE       children_view children()        RYML_NOEXCEPT { assert_readable_(); return detail::make_children_view<children_view>(m_tree, m_id); }       /**< get an iterable view over children */
    C4_ALWAYS_INLINE const_children_view children()  const RYML_NOEXCEPT { assert_readable_(); return detail::make_children_view<const_children_view>(m_tree, m_id); } /**< get an iterable view over children */
    C4_ALWAYS_INLINE const_children_view cchildren() const RYML_NOEXCEPT { assert_readable_(); return detail::make_children_view<const_children_view>(m_tree, m_id); } /**< get an iterable view over children */

    C4_ALWAYS_INLINE       children_view siblings()        RYML_NOEXCEPT { assert_readable_(); return detail::make_siblings_view<children_view>(m_tree, m_id); }       /** get an iterable view over all siblings (including the calling node) */
    C4_ALWAYS_INLINE const_children_view siblings()  const RYML_NOEXCEPT { assert_readable_(); return detail::make_siblings_view<const_children_view>(m_tree, m_id); } /** get an iterable view over all siblings (including the calling node) */
    C4_ALWAYS_INLINE const_children_view csiblings() const RYML_NOEXCEPT { assert_readable_(); return detail::make_siblings_view<const_children_view>(m_tree, m_id); } /** get an iterable view over all siblings (including the calling node) */

    /** @} */

public:

    /** @name legacy operators
     *
     * these methods will be removed in future releases. If you
     * disagree with a particular function being deprecated, let us
     * know by opening a new issue at
     * https://github.com/biojppm/rapidyaml/issues
     */
    /** @{ */

    RYML_LEGACY_OPERATOR(".use the appropriate .set_*() overload")
    void operator= (type_bits t) { create(); m_tree->_p(m_id)->m_type = t; }

    RYML_LEGACY_OPERATOR(".use the appropriate .set_*() overload")
    void operator|= (type_bits t) { create(); m_tree->_add_flags(m_id, t); }

    RYML_LEGACY_OPERATOR("use .set_val()")
    NodeRef& operator= (csubstr v) { set_val(v); return *this; }

    template<size_t N>
    RYML_LEGACY_OPERATOR("use .set_val()")
    NodeRef& operator= (const char (&v)[N]) { csubstr sv; sv.assign<N>(v); set_val(sv); return *this; }

    RYML_LEGACY_OPERATOR("use .set_val()")
    NodeRef& operator= (std::nullptr_t) { set_val(csubstr{}); return *this; }

    template<class T>
    RYML_LEGACY_OPERATOR("use .set_key()")
    NodeRef& operator= (Key<T> const& C4_RESTRICT v) { set_key(v.k); return *this; }

    template<class T>
    RYML_LEGACY_OPERATOR("use .save()")
    NodeRef& operator<< (T const& C4_RESTRICT v) { save(v); return *this; }

    RYML_LEGACY_OPERATOR("use .save()")
    NodeRef& operator<< (csubstr v) { save(v); return *this; }

    template<class T>
    RYML_LEGACY_OPERATOR("use .save_key()")
    NodeRef& operator<< (Key<T> const& C4_RESTRICT v) { save_key(v.k); return *this; }

    /** @} */

public: // deprecated functions

    // these methods will be removed in future releases. If you
    // disagree with a particular function being deprecated, let us
    // know by opening a new issue at
    // https://github.com/biojppm/rapidyaml/issues

    /** @cond dev */ // LCOV_EXCL_START
    C4_SUPPRESS_WARNING_PUSH
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated")
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated-declarations")
    C4_SUPPRESS_WARNING_MSVC(4996) // deprecated
    RYML_DEPRECATED("use NodeRef()") NodeRef(std::nullptr_t) noexcept : m_tree(nullptr), m_id(NONE), m_seed() {}

    RYML_DEPRECATED("use one of readable(), is_seed() or !invalid()") inline bool valid() const { return m_tree != nullptr && m_id != NONE; }
    RYML_DEPRECATED("use !readable()") bool operator== (std::nullptr_t) const { return m_tree == nullptr || m_id == NONE || is_seed(); }
    RYML_DEPRECATED("use readable()")  bool operator!= (std::nullptr_t) const { return !(m_tree == nullptr || m_id == NONE || is_seed()); }
    RYML_DEPRECATED("use `this->val() == s`") bool operator== (csubstr s) const { assert_readable_(); RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, has_val(), m_tree, m_id); return m_tree->val(m_id) == s; }
    RYML_DEPRECATED("use `this->val() != s`") bool operator!= (csubstr s) const { assert_readable_(); RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, has_val(), m_tree, m_id); return m_tree->val(m_id) != s; }

    RYML_DEPRECATED("use .set_*()") void operator= (NodeInit const& v)
    {
        create();
        _apply(v);
    }

    RYML_DEPRECATED("use .set_*()") void operator= (NodeScalar const& v)
    {
        create();
        _apply(v);
    }

    RYML_DEPRECATED("") void clear()
    {
        assert_readable_();
        m_tree->remove_children(m_id);
        m_tree->_clear(m_id);
    }

    RYML_DEPRECATED("") void _apply(csubstr v)
    {
        m_tree->set_val(m_id, v);
    }

    RYML_DEPRECATED("") void _apply(NodeScalar const& v)
    {
        m_tree->_set_val(m_id, v);
    }

    RYML_DEPRECATED("") void _apply(NodeInit const& i)
    {
        m_tree->_set(m_id, i);
    }

    RYML_DEPRECATED("") NodeRef insert_child(NodeInit const& i, NodeRef after)
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, after.m_tree == m_tree, m_tree, m_id);
        NodeRef r(m_tree, m_tree->insert_child(m_id, after.m_id));
        r._apply(i);
        return r;
    }

    RYML_DEPRECATED("") NodeRef prepend_child(NodeInit const& i)
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->insert_child(m_id, NONE));
        r._apply(i);
        return r;
    }

    RYML_DEPRECATED("") NodeRef append_child(NodeInit const& i)
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->append_child(m_id));
        r._apply(i);
        return r;
    }

    RYML_DEPRECATED("") NodeRef insert_sibling(NodeInit const& i, ConstNodeRef const& after)
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, after.m_tree == m_tree, m_tree, m_id);
        NodeRef r(m_tree, m_tree->insert_sibling(m_id, after.m_id));
        r._apply(i);
        return r;
    }

    RYML_DEPRECATED("") NodeRef prepend_sibling(NodeInit const& i)
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->prepend_sibling(m_id));
        r._apply(i);
        return r;
    }

    RYML_DEPRECATED("") NodeRef append_sibling(NodeInit const& i)
    {
        assert_readable_();
        NodeRef r(m_tree, m_tree->append_sibling(m_id));
        r._apply(i);
        return r;
    }

    RYML_DEPRECATED("") void move(ConstNodeRef const& after)
    {
        assert_readable_();
        m_tree->move(m_id, after.m_id);
    }

    RYML_DEPRECATED("") NodeRef duplicate(ConstNodeRef const& after) const
    {
        assert_readable_();
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_tree == after.m_tree || after.m_id == NONE, m_tree, m_id);
        id_type dup = m_tree->duplicate(m_id, m_tree->parent(m_id), after.m_id);
        NodeRef r(m_tree, dup);
        return r;
    }

    C4_SUPPRESS_WARNING_POP

    void _add_flags(NodeType f)
    {
        assert_readable_();
        m_tree->_add_flags(m_id, f);
    }
    /** @endcond */ // LCOV_EXCL_STOP

};

// NOLINTEND(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)

/** @} */


//-----------------------------------------------------------------------------

inline ConstNodeRef::ConstNodeRef(NodeRef const& that) noexcept
    : m_tree(that.m_tree)
    , m_id(!that.is_seed() ? that.id() : (id_type)NONE)
{
}

inline ConstNodeRef::ConstNodeRef(NodeRef && that) noexcept // NOLINT
    : m_tree(that.m_tree)
    , m_id(!that.is_seed() ? that.id() : (id_type)NONE)
{
}


inline ConstNodeRef& ConstNodeRef::operator= (NodeRef const& that) noexcept
{
    m_tree = (that.m_tree);
    m_id = (!that.is_seed() ? that.id() : (id_type)NONE);
    return *this;
}

inline ConstNodeRef& ConstNodeRef::operator= (NodeRef && that) noexcept // NOLINT
{
    m_tree = (that.m_tree);
    m_id = (!that.is_seed() ? that.id() : (id_type)NONE);
    return *this;
}


//-----------------------------------------------------------------------------

/** @addtogroup doc_serialization_node_read
 *
 * freestanding read() and read_key() implementing ryml's built-in
 * deserialization of fundamental types.
 *
 * @{
 */


template<class T>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read(ConstNodeRef const& C4_RESTRICT n, T *v)
{
    // defer to the tree implementation
    return n.m_tree->deserialize(n.m_id, v);
}
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read(ConstNodeRef const& C4_RESTRICT n, T const& wrapper)
{
    // defer to the tree implementation
    return n.m_tree->deserialize(n.m_id, wrapper);
}


template<class T>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read_key(ConstNodeRef const& C4_RESTRICT n, T *v)
{
    // defer to the tree implementation
    return n.m_tree->deserialize_key(n.m_id, v);
}
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read_key(ConstNodeRef const& C4_RESTRICT n, T const& wrapper)
{
    // defer to the tree implementation
    return n.m_tree->deserialize_key(n.m_id, wrapper);
}

/** @} */


//-----------------------------------------------------------------------------

/** @addtogroup doc_serialization_node_write
 *
 * freestanding write() and write_key() implementing ryml's
 * built-in serialization of fundamental types.
 *
 * @{
 */

template<class T>
C4_ALWAYS_INLINE void write(NodeRef *n, T const& v)
{
    n->tree()->set_serialized(n->id(), v); // defer to the tree implementation
}
template<class T>
C4_ALWAYS_INLINE void write_key(NodeRef *n, T const& v)
{
    n->tree()->set_key_serialized(n->id(), v); // defer to the tree implementation
}

template<class T>
C4_ALWAYS_INLINE void write(NodeRef &n, T const& v)
{
    // forward to the pointer call, to ensure the user's function is
    // called regardless if it is write(NodeRef*,T), write(NodeRef&,T)
    // or write(NodeRef,T)
    write(&n, v);
}
template<class T>
C4_ALWAYS_INLINE void write_key(NodeRef &n, T const& v)
{
    // forward to the pointer call, to ensure the user's function is
    // called regardless if it is write(NodeRef*,T), write(NodeRef&,T)
    // or write(NodeRef,T)
    write_key(&n, v);
}

/** @} */


} // namespace yml
} // namespace c4

// NOLINTEND(modernize-avoid-c-style-cast)

C4_SUPPRESS_WARNING_POP

#endif /* C4_YML_NODE_HPP_ */
