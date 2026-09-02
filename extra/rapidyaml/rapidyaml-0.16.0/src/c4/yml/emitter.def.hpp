#ifndef C4_YML_EMITTER_DEF_HPP_
#define C4_YML_EMITTER_DEF_HPP_

/** @file emitter.def.hpp */

#ifndef C4_YML_EMITTER_HPP_
#include "c4/yml/emitter.hpp"
#endif
#ifndef C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif
#ifndef C4_YML_SCALAR_STYLE_HPP_
#include "c4/yml/scalar_style.hpp"
#endif
#ifndef C4_YML_DETAIL_DBGPRINT_HPP_
#include "c4/yml/detail/dbgprint.hpp"
#endif
#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif


C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")
// NOLINTBEGIN(modernize-avoid-c-style-cast)

namespace c4 {
namespace yml {

namespace detail {
enum : type_bits { // NOLINT
    styles_block_key_ = KEY_LITERAL|KEY_FOLDED,
    styles_block_val_ = VAL_LITERAL|VAL_FOLDED,
    styles_block_     = (styles_block_key_ | styles_block_val_),
    styles_flow_key_  = KEY_STYLE & ~styles_block_key_,
    styles_flow_val_  = VAL_STYLE & ~styles_block_val_,
    styles_flow_      = styles_flow_key_ | styles_flow_val_,
    styles_squo_      = KEY_SQUO|VAL_SQUO,
    styles_dquo_      = KEY_DQUO|VAL_DQUO,
    styles_plain_     = KEY_PLAIN|VAL_PLAIN,
    styles_literal_   = KEY_LITERAL|VAL_LITERAL,
    styles_folded_    = KEY_FOLDED|VAL_FOLDED,
};
} // namespace detail


template<class Writer>
void Emitter<Writer>::emit_as(EmitType_e type, Tree const* tree, id_type id)
{
    RYML_ASSERT_BASIC_CB_(tree->callbacks(), !tree || !tree->empty() || id == NONE);
    if(!tree || tree->empty())
        return;
    if(id == NONE)
        id = tree->root_id();
    RYML_CHECK_VISIT_CB_(tree->callbacks(), id < tree->capacity(), tree, id);
    m_tree = tree;
    m_col = 0;
    m_depth = 0;
    m_ilevel = 0;
    m_pws = PWS_NONE_;
    m_flow_pws = {};
    if(type == EMIT_YAML)
        emit_yaml_(id);
    else if(type == EMIT_JSON)
        json_emit_(id);
    else
        RYML_ERR_BASIC_CB_(m_tree->callbacks(), "unknown emit type"); // LCOV_EXCL_LINE
    m_tree = nullptr;
}

/** @cond dev */


//-----------------------------------------------------------------------------

// The startup logic is made complicated from it having to accept
// initial non-root nodes, and having to deal with tricky tokens like
// doc separators, anchors, tags, optional keys or dashes, and
// comments.
//
// This function kickstarts the tree descent by handling all the
// initial and final logic at the top-level scope, thus avoiding
// top-level kickstart branches in the recursive descending code
// (which should be oblivious of such logic). This makes the recursive
// descending code a lot simpler.
template<class Writer>
void Emitter<Writer>::emit_yaml_(id_type id)
{
    const NodeType ty = m_tree->type(id);

    // emit leading tokens, such as keys or comments
    const bool has_parent = !m_tree->is_root(id);
    const bool emit_key = has_parent && ty.has_key() && m_opts.emit_nonroot_key();
    const bool emit_dash = has_parent && !ty.has_key() && !ty.is_doc() && m_opts.emit_nonroot_dash();
    RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, !(emit_key && emit_dash), m_tree, id);

    // emit opening tokens (such as tags, anchors or comments)
    if(emit_key)
    {
        blck_map_open_entry_(id);
        ++m_ilevel;
    }
    else if(emit_dash)
    {
        blck_seq_open_entry_(id);
        ++m_ilevel;
    }
    else
    {
        top_open_entry_(id);
    }

    // emit the payload
    if(ty.is_stream())
    {
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_ilevel == 0, m_tree, id);
        visit_stream_(id);
    }
    else if(ty.is_doc())
    {
        RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, m_ilevel == 0, m_tree, id);
        visit_doc_(id);
    }
    else if(ty.is_container())
    {
        visit_blck_container_(id);
    }
    else if(ty.has_val())
    {
        visit_doc_val_(id);
    }

    // emit closing tokens (such as comments)
    if(emit_key)
    {
        --m_ilevel;
        blck_close_entry_(id);
    }
    else if(emit_dash)
    {
        --m_ilevel;
        blck_close_entry_(id);
    }
    else
    {
        top_close_entry_(id);
    }

    if(ty.is_flow_mlx())
    {
        newl_();
    }
    else if(m_tree->is_root(id)
       || emit_dash || emit_key
       || !ty.is_val()
       || !ty.is_val_plain())
    {
        write_pws_and_pend_(PWS_NONE_);
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_stream_(id_type id)
{
    auto write_tag_directives = [this](const id_type next_node){
        const id_type doc = m_tree->ancestor_doc(next_node);
        const TagDirectiveRange tagds = m_tree->m_tag_directives.lookup_range(doc);
        if(tagds.e != tagds.b)
        {
            const id_type parent = m_tree->parent(next_node);
            if(next_node != m_tree->first_child(parent))
            {
                write_pws_and_pend_(PWS_NEWL_);
                write_("...");
            }
        }
        for(TagDirective const& td : tagds)
        {
            write_pws_and_pend_(PWS_NONE_);
            write_("%TAG ");
            write_(td.handle);
            write_(' ');
            write_(td.prefix);
            pend_newl_();
        }
    };
    const id_type first_child = m_tree->first_child(id);
    if(first_child != NONE)
        write_tag_directives(first_child);
    ++m_depth;
    for(id_type child = first_child; child != NONE; child = m_tree->next_sibling(child))
    {
        NodeType ty = m_tree->type(child);
        m_ilevel = 0;
        write_pws_and_pend_(PWS_NONE_);
        top_open_entry_(child);
        visit_doc_(child);
        top_close_entry_(child);
        if(ty.is_val())
        {
            if(ty.m_bits & VALNIL)
                pend_newl_();
        }
        else if(ty.is_container())
        {
            if(ty.is_flow())
                pend_newl_();
        }
        if(m_tree->next_sibling(child) != NONE)
        {
            write_tag_directives(m_tree->next_sibling(child));
        }
    }
    --m_depth;
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_blck_container_(id_type id)
{
    NodeType ty = m_tree->type(id);
    if(!(ty.m_bits & CONTAINER_STYLE))
        ty.m_bits |= (m_tree->empty(id) ? FLOW_SL : BLOCK);
    write_pws_and_pend_(PWS_NONE_);
    if(ty.is_flow_sl())
        visit_flow_sl_(id);
    else if(ty.is_flow_mlx())
        visit_flow_ml_(id);
    else
        visit_blck_(id);
}

template<class Writer>
void Emitter<Writer>::visit_flow_container_(id_type id)
{
    NodeType ty = m_tree->type(id);
    if(!(ty.m_bits & CONTAINER_STYLE))
        ty.m_bits |= FLOW_SL;
    write_pws_and_pend_(PWS_NONE_);
    if(ty.is_flow_mlx())
        visit_flow_ml_(id);
    else // if(ty.is_flow_sl())
        visit_flow_sl_(id);
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_doc_val_(id_type id)
{
    // some plain scalars such as '...' and '---' must not
    // appear at 0-indentation
    NodeType ty = m_tree->type(id);
    const csubstr val = m_tree->val(id);
    type_bits val_style = ty.m_bits & VAL_STYLE;
    const bool is_ambiguous = ((ty.m_bits & VAL_PLAIN) || !val_style)
        && (val.begins_with("...") || val.begins_with("---"));
    if(is_ambiguous)
    {
        ++m_ilevel;
        if(m_pws != PWS_NONE_)
            pend_newl_();
        else
            indent_(m_ilevel);
    }
    write_pws_and_pend_(PWS_NONE_);
    if(m_tree->is_val_ref(id))
    {
        write_ref_(val);
    }
    else
    {
        if(!val_style)
            val_style = scalar_style_choose_block(val);
        blck_write_scalar_(val, val_style);
    }
    if(is_ambiguous)
    {
        --m_ilevel;
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_doc_(id_type id)
{
    const NodeType ty = m_tree->type(id);
    RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, ty.is_doc(), m_tree, id);
    RYML_ASSERT_VISIT_CB_(m_tree->m_callbacks, !ty.has_key(), m_tree, id);
    if(ty.is_container()) // this is more frequent
    {
        visit_blck_container_(id);
    }
    else if(ty.is_val())
    {
        visit_doc_val_(id);
    }
}


//-----------------------------------------------------------------------------

// to be called only at top level
template<class Writer>
void Emitter<Writer>::top_open_entry_(id_type node)
{
    NodeType ty = m_tree->type(node);
    if(ty.is_doc() && !m_tree->is_root(node))
    {
        write_("---");
        pend_space_();
    }
    if(ty.has_val_anchor())
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_('&');
        write_(m_tree->val_anchor(node));
    }
    if(ty.has_val_tag())
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->val_tag(node));
    }
    if(m_pws == PWS_SPACE_)
    {
        if(ty.has_val())
        {
            if(ty.is_val_plain() && !m_tree->val(node).len)
                pend_none_();
        }
        else
        {
            RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_container(), m_tree, node);
            if(!ty.is_flow())
                pend_newl_();
        }
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::top_close_entry_(id_type node)
{
    NodeType ty = m_tree->type(node);
    if(ty.is_val() && !(ty.m_bits & VALNIL))
    {
        pend_newl_();
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::flow_seq_open_entry_(id_type node)
{
    NodeType ty = m_tree->type(node);
    write_pws_and_pend_(PWS_NONE_);
    if(ty.m_bits & VALANCH)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_('&');
        write_(m_tree->val_anchor(node));
    }
    if(ty.m_bits & VALTAG)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->val_tag(node));
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::flow_map_open_entry_(id_type node)
{
    NodeType ty = m_tree->type(node);
    write_pws_and_pend_(PWS_NONE_);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.has_key(), m_tree, node);
    if(ty.m_bits & KEYANCH)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_("&");
        write_(m_tree->key_anchor(node));
    }
    if(ty.m_bits & KEYTAG)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->key_tag(node));
    }
    if(ty.m_bits & KEYREF)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_ref_(m_tree->key(node));
    }
    else
    {
        write_pws_and_pend_(PWS_NONE_);
        csubstr key = m_tree->key(node);
        if(!(ty.m_bits & detail::styles_flow_key_))
            ty.m_bits |= scalar_style_choose_flow(key) & detail::styles_flow_key_;
        flow_write_scalar_(key, ty.m_bits & detail::styles_flow_key_);
    }
    write_pws_and_pend_(PWS_SPACE_);
    write_(':');
    if(ty.m_bits & VALANCH)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_('&');
        write_(m_tree->val_anchor(node));
    }
    if(ty.m_bits & VALTAG)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->val_tag(node));
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
C4_NODISCARD bool Emitter<Writer>::maybe_start_flow_pws_ml_(id_type node) noexcept
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->type(node) & (FLOW_ML1|FLOW_MLN), m_tree, node);
    if(m_flow_pws.active)
        return false;
    NodeType ty = m_tree->type(node);
    if(m_opts.force_flow_spc())
        ty |= FLOW_SPC;
    m_flow_pws.start(ty, m_opts.max_cols());
    return true;
}

template<class Writer>
C4_NODISCARD typename Emitter<Writer>::flow_pws Emitter<Writer>::setup_flow_pws_sl_(id_type node) noexcept
{
    flow_pws ret = {};
    if(m_flow_pws.active)
    {
        ret = m_flow_pws;
    }
    else
    {
        NodeType ty = m_tree->type(node);
        if(m_opts.force_flow_spc())
            ty |= FLOW_SPC;
        ret.start(ty, 0);
    }
    return ret;
}

template<class Writer>
void Emitter<Writer>::flow_pws::start(NodeType ty, size_t max_cols_) noexcept
{
    max_cols = 0;
    pend_after_comma = (ty.m_bits & FLOW_SPC) ? PWS_SPACE_ : PWS_NONE_;
    if(ty.m_bits & FLOW_MLN)
    {
        max_cols_ = max_cols_ >= 2 ? max_cols_ : 2;
        // subtract 1 for the comma, and maybe the space from pend_after_comma
        max_cols = max_cols_ - 1 - pend_after_comma;
        // line above only works if:
        static_assert((size_t)PWS_NONE_ == 0 && (size_t)PWS_SPACE_ == 1, "invalid assumptions");
        active = true;
    }
    else if(ty.m_bits & FLOW_ML1)
    {
        pend_after_comma = PWS_NEWL_;
    }
}

template<class Writer>
void Emitter<Writer>::flow_close_entry_sl_(id_type node, id_type last_sibling, Pws_e pend_after)
{
    if(node != last_sibling)
    {
        write_pws_and_pend_(pend_after);
        write_(',');
    }
}

template<class Writer>
void Emitter<Writer>::flow_close_entry_ml_(id_type node, id_type last_sibling, Pws_e pend_after)
{
    if(node != last_sibling)
    {
        write_pws_and_pend_(pend_after);
        write_(',');
    }
    else
    {
        pend_newl_();
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::blck_seq_open_entry_(id_type node)
{
    NodeType ty = m_tree->type(node);
    write_pws_and_pend_(PWS_NONE_);
    write_pws_and_pend_(PWS_SPACE_); // pend the space after the following dash
    write_('-');
    bool has_tag_or_anchor = false;
    if(ty.m_bits & VALANCH)
    {
        has_tag_or_anchor = true;
        write_pws_and_pend_(PWS_SPACE_);
        write_('&');
        write_(m_tree->val_anchor(node));
    }
    if(ty.m_bits & VALTAG)
    {
        has_tag_or_anchor = true;
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->val_tag(node));
    }
    if(has_tag_or_anchor && ty.is_container())
    {
        if(!(ty.m_bits & CONTAINER_STYLE))
            ty |= BLOCK;
        if((ty.m_bits & BLOCK) && m_tree->has_children(node))
            pend_newl_();
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::blck_map_open_entry_(id_type node)
{
    NodeType ty = m_tree->type(node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.has_key(), m_tree, node);
    csubstr key = m_tree->key(node);
    if(!(ty.m_bits & (KEY_STYLE|KEYREF)))
        ty.m_bits |= (scalar_style_choose_block(key).m_bits & KEY_STYLE);
    write_pws_and_pend_(PWS_NONE_);
    if(ty.m_bits & KEYANCH)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_('&');
        write_(m_tree->key_anchor(node));
    }
    if(ty.m_bits & KEYTAG)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->key_tag(node));
    }
    if(ty.m_bits & KEYREF)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_ref_(key);
    }
    else
    {
        write_pws_and_pend_(PWS_NONE_);
        const type_bits use_qmrk = ty.m_bits & detail::styles_block_key_;
        if(!use_qmrk)
        {
            blck_write_scalar_(key, ty.m_bits & KEY_STYLE);
        }
        else
        {
            write_("? ");
            blck_write_scalar_(key, ty.m_bits & KEY_STYLE);
            pend_newl_();
        }
    }
    write_pws_and_pend_(PWS_SPACE_); // pend the space after the colon
    write_(':');
    if(ty.m_bits & VALANCH)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_('&');
        write_(m_tree->val_anchor(node));
    }
    if(ty.m_bits & VALTAG)
    {
        write_pws_and_pend_(PWS_SPACE_);
        write_tag_(m_tree->val_tag(node));
    }
    if(ty.is_container() && m_tree->has_children(node))
    {
        if(!(ty.m_bits & CONTAINER_STYLE))
            ty |= BLOCK;
        if(ty.is_block())
            pend_newl_();
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::blck_close_entry_(id_type node)
{
    (void)node;
    pend_newl_();
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_blck_seq_(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_seq(node), m_tree, node);
    bool empty = true;
    for(id_type child = m_tree->first_child(node); child != NONE; child = m_tree->next_sibling(child))
    {
        empty = false;
        NodeType ty = m_tree->type(child);
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_val() || ty.is_container() || ty == NOTYPE, m_tree, node);
        blck_seq_open_entry_(child);
        if(ty.is_val())
        {
            write_pws_and_pend_(PWS_NONE_);
            csubstr val = m_tree->val(child);
            if(!ty.is_val_ref())
            {
                if(!(ty.m_bits & VAL_STYLE))
                    ty.m_bits |= (scalar_style_choose_block(val).m_bits & VAL_STYLE);
                blck_write_scalar_(val, ty.m_bits & VAL_STYLE);
            }
            else
            {
                write_ref_(val);
            }
        }
        else if(ty.is_container())
        {
            ++m_depth;
            ++m_ilevel;
            visit_blck_container_(child);
            --m_depth;
            --m_ilevel;
        }
        blck_close_entry_(child);
    }
    if(empty)
    {
        write_pws_and_pend_(PWS_NONE_);
        write_("[]");
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_blck_map_(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_map(node), m_tree, node);
    bool empty = true;
    for(id_type child = m_tree->first_child(node); child != NONE; child = m_tree->next_sibling(child))
    {
        empty = false;
        NodeType ty = m_tree->type(child);
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_keyval() || ty.is_container() || ty == NOTYPE, m_tree, node);
        blck_map_open_entry_(child); // also writes the key
        if(ty.is_keyval())
        {
            write_pws_and_pend_(PWS_NONE_);
            csubstr val = m_tree->val(child);
            if(!ty.is_val_ref())
            {
                if(!(ty.m_bits & VAL_STYLE))
                    ty |= (scalar_style_choose_block(val).m_bits & VAL_STYLE);
                blck_write_scalar_(val, ty.m_bits & VAL_STYLE);
            }
            else
            {
                write_ref_(val);
            }
        }
        else if(ty.is_container())
        {
            ++m_depth;
            ++m_ilevel;
            visit_blck_container_(child);
            --m_depth;
            --m_ilevel;
        }
        blck_close_entry_(child);
    }
    if(empty)
    {
        write_pws_and_pend_(PWS_NONE_);
        write_("{}");
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_flow_sl_seq_(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_seq(node), m_tree, node);
    const flow_pws pws = setup_flow_pws_sl_(node);
    write_('[');
    for(id_type child = m_tree->first_child(node), last = m_tree->last_child(node); child != NONE; child = m_tree->next_sibling(child))
    {
        NodeType ty = m_tree->type(child);
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), (ty & (VAL|SEQ|MAP)) || ty == NOTYPE, m_tree, node);
        flow_seq_open_entry_(child);
        if(ty.m_bits & VAL)
        {
            write_pws_and_pend_(PWS_NONE_);
            csubstr val = m_tree->val(child);
            if(!ty.is_val_ref())
            {
                if(!(ty.m_bits & detail::styles_flow_val_))
                    ty.m_bits |= (scalar_style_choose_flow(val).m_bits & detail::styles_flow_val_);
                flow_write_scalar_(val, ty.m_bits & detail::styles_flow_val_);
            }
            else
            {
                write_ref_(val);
            }
        }
        else if(ty.m_bits & (SEQ|MAP))
        {
            ++m_depth;
            visit_flow_container_(child);
            --m_depth;
        }
        flow_close_entry_sl_(child, last, pws.next_pws(m_col));
    }
    write_(']');
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_flow_ml_seq_(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_seq(node), m_tree, node);
    write_('[');
    pend_newl_();
    if(m_opts.indent_flow_ml()) ++m_ilevel;
    const bool stop_at_end = maybe_start_flow_pws_ml_(node);
    for(id_type child = m_tree->first_child(node), last = m_tree->last_child(node); child != NONE; child = m_tree->next_sibling(child))
    {
        NodeType ty = m_tree->type(child);
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_val() || ty.is_container() || ty == NOTYPE, m_tree, node);
        flow_seq_open_entry_(child);
        if(ty.is_val())
        {
            write_pws_and_pend_(PWS_NONE_);
            csubstr val = m_tree->val(child);
            if(!ty.is_val_ref())
            {
                if(!(ty.m_bits & detail::styles_flow_val_))
                    ty.m_bits |= (scalar_style_choose_flow(val).m_bits & detail::styles_flow_val_);
                flow_write_scalar_(val, ty.m_bits & detail::styles_flow_val_);
            }
            else
            {
                write_ref_(val);
            }
        }
        else if(ty.is_container())
        {
            ++m_depth;
            visit_flow_container_(child);
            --m_depth;
        }
        flow_close_entry_ml_(child, last, m_flow_pws.next_pws(m_col));
    }
    if(stop_at_end)
        m_flow_pws.stop();
    if(m_opts.indent_flow_ml()) --m_ilevel;
    write_pws_and_pend_(PWS_NONE_);
    write_(']');
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_flow_sl_map_(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_map(node), m_tree, node);
    flow_pws pws = setup_flow_pws_sl_(node);
    write_('{');
    for(id_type child = m_tree->first_child(node), last = m_tree->last_child(node); child != NONE; child = m_tree->next_sibling(child))
    {
        NodeType ty = m_tree->type(child);
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.has_key() && (ty.has_val() || ty.is_container() || ty == NOTYPE), m_tree, node);
        flow_map_open_entry_(child);
        if(ty.has_val())
        {
            write_pws_and_pend_(PWS_NONE_);
            csubstr val = m_tree->val(child);
            if(!ty.is_val_ref())
            {
                if(!(ty.m_bits & detail::styles_flow_val_))
                    ty.m_bits |= (scalar_style_choose_flow(val).m_bits & detail::styles_flow_val_);
                flow_write_scalar_(val, ty.m_bits & detail::styles_flow_val_);
            }
            else
            {
                write_ref_(val);
            }
        }
        else if(ty.is_container())
        {
            ++m_depth;
            visit_flow_container_(child);
            --m_depth;
        }
        flow_close_entry_sl_(child, last, pws.next_pws(m_col));
    }
    write_pws_and_pend_(PWS_NONE_);
    write_('}');
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_flow_ml_map_(id_type node)
{
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_map(node), m_tree, node);
    write_('{');
    pend_newl_();
    if(m_opts.indent_flow_ml()) ++m_ilevel;
    const bool stop_at_end = maybe_start_flow_pws_ml_(node);
    for(id_type child = m_tree->first_child(node), last = m_tree->last_child(node); child != NONE; child = m_tree->next_sibling(child))
    {
        NodeType ty = m_tree->type(child);
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.has_key() && (ty.has_val() || ty.is_container() || ty == NOTYPE), m_tree, node);
        flow_map_open_entry_(child);
        if(ty.has_val())
        {
            write_pws_and_pend_(PWS_NONE_);
            csubstr val = m_tree->val(child);
            if(!ty.is_val_ref())
            {
                if(!(ty.m_bits & detail::styles_flow_val_))
                    ty.m_bits |= (scalar_style_choose_flow(val).m_bits & detail::styles_flow_val_);
                flow_write_scalar_(val, ty.m_bits & detail::styles_flow_val_);
            }
            else
            {
                write_ref_(val);
            }
        }
        else if(ty.is_container())
        {
            ++m_depth;
            visit_flow_container_(child);
            --m_depth;
        }
        flow_close_entry_ml_(child, last, m_flow_pws.next_pws(m_col));
    }
    if(stop_at_end)
        m_flow_pws.stop();
    if(m_opts.indent_flow_ml()) --m_ilevel;
    write_pws_and_pend_(PWS_NONE_);
    write_('}');
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_blck_(id_type node)
{
    const NodeType ty = m_tree->type(node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), !ty.is_stream(), m_tree, node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_container() || ty.is_doc(), m_tree, node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_root(node) || (m_tree->parent_is_map(node) || m_tree->parent_is_seq(node)), m_tree, node);
    if C4_UNLIKELY(m_depth > m_opts.max_depth())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, node, "max depth exceeded");
    if(ty.is_seq())
    {
        visit_blck_seq_(node);
    }
    else
    {
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_map(), m_tree, node);
        visit_blck_map_(node);
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_flow_sl_(id_type node)
{
    const NodeType ty = m_tree->type(node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), !ty.is_stream(), m_tree, node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_container() || ty.is_doc(), m_tree, node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_root(node) || (m_tree->parent_is_map(node) || m_tree->parent_is_seq(node)), m_tree, node);
    if C4_UNLIKELY(m_depth > m_opts.max_depth())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, node, "max depth exceeded");
    if(ty.m_bits & SEQ)
    {
        visit_flow_sl_seq_(node);
    }
    else
    {
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_map(), m_tree, node);
        visit_flow_sl_map_(node);
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::visit_flow_ml_(id_type node)
{
    const NodeType ty = m_tree->type(node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), !ty.is_stream(), m_tree, node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_container() || ty.is_doc(), m_tree, node);
    RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_root(node) || (m_tree->parent_is_map(node) || m_tree->parent_is_seq(node)), m_tree, node);
    if C4_UNLIKELY(m_depth > m_opts.max_depth())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, node, "max depth exceeded");
    if(ty.m_bits & SEQ)
    {
        visit_flow_ml_seq_(node);
    }
    else
    {
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), ty.is_map(), m_tree, node);
        visit_flow_ml_map_(node);
    }
}


//-----------------------------------------------------------------------------

template<class Writer>
void Emitter<Writer>::flow_write_scalar_(csubstr str, type_bits ty)
{
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), !(ty & detail::styles_block_));
    if((ty & detail::styles_plain_) || !(ty & SCALAR_STYLE))
    {
        write_scalar_plain_(str, m_ilevel);
    }
    else if(ty & detail::styles_squo_)
    {
        write_scalar_squo_(str, m_ilevel);
    }
    else // if(ty & detail::styles_dquo_)
    {
        write_scalar_dquo_(str, m_ilevel);
    }
}

template<class Writer>
void Emitter<Writer>::blck_write_scalar_(csubstr str, type_bits ty)
{
    if((ty & detail::styles_plain_) || !(ty & SCALAR_STYLE))
    {
        write_scalar_plain_(str, m_ilevel);
    }
    else if(ty & detail::styles_squo_)
    {
        write_scalar_squo_(str, m_ilevel);
    }
    else if(ty & detail::styles_dquo_)
    {
        write_scalar_dquo_(str, m_ilevel);
    }
    else if(ty & detail::styles_literal_)
    {
        write_scalar_literal_(str, m_ilevel);
    }
    else // if(ty & detail::styles_folded_)
    {
        write_scalar_folded_(str, m_ilevel);
    }
}

template<class Writer>
size_t Emitter<Writer>::write_escaped_newlines_(csubstr s, size_t i)
{
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.len > i);
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.str[i] == '\n');
    //_c4dbgpf("nl@i={} rem=[{}]~~~{}~~~", i, s.sub(i).len, s.sub(i));
    // add an extra newline for each sequence of consecutive
    // newline/whitespace
    newl_();
    do
    {
        newl_(); // write the newline again
        ++i; // increase the outer loop counter!
    } while(i < s.len && s.str[i] == '\n');
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), i > 0);
    --i;
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.str[i] == '\n');
    return i;
}


inline bool _is_indented_block(csubstr s, size_t prev, size_t i) noexcept
{
    if(prev == 0 && s.begins_with_any(" \t"))
        return true;
    const size_t pos = s.first_not_of('\n', i);
    return (pos != npos) && (s.str[pos] == ' ' || s.str[pos] == '\t');
}


template<class Writer>
size_t Emitter<Writer>::write_indented_block_(csubstr s, size_t i, id_type ilevel)
{
    //_c4dbgpf("indblock@i={} rem=[{}]~~~\n{}~~~", i, s.sub(i).len, s.sub(i));
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), i > 0);
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.str[i-1] == '\n');
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), i < s.len);
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.str[i] == ' ' || s.str[i] == '\t' || s.str[i] == '\n');
again:
    size_t pos = s.find("\n ", i);
    if(pos == npos)
        pos = s.find("\n\t", i);
    if(pos != npos)
    {
        ++pos;
        //_c4dbgpf("indblock line@i={} rem=[{}]~~~\n{}~~~", i, s.range(i, pos).len, s.range(i, pos));
        indent_(ilevel + 1);
        write_(s.range(i, pos));
        i = pos;
        goto again; // NOLINT
    }
    // consume the newlines after the indented block
    // to prevent them from being escaped
    pos = s.find('\n', i);
    if(pos != npos)
    {
        const size_t pos2 = s.first_not_of('\n', pos);
        pos = (pos2 != npos) ? pos2 : pos;
        //_c4dbgpf("indblock line@i={} rem=[{}]~~~\n{}~~~", i, s.range(i, pos).len, s.range(i, pos));
        indent_(ilevel + 1);
        write_(s.range(i, pos));
        i = pos;
    }
    return i;
}

template<class Writer>
void Emitter<Writer>::write_scalar_literal_(csubstr s, id_type ilevel)
{
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.find("\r") == csubstr::npos);
    csubstr trimmed = s.trimr('\n');
    const size_t numnewlines_at_end = s.len - trimmed.len;
    const bool is_newline_only = (trimmed.len == 0 && (s.len > 0));
    const bool explicit_indentation = s.triml("\n\r").begins_with_any(" \t");
    //
    write_('|');
    if(explicit_indentation)
        write_('2');
    //
    if(numnewlines_at_end > 1 || is_newline_only)
        write_('+');
    else if(numnewlines_at_end == 0)
        write_('-');
    //
    if(trimmed.len)
    {
        newl_();
        size_t pos = 0; // tracks the last character that was already written
        for(size_t i = 0; i < trimmed.len; ++i)
        {
            if(trimmed[i] != '\n')
                continue;
            // write everything up to this point
            csubstr since_pos = trimmed.range(pos, i+1); // include the newline
            indent_(ilevel + 1);
            write_(since_pos);
            pos = i+1; // already written
        }
        if(pos < trimmed.len)
        {
            indent_(ilevel + 1);
            write_(trimmed.sub(pos));
        }
    }
    for(size_t i = !is_newline_only; i < numnewlines_at_end; ++i)
        newl_();
}

template<class Writer>
void Emitter<Writer>::write_scalar_folded_(csubstr s, id_type ilevel)
{
    RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), s.find("\r") == csubstr::npos);
    csubstr trimmed = s.trimr('\n');
    const size_t numnewlines_at_end = s.len - trimmed.len;
    const bool is_newline_only = (trimmed.len == 0 && (s.len > 0));
    const bool explicit_indentation = s.triml("\n\r").begins_with_any(" \t");
    //
    write_('>');
    if(explicit_indentation)
        write_('2');
    //
    if(numnewlines_at_end == 0)
        write_('-');
    else if(numnewlines_at_end > 1 || is_newline_only)
        write_('+');
    //
    if(trimmed.len)
    {
        newl_();
        size_t pos = 0; // tracks the last character that was already written
        for(size_t i = 0; i < trimmed.len; ++i)
        {
            if(trimmed[i] != '\n')
                continue;
            // escape newline sequences
            if( ! _is_indented_block(s, pos, i))
            {
                if(pos < i)
                {
                    indent_(ilevel + 1);
                    write_(s.range(pos, i));
                    i = write_escaped_newlines_(s, i);
                    pos = i + 1;
                }
                else
                {
                    if(i+1 < s.len)
                    {
                        if(s.str[i+1] == '\n')
                        {
                            ++i;
                            i = write_escaped_newlines_(s, i);
                            pos = i+1;
                        }
                        else
                        {
                            newl_();
                            pos = i+1;
                        }
                    }
                }
            }
            else // do not escape newlines in indented blocks
            {
                ++i;
                indent_(ilevel + 1);
                write_(s.range(pos, i));
                if(pos > 0 || !s.begins_with_any(" \t"))
                    i = write_indented_block_(s, i, ilevel);
                pos = i;
            }
        }
        if(pos < trimmed.len)
        {
            indent_(ilevel + 1);
            write_(trimmed.sub(pos));
        }
    }
    for(size_t i = !is_newline_only; i < numnewlines_at_end; ++i)
        newl_();
}

template<class Writer>
void Emitter<Writer>::write_scalar_squo_(csubstr s, id_type ilevel)
{
    size_t pos = 0; // tracks the last character that was already written
    write_('\'');
    for(size_t i = 0; i < s.len; ++i)
    {
        if(s[i] == '\n')
        {
            write_(s.range(pos, i));  // write everything up to (excluding) this char
            //_c4dbgpf("newline at {}. writing ~~~{}~~~", i, s.range(pos, i));
            i = write_escaped_newlines_(s, i);
            //_c4dbgpf("newline --> {}", i);
            if(i < s.len)
                indent_(ilevel + 1);
            pos = i+1;
        }
        else if(s[i] == '\'')
        {
            csubstr sub = s.range(pos, i+1);
            //_c4dbgpf("squote at {}. writing ~~~{}~~~", i, sub);
            write_(sub); // write everything up to (including) this squote
            write_('\''); // write the squote again
            pos = i+1;
        }
    }
    // write remaining characters at the end of the string
    if(pos < s.len)
        write_(s.sub(pos));
    write_('\'');
}

template<class Writer>
void Emitter<Writer>::write_scalar_dquo_(csubstr s, id_type ilevel)
{
    size_t pos = 0; // tracks the last character that was already written
    write_('"');
    for(size_t i = 0; i < s.len; ++i)
    {
        const char curr = s.str[i];
        switch(curr) // NOLINT
        {
        case '"':
        case '\\':
        {
            csubstr sub = s.range(pos, i);
            write_(sub);  // write everything up to (excluding) this char
            write_('\\'); // write the escape
            write_(curr); // write the char
            pos = i+1;
            break;
        }
#ifndef prefer_writing_newlines_as_double_newlines
        case '\n':
        {
            csubstr sub = s.range(pos, i);
            write_(sub);   // write everything up to (excluding) this char
            write_("\\n"); // write the escape
            pos = i+1;
            (void)ilevel;
            break;
        }
#else
        case '\n':
        {
            // write everything up to (excluding) this newline
            //_c4dbgpf("nl@i={} rem=[{}]~~~{}~~~", i, s.sub(i).len, s.sub(i));
            _write(s.range(pos, i));
            i = _write_escaped_newlines(s, i);
            ++i;
            pos = i;
            // as for the next line...
            if(i < s.len)
            {
                _indent(ilevel + 1); // indent the next line
                // escape leading whitespace, and flush it
                size_t first = s.first_not_of(" \t", i);
                //_c4dbgpf("@i={} first={} rem=[{}]~~~{}~~~", i, first, s.sub(i).len, s.sub(i));
                if(first > i)
                {
                    if(first == npos)
                        first = s.len;
                    _write('\\');
                    _write(s.range(i, first));
                    _write('\\');
                    i = first-1;
                    pos = first;
                }
            }
            break;
        }
        // escape trailing whitespace before a newline
        case ' ':
        case '\t':
        {
            const size_t next = s.first_not_of(" \t\r", i);
            if(next != npos && s.str[next] == '\n')
            {
                csubstr sub = s.range(pos, i);
                _write(sub);  // write everything up to (excluding) this char
                _write('\\'); // escape the whitespace
                pos = i;
            }
            break;
        }
#endif
        case '\r':
        {
            csubstr sub = s.range(pos, i);
            write_(sub);  // write everything up to (excluding) this char
            write_("\\r"); // write the escaped char
            pos = i+1;
            break;
        }
        case '\b':
        {
            csubstr sub = s.range(pos, i);
            write_(sub);  // write everything up to (excluding) this char
            write_("\\b"); // write the escaped char
            pos = i+1;
            break;
        }
        }
    }
    // write remaining characters at the end of the string
    if(pos < s.len)
        write_(s.sub(pos));
    write_('"');
}

template<class Writer>
void Emitter<Writer>::write_scalar_plain_(csubstr s, id_type ilevel)
{
    if C4_UNLIKELY(ilevel == 0 && (s.begins_with("...") || s.begins_with("---")))
    {
        indent_(ilevel + 1); // indent the next line
        ++ilevel;
    }
    size_t pos = 0; // tracks the last character that was already written
    for(size_t i = 0; i < s.len; ++i)
    {
        const char curr = s.str[i];
        if(curr == '\n')
        {
            csubstr sub = s.range(pos, i);
            write_(sub);  // write everything up to (including) this newline
            i = write_escaped_newlines_(s, i);
            pos = i+1;
            if(pos < s.len)
                indent_(ilevel + 1); // indent the next line
        }
    }
    // write remaining characters at the end of the string
    if(pos < s.len)
        write_(s.sub(pos));
}


//-----------------------------------------------------------------------------

namespace detail {
inline type_bits json_type_(type_bits ty)
{
    enum : type_bits { // NOLINT
        ml_bits = (BLOCK|(STREAM & ~SEQ)), // remove SEQ from STREAM to test
        sl_bits = (CONTAINER_STYLE & ~FLOW_SPC),
    };
    if(ty & ml_bits)
    {
        ty &= ~BLOCK;
        ty |= FLOW_ML1;
    }
    else if((ty & (SEQ|MAP)) && !(ty & sl_bits))
    {
        ty |= FLOW_SL;
    }
    return ty;
}
} // namespace detail


template<class Writer>
void Emitter<Writer>::json_emit_(id_type id)
{
    NodeType ty = m_tree->type(id);
    // JSON does not have streams
    if C4_UNLIKELY(ty.is_stream() && m_opts.json_err_on_stream())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "found stream node");
    static_assert(STREAM & SEQ, "STREAM must be a SEQ");
    ty = detail::json_type_(ty);
    if(ty.is_flow_mlx())
    {
        json_visit_ml_(id, ty, 0);
        newl_();
    }
    else
    {
        json_visit_sl_(id, ty, 0);
    }
}

template<class Writer>
void Emitter<Writer>::json_visit_sl_(id_type id, NodeType ty, id_type depth)
{
    if C4_UNLIKELY(depth > m_opts.max_depth())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "max depth exceeded");
    if(ty.is_val())
    {
        json_writev_(id, ty);
    }
    else if(ty.is_keyval())
    {
        json_writek_(id, ty);
        write_(": ");
        json_writev_(id, ty);
    }
    else if(ty.is_container())
    {
        ty = detail::json_type_(ty);
        if(ty.has_key())
        {
            json_writek_(id, ty);
            write_(": ");
        }
        if(ty.is_seq())
            write_('[');
        else if(ty.is_map())
            write_('{');

        for(id_type child = m_tree->first_child(id); child != NONE; child = m_tree->next_sibling(child))
        {
            if(child != m_tree->first_child(id))
            {
                if((ty & FLOW_SPC) || m_opts.force_flow_spc())
                    write_(", ");
                else
                    write_(',');
            }
            json_visit_sl_(child, m_tree->type(child), depth+1);
        }

        if(ty.is_seq())
            write_(']');
        else if(ty.is_map())
            write_('}');
    }  // container
}

template<class Writer>
void Emitter<Writer>::json_visit_ml_(id_type id, NodeType ty, id_type depth)
{
    if C4_UNLIKELY(depth > m_opts.max_depth())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "max depth exceeded");
    if(ty.is_val())
    {
        json_writev_(id, ty);
    }
    else if(ty.is_keyval())
    {
        json_writek_(id, ty);
        write_(": ");
        json_writev_(id, ty);
    }
    else if(ty.is_container())
    {
        ty = detail::json_type_(ty);
        if(ty.has_key())
        {
            json_writek_(id, ty);
            write_(": ");
        }
        if(ty.is_seq())
            write_('[');
        else if(ty.is_map())
            write_('{');

        if(m_tree->has_children(id))
        {
            ++depth;
            if(m_opts.indent_flow_ml()) ++m_ilevel;
            newl_();
            indent_(m_ilevel);
            for(id_type first = m_tree->first_child(id), child = first;
                child != NONE;
                child = m_tree->next_sibling(child))
            {
                if(child != first)
                {
                    write_(',');
                    const size_t maxcols = m_opts.max_cols();
                    if((ty.m_bits & FLOW_MLN) && (m_col+1 < maxcols))
                    {
                        if((ty.m_bits & FLOW_SPC) || m_opts.force_flow_spc())
                            write_(' ');
                    }
                    else if((ty.m_bits & FLOW_ML1) || (m_col+1 >= maxcols))
                    {
                        newl_();
                        indent_(m_ilevel);
                    }
                }
                NodeType chty = m_tree->type(child);
                if(chty.is_flow_sl())
                    json_visit_sl_(child, chty, depth);
                else
                    json_visit_ml_(child, chty, depth);
            }
            if(m_opts.indent_flow_ml()) --m_ilevel;
            --depth;
            newl_();
            indent_(m_ilevel);
        }

        if(ty.is_seq())
            write_(']');
        else if(ty.is_map())
            write_('}');
    }
}

template<class Writer>
bool Emitter<Writer>::json_maybe_write_naninf_(csubstr s)
{
    switch(s.len)
    {
    case 3: case 4: case 5: // inf, nan, .nan, -.inf
    case 8: case 9: // infinity, -infinity
        break;
    default:
        return false;
    }
    const char first = s.str[0];
    csubstr rest = s.sub(1);
    if(s.len == 4 && first == '.')
    {
        if(scalar_is_inf3(rest.str))
            goto write_inf_positive; // NOLINT
        else if(scalar_is_nan3(rest.str))
            goto write_nan; // NOLINT
    }
    else if(first == '-' || first == '+') // begins with sign: must be inf
    {
        // match [-+].inf
        if((rest.len == 4 && rest.str[0] == '.' && scalar_is_inf3(rest.str + 1))
           // match [-+]inf
           || (rest.len == 3 && scalar_is_inf3(rest.str))
           // match [-+]infinity
           || (rest.len == 8 && (0 == memcmp(rest.str, "infinity", 8))))
        {
            if(first == '-')
                goto write_inf_negative; // NOLINT
            else
                goto write_inf_positive; // NOLINT
        }
    }
    else if(s.len == 8 && (0 == memcmp(s.str, "infinity", 8)))
    {
        goto write_inf_positive; // NOLINT
    }
    else if(s.len == 3)
    {
        if(scalar_is_inf3(s.str))
            goto write_inf_positive; // NOLINT
        else if(scalar_is_nan3(s.str))
            goto write_nan; // NOLINT
    }
    return false;
write_inf_positive:
    write_("\".inf\"");
    return true;
write_inf_negative:
    write_("\"-.inf\"");
    return true;
write_nan:
    write_("\".nan\"");
    return true;
}

template<class Writer>
void Emitter<Writer>::json_writek_(id_type id, NodeType ty)
{
    if C4_UNLIKELY(ty.has_key_tag() && m_opts.json_err_on_tag())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "JSON does not have tags");
    if C4_UNLIKELY(ty.has_key_anchor() && m_opts.json_err_on_anchor())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "JSON does not have anchors");
    csubstr key = m_tree->key(id);
    if(key.len)
    {
        if(json_maybe_write_naninf_(key))
            ;
        else
            json_write_scalar_dquo_(key);
    }
    else
    {
        write_("\"\"");
    }
}

template<class Writer>
void Emitter<Writer>::json_writev_(id_type id, NodeType ty)
{
    if C4_UNLIKELY(ty.has_val_tag() && m_opts.json_err_on_tag())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "JSON does not have tags");
    if C4_UNLIKELY(ty.has_val_anchor() && m_opts.json_err_on_anchor())
        RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, id, "JSON does not have anchors");
    csubstr val = m_tree->val(id);
    if(val.len)
    {
        // use double quoted style if the style is marked quoted
        bool dquoted = ((ty.m_bits & VALQUO)
                        || (scalar_style_choose_json(val).m_bits & SCALAR_DQUO)); // choose the style
        if(dquoted)
            json_write_scalar_dquo_(val);
        else if(json_maybe_write_naninf_(val))
            ;
        else if(val.is_number())
            json_write_number_(val);
        else
            write_(val);
    }
    else
    {
        if(val.str || (ty.m_bits & (VALQUO|VALTAG)))
            write_("\"\"");
        else
            write_("null");
    }
}


template<class Writer>
void Emitter<Writer>::json_write_scalar_dquo_(csubstr s)
{
    size_t pos = 0;
    write_('"');
    for(size_t i = 0; i < s.len; ++i)
    {
        switch(s.str[i])
        {
        case '"':
            write_(s.range(pos, i));
            write_("\\\"");
            pos = i + 1;
            break;
        case '\n':
            write_(s.range(pos, i));
            write_("\\n");
            pos = i + 1;
            break;
        case '\t':
            write_(s.range(pos, i));
            write_("\\t");
            pos = i + 1;
            break;
        case '\\':
            write_(s.range(pos, i));
            write_("\\\\");
            pos = i + 1;
            break;
        case '\r':
            write_(s.range(pos, i));
            write_("\\r");
            pos = i + 1;
            break;
        case '\b':
            write_(s.range(pos, i));
            write_("\\b");
            pos = i + 1;
            break;
        case '\f':
            write_(s.range(pos, i));
            write_("\\f");
            pos = i + 1;
            break;
        }
    }
    if(pos < s.len)
    {
        csubstr sub = s.sub(pos);
        write_(sub);
    }
    write_('"');
}

template<class Writer>
void Emitter<Writer>::json_write_number_(csubstr s)
{
    if(s.is_integer())
    {
        write_(s);
    }
    else
    {
        if(s.begins_with('-') && s.len > 1)
        {
            csubstr rest = s.sub(1);
            if(rest.begins_with('.'))
            {
                write_("-0");
                write_(rest);
            }
            else if(rest.ends_with('.'))
            {
                write_(s);
                write_('0');
            }
            else
            {
                write_(s);
            }
        }
        else if(s.begins_with('.'))
        {
            write_('0');
            write_(s);
        }
        else if(s.ends_with('.'))
        {
            write_(s);
            write_('0');
        }
        else
        {
            write_(s);
        }
    }
}

/** @endcond */

} // namespace yml
} // namespace c4

// NOLINTEND(modernize-avoid-c-style-cast)
C4_SUPPRESS_WARNING_GCC_CLANG_POP

#endif /* C4_YML_EMITTTER_DEF_HPP_ */
