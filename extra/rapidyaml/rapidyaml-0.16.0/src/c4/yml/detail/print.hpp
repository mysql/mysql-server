#ifndef C4_YML_DETAIL_PRINT_HPP_
#define C4_YML_DETAIL_PRINT_HPP_

#ifndef C4_YML_NODE_HPP_
#include "c4/yml/node.hpp"
#endif

#ifdef RYML_DBG
#define _c4dbg_tree(...) print_tree(__VA_ARGS__)
#define _c4dbg_node(...) print_tree(__VA_ARGS__)
#else
#define _c4dbg_tree(...)
#define _c4dbg_node(...)
#endif

// NOLINTBEGIN(modernize-avoid-c-style-cast)

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")
C4_SUPPRESS_WARNING_GCC("-Wattributes")
inline char _scalar_code(NodeType masked)
{
    if(masked & (KEY_LITERAL|VAL_LITERAL))
        return '|';
    if(masked & (KEY_FOLDED|VAL_FOLDED))
        return '>';
    if(masked & (KEY_SQUO|VAL_SQUO))
        return '\'';
    if(masked & (KEY_DQUO|VAL_DQUO))
        return '"';
    if(masked & (KEY_PLAIN|VAL_PLAIN))
        return '~';
    return '@';
}
inline char _scalar_code_key(NodeType t)
{
    return _scalar_code(t & KEY_STYLE);
}
inline char _scalar_code_val(NodeType t)
{
    return _scalar_code(t & VAL_STYLE);
}
inline char _scalar_code_key(Tree const& p, id_type node)
{
    return _scalar_code_key(p._p(node)->m_type);
}
inline char _scalar_code_val(Tree const& p, id_type node)
{
    return _scalar_code_val(p._p(node)->m_type);
}
inline C4_NO_INLINE id_type print_node(Tree const& p, id_type node, int level, id_type count, bool print_children, bool print_address=false)
{
    NodeType type = p.type(node);
    if(type.is_doc())
    {
        TagDirectiveRange tagds = p.m_tag_directives.lookup_range(node);
        for(TagDirective const& td : tagds)
        {
            printf("%%TAG[%zd] %.*s %.*s [doc=%zu]\n",
                   &td - p.m_tag_directives.m_directives,
                   (int)td.handle.len, td.handle.str,
                   (int)td.prefix.len, td.prefix.str,
                   td.doc_id);
        }
    }
    printf("[%zu]%*s[%zu]", (size_t)count, (2*level), "", (size_t)node);
    if(print_address) printf(" %p", (void const*)p.get(node));
    if(p.is_root(node)) printf(" [ROOT]");
    char typebuf[128];
    csubstr typestr = type.type_str_sub(typebuf);
    RYML_CHECK_BASIC_(typestr.str);
    printf(" %.*s", (int)typestr.len, typestr.str);
    NodeType ty = p.type(node);
    if(ty.has_key())
    {
        if(ty.has_key_anchor())
        {
            csubstr ka = p.key_anchor(node);
            printf(" &%.*s", (int)ka.len, ka.str);
        }
        if(ty.has_key_tag())
        {
            csubstr kt = p.key_tag(node);
            if(kt.begins_with('<'))
                printf(" %.*s", (int)kt.len, kt.str);
            else
                printf(" <%.*s>", (int)kt.len, kt.str);
        }
        const char code = _scalar_code_key(p, node);
        csubstr k  = p.key(node);
        printf(" %c%.*s%c :", code, (int)k.len, k.str, code);
    }
    if(ty.has_val_anchor())
    {
        csubstr a = p.val_anchor(node);
        printf(" &%.*s", (int)a.len, a.str);
    }
    if(ty.has_val_tag())
    {
        csubstr vt = p.val_tag(node);
        if(vt.begins_with('<'))
            printf(" %.*s", (int)vt.len, vt.str);
        else
            printf(" <%.*s>", (int)vt.len, vt.str);
    }
    if(ty.has_val())
    {
        const char code = _scalar_code_val(p, node);
        csubstr v  = p.val(node);
        printf(" %c%.*s%c", code, (int)v.len, v.str, code);
    }
    printf("  (%zu sibs)", (size_t)p.num_siblings(node));

    ++count;

    if(!ty.is_container())
    {
        printf("\n");
    }
    else
    {
        printf(" (%zu children)\n", (size_t)p.num_children(node));
        if(print_children)
        {
            for(id_type i = p.first_child(node); i != NONE; i = p.next_sibling(i))
            {
                count = print_node(p, i, level+1, count, print_children);
            }
        }
    }

    return count;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

inline void print_node(ConstNodeRef const& p, int level=0, bool print_address=false)
{
    print_node(*p.tree(), p.id(), level, 0, true, print_address);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

inline id_type print_tree(const char *message, Tree const& tree, id_type node=NONE, bool print_address=false)
{
    printf("--------------------------------------\n");
    if(message != nullptr)
        printf("%s:\n", message);
    id_type ret = 0;
    if(!tree.empty())
    {
        if(node == NONE)
            node = tree.root_id();
        ret = print_node(tree, node, 0, 0, true, print_address);
    }
    printf("#nodes=%zu vs #printed=%zu\n", (size_t)tree.size(), (size_t)ret);
    printf("--------------------------------------\n");
    return ret;
}

inline id_type print_tree(Tree const& p, id_type node=NONE, bool print_address=false)
{
    return print_tree(nullptr, p, node, print_address);
}

inline void print_tree(ConstNodeRef const& p, int level=0, bool print_address=false)
{
    print_node(p, level, print_address);
    for(ConstNodeRef ch : p.children())
    {
        print_tree(ch, level+1, print_address);
    }
}

C4_SUPPRESS_WARNING_GCC_CLANG_POP

} /* namespace yml */
} /* namespace c4 */

// NOLINTEND(modernize-avoid-c-style-cast)

#endif /* C4_YML_DETAIL_PRINT_HPP_ */
