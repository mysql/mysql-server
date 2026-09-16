#ifndef C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif
#ifndef C4_YML_DETAIL_DBGPRINT_HPP_
#include "c4/yml/detail/dbgprint.hpp"
#endif
#ifndef C4_YML_NODE_HPP_
#include "c4/yml/node.hpp"
#endif
#ifndef C4_YML_REFERENCE_RESOLVERS_HPP_
#include "c4/yml/reference_resolver.hpp"
#endif


C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4296/*expression is always 'boolean_value'*/)
C4_SUPPRESS_WARNING_MSVC(4702/*unreachable code*/)
C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC("-Wtype-limits")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")
// NOLINTBEGIN(modernize-avoid-c-style-cast)


namespace c4 {
namespace yml {


csubstr serialize_to_arena_str(Tree * tree, csubstr scalar)
{
    if(scalar.len > 0)
    {
        return serialize_to_arena_scalar<csubstr>(tree, scalar);
    }
    else
    {
        if(scalar.str == nullptr)
        {
            return csubstr{};
        }
        else if(tree->m_arena.str == nullptr)
        {
            // Arena is empty and we want to store a non-null
            // zero-length string.
            // Even though the string has zero length, we need
            // some "memory" to store a non-nullptr string
            tree->_grow_arena(1);
        }
        return tree->_request_span(0);
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

NodeRef Tree::rootref()
{
    return NodeRef(this, root_id());
}
ConstNodeRef Tree::rootref() const
{
    return ConstNodeRef(this, root_id());
}

ConstNodeRef Tree::crootref() const
{
    return ConstNodeRef(this, root_id());
}

NodeRef Tree::ref(id_type id)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, id != NONE && id >= 0 && id < m_cap, this, id);
    return NodeRef(this, id);
}
ConstNodeRef Tree::ref(id_type id) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, id != NONE && id >= 0 && id < m_cap, this, id);
    return ConstNodeRef(this, id);
}
ConstNodeRef Tree::cref(id_type id) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, id != NONE && id >= 0 && id < m_cap, this, id);
    return ConstNodeRef(this, id);
}

NodeRef Tree::operator[] (csubstr key)
{
    return rootref()[key];
}
ConstNodeRef Tree::operator[] (csubstr key) const
{
    return crootref()[key];
}

NodeRef Tree::operator[] (id_type i)
{
    return rootref()[i];
}
ConstNodeRef Tree::operator[] (id_type i) const
{
    return crootref()[i];
}

NodeRef Tree::docref(id_type i)
{
    return ref(doc(i));
}
ConstNodeRef Tree::docref(id_type i) const
{
    return ConstNodeRef(this, doc(i));
}
ConstNodeRef Tree::cdocref(id_type i) const
{
    return ConstNodeRef(this, doc(i));
}


//-----------------------------------------------------------------------------
Tree::Tree(Callbacks const& cb)
    : Tree(RYML_DEFAULT_TREE_CAPACITY, RYML_DEFAULT_TREE_ARENA_CAPACITY, cb)
{
}

Tree::Tree(id_type node_capacity, size_t arena_capacity, Callbacks const& cb)
    : m_buf(nullptr)
    , m_cap(0)
    , m_size(0)
    , m_free_head(NONE)
    , m_free_tail(NONE)
    , m_arena()
    , m_arena_pos(0)
    , m_callbacks(cb)
    , m_tag_directives()
{
    if(node_capacity)
        reserve(node_capacity);
    if(arena_capacity)
        reserve_arena(arena_capacity);
}

Tree::~Tree() noexcept
{
    _free();
}


Tree::Tree(Tree const& that) : m_callbacks(that.m_callbacks)
{
    _clear();
    _copy(that);
}

Tree::Tree(Tree && that) noexcept : m_callbacks(that.m_callbacks)
{
    _clear();
    _move(that);
}

Tree& Tree::operator= (Tree const& that)
{
    if(&that != this)
    {
        _free();
        m_callbacks = that.m_callbacks;
        _copy(that);
    }
    return *this;
}

Tree& Tree::operator= (Tree && that) noexcept
{
    if(&that != this)
    {
        _free();
        m_callbacks = that.m_callbacks;
        _move(that);
    }
    return *this;
}

void Tree::_free()
{
    if(m_buf)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_cap > 0, this, NONE);
        RYML_CB_FREE_(m_callbacks, m_buf, NodeData, (size_t)m_cap);
    }
    if(m_arena.str)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.len > 0, this, NONE);
        RYML_CB_FREE_(m_callbacks, m_arena.str, char, m_arena.len);
    }
    _clear();
}


C4_SUPPRESS_WARNING_GCC_PUSH
#if defined(__GNUC__) && __GNUC__>= 8
    C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wclass-memaccess") // error: ‘void* memset(void*, int, size_t)’ clearing an object of type ‘class c4::yml::Tree’ with no trivial copy-assignment; use assignment or value-initialization instead
#endif

void Tree::_clear()
{
    m_buf = nullptr;
    m_cap = 0;
    m_size = 0;
    m_free_head = 0;
    m_free_tail = 0;
    m_arena = {};
    m_arena_pos = 0;
    m_tag_directives.clear();
}

void Tree::_copy(Tree const& that)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_buf == nullptr, this, NONE);
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.str == nullptr, this, NONE);
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.len == 0, this, NONE);
    if(that.m_cap)
    {
        m_buf = RYML_CB_ALLOC_HINT_(m_callbacks, NodeData, (size_t)that.m_cap, that.m_buf);
        memcpy(m_buf, that.m_buf, (size_t)that.m_cap * sizeof(NodeData));
    }
    m_cap = that.m_cap;
    m_size = that.m_size;
    m_free_head = that.m_free_head;
    m_free_tail = that.m_free_tail;
    m_arena_pos = that.m_arena_pos;
    m_arena = that.m_arena;
    m_tag_directives = that.m_tag_directives;
    if(that.m_arena.str)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, that.m_arena.len > 0, this, NONE);
        substr arena;
        arena.str = RYML_CB_ALLOC_HINT_(m_callbacks, char, that.m_arena.len, that.m_arena.str);
        arena.len = that.m_arena.len;
        _relocate(arena); // does a memcpy of the arena and updates nodes using the old arena
        m_arena = arena;
    }
}

void Tree::_move(Tree & that) noexcept
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_buf == nullptr, this, NONE);
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.str == nullptr, this, NONE);
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_arena.len == 0, this, NONE);
    m_buf = that.m_buf;
    m_cap = that.m_cap;
    m_size = that.m_size;
    m_free_head = that.m_free_head;
    m_free_tail = that.m_free_tail;
    m_arena = that.m_arena;
    m_arena_pos = that.m_arena_pos;
    m_tag_directives = that.m_tag_directives;
    that._clear();
}

void Tree::_relocate(substr next_arena)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, next_arena.not_empty(), this, NONE);
    RYML_ASSERT_VISIT_CB_(m_callbacks, next_arena.len >= m_arena.len, this, NONE);
    if(m_arena_pos)
    {
        memcpy(next_arena.str, m_arena.str, m_arena_pos);
    }
    for(NodeData *C4_RESTRICT n = m_buf, *e = m_buf + m_cap; n != e; ++n)
    {
        if(in_arena(n->m_key.scalar))
            n->m_key.scalar = _relocated(n->m_key.scalar, next_arena);
        if(in_arena(n->m_key.tag))
            n->m_key.tag = _relocated(n->m_key.tag, next_arena);
        if(in_arena(n->m_key.anchor))
            n->m_key.anchor = _relocated(n->m_key.anchor, next_arena);
        if(in_arena(n->m_val.scalar))
            n->m_val.scalar = _relocated(n->m_val.scalar, next_arena);
        if(in_arena(n->m_val.tag))
            n->m_val.tag = _relocated(n->m_val.tag, next_arena);
        if(in_arena(n->m_val.anchor))
            n->m_val.anchor = _relocated(n->m_val.anchor, next_arena);
    }
    for(TagDirective &C4_RESTRICT td : m_tag_directives)
    {
        if(in_arena(td.prefix))
            td.prefix = _relocated(td.prefix, next_arena);
        if(in_arena(td.handle))
            td.handle = _relocated(td.handle, next_arena);
    }
}


//-----------------------------------------------------------------------------
void Tree::reserve(id_type cap)
{
    if(cap > m_cap)
    {
        NodeData *buf = RYML_CB_ALLOC_HINT_(m_callbacks, NodeData, (size_t)cap, m_buf);
        if(m_buf)
        {
            memcpy(buf, m_buf, (size_t)m_cap * sizeof(NodeData));
            RYML_CB_FREE_(m_callbacks, m_buf, NodeData, (size_t)m_cap);
        }
        id_type first = m_cap, del = cap - m_cap;
        m_cap = cap;
        m_buf = buf;
        _clear_range(first, del);
        if(m_free_head != NONE)
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, m_buf != nullptr, this, NONE);
            RYML_ASSERT_VISIT_CB_(m_callbacks, m_free_tail != NONE, this, NONE);
            m_buf[m_free_tail].m_next_sibling = first;
            m_buf[first].m_prev_sibling = m_free_tail;
            m_free_tail = cap-1;
        }
        else
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, m_free_tail == NONE, this, NONE);
            m_free_head = first;
            m_free_tail = cap-1;
        }
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_free_head == NONE || (m_free_head >= 0 && m_free_head < cap), this, NONE);
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_free_tail == NONE || (m_free_tail >= 0 && m_free_tail < cap), this, NONE);

        if( ! m_size)
            _claim_root();
    }
}


//-----------------------------------------------------------------------------
void Tree::clear()
{
    _clear_range(0, m_cap);
    m_size = 0;
    if(m_buf)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_cap >= 0, this, NONE);
        m_free_head = 0;
        m_free_tail = m_cap-1;
        _claim_root();
    }
    else
    {
        m_free_head = NONE;
        m_free_tail = NONE;
    }
    m_tag_directives.clear();
}

void Tree::_claim_root()
{
    id_type r = _claim();
    RYML_ASSERT_VISIT_CB_(m_callbacks, r == 0, this, r);
    _set_hierarchy(r, NONE, NONE);
}


//-----------------------------------------------------------------------------
void Tree::_clear_range(id_type first, id_type num)
{
    if(num == 0)
        return; // prevent overflow when subtracting
    RYML_ASSERT_VISIT_CB_(m_callbacks, first >= 0 && first + num <= m_cap, this, first);
    memset(m_buf + first, 0, (size_t)num * sizeof(NodeData)); // TODO we should not need this
    for(id_type i = first, e = first + num; i < e; ++i)
    {
        _clear(i);
        NodeData *n = m_buf + i;
        n->m_prev_sibling = i - 1;
        n->m_next_sibling = i + 1;
    }
    m_buf[first + num - 1].m_next_sibling = NONE;
}

C4_SUPPRESS_WARNING_GCC_POP


//-----------------------------------------------------------------------------
void Tree::_release(id_type i)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, i >= 0 && i < m_cap, this, i);

    _rem_hierarchy(i);
    _free_list_add(i);
    _clear(i);

    --m_size;
}

//-----------------------------------------------------------------------------
// add to the front of the free list
void Tree::_free_list_add(id_type i)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, i >= 0 && i < m_cap, this, i);
    NodeData &C4_RESTRICT w = m_buf[i];

    w.m_parent = NONE;
    w.m_next_sibling = m_free_head;
    w.m_prev_sibling = NONE;
    if(m_free_head != NONE)
        m_buf[m_free_head].m_prev_sibling = i;
    m_free_head = i;
    if(m_free_tail == NONE)
        m_free_tail = m_free_head;
}

void Tree::_free_list_rem(id_type i)
{
    if(m_free_head == i)
        m_free_head = _p(i)->m_next_sibling;
    _rem_hierarchy(i);
}

//-----------------------------------------------------------------------------
id_type Tree::_claim()
{
    if(m_free_head == NONE || m_buf == nullptr)
    {
        id_type sz = 2 * m_cap;
        sz = sz ? sz : 16;
        reserve(sz);
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_free_head != NONE, this, NONE);
    }

    RYML_ASSERT_VISIT_CB_(m_callbacks, m_size < m_cap, this, NONE);
    RYML_ASSERT_VISIT_CB_(m_callbacks, m_free_head >= 0 && m_free_head < m_cap, this, NONE);

    id_type ichild = m_free_head;
    NodeData *child = m_buf + ichild;

    ++m_size;
    m_free_head = child->m_next_sibling;
    if(m_free_head == NONE)
    {
        m_free_tail = NONE;
        RYML_ASSERT_VISIT_CB_(m_callbacks, m_size == m_cap, this, NONE);
    }

    _clear(ichild);

    return ichild;
}

//-----------------------------------------------------------------------------

void Tree::_set_hierarchy(id_type ichild, id_type iparent, id_type iprev_sibling)
{
    C4_SUPPRESS_WARNING_PUSH
    C4_SUPPRESS_WARNING_CLANG("-Wnull-dereference")
    #if defined(__GNUC__)
    #if (__GNUC__ >= 6)
    C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
    #endif
    #if (__GNUC__ > 9)
    C4_SUPPRESS_WARNING_GCC("-Wanalyzer-fd-leak")
    #endif
    #endif
    RYML_ASSERT_VISIT_CB_(m_callbacks, ichild >= 0 && ichild < m_cap, this, ichild);
    RYML_ASSERT_VISIT_CB_(m_callbacks, iparent == NONE || (iparent >= 0 && iparent < m_cap), this, iparent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, iprev_sibling == NONE || (iprev_sibling >= 0 && iprev_sibling < m_cap), this, iprev_sibling);

    NodeData *C4_RESTRICT child = _p(ichild);

    child->m_parent = iparent;
    child->m_prev_sibling = NONE;
    child->m_next_sibling = NONE;

    if(iparent == NONE)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, ichild == 0, this, ichild);
        RYML_ASSERT_VISIT_CB_(m_callbacks, iprev_sibling == NONE, this, iprev_sibling);
    }

    if(iparent == NONE)
        return;

    id_type inext_sibling = iprev_sibling != NONE ? next_sibling(iprev_sibling) : first_child(iparent);
    NodeData *C4_RESTRICT parent = get(iparent);
    NodeData *C4_RESTRICT psib   = get(iprev_sibling);
    NodeData *C4_RESTRICT nsib   = get(inext_sibling);

    if(psib)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, next_sibling(iprev_sibling) == id(nsib), this, iprev_sibling);
        child->m_prev_sibling = id(psib);
        psib->m_next_sibling = id(child);
        RYML_ASSERT_VISIT_CB_(m_callbacks, psib->m_prev_sibling != psib->m_next_sibling || psib->m_prev_sibling == NONE, this, iprev_sibling);
    }

    if(nsib)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, prev_sibling(inext_sibling) == id(psib), this, inext_sibling);
        child->m_next_sibling = id(nsib);
        nsib->m_prev_sibling = id(child);
        RYML_ASSERT_VISIT_CB_(m_callbacks, nsib->m_prev_sibling != nsib->m_next_sibling || nsib->m_prev_sibling == NONE, this, inext_sibling);
    }

    if(parent->m_first_child == NONE)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, parent->m_last_child == NONE, this, parent->m_last_child);
        parent->m_first_child = id(child);
        parent->m_last_child = id(child);
    }
    else
    {
        if(child->m_next_sibling == parent->m_first_child)
            parent->m_first_child = id(child);

        if(child->m_prev_sibling == parent->m_last_child)
            parent->m_last_child = id(child);
    }
    C4_SUPPRESS_WARNING_POP
}



//-----------------------------------------------------------------------------
void Tree::_rem_hierarchy(id_type i)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, i >= 0 && i < m_cap, this, i);

    NodeData &C4_RESTRICT w = m_buf[i];

    // remove from the parent
    if(w.m_parent != NONE)
    {
        NodeData &C4_RESTRICT p = m_buf[w.m_parent];
        if(p.m_first_child == i)
        {
            p.m_first_child = w.m_next_sibling;
        }
        if(p.m_last_child == i)
        {
            p.m_last_child = w.m_prev_sibling;
        }
    }

    // remove from the used list
    if(w.m_prev_sibling != NONE)
    {
        NodeData *C4_RESTRICT prev = get(w.m_prev_sibling);
        prev->m_next_sibling = w.m_next_sibling;
    }
    if(w.m_next_sibling != NONE)
    {
        NodeData *C4_RESTRICT next = get(w.m_next_sibling);
        next->m_prev_sibling = w.m_prev_sibling;
    }
}

//-----------------------------------------------------------------------------
/** @cond dev */
id_type Tree::_do_reorder(id_type *node, id_type count)
{
    // swap this node if it's not in place
    if(*node != count)
    {
        _swap(*node, count);
        *node = count;
    }
    ++count; // bump the count from this node

    // now descend in the hierarchy
    for(id_type i = first_child(*node); i != NONE; i = next_sibling(i))
    {
        // this child may have been relocated to a different index,
        // so get an updated version
        count = _do_reorder(&i, count);
    }
    return count;
}
/** @endcond */

void Tree::reorder()
{
    id_type r = root_id();
    _do_reorder(&r, 0);
}


//-----------------------------------------------------------------------------
/** @cond dev */
void Tree::_swap(id_type n_, id_type m_)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, (parent(n_) != NONE) || type(n_) == NOTYPE, this, n_);
    RYML_ASSERT_VISIT_CB_(m_callbacks, (parent(m_) != NONE) || type(m_) == NOTYPE, this, m_);
    NodeType tn = type(n_);
    NodeType tm = type(m_);
    if(tn != NOTYPE && tm != NOTYPE)
    {
        _swap_props(n_, m_);
        _swap_hierarchy(n_, m_);
    }
    else if(tn == NOTYPE && tm != NOTYPE)
    {
        _copy_props(n_, m_);
        _free_list_rem(n_);
        _copy_hierarchy(n_, m_);
        _clear(m_);
        _free_list_add(m_);
    }
    else if(tn != NOTYPE && tm == NOTYPE)
    {
        _copy_props(m_, n_);
        _free_list_rem(m_);
        _copy_hierarchy(m_, n_);
        _clear(n_);
        _free_list_add(n_);
    }
    else
    {
        C4_NEVER_REACH();
    }
}

//-----------------------------------------------------------------------------
void Tree::_swap_hierarchy(id_type ia, id_type ib)
{
    if(ia == ib) return;

    for(id_type i = first_child(ia); i != NONE; i = next_sibling(i))
    {
        if(i == ib || i == ia)
            continue;
        _p(i)->m_parent = ib;
    }

    for(id_type i = first_child(ib); i != NONE; i = next_sibling(i))
    {
        if(i == ib || i == ia)
            continue;
        _p(i)->m_parent = ia;
    }

    auto & C4_RESTRICT a  = *_p(ia);
    auto & C4_RESTRICT b  = *_p(ib);
    auto & C4_RESTRICT pa = *_p(a.m_parent);
    auto & C4_RESTRICT pb = *_p(b.m_parent);

    if(&pa == &pb)
    {
        if((pa.m_first_child == ib && pa.m_last_child == ia)
            ||
           (pa.m_first_child == ia && pa.m_last_child == ib))
        {
            std::swap(pa.m_first_child, pa.m_last_child);
        }
        else
        {
            bool changed = false;
            if(pa.m_first_child == ia)
            {
                pa.m_first_child = ib;
                changed = true;
            }
            if(pa.m_last_child  == ia)
            {
                pa.m_last_child = ib;
                changed = true;
            }
            if(pb.m_first_child == ib && !changed)
            {
                pb.m_first_child = ia;
            }
            if(pb.m_last_child  == ib && !changed)
            {
                pb.m_last_child  = ia;
            }
        }
    }
    else
    {
        if(pa.m_first_child == ia)
            pa.m_first_child = ib;
        if(pa.m_last_child  == ia)
            pa.m_last_child  = ib;
        if(pb.m_first_child == ib)
            pb.m_first_child = ia;
        if(pb.m_last_child  == ib)
            pb.m_last_child  = ia;
    }
    std::swap(a.m_first_child , b.m_first_child);
    std::swap(a.m_last_child  , b.m_last_child);

    if(a.m_prev_sibling != ib && b.m_prev_sibling != ia &&
       a.m_next_sibling != ib && b.m_next_sibling != ia)
    {
        if(a.m_prev_sibling != NONE && a.m_prev_sibling != ib)
            _p(a.m_prev_sibling)->m_next_sibling = ib;
        if(a.m_next_sibling != NONE && a.m_next_sibling != ib)
            _p(a.m_next_sibling)->m_prev_sibling = ib;
        if(b.m_prev_sibling != NONE && b.m_prev_sibling != ia)
            _p(b.m_prev_sibling)->m_next_sibling = ia;
        if(b.m_next_sibling != NONE && b.m_next_sibling != ia)
            _p(b.m_next_sibling)->m_prev_sibling = ia;
        std::swap(a.m_prev_sibling, b.m_prev_sibling);
        std::swap(a.m_next_sibling, b.m_next_sibling);
    }
    else
    {
        if(a.m_next_sibling == ib) // n will go after m
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, b.m_prev_sibling == ia, this, ia);
            if(a.m_prev_sibling != NONE)
            {
                RYML_ASSERT_VISIT_CB_(m_callbacks, a.m_prev_sibling != ib, this, ib);
                _p(a.m_prev_sibling)->m_next_sibling = ib;
            }
            if(b.m_next_sibling != NONE)
            {
                RYML_ASSERT_VISIT_CB_(m_callbacks, b.m_next_sibling != ia, this, ia);
                _p(b.m_next_sibling)->m_prev_sibling = ia;
            }
            id_type ns = b.m_next_sibling;
            b.m_prev_sibling = a.m_prev_sibling;
            b.m_next_sibling = ia;
            a.m_prev_sibling = ib;
            a.m_next_sibling = ns;
        }
        else if(a.m_prev_sibling == ib) // m will go after n
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, b.m_next_sibling == ia, this, ia);
            if(b.m_prev_sibling != NONE)
            {
                RYML_ASSERT_VISIT_CB_(m_callbacks, b.m_prev_sibling != ia, this, ia);
                _p(b.m_prev_sibling)->m_next_sibling = ia;
            }
            if(a.m_next_sibling != NONE)
            {
                RYML_ASSERT_VISIT_CB_(m_callbacks, a.m_next_sibling != ib, this, ib);
                _p(a.m_next_sibling)->m_prev_sibling = ib;
            }
            id_type ns = b.m_prev_sibling;
            a.m_prev_sibling = b.m_prev_sibling;
            a.m_next_sibling = ib;
            b.m_prev_sibling = ia;
            b.m_next_sibling = ns;
        }
        else
        {
            C4_NEVER_REACH();
        }
    }
    RYML_ASSERT_VISIT_CB_(m_callbacks, a.m_next_sibling != ia, this, ia);
    RYML_ASSERT_VISIT_CB_(m_callbacks, a.m_prev_sibling != ia, this, ia);
    RYML_ASSERT_VISIT_CB_(m_callbacks, b.m_next_sibling != ib, this, ib);
    RYML_ASSERT_VISIT_CB_(m_callbacks, b.m_prev_sibling != ib, this, ib);

    if(a.m_parent != ib && b.m_parent != ia)
    {
        std::swap(a.m_parent, b.m_parent);
    }
    else
    {
        if(a.m_parent == ib && b.m_parent != ia)
        {
            a.m_parent = b.m_parent;
            b.m_parent = ia;
        }
        else if(a.m_parent != ib && b.m_parent == ia)
        {
            b.m_parent = a.m_parent;
            a.m_parent = ib;
        }
        else
        {
            C4_NEVER_REACH();
        }
    }
}

//-----------------------------------------------------------------------------
void Tree::_copy_hierarchy(id_type dst_, id_type src_)
{
    auto const& C4_RESTRICT src = *_p(src_);
    auto      & C4_RESTRICT dst = *_p(dst_);
    auto      & C4_RESTRICT prt = *_p(src.m_parent);
    for(id_type i = src.m_first_child; i != NONE; i = next_sibling(i))
    {
        _p(i)->m_parent = dst_;
    }
    if(src.m_prev_sibling != NONE)
    {
        _p(src.m_prev_sibling)->m_next_sibling = dst_;
    }
    if(src.m_next_sibling != NONE)
    {
        _p(src.m_next_sibling)->m_prev_sibling = dst_;
    }
    if(prt.m_first_child == src_)
    {
        prt.m_first_child = dst_;
    }
    if(prt.m_last_child  == src_)
    {
        prt.m_last_child  = dst_;
    }
    dst.m_parent       = src.m_parent;
    dst.m_first_child  = src.m_first_child;
    dst.m_last_child   = src.m_last_child;
    dst.m_prev_sibling = src.m_prev_sibling;
    dst.m_next_sibling = src.m_next_sibling;
}

//-----------------------------------------------------------------------------
void Tree::_swap_props(id_type n_, id_type m_)
{
    NodeData &C4_RESTRICT n = *_p(n_);
    NodeData &C4_RESTRICT m = *_p(m_);
    std::swap(n.m_type, m.m_type);
    std::swap(n.m_key, m.m_key);
    std::swap(n.m_val, m.m_val);
}
/** @endcond */


//-----------------------------------------------------------------------------
void Tree::move(id_type node, id_type after)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != after, this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! is_root(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, (after == NONE) || (has_sibling(node, after) && has_sibling(after, node)), this, node);

    _rem_hierarchy(node);
    _set_hierarchy(node, parent(node), after);
}

//-----------------------------------------------------------------------------

void Tree::move(id_type node, id_type new_parent, id_type after)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != after, this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, new_parent != NONE, this, new_parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, new_parent != node, this, new_parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, new_parent != after, this, new_parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! is_root(node), this, node);

    _rem_hierarchy(node);
    _set_hierarchy(node, new_parent, after);
}

id_type Tree::move(Tree *src, id_type node, id_type new_parent, id_type after)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, src != nullptr, this, new_parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, new_parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, new_parent != NONE, this, new_parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, new_parent != after, this, new_parent);

    id_type dup = duplicate(src, node, new_parent, after);
    src->remove(node);
    return dup;
}

void Tree::set_root_as_stream()
{
    id_type root = root_id();
    NodeType ty = type(root);
    if(ty.is_stream())
        return;
    _c4dbgpf("set_root_as_stream. rootty={}", type(root).m_bits);
    bool empty_root = ((type(root) & (SEQ|MAP|VAL)) == 0);
    for(TagDirective &C4_RESTRICT td : m_tag_directives)
    {
        if(td.doc_id >= m_cap || _p(td.doc_id)->m_parent == NONE)
        {
            _c4dbgpf("tagd[{}]: id={}->NONE", &td-m_tag_directives.m_directives, td.doc_id);
            td.doc_id = NONE;
        }
    }
    // don't use _add_flags() because it's checked and will fail
    id_type next_doc;
    if(!has_children(root))
    {
        if(ty.is_container())
        {
            next_doc = append_child(root);
            _copy_props_wo_key(next_doc, root);
            _p(next_doc)->m_type.add(DOC);
        }
        else
        {
            _p(root)->m_type.add(SEQ);
            next_doc = append_child(root);
            _copy_props_wo_key(next_doc, root);
            _p(next_doc)->m_type.add(DOC);
            _p(next_doc)->m_type.rem(SEQ);
        }
    }
    else
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, !ty.has_key(), this, root);
        next_doc = append_child(root);
        _copy_props_wo_key(next_doc, root);
        _add_flags(next_doc, DOC);
        for(id_type prev = NONE, ch = first_child(root), next = next_sibling(ch); ch != NONE; )
        {
            if(ch == next_doc)
                break;
            move(ch, next_doc, prev);
            prev = ch;
            ch = next;
            next = next_sibling(next);
        }
    }
    _p(root)->m_type = STREAM;
    for(TagDirective &C4_RESTRICT td : m_tag_directives)
    {
        id_type id = (td.doc_id != NONE) ? next_doc : (empty_root ? first_child(root) : m_free_head);
        _c4dbgpf("tagd[{}]: id={}->{}", &td-m_tag_directives.m_directives, td.doc_id, id);
        td.doc_id = id;
    }
}


//-----------------------------------------------------------------------------
void Tree::remove_children(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, get(node) != nullptr, this, node);
    C4_SUPPRESS_WARNING_GCC_PUSH
    #if defined(__GNUC__) && __GNUC__ >= 6
    C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
    #endif
    id_type ich = get(node)->m_first_child;
    while(ich != NONE)
    {
        remove_children(ich);
        RYML_ASSERT_VISIT_CB_(m_callbacks, get(ich) != nullptr, this, node);
        id_type next = get(ich)->m_next_sibling;
        _release(ich);
        if(ich == get(node)->m_last_child)
            break;
        ich = next;
    }
    C4_SUPPRESS_WARNING_GCC_POP
}


//-----------------------------------------------------------------------------
bool Tree::change_type(id_type node, NodeType next)
{
    NodeType curr = this->type(node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, next.is_val() || next.is_map() || next.is_seq(), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, next.is_val() + next.is_map() + next.is_seq() == 1, this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, next.has_key() == curr.has_key() || (curr.has_key() && !next.has_key()), this, node);
    NodeData *d = _p(node);
    if(next.is_map() && curr.is_map())
        return false;
    else if(next.is_seq() && curr.is_seq())
        return false;
    else if(next.is_val() && curr.is_val())
        return false;
    d->m_type.m_bits = (d->m_type.m_bits & (~(MAP|SEQ|VAL|CONTAINER_STYLE|KEY_STYLE|VAL_STYLE))) | next;
    remove_children(node);
    return true;
}


//-----------------------------------------------------------------------------
id_type Tree::duplicate(id_type node, id_type parent, id_type after)
{
    return duplicate(this, node, parent, after);
}

id_type Tree::duplicate(Tree const* src, id_type node, id_type parent, id_type after)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, src != nullptr, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent != NONE, this, parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! src->is_root(node), src, node);

    id_type copy = _claim();

    _copy_props(copy, src, node);
    _set_hierarchy(copy, parent, after);
    duplicate_children(src, node, copy, NONE);

    return copy;
}


//-----------------------------------------------------------------------------
id_type Tree::duplicate_children(id_type node, id_type parent, id_type after)
{
    return duplicate_children(this, node, parent, after);
}

id_type Tree::duplicate_children(Tree const* src, id_type node, id_type parent, id_type after)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, src != nullptr, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent != NONE, this, parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, after == NONE || has_child(parent, after), this, parent);

    id_type prev = after;
    for(id_type i = src->first_child(node); i != NONE; i = src->next_sibling(i))
    {
        prev = duplicate(src, i, parent, prev);
    }

    return prev;
}

//-----------------------------------------------------------------------------
void Tree::duplicate_contents(id_type node, id_type where)
{
    duplicate_contents(this, node, where);
}

void Tree::duplicate_contents(Tree const *src, id_type node, id_type where)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, src != nullptr, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, where != NONE, this, where);
    _copy_props_wo_key(where, src, node);
    duplicate_children(src, node, where, last_child(where));
}

//-----------------------------------------------------------------------------
id_type Tree::duplicate_children_no_rep(id_type node, id_type parent, id_type after)
{
    return duplicate_children_no_rep(this, node, parent, after);
}

id_type Tree::duplicate_children_no_rep(Tree const *src, id_type node, id_type parent, id_type after)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, src, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent != NONE, this, parent);
    RYML_ASSERT_VISIT_CB_(m_callbacks, after == NONE || has_child(parent, after), this, parent);

    // don't loop using pointers as there may be a relocation

    // find the position where "after" is
    id_type after_pos = NONE;
    if(after != NONE)
    {
        for(id_type i = first_child(parent), icount = 0; i != NONE; ++icount, i = next_sibling(i))
        {
            if(i == after)
            {
                after_pos = icount;
                break;
            }
        }
        RYML_ASSERT_VISIT_CB_(m_callbacks, after_pos != NONE, this, node);
    }

    // for each child to be duplicated...
    id_type prev = after;
    NodeType pty = type(parent);
    for(id_type i = src->first_child(node); i != NONE; i = src->next_sibling(i))
    {
        _c4dbgpf("duplicate_no_rep: {} -> {}/{}", i, parent, prev);
        RYML_CHECK_VISIT_CB_(m_callbacks, this != src || (parent != i && !is_ancestor(parent, i)), this, parent);
        if(pty.is_seq())
        {
            _c4dbgpf("duplicate_no_rep: {} is seq", parent);
            prev = duplicate(src, i, parent, prev);
        }
        else
        {
            _c4dbgpf("duplicate_no_rep: {} is map", parent);
            RYML_ASSERT_VISIT_CB_(m_callbacks, pty.is_map(), this, parent);
            // does the parent already have a node with key equal to that of the current duplicate?
            id_type dstnode_dup = NONE, dstnode_dup_pos = NONE;
            {
                csubstr srckey = src->key(i);
                for(id_type j = first_child(parent), jcount = 0; j != NONE; ++jcount, j = next_sibling(j))
                {
                    if(key(j) == srckey)
                    {
                        _c4dbgpf("duplicate_no_rep: found matching key '{}' src={}/{} dst={}/{}", srckey, node, i, parent, j);
                        dstnode_dup = j;
                        dstnode_dup_pos = jcount;
                        break;
                    }
                }
            }
            _c4dbgpf("duplicate_no_rep: dstnode_dup={} dstnode_dup_pos={} after_pos={}", dstnode_dup, dstnode_dup_pos, after_pos);
            if(dstnode_dup == NONE) // there is no repetition; just duplicate
            {
                _c4dbgpf("duplicate_no_rep: no repetition, just duplicate i={} parent={} prev={}", i, parent, prev);
                prev = duplicate(src, i, parent, prev);
            }
            else  // yes, there is a repetition
            {
                if(after_pos != NONE && dstnode_dup_pos <= after_pos)
                {
                    // the dst duplicate is located before the node which will be inserted,
                    // and will be overridden by the duplicate. So replace it.
                    _c4dbgpf("duplicate_no_dstnode_dup: replace {}/{} with {}/{}", parent, dstnode_dup, node, i);
                    if(prev == dstnode_dup)
                        prev = prev_sibling(dstnode_dup);
                    remove(dstnode_dup);
                    prev = duplicate(src, i, parent, prev);
                }
                else if(prev == NONE)
                {
                    _c4dbgpf("duplicate_no_dstnode_dup: {}=prev <- {}", prev, dstnode_dup);
                    // first iteration with prev = after = NONE and dstnode_dupetition
                    prev = dstnode_dup;
                }
                else if(dstnode_dup != prev)
                {
                    // dstnode_dup is located after the node which will be inserted
                    // and overrides it. So move the dstnode_dup into this node's place.
                    _c4dbgpf("duplicate_no_dstnode_dup: move({}, {})", dstnode_dup, prev);
                    move(dstnode_dup, prev);
                    prev = dstnode_dup;
                }
            } // there's a dstnode_dupetition
        }
    }

    return prev;
}


//-----------------------------------------------------------------------------

void Tree::merge_with(Tree const *src, id_type src_node, id_type dst_node)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, src != nullptr, src, src_node);
    if(src_node == NONE)
        src_node = src->root_id();
    if(dst_node == NONE)
        dst_node = root_id();
    NodeType srcty = src->type(src_node);
    NodeType dstty = type(dst_node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, srcty.has_val() || srcty.is_seq() || srcty.is_map(), src, src_node);
    if(srcty.has_val())
    {
        type_bits mask_src = ~STYLE; // keep the existing style if it is already a val
        if( ! dstty.has_val())
        {
            if(has_children(dst_node))
                remove_children(dst_node);
            mask_src |= VAL_STYLE; // copy the src style
        }
        if(srcty.is_keyval())
        {
            _copy_props(dst_node, src, src_node, mask_src);
        }
        else
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, srcty.is_val(), src, src_node);
            _copy_props_wo_key(dst_node, src, src_node, mask_src);
        }
    }
    else if(srcty.is_seq())
    {
        if( ! dstty.is_seq())
        {
            if(has_children(dst_node))
                remove_children(dst_node);
            _clear_type(dst_node);
            if(src->has_key(src_node))
                set_key(dst_node, src->key(src_node));
            set_seq(dst_node);
            _p(dst_node)->m_type = src->_p(src_node)->m_type;
        }
        for(id_type sch = src->first_child(src_node); sch != NONE; sch = src->next_sibling(sch))
        {
            id_type dch = append_child(dst_node);
            _copy_props_wo_key(dch, src, sch);
            merge_with(src, sch, dch);
        }
    }
    else
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, srcty.is_map(), src, src_node);
        if( ! dstty.is_map())
        {
            if(has_children(dst_node))
                remove_children(dst_node);
            _clear_type(dst_node);
            if(src->has_key(src_node))
                set_key(dst_node, src->key(src_node));
            set_map(dst_node);
            _p(dst_node)->m_type = src->_p(src_node)->m_type;
        }
        for(id_type sch = src->first_child(src_node); sch != NONE; sch = src->next_sibling(sch))
        {
            id_type dch = find_child(dst_node, src->key(sch));
            if(dch == NONE)
            {
                dch = append_child(dst_node);
                _copy_props(dch, src, sch);
            }
            merge_with(src, sch, dch);
        }
    }
}


//-----------------------------------------------------------------------------

void Tree::resolve(bool clear_anchors)
{
    if(m_size == 0)
        return;
    ReferenceResolver rr;
    resolve(&rr, clear_anchors);
}

void Tree::resolve(ReferenceResolver *C4_RESTRICT rr, bool clear_anchors)
{
    if(m_size == 0)
        return;
    rr->resolve(this, clear_anchors);
}


//-----------------------------------------------------------------------------

id_type Tree::num_children(id_type node) const
{
    id_type count = 0;
    for(id_type i = first_child(node); i != NONE; i = next_sibling(i))
        ++count;
    return count;
}

id_type Tree::child_pos(id_type node, id_type ch) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    id_type count = 0;
    for(id_type i = first_child(node); i != NONE; i = next_sibling(i))
    {
        if(i == ch)
            return count;
        ++count;
    }
    return NONE;
}

id_type Tree::child(id_type node, id_type pos) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    id_type count = 0;
    for(id_type i = first_child(node); i != NONE; i = next_sibling(i))
    {
        if(count++ == pos)
            return i;
    }
    return NONE;
}

id_type Tree::find_child(id_type node, csubstr const& name) const
{
    C4_SUPPRESS_WARNING_PUSH
    #if defined(__clang__)
    #elif defined(__GNUC__)
    #   if __GNUC__ >= 6
            C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
    #   endif
    #   if __GNUC__ > 9
            C4_SUPPRESS_WARNING_GCC("-Wanalyzer-null-dereference")
    #   endif
    #endif
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, _p(node)->m_type.m_bits & MAP, this, node);
    for(id_type i = first_child(node); i != NONE; i = next_sibling(i))
    {
        if(_p(i)->m_key.scalar == name)
            return i;
    }
    return NONE;
    C4_SUPPRESS_WARNING_POP
}


namespace {
id_type depth_desc_(Tree const& C4_RESTRICT t, id_type id, id_type currdepth=0, id_type maxdepth=0)
{
    maxdepth = currdepth > maxdepth ? currdepth : maxdepth;
    for(id_type child = t.first_child(id); child != NONE; child = t.next_sibling(child))
    {
        const id_type d = depth_desc_(t, child, currdepth+1, maxdepth);
        maxdepth = d > maxdepth ? d : maxdepth;
    }
    return maxdepth;
}
}

id_type Tree::depth_desc(id_type node) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    return depth_desc_(*this, node);
}

id_type Tree::depth_asc(id_type node) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    id_type depth = 0;
    while(!is_root(node))
    {
        ++depth;
        node = parent(node);
    }
    return depth;
}

bool Tree::is_ancestor(id_type node, id_type ancestor) const
{
    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, node);
    id_type p = parent(node);
    while(p != NONE)
    {
        if(p == ancestor)
            return true;
        p = parent(p);
    }
    return false;
}


//-----------------------------------------------------------------------------

/** @cond dev */ // LCOV_EXCL_START
void Tree::to_val(id_type node, csubstr val, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent(node) == NONE || ! parent_is_map(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, !is_seq(node) && !is_map(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = VAL|more_flags;
    nd->m_key.clear();
    nd->m_val = val;
}

void Tree::to_keyval(id_type node, csubstr key, csubstr val, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent(node) == NONE || parent_is_map(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, !is_seq(node) && !is_map(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = KEYVAL|more_flags;
    nd->m_key = key;
    nd->m_val = val;
}

void Tree::to_map(id_type node, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = MAP|more_flags;
    nd->m_key.clear();
    nd->m_val.clear();
}

void Tree::to_map(id_type node, csubstr key, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent(node) == NONE || parent_is_map(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = KEY|MAP|more_flags;
    nd->m_key = key;
    nd->m_val.clear();
}

void Tree::to_seq(id_type node, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent(node) == NONE || parent_is_seq(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = SEQ|more_flags;
    nd->m_key.clear();
    nd->m_val.clear();
}

void Tree::to_seq(id_type node, csubstr key, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    RYML_ASSERT_VISIT_CB_(m_callbacks, parent(node) == NONE || parent_is_map(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = KEY|SEQ|more_flags;
    nd->m_key = key;
    nd->m_val.clear();
}

void Tree::to_doc(id_type node, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = DOC|more_flags;
    nd->m_key.clear();
    nd->m_val.clear();
}

void Tree::to_stream(id_type node, type_bits more_flags)
{
    RYML_ASSERT_VISIT_CB_(m_callbacks,  ! has_children(node), this, node);
    NodeData* C4_RESTRICT nd = _p(node);
    nd->m_type = STREAM|more_flags;
    nd->m_key.clear();
    nd->m_val.clear();
}
/** @endcond */ // LCOV_EXCL_STOP


//-----------------------------------------------------------------------------

void Tree::clear_style(id_type node, bool recurse)
{
    NodeData *C4_RESTRICT d = _p(node);
    d->m_type.clear_style();
    if(!recurse)
        return;
    for(id_type child = d->m_first_child; child != NONE; child = next_sibling(child))
        clear_style(child, recurse);
}

void Tree::set_style_conditionally(id_type node,
                                   NodeType type_mask,
                                   NodeType rem_style_flags,
                                   NodeType add_style_flags,
                                   bool recurse)
{
    NodeData *C4_RESTRICT d = _p(node);
    if((d->m_type & type_mask) == type_mask)
    {
        d->m_type &= ~(NodeType)rem_style_flags;
        d->m_type |= (NodeType)add_style_flags;
    }
    if(!recurse)
        return;
    for(id_type child = d->m_first_child; child != NONE; child = next_sibling(child))
        set_style_conditionally(child, type_mask, rem_style_flags, add_style_flags, recurse);
}


//-----------------------------------------------------------------------------
id_type Tree::num_tag_directives() const
{
    return m_tag_directives.size();
}

void Tree::clear_tag_directives()
{
    m_tag_directives.clear();
}

void Tree::add_tag_directive(csubstr handle, csubstr prefix, id_type id)
{
    RYML_CHECK_BASIC_CB_(m_callbacks,
                       !handle.empty()
                       &&
                       !prefix.empty()
                       &&
                       is_valid_tag_handle(handle)
                       &&
                       m_tag_directives.add(handle, prefix, id));
}

size_t Tree::resolve_tag(substr output, csubstr tag, id_type node_id) const
{
    size_t reqsz = 0;
    m_tag_directives.resolve(output, &reqsz, tag, node_id, Location{}, callbacks());
    return reqsz;
}

namespace {
// return the extra size needed for the arena to accomodate the resolved tag
size_t _transform_tag(Tree *t, id_type node_id, id_type doc_id, TagCache &cache, csubstr tag, csubstr *resolved)
{
    _c4dbgpf("tag: doc={} node={} resolving tag ~~~{}~~~", doc_id, node_id, tag);
    (void)node_id;
    size_t reqsize = 0;
    if(tag.begins_with('<'))
    {
        *resolved = tag;
    }
    else
    {
        RYML_ASSERT_VISIT_CB_(t->callbacks(), !tag.begins_with("!<"), t, node_id); // this should have been handled elsewhere
        TagCache::LookupResult ret = cache.find(tag, doc_id);
        if(ret)
        {
            _c4dbgpf("tag: doc={} node={} resolving tag: found in cache[{}]: {}", doc_id, node_id, ret.pos, prs_(ret.resolved));
            *resolved = ret.resolved;
        }
        else
        {
            _c4dbgpf("tag: doc={} node={} tag not in cache ~~~{}~~~", doc_id, node_id, tag);
            substr buf = t->m_arena.sub(t->m_arena_pos);
            reqsize = t->resolve_tag(buf, tag, doc_id);
            if(!reqsize)
            {
                *resolved = tag;
            }
            else if(reqsize <= buf.len)
            {
                t->m_arena_pos += reqsize;
                *resolved = buf.first(reqsize);
                cache.add(tag, *resolved, doc_id, ret.pos);
                reqsize = 0;
            }
            else
            {
                _c4dbgpf("tag: doc={} node={} extra size needed: {}", doc_id, node_id, reqsize);
            }
            _c4dbgpf("tag: doc={} node={} resolved tag: ~~~{}~~~", doc_id, node_id, *resolved);
        }
    }
    return reqsize;
}
size_t _resolve_tags(Tree *t, id_type node, id_type doc_id, TagCache &cache, bool all=true)
{
    NodeData *C4_RESTRICT d = t->_p(node);
    size_t extra_size = 0;
    if((d->m_type & KEYTAG) && (all || is_custom_tag(d->m_key.tag)))
        extra_size += _transform_tag(t, node, doc_id, cache, d->m_key.tag, &d->m_key.tag);
    if((d->m_type & VALTAG) && (all || is_custom_tag(d->m_val.tag)))
        extra_size += _transform_tag(t, node, doc_id, cache, d->m_val.tag, &d->m_val.tag);
    for(id_type child = t->first_child(node); child != NONE; child = t->next_sibling(child))
        extra_size += _resolve_tags(t, child, doc_id, cache);
    return extra_size;
}
size_t _resolve_tags(Tree *t, TagCache &cache, bool all)
{
    id_type r = t->root_id();
    size_t extra_size = 0;
    if(!t->is_stream(r))
        extra_size += _resolve_tags(t, r, r, cache, all);
    else
        for(id_type doc_id = t->first_child(r); doc_id != NONE; doc_id = t->next_sibling(doc_id))
            extra_size += _resolve_tags(t, doc_id, doc_id, cache, all);
    return extra_size;
}
void _normalize_tags(Tree *t, id_type node)
{
    NodeData *C4_RESTRICT d = t->_p(node);
    if(d->m_type & KEYTAG)
        d->m_key.tag = normalize_tag(d->m_key.tag);
    if(d->m_type & VALTAG)
        d->m_val.tag = normalize_tag(d->m_val.tag);
    for(id_type child = t->first_child(node); child != NONE; child = t->next_sibling(child))
        _normalize_tags(t, child);
}
void _normalize_tags_long(Tree *t, id_type node)
{
    NodeData *C4_RESTRICT d = t->_p(node);
    if(d->m_type & KEYTAG)
        d->m_key.tag = normalize_tag_long(d->m_key.tag);
    if(d->m_type & VALTAG)
        d->m_val.tag = normalize_tag_long(d->m_val.tag);
    for(id_type child = t->first_child(node); child != NONE; child = t->next_sibling(child))
        _normalize_tags_long(t, child);
}
} // namespace

void Tree::resolve_tags(TagCache &cache, bool all)
{
    if(empty())
        return;
    // try to resolve. While doing so, get the extra size needed for
    // the arena, if the arena is currently too small.
    size_t extra_size = _resolve_tags(this, cache, all);
    // if the arena requires extra size, grow it and then resolve the
    // missing entries
    if(extra_size)
    {
        _c4dbgpf("tag: extrasize={} -- retry! {}->{}", extra_size, m_arena.len, m_arena.len + extra_size);
        _grow_arena(extra_size);
        extra_size = _resolve_tags(this, cache, all);
        RYML_ASSERT_BASIC_CB_(callbacks(), extra_size == 0);
    }
}

void Tree::normalize_tags()
{
    if(empty())
        return;
    _normalize_tags(this, root_id());
}

void Tree::normalize_tags_long()
{
    if(empty())
        return;
    _normalize_tags_long(this, root_id());
}


//-----------------------------------------------------------------------------

csubstr Tree::lookup_result::resolved() const
{
    csubstr p = path.first(path_pos);
    if(p.ends_with('.'))
        p = p.first(p.len-1);
    return p;
}

csubstr Tree::lookup_result::unresolved() const
{
    return path.sub(path_pos);
}

void Tree::_advance(lookup_result *r, size_t more)
{
    r->path_pos += more;
    if(r->path.sub(r->path_pos).begins_with('.'))
        ++r->path_pos;
}

Tree::lookup_result Tree::lookup_path(csubstr path, id_type start) const
{
    if(start == NONE)
        start = root_id();
    lookup_result r(path, start);
    if(path.empty())
        return r;
    _lookup_path(&r);
    if(r.target == NONE && r.closest == start)
        r.closest = NONE;
    return r;
}

id_type Tree::lookup_path_or_modify(csubstr default_value, csubstr path, id_type start)
{
    id_type target = _lookup_path_or_create(path, start);
    set_val(target, default_value);
    return target;
}

id_type Tree::lookup_path_or_modify(Tree const *src, id_type src_node, csubstr path, id_type start)
{
    id_type target = _lookup_path_or_create(path, start);
    merge_with(src, src_node, target);
    return target;
}

id_type Tree::_lookup_path_or_create(csubstr path, id_type start)
{
    if(start == NONE)
        start = root_id();
    lookup_result r(path, start);
    _lookup_path(&r);
    if(r.target != NONE)
    {
        C4_ASSERT(r.unresolved().empty());
        return r.target;
    }
    _lookup_path_modify(&r);
    return r.target;
}

void Tree::_lookup_path(lookup_result *r) const
{
    C4_ASSERT( ! r->unresolved().empty());
    _lookup_path_token parent{"", type(r->closest)};
    id_type node;
    do
    {
        node = _next_node(r, &parent);
        if(node != NONE)
            r->closest = node;
        if(r->unresolved().empty())
        {
            r->target = node;
            return;
        }
    } while(node != NONE);
}

void Tree::_lookup_path_modify(lookup_result *r)
{
    C4_ASSERT( ! r->unresolved().empty());
    _lookup_path_token parent{"", type(r->closest)};
    id_type node;
    do
    {
        node = _next_node_modify(r, &parent);
        if(node != NONE)
            r->closest = node;
        if(r->unresolved().empty())
        {
            r->target = node;
            return;
        }
    } while(node != NONE);
}

id_type Tree::_next_node(lookup_result * r, _lookup_path_token *parent) const
{
    _lookup_path_token token = _next_token(r, *parent);
    if( ! token)
        return NONE;

    id_type node = NONE;
    csubstr prev = token.value;
    if(token.type == MAP || token.type == SEQ)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, !token.value.begins_with('['), this, r->closest);
        //RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(r->closest) || r->closest == NONE);
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_map(r->closest), this, r->closest);
        node = find_child(r->closest, token.value);
    }
    else if(token.type == KEYVAL)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, r->unresolved().empty(), this, r->closest);
        if(is_map(r->closest))
            node = find_child(r->closest, token.value);
    }
    else if(token.type == KEY)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, token.value.begins_with('[') && token.value.ends_with(']'), this, r->closest);
        token.value = token.value.offs(1, 1).trim(' ');
        id_type idx = 0;
        RYML_CHECK_BASIC_CB_(m_callbacks, from_chars(token.value, &idx));
        node = child(r->closest, idx);
    }
    else
    {
        C4_NEVER_REACH();
    }

    if(node != NONE)
    {
        *parent = token;
    }
    else
    {
        csubstr p = r->path.sub(r->path_pos > 0 ? r->path_pos - 1 : r->path_pos);
        r->path_pos -= prev.len;
        if(p.begins_with('.'))
            r->path_pos -= 1u;
    }

    return node;
}

id_type Tree::_next_node_modify(lookup_result * r, _lookup_path_token *parent)
{
    _lookup_path_token token = _next_token(r, *parent);
    if( ! token)
        return NONE;

    id_type node = NONE;
    NodeType ty = type(r->closest);
    if(token.type == MAP || token.type == SEQ)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, !token.value.begins_with('['), this, r->closest);
        //RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(r->closest) || r->closest == NONE);
        if( ! ty.is_container())
        {
            set_map(r->closest);
        }
        else
        {
            if(ty.is_map())
            {
                node = find_child(r->closest, token.value);
            }
            else
            {
                id_type pos = NONE;
                RYML_CHECK_BASIC_CB_(m_callbacks, c4::atox(token.value, &pos));
                RYML_ASSERT_VISIT_CB_(m_callbacks, pos != NONE, this, r->closest);
                node = child(r->closest, pos);
            }
        }
        if(node == NONE)
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, is_map(r->closest), this, r->closest);
            node = append_child(r->closest);
            NodeData *n = _p(node);
            n->m_key.scalar = token.value;
            n->m_type.add(KEY);
        }
    }
    else if(token.type == KEYVAL)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, r->unresolved().empty(), this, r->closest);
        if(ty.is_map())
        {
            node = find_child(r->closest, token.value);
            if(node == NONE)
                node = append_child(r->closest);
        }
        else
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, !ty.is_seq(), this, r->closest);
            _add_flags(r->closest, MAP);
            node = append_child(r->closest);
        }
        NodeData *n = _p(node);
        n->m_key.scalar = token.value;
        n->m_val.scalar = "";
        n->m_type.add(KEYVAL);
    }
    else if(token.type == KEY)
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, token.value.begins_with('[') && token.value.ends_with(']'), this, r->closest);
        token.value = token.value.offs(1, 1).trim(' ');
        id_type idx;
        if( ! from_chars(token.value, &idx))
        {
             return NONE;
        }
        if( ! is_container(r->closest))
        {
            set_seq(r->closest);
        }
        RYML_ASSERT_VISIT_CB_(m_callbacks, is_container(r->closest), this, r->closest);
        node = child(r->closest, idx);
        if(node == NONE)
        {
            RYML_ASSERT_VISIT_CB_(m_callbacks, num_children(r->closest) <= idx, this, r->closest);
            for(id_type i = num_children(r->closest); i <= idx; ++i)
            {
                node = append_child(r->closest);
                if(i < idx)
                {
                    if(is_map(r->closest))
                    {
                        _clear_type(node);
                        set_key(node, {});
                        set_val(node, {});
                    }
                    else
                    {
                        RYML_ASSERT_VISIT_CB_(m_callbacks, is_seq(r->closest), this, r->closest);
                        _clear_type(node);
                        set_val(node, {});
                    }
                }
            }
        }
    }
    else
    {
        C4_NEVER_REACH();
    }

    RYML_ASSERT_VISIT_CB_(m_callbacks, node != NONE, this, r->closest);
    *parent = token;
    return node;
}

/* types of tokens:
 * - seeing "map."  ---> "map"/MAP
 * - finishing "scalar" ---> "scalar"/KEYVAL
 * - seeing "seq[n]" ---> "seq"/SEQ (--> "[n]"/KEY)
 * - seeing "[n]" ---> "[n]"/KEY
 */
Tree::_lookup_path_token Tree::_next_token(lookup_result *r, _lookup_path_token const& parent) const
{
    csubstr unres = r->unresolved();
    if(unres.empty())
        return {}; // LCOV_EXCL_LINE

    // is it an indexation like [0], [1], etc?
    if(unres.begins_with('['))
    {
        size_t pos = unres.find(']');
        if(pos == csubstr::npos)
            return {}; // LCOV_EXCL_LINE
        csubstr idx = unres.first(pos + 1);
        _advance(r, pos + 1);
        return {idx, KEY};
    }

    // no. so it must be a name
    size_t pos = unres.first_of(".[");
    if(pos == csubstr::npos)
    {
        _advance(r, unres.len);
        NodeType t;
        if(( ! parent) || parent.type.is_seq())
            return {unres, VAL};
        return {unres, KEYVAL};
    }

    // it's either a map or a seq
    RYML_ASSERT_VISIT_CB_(m_callbacks, unres[pos] == '.' || unres[pos] == '[', this, r->closest);
    if(unres[pos] == '.')
    {
        RYML_ASSERT_VISIT_CB_(m_callbacks, pos != 0, this, r->closest);
        _advance(r, pos + 1);
        return {unres.first(pos), MAP};
    }

    RYML_ASSERT_VISIT_CB_(m_callbacks, unres[pos] == '[', this, r->closest);
    _advance(r, pos);
    return {unres.first(pos), SEQ};
}


} // namespace yml
} // namespace c4


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#ifndef C4_YML_EVENT_HANDLER_TREE_HPP_
#include "c4/yml/event_handler_tree.hpp"
#endif
#ifndef C4_YML_PARSE_ENGINE_DEF_HPP_
#include "c4/yml/parse_engine.def.hpp"
#endif
#ifndef C4_YML_PARSE_HPP_
#include "c4/yml/parse.hpp"
#endif

namespace c4 {
namespace yml {

Location Tree::location(Parser const& parser, id_type node) const
{
    // try hard to avoid getting the location from a null string.
    Location loc;
    if(_location_from_node(parser, node, &loc, 0))
        return loc;
    return parser.val_location(parser.source().str);
}

bool Tree::_location_from_node(Parser const& parser, id_type node, Location *C4_RESTRICT loc, id_type level) const
{
    NodeType ty = type(node);
    if(ty.has_key())
    {
        csubstr k = key(node);
        if C4_LIKELY(k.str != nullptr)
        {
            RYML_ASSERT_BASIC_CB_(m_callbacks, k.is_sub(parser.source()));
            RYML_ASSERT_BASIC_CB_(m_callbacks, parser.source().is_super(k));
            *loc = parser.val_location(k.str);
            return true;
        }
    }

    if(ty.has_val())
    {
        csubstr v = val(node);
        if C4_LIKELY(v.str != nullptr)
        {
            RYML_ASSERT_BASIC_CB_(m_callbacks, v.is_sub(parser.source()));
            RYML_ASSERT_BASIC_CB_(m_callbacks, parser.source().is_super(v));
            *loc = parser.val_location(v.str);
            return true;
        }
    }

    if(ty.is_container())
    {
        if(_location_from_cont(parser, node, loc))
            return true;
    }

    if(type(node) != NOTYPE && level == 0)
    {
        // try the prev sibling
        {
            const id_type prev = prev_sibling(node);
            if(prev != NONE)
            {
                if(_location_from_node(parser, prev, loc, level+1))
                    return true;
            }
        }
        // try the next sibling
        {
            const id_type next = next_sibling(node);
            if(next != NONE)
            {
                if(_location_from_node(parser, next, loc, level+1))
                    return true;
            }
        }
        // try the parent
        {
            const id_type parent = this->parent(node);
            if(parent != NONE)
            {
                if(_location_from_node(parser, parent, loc, level+1))
                    return true;
            }
        }
    }
    return false;
}

bool Tree::_location_from_cont(Parser const& parser, id_type node, Location *C4_RESTRICT loc) const
{
    RYML_ASSERT_BASIC_CB_(m_callbacks, type(node).is_container());
    if(!type(node).is_stream())
    {
        const char *node_start = _p(node)->m_val.scalar.str;  // this was stored in the container
        if(has_children(node))
        {
            id_type child = first_child(node);
            if(type(child).has_key())
            {
                // when a map starts, the container was set after the key
                csubstr k = key(child);
                if(k.str && node_start > k.str)
                    node_start = k.str;
            }
        }
        *loc = parser.val_location(node_start);
        return true;
    }
    else // it's a stream
    {
        *loc = parser.val_location(parser.source().str); // just return the front of the buffer
    }
    return true;
}

} // namespace yml
} // namespace c4


// NOLINTEND(modernize-avoid-c-style-cast)
C4_SUPPRESS_WARNING_GCC_CLANG_POP
C4_SUPPRESS_WARNING_MSVC_POP
