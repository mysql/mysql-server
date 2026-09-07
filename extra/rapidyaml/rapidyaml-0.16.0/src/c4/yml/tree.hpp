#ifndef C4_YML_TREE_HPP_
#define C4_YML_TREE_HPP_

/** @file tree.hpp */

#ifndef C4_ERROR_HPP_
#include "c4/error.hpp"
#endif
#ifndef C4_LANGUAGE_HPP_
#include "c4/language.hpp"
#endif
#ifndef C4_YML_FWD_HPP_
#include "c4/yml/fwd.hpp"
#endif
#ifndef C4_YML_COMMON_HPP_
#include "c4/yml/common.hpp"
#endif
#ifndef C4_YML_NODE_TYPE_HPP_
#include "c4/yml/node_type.hpp"
#endif
#ifndef C4_YML_TAG_HPP_
#include "c4/yml/tag.hpp"
#endif
#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif
#ifndef C4_YML_SCALAR_CHARCONV_HPP_
#include "c4/yml/scalar_charconv.hpp"
#endif

#include <math.h>
#include <limits.h>


C4_SUPPRESS_WARNING_PUSH
C4_SUPPRESS_WARNING_GCC("-Wtype-limits")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")
C4_SUPPRESS_WARNING_GCC_CLANG("-Wold-style-cast")
#if defined(__GNUC__) && (__GNUC__ >= 6)
C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
#endif
C4_SUPPRESS_WARNING_CLANG("-Wnull-dereference")
C4_SUPPRESS_WARNING_CLANG("-Wtautological-compare")
C4_SUPPRESS_WARNING_MSVC(4127) // conditional expression is constant
C4_SUPPRESS_WARNING_MSVC(4296) // expression is always 'boolean_value'
C4_SUPPRESS_WARNING_MSVC(4251) // needs to have dll-interface
// NOLINTBEGIN(modernize-avoid-c-style-cast)


namespace c4 {
namespace yml {


/** @cond dev */
template<class T>
using _is_string_nocvref = is_string<typename detail::_remove_cvref<T>::type>;
/** @endcond */


/** @addtogroup doc_serialization_tree_write_arena
 *
 * @{
 */


/** Serialize a scalar to the tree's arena. This is an implementation
 * helper for @ref serialize_to_arena(), serializing through
 * @ref scalar_serialize(). */
template<class T>
csubstr serialize_to_arena_scalar(Tree * tree, T const& scalar);


/** Serialize a string type (as specified by @ref c4::is_string) to a
 * tree's arena, ensuring that there is an entry for the string in the
 * arena even if the string is empty. This is an implementation helper
 * for @ref serialize_to_arena(), serializing through @ref
 * scalar_serialize() and then ensuring that the serialized string
 * will be placed in the arena, even if the string is zero-length. */
RYML_EXPORT csubstr serialize_to_arena_str(Tree * tree, csubstr scalar);



#if (C4_CPP >= 17) || defined(__DOXYGEN__)
/** Serialize a scalar to a tree's arena, dispatching to either @ref
 * serialize_to_arena_scalar() or @ref serialize_to_arena_str() when
 * the type is a string according to @ref c4::is_string. This is the
 * entry point for customizing how a scalar is serialized to a tree's
 * arena. It is never needed for the user to call this function, and
 * generally there is no reason for overriding this function for user
 * types, unless it has specific requirements for the tree's arena, as
 * happens for example with string types. For user string types,
 * defining @ref c4::is_string is enough. For example:
 *
 * @note When using a standard older than C++17, `if constexpr` is not
 * available, and the implementation reverts to SFINAE to achieve the
 * compile-time dispatch.
 *
 * @code{c++}
 * namespace foo {
 * class MyStringType {...}; // an example of a user-defined string type
 * // define conversion to/from substrings
 * c4::yml::csubstr to_csubstr(MyStringType const& s) { return ...; }
 * c4::yml::substr to_substr(MyStringType & s) { return ...; }
 * } // namespace foo
 * // tell ryml to treat this type as a string
 * template<> struct c4::is_string<foo::MyStringType> : std::true_type {};
 * @endcode */
template<class T>
C4_ALWAYS_INLINE csubstr serialize_to_arena(Tree * tree, T const& scalar)
{
    if constexpr (_is_string_nocvref<T>::value)
        return serialize_to_arena_str(tree, to_csubstr(scalar));
    else
        return serialize_to_arena_scalar<T>(tree, scalar);
}

#else // pre-C++17: need to use SFINAE

template<class T>
C4_ALWAYS_INLINE auto serialize_to_arena(Tree * tree, T const& scalar)
    -> typename std::enable_if<_is_string_nocvref<T>::value, csubstr>::type
{
    return serialize_to_arena_str(tree, to_csubstr(scalar));
}

template<class T>
C4_ALWAYS_INLINE auto serialize_to_arena(Tree * tree, T const& scalar)
    -> typename std::enable_if< ! _is_string_nocvref<T>::value, csubstr>::type
{
    return serialize_to_arena_scalar<T>(tree, scalar);
}

#endif // pre-C++17: need to use SFINAE

/** implementation for null values */
C4_ALWAYS_INLINE csubstr serialize_to_arena(Tree * C4_RESTRICT, std::nullptr_t /*scalar*/) noexcept
{
    return csubstr{};
}

/** @} */


//-----------------------------------------------------------------------------
// fwd decl: write/read, write_key/read_key

/** @cond dev */

template<class T> void write(Tree * tree, id_type id, T const& v);
template<class T> void write_key(Tree *, id_type id, T const& v);

template<class T> C4_NODISCARD C4_ALWAYS_INLINE ReadResult read(Tree const* tree, id_type id, T * v);
template<class T> C4_NODISCARD C4_ALWAYS_INLINE ReadResult read_key(Tree const* tree, id_type id, T * v);
template<class Wrapper> C4_NODISCARD C4_ALWAYS_INLINE ReadResult read(Tree const* tree, id_type id, Wrapper const& w);
template<class Wrapper> C4_NODISCARD C4_ALWAYS_INLINE ReadResult read_key(Tree const* tree, id_type id, Wrapper const& w);
/** @endcond */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @addtogroup doc_tree
 *
 * @{
 */

/** a node scalar is a csubstr, which may be tagged and anchored. */
struct NodeScalar
{
    csubstr tag;
    csubstr scalar;
    csubstr anchor;

public:

    /// initialize as an empty scalar
    NodeScalar() noexcept : tag(), scalar(), anchor() {} // NOLINT

    /// initialize as an untagged scalar
    template<size_t N>
    NodeScalar(const char (&s)[N]) noexcept : tag(), scalar(s), anchor() {}
    NodeScalar(csubstr      s    ) noexcept : tag(), scalar(s), anchor() {}

    /// initialize as a tagged scalar
    template<size_t N, size_t M>
    NodeScalar(const char (&t)[N], const char (&s)[N]) noexcept : tag(t), scalar(s), anchor() {}
    NodeScalar(csubstr      t    , csubstr      s    ) noexcept : tag(t), scalar(s), anchor() {}

public:

    ~NodeScalar() noexcept = default;
    NodeScalar(NodeScalar &&) noexcept = default;
    NodeScalar(NodeScalar const&) noexcept = default;
    NodeScalar& operator= (NodeScalar &&) noexcept = default;
    NodeScalar& operator= (NodeScalar const&) noexcept = default;

public:

    bool empty() const noexcept { return tag.empty() && scalar.empty() && anchor.empty(); }

    void clear() noexcept { tag.clear(); scalar.clear(); anchor.clear(); }

    void set_ref_maybe_replacing_scalar(csubstr ref, bool has_scalar) RYML_NOEXCEPT
    {
        csubstr trimmed = ref.begins_with('*') ? ref.sub(1) : ref;
        anchor = trimmed;
        if((!has_scalar) || !scalar.ends_with(trimmed))
            scalar = ref;
    }
};
static_assert(std::is_trivially_copyable<NodeScalar>::value, "must be trivially copyable");


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @cond dev */ // LCOV_EXCL_START
struct RYML_DEPRECATED("") NodeInit
{

    NodeType   type;
    NodeScalar key;
    NodeScalar val;

public:

    /// initialize as an empty node
    NodeInit() : type(NOTYPE), key(), val() {}
    /// initialize as a typed node
    NodeInit(type_bits t) : type(t), key(), val() {}
    /// initialize as a sequence member
    NodeInit(NodeScalar const& v) : type(VAL), key(), val(v) { _add_flags(); }
    /// initialize as a sequence member with explicit type
    NodeInit(NodeScalar const& v, type_bits t) : type(t|VAL), key(), val(v) { _add_flags(); }
    /// initialize as a mapping member
    NodeInit(              NodeScalar const& k, NodeScalar const& v) : type(KEYVAL), key(k), val(v) { _add_flags(); }
    /// initialize as a mapping member with explicit type
    NodeInit(type_bits t, NodeScalar const& k, NodeScalar const& v) : type(t), key(k), val(v) { _add_flags(); }
    /// initialize as a mapping member with explicit type (eg for SEQ or MAP)
    NodeInit(type_bits t, NodeScalar const& k                     ) : type(t), key(k), val( ) { _add_flags(KEY); }

public:

    void clear()
    {
        type.clear();
        key.clear();
        val.clear();
    }

    void _add_flags(type_bits more_flags=0)
    {
        type = (type.m_bits|more_flags);
        if( ! key.tag.empty())
            type = (type.m_bits|KEYTAG);
        if( ! val.tag.empty())
            type = (type.m_bits|VALTAG);
        if( ! key.anchor.empty())
            type = (type.m_bits|KEYANCH);
        if( ! val.anchor.empty())
            type = (type.m_bits|VALANCH);
    }

    bool _check() const
    {
        // key cannot be empty
        RYML_ASSERT_BASIC_(key.scalar.empty() == ((type & KEY) == 0));
        // key tag cannot be empty
        RYML_ASSERT_BASIC_(key.tag.empty() == ((type & KEYTAG) == 0));
        // val may be empty even though VAL is set. But when VAL is not set, val must be empty
        RYML_ASSERT_BASIC_(((type & VAL) != 0) || val.scalar.empty());
        // val tag cannot be empty
        RYML_ASSERT_BASIC_(val.tag.empty() == ((type & VALTAG) == 0));
        return true;
    }
};
/** @endcond */ // LCOV_EXCL_STOP


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** contains the data for each YAML node. */
struct NodeData
{
    NodeType   m_type;

    NodeScalar m_key;
    NodeScalar m_val;

    id_type    m_parent;
    id_type    m_first_child;
    id_type    m_last_child;
    id_type    m_next_sibling;
    id_type    m_prev_sibling;
};
static_assert(std::is_trivially_copyable<NodeData>::value, "must be trivially copyable");


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class RYML_EXPORT Tree
{
public:

    /** @name construction and assignment */
    /** @{ */

    Tree() : Tree(get_callbacks()) {}
    Tree(Callbacks const& cb);
    Tree(id_type node_capacity, size_t arena_capacity=RYML_DEFAULT_TREE_ARENA_CAPACITY) : Tree(node_capacity, arena_capacity, get_callbacks()) {}
    Tree(id_type node_capacity, size_t arena_capacity, Callbacks const& cb);

    ~Tree() noexcept;

    Tree(Tree const& that);
    Tree(Tree     && that) noexcept;

    Tree& operator= (Tree const& that);
    Tree& operator= (Tree     && that) noexcept;

    /** @} */

public:

    /** @name memory and sizing */
    /** @{ */

    void reserve(id_type node_capacity=RYML_DEFAULT_TREE_CAPACITY);

    /** clear the tree and zero every node
     * @note does NOT clear the arena
     * @see clear_arena() */
    void clear();
    void clear_arena() { m_arena_pos = 0; }

    /** Query for zero size. The tree can be empty only when constructed with explicitly zero-capacity. */
    bool empty() const { return m_size == 0; }

    id_type size() const { return m_size; }
    id_type capacity() const { return m_cap; }
    id_type slack() const { RYML_ASSERT_BASIC_(m_cap >= m_size); return m_cap - m_size; }

    Callbacks const& callbacks() const { return m_callbacks; }
    void callbacks(Callbacks const& cb) { m_callbacks = cb; }

    /** @} */

public:

    /** @name node getters */
    /** @{ */

    //! get a pointer to a node's NodeData.
    //! node can be NONE, in which case a nullptr is returned
    NodeData *get(id_type node) // NOLINT(readability-make-member-function-const)
    {
        if(node == NONE)
            return nullptr;
        RYML_ASSERT_VISIT_CB_(m_callbacks, node >= 0 && node < m_cap, this, node);
        return m_buf + node;
    }
    //! get a pointer to a node's NodeData.
    //! node can be NONE, in which case a nullptr is returned.
    NodeData const *get(id_type node) const
    {
        if(node == NONE)
            return nullptr;
        RYML_ASSERT_VISIT_CB_(m_callbacks, node >= 0 && node < m_cap, this, node);
        return m_buf + node;
    }

    //! An if-less form of get() that demands a valid node index.
    //! This function is implementation only; use at your own risk.
    NodeData       * _p(id_type node)       { RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node); return m_buf + node; } // NOLINT(readability-make-member-function-const)
    //! An if-less form of get() that demands a valid node index.
    //! This function is implementation only; use at your own risk.
    NodeData const * _p(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node); return m_buf + node; }

    //! Get the id of the root node. The tree must not be empty. The tree can be empty only when constructed with explicitly zero-capacity.
    id_type root_id() const { RYML_ASSERT_VISIT_CB_(m_callbacks, m_size > 0, this, id_type(0)); return 0; }
    //! Get the id of the root node, or NONE if the tree is empty.
    id_type root_id_maybe() const { return m_size ? 0 : id_type(NONE); }
    //! get the id of a node belonging to this tree.
    //! @p n can be nullptr, in which case NONE is returned
    //! @p n must belong to this tree
    id_type id(NodeData const* n) const
    {
        if( ! n)
            return NONE;
        RYML_ASSERT_VISIT_CB_(m_callbacks, n >= m_buf && n < m_buf + m_cap, this, NONE);
        return static_cast<id_type>(n - m_buf);
    }

    /** @} */

public:

    /** @name NodeRef helpers */
    /** @{ */

    //! Get a NodeRef of a node by id
    NodeRef      ref(id_type node);
    //! Get a NodeRef of a node by id
    ConstNodeRef ref(id_type node) const;
    //! Get a NodeRef of a node by id
    ConstNodeRef cref(id_type node) const;

    //! Get the root as a @ref NodeRef . Note that a non-const Tree implicitly converts to @ref NodeRef.
    NodeRef      rootref();
    //! Get the root as a @ref ConstNodeRef . Note that Tree implicitly converts to @ref ConstNodeRef.
    ConstNodeRef rootref() const;
    //! Get the root as a @ref ConstNodeRef . Note that Tree implicitly converts to @ref ConstNodeRef.
    ConstNodeRef crootref() const;

    //! get the i-th document of the stream
    //! @note @p i is NOT the node id, but the doc position within the stream
    NodeRef      docref(id_type i);
    //! get the i-th document of the stream
    //! @note @p i is NOT the node id, but the doc position within the stream
    ConstNodeRef docref(id_type i) const;
    //! get the i-th document of the stream
    //! @note @p i is NOT the node id, but the doc position within the stream
    ConstNodeRef cdocref(id_type i) const;

    //! find a root child (ie child of root) by name, return it as a NodeRef
    //! @note requires the root to be a map.
    NodeRef      operator[] (csubstr key);
    //! find a root child (ie child of root) by name, return it as a NodeRef
    //! @note requires the root to be a map.
    ConstNodeRef operator[] (csubstr key) const;

    //! find a root child (ie child of root) by index: return the root node's @p i-th child as a NodeRef
    //! @note @p i is NOT the node id, but the child's position within the parent
    NodeRef      operator[] (id_type i);
    //! find a root child (ie child of root) by index: return the root node's @p i-th child as a NodeRef
    //! @note @p i is NOT the node id, but the child's position within the parent
    ConstNodeRef operator[] (id_type i) const;

    /** @} */

public:

    /** @name node property getters */
    /** @{ */

    NodeType type(id_type node) const { return _p(node)->m_type; }

    csubstr    const& key       (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node); return _p(node)->m_key.scalar; }
    csubstr    const& key_tag   (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key_tag(node), this, node); return _p(node)->m_key.tag; }
    csubstr    const& key_ref   (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, is_key_ref(node), this, node); return _p(node)->m_key.anchor; }
    csubstr    const& key_anchor(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key_anchor(node), this, node); return _p(node)->m_key.anchor; }
    NodeScalar const& keysc     (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node); return _p(node)->m_key; }

    csubstr    const& val       (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val(node), this, node); return _p(node)->m_val.scalar; }
    csubstr    const& val_tag   (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val_tag(node), this, node); return _p(node)->m_val.tag; }
    csubstr    const& val_ref   (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, is_val_ref(node), this, node); return _p(node)->m_val.anchor; }
    csubstr    const& val_anchor(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val_anchor(node), this, node); return _p(node)->m_val.anchor; }
    NodeScalar const& valsc     (id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val(node), this, node); return _p(node)->m_val; }

    /** @} */

public:

    /** @name node type predicates */
    /** @{ */

    C4_ALWAYS_INLINE bool type_has_any(id_type node, type_bits bits) const { return _p(node)->m_type.has_any(bits); }
    C4_ALWAYS_INLINE bool type_has_all(id_type node, type_bits bits) const { return _p(node)->m_type.has_all(bits); }
    C4_ALWAYS_INLINE bool type_has_none(id_type node, type_bits bits) const { return _p(node)->m_type.has_none(bits); }

    C4_ALWAYS_INLINE bool is_stream(id_type node) const { return _p(node)->m_type.is_stream(); }
    C4_ALWAYS_INLINE bool is_doc(id_type node) const { return _p(node)->m_type.is_doc(); }
    C4_ALWAYS_INLINE bool is_container(id_type node) const { return _p(node)->m_type.is_container(); }
    C4_ALWAYS_INLINE bool is_map(id_type node) const { return _p(node)->m_type.is_map(); }
    C4_ALWAYS_INLINE bool is_seq(id_type node) const { return _p(node)->m_type.is_seq(); }
    C4_ALWAYS_INLINE bool has_key(id_type node) const { return _p(node)->m_type.has_key(); }
    C4_ALWAYS_INLINE bool has_val(id_type node) const { return _p(node)->m_type.has_val(); }
    C4_ALWAYS_INLINE bool is_val(id_type node) const { return _p(node)->m_type.is_val(); }
    C4_ALWAYS_INLINE bool is_keyval(id_type node) const { return _p(node)->m_type.is_keyval(); }
    C4_ALWAYS_INLINE bool has_key_tag(id_type node) const { return _p(node)->m_type.has_key_tag(); }
    C4_ALWAYS_INLINE bool has_val_tag(id_type node) const { return _p(node)->m_type.has_val_tag(); }
    C4_ALWAYS_INLINE bool has_key_anchor(id_type node) const { return _p(node)->m_type.has_key_anchor(); }
    C4_ALWAYS_INLINE bool has_val_anchor(id_type node) const { return _p(node)->m_type.has_val_anchor(); }
    C4_ALWAYS_INLINE bool has_anchor(id_type node) const { return _p(node)->m_type.has_anchor(); }
    C4_ALWAYS_INLINE bool is_key_ref(id_type node) const { return _p(node)->m_type.is_key_ref(); }
    C4_ALWAYS_INLINE bool is_val_ref(id_type node) const { return _p(node)->m_type.is_val_ref(); }
    C4_ALWAYS_INLINE bool is_ref(id_type node) const { return _p(node)->m_type.is_ref(); }

    C4_ALWAYS_INLINE bool parent_is_seq(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_parent(node), this, node); return is_seq(_p(node)->m_parent); }
    C4_ALWAYS_INLINE bool parent_is_map(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_parent(node), this, node); return is_map(_p(node)->m_parent); }

    /** true when the node has an anchor named a */
    C4_ALWAYS_INLINE bool has_anchor(id_type node, csubstr a) const { return _p(node)->m_key.anchor == a || _p(node)->m_val.anchor == a; }

    /** true if the node key is empty, or its scalar verifies @ref scalar_is_null().
     * @warning the node must verify @ref Tree::has_key() (asserted) (ie must be a member of a map)
     * @see https://github.com/biojppm/rapidyaml/issues/413 */
    C4_ALWAYS_INLINE bool key_is_null(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node); NodeData const* C4_RESTRICT n = _p(node); return !n->m_type.is_key_quoted() && (n->m_type.key_is_null() || scalar_is_null(n->m_key.scalar)); }
    /** true if the node val is empty, or its scalar verifies @ref scalar_is_null().
     * @warning the node must verify @ref Tree::has_val() (asserted) (ie must be a scalar / must not be a container)
     * @see https://github.com/biojppm/rapidyaml/issues/413 */
    C4_ALWAYS_INLINE bool val_is_null(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val(node), this, node); NodeData const* C4_RESTRICT n = _p(node); return !n->m_type.is_val_quoted() && (n->m_type.val_is_null() || scalar_is_null(n->m_val.scalar)); }

    /// true if the key was a scalar requiring filtering and was left
    /// unfiltered during the parsing (see ParserOptions)
    C4_ALWAYS_INLINE bool is_key_unfiltered(id_type node) const { return _p(node)->m_type.is_key_unfiltered(); }
    /// true if the val was a scalar requiring filtering and was left
    /// unfiltered during the parsing (see ParserOptions)
    C4_ALWAYS_INLINE bool is_val_unfiltered(id_type node) const { return _p(node)->m_type.is_val_unfiltered(); }

    RYML_DEPRECATED("use has_key_anchor()")    bool is_key_anchor(id_type node) const { return _p(node)->m_type.has_key_anchor(); }
    RYML_DEPRECATED("use has_val_anchor()")    bool is_val_anchor(id_type node) const { return _p(node)->m_type.has_val_anchor(); }
    RYML_DEPRECATED("use has_anchor()")        bool is_anchor(id_type node) const { return _p(node)->m_type.has_anchor(); }
    RYML_DEPRECATED("use has_anchor_or_ref()") bool is_anchor_or_ref(id_type node) const { return _p(node)->m_type.has_anchor() || _p(node)->m_type.is_ref(); }

    /** @} */

public:

    /** @name hierarchy predicates */
    /** @{ */

    bool is_root(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, _p(node)->m_parent != NONE || node == 0, this, node); return _p(node)->m_parent == NONE; }

    bool has_parent(id_type node) const { return _p(node)->m_parent != NONE; }

    /** true when ancestor is parent or parent of a parent of node */
    bool is_ancestor(id_type node, id_type ancestor) const;

    /** true when key and val are empty, and has no children */
    bool empty(id_type node) const { return ! has_children(node) && _p(node)->m_key.empty() && (( ! (_p(node)->m_type & VAL)) || _p(node)->m_val.empty()); }

    /** true if @p node has a child with id @p ch */
    bool has_child(id_type node, id_type ch) const { return _p(ch)->m_parent == node; }
    /** true if @p node has a child with key @p key */
    bool has_child(id_type node, csubstr key) const { return find_child(node, key) != NONE; }
    /** true if @p node has any children */
    bool has_children(id_type node) const { return _p(node)->m_first_child != NONE; }

    /** true if @p node has a sibling with id @p sib */
    bool has_sibling(id_type node, id_type sib) const { return _p(node)->m_parent == _p(sib)->m_parent; }
    /** true if one of the node's siblings has the given key */
    bool has_sibling(id_type node, csubstr key) const { return find_sibling(node, key) != NONE; }
    /** true if node is not a single child */
    bool has_other_siblings(id_type node) const
    {
        NodeData const *n = _p(node);
        if C4_LIKELY(n->m_parent != NONE)
        {
            n = _p(n->m_parent);
            return n->m_first_child != n->m_last_child;
        }
        return false;
    }

    /** @} */

public:

    /** @name hierarchy getters */
    /** @{ */

    id_type parent(id_type node) const { return _p(node)->m_parent; }

    id_type prev_sibling(id_type node) const { return _p(node)->m_prev_sibling; }
    id_type next_sibling(id_type node) const { return _p(node)->m_next_sibling; }

    /** O(#num_children) */
    id_type num_children(id_type node) const;
    id_type child_pos(id_type node, id_type ch) const;
    id_type first_child(id_type node) const { return _p(node)->m_first_child; }
    id_type last_child(id_type node) const { return _p(node)->m_last_child; }
    id_type child(id_type node, id_type pos) const; ///< find child by position, or NONE if there are less than pos children posi
    id_type find_child(id_type node, csubstr const& key) const; ///< find child by name, or NONE if no child is found with this key
    /// like @ref Tree::child(), but return a @ref ReadResult with the status
    C4_NODISCARD ReadResult child_r(id_type node, id_type pos, id_type *child_id) const { return ReadResult(node, *child_id = child(node, pos)); }
    /// like @ref Tree::find_child(), but return a @ref ReadResult with the status
    C4_NODISCARD ReadResult find_child_r(id_type node, csubstr const& key, id_type *child_id) const { return ReadResult(node, *child_id = find_child(node, key)); }

    /** O(#num_siblings) */
    /** counts with this */
    id_type num_siblings(id_type node) const { return is_root(node) ? 1 : num_children(_p(node)->m_parent); }
    /** does not count with this */
    id_type num_other_siblings(id_type node) const { id_type ns = num_siblings(node); RYML_ASSERT_VISIT_CB_(m_callbacks, ns > 0, this, node); return ns-1; }
    id_type sibling_pos(id_type node, id_type sib) const { RYML_ASSERT_VISIT_CB_(m_callbacks,  ! is_root(node) || node == root_id(), this, node); return child_pos(_p(node)->m_parent, sib); }
    id_type first_sibling(id_type node) const { return is_root(node) ? node : _p(_p(node)->m_parent)->m_first_child; }
    id_type last_sibling(id_type node) const { return is_root(node) ? node : _p(_p(node)->m_parent)->m_last_child; }
    id_type sibling(id_type node, id_type pos) const { return child(_p(node)->m_parent, pos); }
    id_type find_sibling(id_type node, csubstr const& key) const { return find_child(_p(node)->m_parent, key); }
    /// like @ref Tree::sibling(), but return a @ref ReadResult with the status
    C4_NODISCARD ReadResult sibling_r(id_type node, id_type pos, id_type *sibling_id) const { return child_r(_p(node)->m_parent, pos, sibling_id); }
    /// like @ref Tree::find_sibling(), but return a @ref ReadResult if with the status
    C4_NODISCARD ReadResult find_sibling_r(id_type node, csubstr const& key, id_type *sibling_id) const { return find_child_r(_p(node)->m_parent, key, sibling_id); }

    id_type depth_asc(id_type node) const; /**< O(log(num_tree_nodes)) get the ascending depth of the node: number of levels between root and node */
    id_type depth_desc(id_type node) const; /**< O(num_tree_nodes) get the descending depth of the node: number of levels between node and deepest child */

    /** gets the @p i document node index. requires that the root node is a stream. */
    id_type doc(id_type i) const { id_type rid = root_id(); RYML_ASSERT_VISIT_CB_(m_callbacks, is_stream(rid), this, rid); return child(rid, i); }

    /** get the document which is a parent document of node i, or the root if the tree is not a stream */
    id_type ancestor_doc(id_type node) const
    {
        NodeData const *nd;
        do
        {
            nd = _p(node);
            if(nd->m_type.is_doc() || nd->m_parent == NONE)
                break;
            node = nd->m_parent;
        } while(nd->m_parent != NONE);
        return node;
    }

    /** @} */

public:

    /** @name node style predicates and modifiers. see the corresponding predicate in NodeType */
    /** @{ */

    C4_ALWAYS_INLINE bool is_container_styled(id_type node) const { return _p(node)->m_type.is_container_styled(); }
    C4_ALWAYS_INLINE bool is_block(id_type node) const { return _p(node)->m_type.is_block(); }
    C4_ALWAYS_INLINE bool is_flow_sl(id_type node) const { return _p(node)->m_type.is_flow_sl(); }
    RYML_DEPRECATED("use one of .is_flow_ml{1,n,x}()")
    C4_ALWAYS_INLINE bool is_flow_ml(id_type node) const { return _p(node)->m_type.is_flow_ml1(); }
    C4_ALWAYS_INLINE bool is_flow_ml1(id_type node) const { return _p(node)->m_type.is_flow_ml1(); }
    C4_ALWAYS_INLINE bool is_flow_mln(id_type node) const { return _p(node)->m_type.is_flow_mln(); }
    C4_ALWAYS_INLINE bool is_flow_mlx(id_type node) const { return _p(node)->m_type.is_flow_mlx(); }
    C4_ALWAYS_INLINE bool is_flow(id_type node) const { return _p(node)->m_type.is_flow(); }
    C4_ALWAYS_INLINE bool has_flow_space(id_type node) const { return _p(node)->m_type.has_flow_space(); }

    C4_ALWAYS_INLINE bool is_key_styled(id_type node) const { return _p(node)->m_type.is_key_styled(); }
    C4_ALWAYS_INLINE bool is_val_styled(id_type node) const { return _p(node)->m_type.is_val_styled(); }
    C4_ALWAYS_INLINE bool is_key_literal(id_type node) const { return _p(node)->m_type.is_key_literal(); }
    C4_ALWAYS_INLINE bool is_val_literal(id_type node) const { return _p(node)->m_type.is_val_literal(); }
    C4_ALWAYS_INLINE bool is_key_folded(id_type node) const { return _p(node)->m_type.is_key_folded(); }
    C4_ALWAYS_INLINE bool is_val_folded(id_type node) const { return _p(node)->m_type.is_val_folded(); }
    C4_ALWAYS_INLINE bool is_key_squo(id_type node) const { return _p(node)->m_type.is_key_squo(); }
    C4_ALWAYS_INLINE bool is_val_squo(id_type node) const { return _p(node)->m_type.is_val_squo(); }
    C4_ALWAYS_INLINE bool is_key_dquo(id_type node) const { return _p(node)->m_type.is_key_dquo(); }
    C4_ALWAYS_INLINE bool is_val_dquo(id_type node) const { return _p(node)->m_type.is_val_dquo(); }
    C4_ALWAYS_INLINE bool is_key_plain(id_type node) const { return _p(node)->m_type.is_key_plain(); }
    C4_ALWAYS_INLINE bool is_val_plain(id_type node) const { return _p(node)->m_type.is_val_plain(); }
    C4_ALWAYS_INLINE bool is_key_quoted(id_type node) const { return _p(node)->m_type.is_key_quoted(); }
    C4_ALWAYS_INLINE bool is_val_quoted(id_type node) const { return _p(node)->m_type.is_val_quoted(); }
    C4_ALWAYS_INLINE bool is_quoted(id_type node) const { return _p(node)->m_type.is_quoted(); }

    C4_ALWAYS_INLINE NodeType key_style(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node); return _p(node)->m_type.key_style(); }
    C4_ALWAYS_INLINE NodeType val_style(id_type node) const { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val(node) || is_root(node), this, node); return _p(node)->m_type.val_style(); }

    C4_ALWAYS_INLINE void set_container_style(id_type node, type_bits style) { RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(node), this, node); _p(node)->m_type.set_container_style(style); }
    C4_ALWAYS_INLINE void set_key_style(id_type node, type_bits style) { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node); _p(node)->m_type.set_key_style(style); }
    C4_ALWAYS_INLINE void set_val_style(id_type node, type_bits style) { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val(node), this, node); _p(node)->m_type.set_val_style(style); }

    void clear_style(id_type node, bool recurse=false);
    void set_style_conditionally(id_type node,
                                 NodeType type_mask,
                                 NodeType rem_style_flags,
                                 NodeType add_style_flags,
                                 bool recurse=false);
    /** @} */

public:

    /** @name node type modifiers */
    /** @{ */

    void set_stream(id_type node)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_root(node), this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, (_p(node)->m_type & (DOC|MAP|VAL)) == 0, this, node);
        _p(node)->m_type |= STREAM;
    }

    void set_doc(id_type node)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_root(node) || (is_root(parent(node)) && is_stream(parent(node))), this, node);
        _p(node)->m_type |= DOC;
    }

    void set_val(id_type node, csubstr val) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, (_p(node)->m_type & (SEQ|MAP)) == 0, this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, (parent(node) == NONE || (_p(parent(node))->m_type & (SEQ|MAP))), this, node);
        NodeData *C4_RESTRICT nd = _p(node);
        nd->m_type |= VAL;
        nd->m_val.scalar = val;
    }
    void set_val(id_type node, csubstr val, NodeType more_flags) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, (_p(node)->m_type & (SEQ|MAP)) == 0, this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, (parent(node) == NONE || (_p(parent(node))->m_type & (SEQ|MAP))), this, node);
        NodeData *C4_RESTRICT nd = _p(node);
        nd->m_type |= VAL|more_flags;
        nd->m_val.scalar = val;
    }

    void set_key(id_type node, csubstr key) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, (parent(node) != NONE && (_p(parent(node))->m_type & MAP)), this, node);
        NodeData *C4_RESTRICT nd = _p(node);
        nd->m_type |= KEY;
        nd->m_key.scalar = key;
    }
    void set_key(id_type node, csubstr key, NodeType more_flags) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, (parent(node) != NONE && (_p(parent(node))->m_type & MAP)), this, node);
        NodeData *C4_RESTRICT nd = _p(node);
        nd->m_type |= KEY|more_flags;
        nd->m_key.scalar = key;
    }

    void set_seq(id_type node) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, (_p(node)->m_type & (VAL|MAP)) == 0, this, node);
        _p(node)->m_type |= SEQ;
    }
    void set_seq(id_type node, NodeType more_flags) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, ((_p(node)->m_type|more_flags) & (VAL|MAP)) == 0, this, node);
        _p(node)->m_type |= SEQ|more_flags;
    }

    void set_map(id_type node) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, (_p(node)->m_type & (VAL|SEQ)) == 0, this, node);
        _p(node)->m_type |= MAP;
    }
    void set_map(id_type node, NodeType more_flags) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, ((_p(node)->m_type|more_flags) & (VAL|SEQ)) == 0, this, node);
        _p(node)->m_type |= MAP|more_flags;
    }

    void set_key_tag(id_type node, csubstr tag) { RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node); _p(node)->m_key.tag = tag; _add_flags(node, KEYTAG); }
    void set_val_tag(id_type node, csubstr tag) { RYML_ASSERT_VISIT_CB_(m_callbacks, has_val(node) || is_container(node), this, node); _p(node)->m_val.tag = tag; _add_flags(node, VALTAG); }

    void set_key_anchor(id_type node, csubstr anchor) { RYML_ASSERT_VISIT_CB_(m_callbacks,  ! is_key_ref(node), this, node); _p(node)->m_key.anchor = anchor.triml('&'); _add_flags(node, KEYANCH); }
    void set_val_anchor(id_type node, csubstr anchor) { RYML_ASSERT_VISIT_CB_(m_callbacks,  ! is_val_ref(node), this, node); _p(node)->m_val.anchor = anchor.triml('&'); _add_flags(node, VALANCH); }
    void set_key_ref   (id_type node, csubstr ref   ) { RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_key_anchor(node), this, node); NodeData* C4_RESTRICT n = _p(node); n->m_key.set_ref_maybe_replacing_scalar(ref, n->m_type.has_key()); _add_flags(node, KEY|KEYREF); }
    void set_val_ref   (id_type node, csubstr ref   ) { RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_val_anchor(node), this, node); NodeData* C4_RESTRICT n = _p(node); n->m_val.set_ref_maybe_replacing_scalar(ref, n->m_type.has_val()); _add_flags(node, VAL|VALREF); }

    void rem_key_anchor(id_type node) { _p(node)->m_key.anchor.clear(); _rem_flags(node, KEYANCH); }
    void rem_val_anchor(id_type node) { _p(node)->m_val.anchor.clear(); _rem_flags(node, VALANCH); }
    void rem_key_ref   (id_type node) { _p(node)->m_key.anchor.clear(); _rem_flags(node, KEYREF); }
    void rem_val_ref   (id_type node) { _p(node)->m_val.anchor.clear(); _rem_flags(node, VALREF); }
    void rem_anchor_ref(id_type node) { _p(node)->m_key.anchor.clear(); _p(node)->m_val.anchor.clear(); _rem_flags(node, KEYANCH|VALANCH|KEYREF|VALREF); }

    /** @} */

private:

    C4_NORETURN C4_NO_INLINE C4_COLD
    void err_visit_(id_type node) const
    {
        RYML_ERR_VISIT_CB_(m_callbacks, this, node, "invalid node");
    }

public:

    /** @name serialization - checked */
    /** @{ */

    template<class T>
    C4_ALWAYS_INLINE void save(id_type node, T const& val)
    {
        if C4_LIKELY(node != NONE && node < m_cap && node >= 0)
        {
            write(this, node, val);
            return;
        }
        err_visit_(node);
    }
    template<class T>
    C4_ALWAYS_INLINE void save(id_type node, T const& val, NodeType more_flags)
    {
        if C4_LIKELY(node != NONE && node < m_cap && node >= 0)
        {
            write(this, node, val);
            _p(node)->m_type |= more_flags;
            return;
        }
        err_visit_(node);
    }

    template<class T>
    C4_ALWAYS_INLINE void save_key(id_type node, T const& key)
    {
        if C4_LIKELY(node != NONE && node < m_cap && node >= 0)
        {
            write_key(this, node, key);
            return;
        }
        err_visit_(node);
    }
    template<class T>
    C4_ALWAYS_INLINE void save_key(id_type node, T const& key, NodeType more_flags)
    {
        if C4_LIKELY(node != NONE && node < m_cap && node >= 0)
        {
            write_key(this, node, key);
            _p(node)->m_type |= more_flags;
            return;
        }
        err_visit_(node);
    }

    /** @} */

public:

    /** @name serialization - asserted */
    /** @{ */

    template<class T>
    C4_ALWAYS_INLINE void set_serialized(id_type node, T const& val) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        write(this, node, val);
    }
    template<class T>
    C4_ALWAYS_INLINE void set_serialized(id_type node, T const& val, NodeType more_flags) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        write(this, node, val);
        _p(node)->m_type |= more_flags;
    }

    template<class T>
    C4_ALWAYS_INLINE void set_key_serialized(id_type node, T const& key) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        write_key(this, node, key);
    }
    template<class T>
    C4_ALWAYS_INLINE void set_key_serialized(id_type node, T const& key, NodeType more_flags) RYML_NOEXCEPT
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        write_key(this, node, key);
        _p(node)->m_type |= more_flags;
    }

    /** @} */

public:

    /** @name deserialization - checked
     *
     * These methods raise an error if the deserialization failed or
     * optionally if the node is not readable.
     */
    /** @{ */

    static C4_ALWAYS_INLINE ReadResult to_result_(bool, id_type node) noexcept { return ReadResult(node); }
    static C4_ALWAYS_INLINE ReadResult to_result_(ReadResult notlegacy, id_type) noexcept { return notlegacy; }

    /** (1) deserialize the node's contents (val or container) to the
     * given variable, forwarding to the user-overrideable @ref read()
     * function (see @ref doc_serialization_tree_read). This function
     * differs from @ref Tree::deserialize() in that here the error
     * callback is called if the deserialization failed, or
     * (optionally) the node is not readable. */
    template<class T>
    C4_ALWAYS_INLINE void load(id_type node, T * v, bool check_readable=true) const
    {
        const bool can_read_val = (node != NONE && node < m_cap && node >= 0 && (_p(node)->m_type & (VAL|MAP|SEQ)));
        RYML_ASSERT_VISIT_CB_(m_callbacks, can_read_val, this, node);
        if C4_LIKELY(!check_readable || can_read_val)
        {
            const ReadResult result(read(this, node, v), node);
            if C4_LIKELY(result)
                return;
            else
                node = result.node;
        }
        err_visit_(node);
    }
    /** (2) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    C4_ALWAYS_INLINE void load(id_type node, Wrapper const& w, bool check_readable=true) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        const bool can_read_val = (node != NONE && node < m_cap && node >= 0 && (_p(node)->m_type & (VAL|MAP|SEQ)));
        RYML_ASSERT_VISIT_CB_(m_callbacks, can_read_val, this, node);
        if C4_LIKELY(!check_readable || can_read_val)
        {
            const ReadResult result(read(this, node, w), node);
            if C4_LIKELY(result)
                return;
            else
                node = result.node;
        }
        err_visit_(node);
    }

    /** (1) deserialize the node's key (necessarily a scalar) to the
     * given variable, forwarding to the user-overrideable @ref
     * read_key() function (see @ref
     * doc_serialization_node_read). This function differs from @ref
     * Tree::deserialize_key() in that here the error callback is
     * called if the deserialization failed, or (optionally) the node
     * is not readable. */
    template<class T>
    C4_ALWAYS_INLINE void load_key(id_type node, T * k, bool check_readable=true) const
    {
        const bool can_read_key = (node != NONE && node < m_cap && node >= 0 && (_p(node)->m_type & KEY));
        RYML_ASSERT_VISIT_CB_(m_callbacks, can_read_key, this, node);
        if C4_LIKELY(!check_readable || can_read_key)
        {
            const ReadResult result(read_key(this, node, k), node);
            if C4_LIKELY(result)
                return;
            else
                node = result.node;
        }
        err_visit_(node);
    }
    /** (2) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    C4_ALWAYS_INLINE void load_key(id_type node, Wrapper const& w, bool check_readable=true) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        bool can_read_key = (node != NONE && node < m_cap && node >= 0 && (_p(node)->m_type & KEY));
        RYML_ASSERT_VISIT_CB_(m_callbacks, can_read_key, this, node);
        if C4_LIKELY(!check_readable || can_read_key)
        {
            const ReadResult result(read_key(this, node, w), node);
            if C4_LIKELY(result)
                return;
            else
                node = result.node;
        }
        err_visit_(node);
    }

    /** @} */

public:

    /** @name deserialization - asserted preconditions */
    /** @{ */

    /** (1) deserialize a node's contents to a variable */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize(id_type node, T * v) const
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, _p(node)->m_type & (VAL|MAP|SEQ), this, node);
        // use the adapter ctor to accomodate legacy read() implementations
        return ReadResult(read(this, node, v), node);
    }
    /** (2) like (1), but for a wrapper type like those used in tag
     * functions */
    template<class Wrapper>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize(id_type node, Wrapper const& w) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, _p(node)->m_type & (VAL|MAP|SEQ), this, node);
        // use the adapter ctor to accomodate legacy read() implementations
        return ReadResult(read(this, node, w), node);
    }

    /** (1) deserialize a node's key to a variable */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_key(id_type node, T * v) const
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node);
        // use the adapter ctor to accomodate legacy read_key() implementations
        return ReadResult(read_key(this, node, v), node);
    }
    /** (2) like (1), but for a wrapper type like those used in tag
     * functions */
    template<class Wrapper>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_key(id_type node, Wrapper const& w) const
    {
        RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(Wrapper);
        RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE && node >= 0 && node < m_cap, this, node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, has_key(node), this, node);
        // use the adapter ctor to accomodate legacy read_key() implementations
        return ReadResult(read_key(this, node, w), node);
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
     * @see see also @ref Tree::find_child_r()
     */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type node, csubstr child_key, T * v) const
    {
        id_type ch;
        ReadResult r = find_child_r(node, child_key, &ch);
        if(r)
            r = deserialize(ch, v);
        return r;
    }
    /** (2) like (1), but assign from fallback if no such child exists. */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type node, csubstr child_key, T * v, T const& fallback) const
    {
        id_type ch;
        if(find_child_r(node, child_key, &ch))
            return deserialize(ch, v);
        *v = fallback;
        return ReadResult();
    }
    /** (3) like (1), but for wrapper tag types such as @ref c4::fmt::base64() */
    template<class Wrapper>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type node, csubstr child_key, Wrapper const& wrapper) const
    {
        id_type ch;
        ReadResult r = find_child_r(node, child_key, &ch);
        if(r)
            r = deserialize(ch, wrapper);
        return r;
    }

    /** (1) find a child by position and deserialize its contents to
     * the given variable (ie call .deserialize() on the child if it
     * exists). Otherwise, the variable is kept unchanged.
     *
     * @return a @ref ReadResult set with this node's id if no child
     * exists, or the ReadResult from the deserialization.
     *
     * @see see also @ref Tree::child_r()
     */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type node, id_type child_pos, T * v) const
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(node), this, node); // this assertion is needed because child_r() does not assert, contrary to find_child_r()
        id_type ch;
        ReadResult r = child_r(node, child_pos, &ch);
        if(r)
            r = deserialize(ch, v);
        return r;
    }
    /** (2) like (1), but assign from fallback if no such child exists */
    template<class T>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type node, id_type child_pos, T * v, T const& fallback) const
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(node), this, node); // this assertion is needed because child_r() does not assert, contrary to find_child_r()
        id_type ch;
        if(child_r(node, child_pos, &ch))
            return deserialize(ch, v);
        *v = fallback;
        return ReadResult();
    }
    /** (3) like (1), but for wrapper tag types such as @ref
     * c4::fmt::base64() */
    template<class Wrapper>
    C4_NODISCARD C4_ALWAYS_INLINE ReadResult deserialize_child(id_type node, id_type child_pos, Wrapper const& wrapper) const
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(node), this, node); // this assertion is needed because child_r() does not assert, contrary to find_child_r()
        id_type ch;
        ReadResult r = child_r(node, child_pos, &ch);
        if(r)
            r = deserialize(ch, wrapper);
        return r;
    }

    /** @} */

public:

    /** @name tree modifiers */
    /** @{ */

    /** reorder the tree in memory so that all the nodes are stored
     * in a linear sequence when visited in depth-first order.
     * This will invalidate existing ids, since the node id is its
     * position in the tree's node array. */
    void reorder();

    /** @} */

public:

    /** @name anchors and references/aliases */
    /** @{ */

    /** Resolve references (aliases <- anchors), by forwarding to @ref
     * ReferenceResolver::resolve(); refer to @ref
     * ReferenceResolver::resolve() for further details. */
    void resolve(ReferenceResolver *C4_RESTRICT rr, bool clear_anchors=true);

    /** Resolve references (aliases <- anchors), by forwarding to @ref
     * ReferenceResolver::resolve(); refer to @ref
     * ReferenceResolver::resolve() for further details. This overload
     * uses a throwaway resolver object. */
    void resolve(bool clear_anchors=true);

    /** @} */

public:

    /** @name tags and tag directives */
    /** @{ */

    /** Resolve tags in the tree such as \c "!!str" ->
     * \c "<tag:yaml.org,2002:str>", \c "!foo" -> \c "<!foo>" and custom tags
     * as well, ie tags of the form \c "!handle!tag" for which there is a
     * corresponding \c "%TAG" directive
     *
     * @param cache an object of type @ref TagCache to minimize memory
     *        usage by avoiding repeated instantiation of the resolved
     *        tags in the tree's arena.
     *
     * @param all if true, resolve all tags; if false resolve only
     *        custom tags, ie those that have a prefix such as
     *        \c "!m!tag" with a matching \c "\%TAG" directive */
    void resolve_tags(TagCache &cache, bool all=true);
    void normalize_tags();
    void normalize_tags_long();

    id_type num_tag_directives() const;
    void add_tag_directive(csubstr handle, csubstr prefix, id_type id);
    void clear_tag_directives();

    /** resolve the given tag, appearing at node_id. Write the result into output.
     * @return the number of characters required for the resolved tag */
    size_t resolve_tag(substr output, csubstr tag, id_type node_id) const;
    /** Wrapper for @ref Tree::resolve_tag(), returning a substring */
    csubstr resolve_tag_sub(substr output, csubstr tag, id_type node_id) const
    {
        size_t needed = resolve_tag(output, tag, node_id);
        return needed <= output.len ? output.first(needed) : output;
    }

    c4::yml::TagDirectiveRange tag_directives() const { return m_tag_directives.directives(); }

    /** @cond dev */
    RYML_DEPRECATED("use c4::yml::tag_directive_const_iterator") typedef TagDirective const* tag_directive_const_iterator;
    RYML_DEPRECATED("use c4::yml::TagDirectiveRange") typedef c4::yml::TagDirectiveRange TagDirectiveProxy;
    /** @endcond */

    /** @} */

public:

    /** @name modifying hierarchy */
    /** @{ */

    /** create and insert a new child of @p parent. insert after the (to-be)
     * sibling @p after, which must be a child of @p parent. To insert as the
     * first child, set after to NONE */
    C4_ALWAYS_INLINE id_type insert_child(id_type parent, id_type after)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, parent != NONE, this, parent);
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(parent) || is_root(parent), this, parent);
        RYML_ASSERT_VISIT_CB_(m_callbacks, after == NONE || (_p(after)->m_parent == parent), this, parent);
        id_type child = _claim();
        _set_hierarchy(child, parent, after);
        return child;
    }
    /** create and insert a node as the first child of @p parent */
    C4_ALWAYS_INLINE id_type prepend_child(id_type parent) { return insert_child(parent, NONE); }
    /** create and insert a node as the last child of @p parent */
    C4_ALWAYS_INLINE id_type append_child(id_type parent) { return insert_child(parent, _p(parent)->m_last_child); }
    C4_ALWAYS_INLINE id_type _append_child__unprotected(id_type parent)
    {
        id_type child = _claim();
        _set_hierarchy(child, parent, _p(parent)->m_last_child);
        return child;
    }

public:

    //! create and insert a new sibling of n. insert after "after"
    C4_ALWAYS_INLINE id_type insert_sibling(id_type node, id_type after)
    {
        return insert_child(_p(node)->m_parent, after);
    }
    /** create and insert a node as the first node of @p parent */
    C4_ALWAYS_INLINE id_type prepend_sibling(id_type node) { return prepend_child(_p(node)->m_parent); }
    C4_ALWAYS_INLINE id_type  append_sibling(id_type node) { return append_child(_p(node)->m_parent); }

public:

    /** remove an entire branch at once: ie remove the children and the node itself */
    void remove(id_type node)
    {
        remove_children(node);
        _release(node);
    }

    /** remove all the node's children, but keep the node itself */
    void remove_children(id_type node);

public:

    /** change the node's position in the parent */
    void move(id_type node, id_type after);

    /** change the node's parent and position */
    void move(id_type node, id_type new_parent, id_type after);

    /** change the node's parent and position to a different tree
     * @return the index of the new node in the destination tree */
    id_type move(Tree * src, id_type node, id_type new_parent, id_type after);

    /** ensure the first node is a stream. Eg, change this tree
     *
     *  DOCMAP
     *    MAP
     *      KEYVAL
     *      KEYVAL
     *    SEQ
     *      VAL
     *
     * to
     *
     *  STREAM
     *    DOCMAP
     *      MAP
     *        KEYVAL
     *        KEYVAL
     *      SEQ
     *        VAL
     *
     * If the root is already a stream, this is a no-op.
     */
    void set_root_as_stream();

    bool change_type(id_type node, NodeType type);
    bool change_type(id_type node, type_bits type)
    {
        return change_type(node, (NodeType)type);
    }

public:

    /** recursively duplicate a node from this tree into a new parent,
     * placing it after one of its children
     * @return the index of the copy */
    id_type duplicate(id_type node, id_type new_parent, id_type after);
    /** recursively duplicate a node from a different tree into a new parent,
     * placing it after one of its children
     * @return the index of the copy */
    id_type duplicate(Tree const* src, id_type node, id_type new_parent, id_type after);

    /** recursively duplicate the node's children (but not the node)
     * @return the index of the last duplicated child */
    id_type duplicate_children(id_type node, id_type parent, id_type after);
    /** recursively duplicate the node's children (but not the node), where
     * the node is from a different tree
     * @return the index of the last duplicated child */
    id_type duplicate_children(Tree const* src, id_type node, id_type parent, id_type after);

    /** duplicate the node's children (but not the node) in a new parent, but
     * omit repetitions where a duplicated node has the same key (in maps) or
     * value (in seqs). If one of the duplicated children has the same key
     * (in maps) or value (in seqs) as one of the parent's children, the one
     * that is placed closest to the end will prevail. */
    id_type duplicate_children_no_rep(id_type node, id_type parent, id_type after);
    id_type duplicate_children_no_rep(Tree const* src, id_type node, id_type parent, id_type after);

    void duplicate_contents(id_type node, id_type where);
    void duplicate_contents(Tree const* src, id_type node, id_type where);

public:

    void merge_with(Tree const* src, id_type src_node=NONE, id_type dst_root=NONE);

    /** @} */

public:

    /** @name locations */
    /** @{ */

    /** Get the location of a node from the parse used to parse this tree. */
    Location location(Parser const& p, id_type node) const;

private:

    bool _location_from_node(Parser const& p, id_type node, Location *C4_RESTRICT loc, id_type level) const;
    bool _location_from_cont(Parser const& p, id_type node, Location *C4_RESTRICT loc) const;

    /** @} */

public:

    /** @name internal string arena */
    /** @{ */

    /** get the current size of the tree's internal arena */
    size_t arena_size() const { return m_arena_pos; }
    /** get the current capacity of the tree's internal arena */
    size_t arena_capacity() const { return m_arena.len; }
    /** get the current slack of the tree's internal arena */
    size_t arena_slack() const { RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.len >= m_arena_pos, this, NONE); return m_arena.len - m_arena_pos; }
    RYML_DEPRECATED("use arena_size() instead") size_t arena_pos() const { return m_arena_pos; }

    /** get the current arena */
    csubstr arena() const { return m_arena.first(m_arena_pos); }
    /** get the current arena */
    substr arena() { return m_arena.first(m_arena_pos); } // NOLINT(readability-make-member-function-const)

    /** get the free space at the end of the arena */
    csubstr arena_rem() const { return m_arena.sub(m_arena_pos); }
    /** get the free space at the end of the arena */
    substr arena_rem() { return m_arena.sub(m_arena_pos); } // NOLINT(readability-make-member-function-const)

    /** return true if the given substring is part of the tree's string arena */
    C4_ALWAYS_INLINE bool in_arena(csubstr s) const
    {
        return m_arena.is_super(s);
    }

    /** serialize the given variable to the tree's
     * arena, growing it as needed to accomodate the serialization.
     *
     * @note To customize how the type gets serialized to a string,
     * you can overload c4::to_chars(substr, T const&)
     *
     * @note To customize how the type gets serialized to the arena,
     * you can overload @ref scalar_serialize()
     *
     * @note Growing the arena may cause relocation of the entire
     * existing arena, and thus change the contents of individual
     * nodes, and thus cost O(numnodes)+O(arenasize). To avoid this
     * cost, ensure that the arena is reserved to an appropriate size
     * using @ref Tree::reserve_arena().
     *
     * @see alloc_arena() */
    template<class T>
    C4_ALWAYS_INLINE csubstr to_arena(T const& C4_RESTRICT a)
    {
        return serialize_to_arena(this, a);
    }

    /** copy the given string to the tree's arena, growing the arena
     * by the required size.
     *
     * @note this method differs from @ref to_arena() in that it
     * returns a mutable substr, and further it does not deal
     * with some corner cases for null/empty strings
     *
     * @note Growing the arena may cause relocation of the entire
     * existing arena, and thus change the contents of individual
     * nodes, and thus cost O(numnodes)+O(arenasize). To avoid this
     * cost, ensure that the arena is reserved to an appropriate size
     * before using @ref Tree::reserve_arena()
     *
     * @see reserve_arena()
     * @see alloc_arena()
     */
    substr copy_to_arena(csubstr s)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, !s.overlaps(m_arena), this, NONE);
        substr cp = alloc_arena(s.len);
        RYML_ASSERT_VISIT_CB_(m_callbacks, cp.len == s.len, this, NONE);
        RYML_ASSERT_VISIT_CB_(m_callbacks, !s.overlaps(cp), this, NONE);
        C4_SUPPRESS_WARNING_GCC_PUSH
        #if (!defined(__clang__)) && (defined(__GNUC__) && __GNUC__ >= 10)
        C4_SUPPRESS_WARNING_GCC("-Wstringop-overflow=") // no need for terminating \0
        C4_SUPPRESS_WARNING_GCC("-Wrestrict") // there's an assert to ensure no violation of restrict behavior
        #endif
        if(s.len)
            memcpy(cp.str, s.str, s.len);
        C4_SUPPRESS_WARNING_GCC_POP
        return cp;
    }

    /** grow the tree's string arena by the given size and return a substr
     * of the added portion
     *
     * @note Growing the arena may cause relocation of the entire
     * existing arena, and thus change the contents of individual
     * nodes, and thus cost O(numnodes)+O(arenasize). To avoid this
     * cost, ensure that the arena is reserved to an appropriate size
     * using @ref Tree::reserve_arena().
     *
     * @see reserve_arena() */
    substr alloc_arena(size_t sz)
    {
        if(sz > arena_slack())
            _grow_arena(sz - arena_slack());
        substr s = _request_span(sz);
        return s;
    }

    /** ensure the tree's internal string arena is at least the given capacity
     * @warning This operation may be expensive, with a potential complexity of O(numNodes)+O(arenasize).
     * @warning Growing the arena may cause relocation of the entire
     * existing arena, and thus change the contents of individual nodes. */
    void reserve_arena(size_t arena_cap=RYML_DEFAULT_TREE_ARENA_CAPACITY)
    {
        if(arena_cap > m_arena.len)
        {
            substr buf;
            buf.str = RYML_CB_ALLOC_(m_callbacks, char, arena_cap);
            buf.len = arena_cap;
            if(m_arena.str)
            {
                RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.len >= 0, this, NONE);
                _relocate(buf); // does a memcpy and changes nodes using the arena
                RYML_CB_FREE_(m_callbacks, m_arena.str, char, m_arena.len);
            }
            m_arena = buf;
        }
    }

    /** @} */

public:

    /** @cond dev */

    substr _grow_arena(size_t more)
    {
        size_t cap = m_arena.len + more;
        cap = cap < RYML_DEFAULT_TREE_ARENA_CAPACITY_START ? RYML_DEFAULT_TREE_ARENA_CAPACITY_START : cap;
        cap = cap < 2 * m_arena.len ? 2 * m_arena.len : cap;
        reserve_arena(cap);
        return m_arena.sub(m_arena_pos);
    }

    substr _request_span(size_t sz)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena_pos + sz <= m_arena.len, this, NONE);
        substr s;
        s = m_arena.sub(m_arena_pos, sz);
        m_arena_pos += sz;
        return s;
    }

    substr _relocated(csubstr s, substr next_arena) const
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.is_super(s) || s.len == 0, this, NONE);
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.sub(0, m_arena_pos).is_super(s) || s.len == 0, this, NONE);
        auto pos = (s.str - m_arena.str); // this is larger than 0 based on the assertions above
        substr r(next_arena.str + pos, s.len);
        RYML_ASSERT_VISIT_CB_(m_callbacks, r.str - next_arena.str == pos, this, NONE);
        RYML_ASSERT_VISIT_CB_(m_callbacks, next_arena.sub(0, m_arena_pos).is_super(r) || r.len == 0, this, NONE);
        return r;
    }

    /** @endcond */

public:

    /** @name lookup */
    /** @{ */

    struct RYML_EXPORT lookup_result
    {
        id_type  target;
        id_type  closest;
        size_t  path_pos;
        csubstr path;

        operator bool() const { return target != NONE; }

        lookup_result() : target(NONE), closest(NONE), path_pos(0), path() {}
        lookup_result(csubstr path_, id_type start) : target(NONE), closest(start), path_pos(0), path(path_) {}

        /** get the part ot the input path that was resolved */
        csubstr resolved() const;
        /** get the part ot the input path that was unresolved */
        csubstr unresolved() const;
    };

    /** for example foo.bar[0].baz */
    lookup_result lookup_path(csubstr path, id_type start=NONE) const;

    /** defaulted lookup: lookup @p path; if the lookup fails, recursively modify
     * the tree so that the corresponding lookup_path() would return the
     * default value.
     * @see lookup_path() */
    id_type lookup_path_or_modify(csubstr default_value, csubstr path, id_type start=NONE);

    /** defaulted lookup: lookup @p path; if the lookup fails, recursively modify
     * the tree so that the corresponding lookup_path() would return the
     * branch @p src_node (from the tree @p src).
     * @see lookup_path() */
    id_type lookup_path_or_modify(Tree const *src, id_type src_node, csubstr path, id_type start=NONE);

    /** @} */

private:

    struct _lookup_path_token
    {
        csubstr value;
        NodeType type;
        _lookup_path_token() : value(), type() {} // LCOV_EXCL_LINE
        _lookup_path_token(csubstr v, NodeType t) : value(v), type(t) {}
        operator bool() const { return type != NOTYPE; }
        bool is_index() const { return value.begins_with('[') && value.ends_with(']'); }
    };

    id_type _lookup_path_or_create(csubstr path, id_type start);

    void   _lookup_path       (lookup_result *r) const;
    void   _lookup_path_modify(lookup_result *r);

    id_type _next_node       (lookup_result *r, _lookup_path_token *parent) const;
    id_type _next_node_modify(lookup_result *r, _lookup_path_token *parent);

    static void _advance(lookup_result *r, size_t more);

    _lookup_path_token _next_token(lookup_result *r, _lookup_path_token const& parent) const;

private:

    void _clear();
    void _free();
    void _copy(Tree const& that);
    void _move(Tree      & that) noexcept;

    void _relocate(substr next_arena);

public:

    /** @cond dev*/

    #if ! RYML_USE_ASSERT
    C4_ALWAYS_INLINE void _check_next_flags(id_type, type_bits) {}
    #else
    void _check_next_flags(id_type node, type_bits f)
    {
        NodeData *n = _p(node);
        type_bits o = n->m_type; // old
        C4_UNUSED(o);
        if(f & MAP)
        {
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (f & SEQ) == 0, this, node, "cannot mark simultaneously as map and seq");
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (f & VAL) == 0, this, node, "cannot mark simultaneously as map and val");
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (o & SEQ) == 0, this, node, "cannot turn a seq into a map; clear first");
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (o & VAL) == 0, this, node, "cannot turn a val into a map; clear first");
        }
        else if(f & SEQ)
        {
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (f & MAP) == 0, this, node, "cannot mark simultaneously as seq and map");
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (f & VAL) == 0, this, node, "cannot mark simultaneously as seq and val");
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (o & MAP) == 0, this, node, "cannot turn a map into a seq; clear first");
            RYML_ASSERT_VISIT_MSG_CB_(m_callbacks, (o & VAL) == 0, this, node, "cannot turn a val into a seq; clear first");
        }
        if(f & KEY)
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, !is_root(node), this, node);
            RYML_ASSERT_VISIT_CB_(m_callbacks, is_map(parent(node)), this, node);
        }
        if((f & VAL) && !is_root(node))
        {
            auto pid = parent(node); C4_UNUSED(pid);
            RYML_ASSERT_VISIT_CB_(m_callbacks, is_map(pid) || is_seq(pid), this, node);
        }
    }
    #endif

    void _add_flags(id_type node, type_bits f) { NodeData *d = _p(node); type_bits fb = f |  d->m_type; _check_next_flags(node, fb); d->m_type = fb; }
    void _rem_flags(id_type node, type_bits f) { NodeData *d = _p(node); type_bits fb = d->m_type & ~f; _check_next_flags(node, fb); d->m_type = fb; }

    id_type _do_reorder(id_type *node, id_type count);

    void _swap(id_type n_, id_type m_);
    void _swap_props(id_type n_, id_type m_);
    void _swap_hierarchy(id_type n_, id_type m_);
    void _copy_hierarchy(id_type dst_, id_type src_);

    void _copy_props(id_type dst_, id_type src_)
    {
        _copy_props(dst_, this, src_);
    }

    void _copy_props_wo_key(id_type dst_, id_type src_)
    {
        _copy_props_wo_key(dst_, this, src_);
    }

    void _copy_props(id_type dst_, Tree const* that_tree, id_type src_)
    {
        NodeData      & C4_RESTRICT dst = *_p(dst_);
        NodeData const& C4_RESTRICT src = *that_tree->_p(src_);
        dst.m_type = src.m_type;
        dst.m_key  = src.m_key;
        dst.m_val  = src.m_val;
    }

    void _copy_props(id_type dst_, Tree const* that_tree, id_type src_, type_bits src_mask)
    {
        NodeData      & C4_RESTRICT dst = *_p(dst_);
        NodeData const& C4_RESTRICT src = *that_tree->_p(src_);
        dst.m_type = (src.m_type & src_mask) | (dst.m_type & ~src_mask);
        dst.m_key  = src.m_key;
        dst.m_val  = src.m_val;
    }

    void _copy_props_wo_key(id_type dst_, Tree const* that_tree, id_type src_)
    {
        auto      & C4_RESTRICT dst = *_p(dst_);
        auto const& C4_RESTRICT src = *that_tree->_p(src_);
        dst.m_type = (src.m_type & ~KEYMASK_) | (dst.m_type & KEYMASK_);
        dst.m_val  = src.m_val;
    }

    void _copy_props_wo_key(id_type dst_, Tree const* that_tree, id_type src_, type_bits src_mask)
    {
        auto      & C4_RESTRICT dst = *_p(dst_);
        auto const& C4_RESTRICT src = *that_tree->_p(src_);
        dst.m_type = (src.m_type & ((~KEYMASK_)|src_mask)) | (dst.m_type & (KEYMASK_|~src_mask));
        dst.m_val  = src.m_val;
    }

    void _clear_type(id_type node)
    {
        _p(node)->m_type = NOTYPE;
    }

    void _clear(id_type node)
    {
        auto *C4_RESTRICT n = _p(node);
        n->m_type = NOTYPE;
        n->m_key.clear();
        n->m_val.clear();
        n->m_parent = NONE;
        n->m_first_child = NONE;
        n->m_last_child = NONE;
    }

    void _clear_key(id_type node)
    {
        _p(node)->m_key.clear();
        _rem_flags(node, KEY);
    }

    void _clear_val(id_type node)
    {
        _p(node)->m_val.clear();
        _rem_flags(node, VAL);
    }

    /** @endcond */

private:

    void _clear_range(id_type first, id_type num);

    id_type _claim();
    void    _claim_root();
    void    _release(id_type node);
    void    _free_list_add(id_type node);
    void    _free_list_rem(id_type node);

    void _set_hierarchy(id_type node, id_type parent, id_type after_sibling);
    void _rem_hierarchy(id_type node);

public:

    // members are exposed, but you should NOT access them directly

    NodeData *m_buf;
    id_type   m_cap;
    id_type   m_size;

    id_type m_free_head;
    id_type m_free_tail;

    substr m_arena;
    size_t m_arena_pos;

    Callbacks m_callbacks;

    TagDirectives m_tag_directives;

public:

    /** @cond dev */ // LCOV_EXCL_START
    C4_SUPPRESS_WARNING_GCC_CLANG_PUSH
    C4_SUPPRESS_WARNING_MSVC_PUSH
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated")
    C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated-declarations")
    C4_SUPPRESS_WARNING_MSVC(4996) // deprecated
    RYML_DEPRECATED("use .type().type_str(buf)") const char* type_str(id_type node) const { return NodeType::type_str(_p(node)->m_type); }
    RYML_DEPRECATED("use has_other_siblings()") static bool has_siblings(id_type /*node*/) { return true; }
    RYML_DEPRECATED("use set_key()+set_val()") void to_keyval(id_type node, csubstr key, csubstr val, type_bits more_flags=0);
    RYML_DEPRECATED("use set_key()+set_map()") void to_map(id_type node, csubstr key, type_bits more_flags=0);
    RYML_DEPRECATED("use set_key()+set_seq()") void to_seq(id_type node, csubstr key, type_bits more_flags=0);
    RYML_DEPRECATED("use set_val()") void to_val(id_type node, csubstr val, type_bits more_flags=0);
    RYML_DEPRECATED("use set_map()") void to_map(id_type node, type_bits more_flags=0);
    RYML_DEPRECATED("use set_seq()") void to_seq(id_type node, type_bits more_flags=0);
    RYML_DEPRECATED("use set_doc()") void to_doc(id_type node, type_bits more_flags=0);
    RYML_DEPRECATED("use set_stream()") void to_stream(id_type node, type_bits more_flags=0);
    RYML_DEPRECATED("use resolve_tags(TagCache&)") void resolve_tags() { TagCache cache; resolve_tags(cache); }
    RYML_DEPRECATED("") void _set(id_type node, NodeInit const& i)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, i._check(), this, node);
        NodeData *n = _p(node);
        RYML_ASSERT_VISIT_CB_(m_callbacks, n->m_key.scalar.empty() || i.key.scalar.empty() || i.key.scalar == n->m_key.scalar, this, node);
        _add_flags(node, i.type);
        if(n->m_key.scalar.empty())
        {
            if( ! i.key.scalar.empty())
            {
                set_key(node, i.key.scalar);
            }
        }
        n->m_key.tag = i.key.tag;
        n->m_val = i.val;
    }
    RYML_DEPRECATED("") void _set_key(id_type node, NodeScalar const& key, NodeType more_flags=0)
    {
        _p(node)->m_key = key;
        _add_flags(node, KEY|more_flags);
    }
    RYML_DEPRECATED("") void _set_val(id_type node, NodeScalar const& val, NodeType more_flags=0)
    {
        _p(node)->m_val = val;
        _add_flags(node, VAL|more_flags);
    }
    C4_SUPPRESS_WARNING_MSVC_POP
    C4_SUPPRESS_WARNING_GCC_CLANG_POP
    /** @endcond */ // LCOV_EXCL_STOP
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/** @addtogroup doc_serialization_tree_write_arena
 *
 * @{
 */

/** Forward to @ref scalar_serialize(), giving it the tree's arena and
 * resizing the arena as needed to fit the result. */
template<class T>
csubstr serialize_to_arena_scalar(Tree * tree, T const& a)
{
    substr buf = tree->arena_rem(); // buffer: the free part of the tree's arenra.
 again:
    size_t num = scalar_serialize(buf, a); // try to write into it
    if C4_LIKELY(num <= buf.len) // was it enough?
    {
        buf = buf.first(num); // fit the payload
    }
    else
    {
        buf = tree->_grow_arena(num); // does not advance pos
        goto again; // NOLINT
    }
    tree->m_arena_pos += num;
    return buf;
}

/** @} */


/** @addtogroup doc_serialization_tree_write
 *
 * @{
 */

#if (C4_CPP >= 17) || defined(__DOXYGEN__)

/** Return extra style flags to use when setting a scalar as val.
 * Defaults to VAL_PLAIN for arithmetic types, or NOTYPE otherwise */
template<class T>
C4_ALWAYS_INLINE type_bits scalar_flags_val(T const&) noexcept
{
    if constexpr (std::is_arithmetic_v<T>)
        return VAL_PLAIN;
    else
        return NOTYPE;
}
/** Return extra style flags to use when setting a scalar as key.
 * Defaults to KEY_PLAIN for arithmetic types, or NOTYPE otherwise */
template<class T>
C4_ALWAYS_INLINE type_bits scalar_flags_key(T const&) noexcept
{
    if constexpr (std::is_arithmetic_v<T>)
        return KEY_PLAIN;
    else
        return NOTYPE;
}

#else // pre-C++17 implementation: need to use SFINAE

template<class T>
C4_ALWAYS_INLINE auto scalar_flags_val(T const&) noexcept
    -> typename std::enable_if<std::is_arithmetic<T>::value, type_bits>::type
{
    return VAL_PLAIN;
}
template<class T>
C4_ALWAYS_INLINE auto scalar_flags_key(T const&) noexcept
    -> typename std::enable_if<std::is_arithmetic<T>::value, type_bits>::type
{
    return KEY_PLAIN;
}

template<class T>
C4_ALWAYS_INLINE auto scalar_flags_val(T const&) noexcept
    -> typename std::enable_if< ! std::is_arithmetic<T>::value, type_bits>::type
{
    return NOTYPE;
}
template<class T>
C4_ALWAYS_INLINE auto scalar_flags_key(T const&) noexcept
    -> typename std::enable_if< ! std::is_arithmetic<T>::value, type_bits>::type
{
    return NOTYPE;
}

#endif // pre-C++17 implementation


//-----------------------------------------------------------------------------

/** Serialize a variable to the tree's arena, and set it as the node's val.  */
template<class T>
C4_ALWAYS_INLINE void write(Tree * tree, id_type id, T const& v)
{
    tree->set_val(id, serialize_to_arena(tree, v), scalar_flags_val(v));
}

/** Serialize a variable to the tree's arena, and set it as the node's key.
 *
 * @warning The key MUST be a scalar value. The tree cannot handle
 * container keys. */
template<class T>
C4_ALWAYS_INLINE void write_key(Tree * tree, id_type id, T const& v)
{
    tree->set_key(id, serialize_to_arena(tree, v), scalar_flags_key(v));
}

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @addtogroup doc_serialization_tree_read
 * @{
 */

/** Deserialize a scalar node's val from a tree object, returning
 * false if the conversion failed.
 *
 * @return false if the conversion failed
 * @see @ref scalar_deserialize()
 * @see the counterpart function @ref read_key() */
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read(Tree const* tree, id_type id, T * v)
{
    // caller only checks that type is one of VAL|MAP|SEQ (because it
    // can't be more concrete than that). Here, we expect a VAL so now
    // we can check for that:
    NodeData const* C4_RESTRICT nd = tree->_p(id);
    return ReadResult((nd->m_type & VAL) && scalar_deserialize(nd->m_val.scalar, v), id);
}
/** overload to enable use of wrapper tag-types like eg @ref
 * c4::fmt::base64() */
template<class Wrapper>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read(Tree const* tree, id_type id, Wrapper const& w)
{
    // caller only checks that type is one of VAL|MAP|SEQ (because it
    // can't be more concrete than that). Here, we expect a VAL so now
    // we can check for that:
    NodeData const* C4_RESTRICT nd = tree->_p(id);
    return ReadResult((nd->m_type & VAL) && from_chars(nd->m_val.scalar, w), id);
}


/** Deserialize a node's key from a tree object, returning false if
 * the conversion failed
 *
 * @return false if the conversion failed
 * @see the counterpart function @ref read(Tree const*, id_type, T*) */
template<class T>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read_key(Tree const* tree, id_type id, T * v)
{
    // caller already checks availability of key
    return ReadResult(scalar_deserialize(tree->_p(id)->m_key.scalar, v), id);
}
/** overload to enable use of wrapper tag-types like eg @ref
 * c4::fmt::base64() */
template<class Wrapper>
C4_NODISCARD C4_ALWAYS_INLINE ReadResult read_key(Tree const* tree, id_type id, Wrapper const& w)
{
    // caller already checks availability of key
    return ReadResult(from_chars(tree->_p(id)->m_key.scalar, w), id);
}

/** @} */

/** @} */


} // namespace yml
} // namespace c4

// NOLINTEND(modernize-avoid-c-style-cast)
C4_SUPPRESS_WARNING_POP

#endif /* C4_YML_TREE_HPP_ */
