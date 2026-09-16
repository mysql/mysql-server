#ifndef C4_YML_EVENT_HANDLER_TREE_HPP_
#define C4_YML_EVENT_HANDLER_TREE_HPP_

#ifndef C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif

#ifndef C4_YML_EVENT_HANDLER_STACK_HPP_
#include "c4/yml/event_handler_stack.hpp"
#endif

C4_SUPPRESS_WARNING_GCC_PUSH
C4_SUPPRESS_WARNING_MSVC_PUSH
C4_SUPPRESS_WARNING_MSVC(4702) // unreachable code
#if defined(__GNUC__) && __GNUC__ >= 6
C4_SUPPRESS_WARNING_GCC("-Wnull-dereference")
#endif

// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** @addtogroup doc_event_handlers_tree
 *
 * An event handler used by @ref ParseEngine to create the @ref Tree
 * (see @ref Parser).
 *
 * @{ */


/** @cond dev */
struct EventHandlerTreeState : public ParserState
{
    NodeData *tr_data;
};
/** @endcond */


/** The event handler to create a ryml @ref Tree. See the
 * documentation for @ref doc_event_handlers, which has important
 * notes about the event model used by rapidyaml. */
struct EventHandlerTree : public EventHandlerStack<EventHandlerTree, EventHandlerTreeState>
{

    /** @name types
     * @{ */

    using state = EventHandlerTreeState;
    enum { requires_strings_on_buffers = false }; // NOLINT

    /** @} */

public:

    /** @cond dev */
    Tree *C4_RESTRICT m_tree;
    id_type m_curr_doc;
    TagCache m_tag_cache;

    #ifdef RYML_DBG
    #define ryml_enable_(bits) enable_<bits>(); _c4dbgpf("node[{}]: enable {}", m_curr->node_id, #bits)
    #define ryml_disable_(bits) disable_<bits>(); _c4dbgpf("node[{}]: disable {}", m_curr->node_id, #bits)
    #else
    #define ryml_enable_(bits) enable_<bits>()
    #define ryml_disable_(bits) disable_<bits>()
    #endif
    #define ryml_hasany_(bits) has_any_<bits>()
    /** @endcond */

public:

    /** @name construction and resetting
     * @{ */

    EventHandlerTree() noexcept : EventHandlerStack(), m_tree(), m_curr_doc() {}
    EventHandlerTree(Callbacks const& cb) noexcept : EventHandlerStack(cb), m_tree(), m_curr_doc() {}
    EventHandlerTree(Tree *tree, id_type id) /*except!*/ : EventHandlerStack(tree->callbacks()), m_tree(tree), m_curr_doc()
    {
        reset(tree, id);
    }

    void reset(Tree *tree, id_type id)
    {
        if C4_UNLIKELY(!tree)
            RYML_ERR_BASIC_CB_(m_stack.m_callbacks, "null tree");
        if C4_UNLIKELY(id >= tree->capacity())
            RYML_ERR_VISIT_CB_(tree->callbacks(), tree, id, "invalid node");
        if C4_UNLIKELY(!tree->is_root(id))
            if C4_UNLIKELY(tree->is_map(tree->parent(id)))
                if C4_UNLIKELY(!tree->has_key(id))
                    RYML_ERR_BASIC_CB_(tree->callbacks(), "destination node belongs to a map and has no key");
        m_tree = tree;
        if(m_tree->is_root(id))
        {
            _stack_reset_root();
            _reset_parser_state(m_curr, id, m_tree->root_id());
        }
        else
        {
            _stack_reset_non_root();
            _reset_parser_state(m_parent, id, m_tree->parent(id));
            _reset_parser_state(m_curr, id, id);
        }
        m_curr_doc = m_tree->ancestor_doc(id);
        m_tag_cache.clear();
    }

    Callbacks const& callbacks() const { return m_stack.m_callbacks; }

    C4_ALWAYS_INLINE TagDirectives& tag_directives() { return m_tree->m_tag_directives; } // NOLINT(readability-make-member-function-const)
    C4_ALWAYS_INLINE TagCache &tag_cache() { return m_tag_cache; }

    /** @} */

public:

    /** @name parse events
     * @{ */

    void start_parse(const char* filename, substr ymlsrc)
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree != nullptr);
        this->_stack_start_parse(filename, ymlsrc);
    }

    void finish_parse()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree != nullptr);
        this->_stack_finish_parse();
        /* This pointer is temporary. Remember that:
         *
         * - this handler object may be held by the user
         * - it may be used with a temporary tree inside the parse function
         * - when the parse function returns the temporary tree, its address
         *   will change
         *
         * As a result, the user could try to read the tree from m_tree, and
         * end up reading the stale temporary object.
         *
         * So it is better to clear it here; then the user will get an obvious
         * segfault if reading from m_tree. */
        m_tree = nullptr;
    }

    void cancel_parse()
    {
        m_tree = nullptr;
    }

    /** @} */

public:

    /** @name YAML stream events */
    /** @{ */

    C4_ALWAYS_INLINE void begin_stream() const noexcept { /* nothing to do */ }

    C4_ALWAYS_INLINE void end_stream() const noexcept { /* nothing to do */ }

    /** @} */

public:

    /** @name YAML document events */
    /** @{ */

    /** implicit doc start (without ---) */
    void begin_doc()
    {
        _c4dbgp("begin_doc");
        if(_stack_should_push_on_begin_doc())
        {
            _c4dbgp("push!");
            _set_root_as_stream();
            _push();
            ryml_enable_(DOC);
        }
        m_curr_doc = m_curr->node_id;
    }
    /** implicit doc end (without ...) */
    void end_doc()
    {
        _c4dbgp("end_doc");
        m_curr_doc = m_tree->size();
        if(_stack_should_pop_on_end_doc())
        {
            _remove_speculative();
            _c4dbgp("pop!");
            _pop();
        }
    }

    /** explicit doc start, with --- */
    void begin_doc_expl()
    {
        _c4dbgp("begin_doc_expl");
        RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, m_tree->root_id() == m_curr->node_id, m_tree, m_curr->node_id);
        if(m_tree->is_stream(m_tree->root_id())) //if(_should_push_on_begin_doc())
        {
            _c4dbgp("push!");
            _push();
        }
        else
        {
            _c4dbgp("ensure stream");
            _set_root_as_stream();
            const id_type root = m_tree->root_id();
            const id_type first = m_tree->first_child(root);
            RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, m_tree->is_stream(root), m_tree, root);
            RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, m_tree->num_children(root) == 1u, m_tree, root);
            if(m_tree->is_container(first) || m_tree->is_val(first))
            {
                _c4dbgp("push!");
                _push();
                #ifdef RYML_WITH_COMMENTS
                m_tree->_p(root)->m_first_comment = NONE;
                m_tree->_p(root)->m_last_comment = NONE;
                #endif
            }
            else
            {
                _c4dbgp("tweak");
                _push();
                _remove_speculative();
                m_curr->node_id = m_tree->last_child(root);
                m_curr->tr_data = m_tree->_p(m_curr->node_id);
            }
        }
        ryml_enable_(DOC);
        m_curr_doc = m_curr->node_id;
    }
    /** explicit doc end, with ... */
    void end_doc_expl()
    {
        _c4dbgp("end_doc_expl");
        m_curr_doc = m_tree->size();
        _remove_speculative();
        if(_stack_should_pop_on_end_doc())
        {
            _c4dbgp("pop!");
            _pop();
        }
    }

    /** @} */

public:

    /** @name YAML map events */
    /** @{ */

    C4_NORETURN void begin_map_key_flow()
    {
        RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "ryml trees cannot handle containers as keys");
    }
    C4_NORETURN void begin_map_key_block()
    {
        RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "ryml trees cannot handle containers as keys");
    }

    void begin_map_val_flow()
    {
        _c4dbgpf("node[{}]: begin_map_val_flow", m_curr->node_id);
        RYML_CHECK_BASIC_CB_(m_stack.m_callbacks, !ryml_hasany_(VAL));
        ryml_enable_(MAP|FLOW_SL);
        _save_loc();
        _push();
    }
    void begin_map_val_block()
    {
        _c4dbgpf("node[{}]: begin_map_val_block", m_curr->node_id);
        RYML_CHECK_BASIC_CB_(m_stack.m_callbacks, !ryml_hasany_(VAL));
        ryml_enable_(MAP|BLOCK);
        _save_loc();
        _push();
    }

    void end_map_block()
    {
        _c4dbgpf("node[{}]: end_map_block", m_parent->node_id, m_parent->pos.line, m_curr->pos.line);
        _pop();
    }

    void end_map_flow(bool multiline, type_bits multiline_style=FLOW_ML1)
    {
        _c4dbgpf("node[{}]: end_map. multiline={} startline={} endline={}", m_parent->node_id, multiline, m_parent->pos.line, m_curr->pos.line);
        _pop();
        if(multiline)
        {
            ryml_disable_(FLOW_SL);
            enable_(multiline_style);
        }
    }

    /** @} */

public:

    /** @name YAML seq events */
    /** @{ */

    C4_NORETURN void begin_seq_key_flow()
    {
        RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "ryml trees cannot handle containers as keys");
    }
    C4_NORETURN void begin_seq_key_block()
    {
        RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "ryml trees cannot handle containers as keys");
    }

    void begin_seq_val_flow()
    {
        _c4dbgpf("node[{}]: begin_seq_val_flow", m_curr->node_id);
        RYML_CHECK_BASIC_CB_(m_stack.m_callbacks, !ryml_hasany_(VAL));
        ryml_enable_(SEQ|FLOW_SL);
        _save_loc();
        _push();
    }
    void begin_seq_val_block()
    {
        _c4dbgpf("node[{}]: begin_seq_val_block", m_curr->node_id);
        RYML_CHECK_BASIC_CB_(m_stack.m_callbacks, !ryml_hasany_(VAL));
        ryml_enable_(SEQ|BLOCK);
        _save_loc();
        _push();
    }

    void end_seq_block()
    {
        _c4dbgpf("node[{}]: end_seq_block", m_parent->node_id, m_parent->pos.line, m_curr->pos.line);
        _pop();
    }

    void end_seq_flow(bool multiline, type_bits multiline_style=FLOW_ML1)
    {
        _c4dbgpf("node[{}]: end_seq. multiline={} startline={} endline={}", m_parent->node_id, multiline, m_parent->pos.line, m_curr->pos.line);
        _pop();
        if(multiline)
        {
            ryml_disable_(FLOW_SL);
            enable_(multiline_style);
        }
    }

    /** @} */

public:

    /** @name YAML structure events */
    /** @{ */

    void add_sibling()
    {
        #if defined(__GNUC__) && (__GNUC__ >= 6)
        C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wnull-dereference")
        #endif
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_parent);
        RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, m_tree->has_children(m_parent->node_id), m_tree, m_parent->node_id);
        NodeData const* const prev = m_tree->m_buf; // watchout against relocation of the tree nodes
        _set_state_(m_curr, m_tree->_append_child__unprotected(m_parent->node_id));
        if(prev != m_tree->m_buf)
            _refresh_after_relocation();
        _c4dbgpf("node[{}]: added sibling={} prev={}", m_parent->node_id, m_curr->node_id, m_tree->prev_sibling(m_curr->node_id));
        #if defined(__GNUC__) && (__GNUC__ >= 6)
        C4_SUPPRESS_WARNING_GCC_POP
        #endif
    }

    /** reset the previous val as the first key of a new map, with flow style.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_flow()
    {
        if C4_UNLIKELY(m_tree->is_container(m_curr->node_id))
            RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "ryml trees cannot handle containers as keys");
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_parent);
        RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, m_tree->is_seq(m_parent->node_id), m_tree, m_parent->node_id);
        RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, !m_tree->is_container(m_curr->node_id), m_tree, m_curr->node_id);
        RYML_ASSERT_VISIT_CB_(m_stack.m_callbacks, !m_tree->has_key(m_curr->node_id), m_tree, m_curr->node_id);
        const NodeData tmp = _val2key_(*m_curr->tr_data);
        ryml_disable_(VALMASK_|VAL_STYLE|VALNIL);
        m_curr->tr_data->m_val = {};
        begin_map_val_flow();
        m_curr->tr_data->m_type = tmp.m_type;
        m_curr->tr_data->m_key = tmp.m_key;
    }

    /** like its flow counterpart, but this function can only be
     * called after the end of a flow-val at root or doc level.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    C4_NORETURN void actually_val_is_first_key_of_new_map_block()
    {
        RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "ryml trees cannot handle containers as keys");
    }

    /** @} */

public:

    /** @name YAML scalar events */
    /** @{ */


    C4_ALWAYS_INLINE void set_key_scalar_plain_empty() noexcept
    {
        _c4dbgpf("node[{}]: set key scalar plain as empty", m_curr->node_id);
        m_curr->tr_data->m_key.scalar = {};
        ryml_enable_(KEY|KEY_PLAIN|KEYNIL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain_empty() noexcept
    {
        _c4dbgpf("node[{}]: set val scalar plain as empty", m_curr->node_id);
        m_curr->tr_data->m_val.scalar = {};
        ryml_enable_(VAL|VAL_PLAIN|VALNIL);
    }

    C4_ALWAYS_INLINE void set_key_scalar_plain(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar plain: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        ryml_enable_(KEY|KEY_PLAIN);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar plain: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        ryml_enable_(VAL|VAL_PLAIN);
    }


    C4_ALWAYS_INLINE void set_key_scalar_dquoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar dquot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        ryml_enable_(KEY|KEY_DQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_dquoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar dquot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        ryml_enable_(VAL|VAL_DQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_squoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar squot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        ryml_enable_(KEY|KEY_SQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_squoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar squot: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        ryml_enable_(VAL|VAL_SQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_literal(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar literal: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        ryml_enable_(KEY|KEY_LITERAL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_literal(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar literal: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        ryml_enable_(VAL|VAL_LITERAL);
    }


    C4_ALWAYS_INLINE void set_key_scalar_folded(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar folded: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_key.scalar = scalar;
        ryml_enable_(KEY|KEY_FOLDED);
    }
    C4_ALWAYS_INLINE void set_val_scalar_folded(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar folded: [{}]~~~{}~~~", m_curr->node_id, scalar.len, scalar);
        m_curr->tr_data->m_val.scalar = scalar;
        ryml_enable_(VAL|VAL_FOLDED);
    }


    C4_ALWAYS_INLINE void mark_key_scalar_unfiltered() noexcept
    {
        ryml_enable_(KEY_UNFILT);
    }
    C4_ALWAYS_INLINE void mark_val_scalar_unfiltered() noexcept
    {
        ryml_enable_(VAL_UNFILT);
    }

    /** @} */

public:

    /** @name YAML anchor/reference events */
    /** @{ */

    void set_key_anchor(csubstr anchor)
    {
        _c4dbgpf("node[{}]: set key anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, !ryml_hasany_(KEYREF));
        RYML_ASSERT_PARSE_CB_(m_tree->callbacks(), !anchor.begins_with('&'), m_curr->pos);
        ryml_enable_(KEYANCH);
        m_curr->tr_data->m_key.anchor = anchor;
    }
    void set_val_anchor(csubstr anchor)
    {
        _c4dbgpf("node[{}]: set val anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, !ryml_hasany_(VALREF));
        RYML_ASSERT_PARSE_CB_(m_tree->callbacks(), !anchor.begins_with('&'), m_curr->pos);
        ryml_enable_(VALANCH);
        m_curr->tr_data->m_val.anchor = anchor;
    }

    void set_key_ref(csubstr ref)
    {
        _c4dbgpf("node[{}]: set key ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        RYML_ASSERT_PARSE_CB_(m_stack.m_callbacks, ref.begins_with('*'), m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_stack.m_callbacks, !ryml_hasany_(KEYANCH), m_curr->pos);
        ryml_enable_(KEY|KEYREF);
        m_curr->tr_data->m_key.anchor = ref.sub(1);
        m_curr->tr_data->m_key.scalar = ref;
    }
    void set_val_ref(csubstr ref)
    {
        _c4dbgpf("node[{}]: set val ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        RYML_ASSERT_PARSE_CB_(m_stack.m_callbacks, ref.begins_with('*'), m_curr->pos);
        RYML_ASSERT_PARSE_CB_(m_stack.m_callbacks, !ryml_hasany_(VALANCH), m_curr->pos);
        ryml_enable_(VAL|VALREF);
        m_curr->tr_data->m_val.anchor = ref.sub(1);
        m_curr->tr_data->m_val.scalar = ref;
    }

    /** @} */

public:

    /** @name YAML tag events */
    /** @{ */

    void set_key_tag(csubstr tag)
    {
        _c4dbgpf("node[{}]: set key tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag);
        ryml_enable_(KEYTAG);
        m_curr->tr_data->m_key.tag = tag;
    }
    void set_val_tag(csubstr tag)
    {
        _c4dbgpf("node[{}]: set val tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag);
        ryml_enable_(VALTAG);
        m_curr->tr_data->m_val.tag = tag;
    }

    /** @} */

public:

    /** @name YAML directive events */
    /** @{ */

    void add_directive_yaml(csubstr yaml_version) // NOLINT(readability-convert-member-functions-to-static)
    {
        _c4dbgpf("%YAML directive! version={}", yaml_version);
        (void)yaml_version;
    }

    void add_directive_tag(csubstr handle, csubstr prefix)
    {
        _c4dbgpf("%TAG directive! handle={} prefix={} id={}", handle, prefix, m_curr_doc);
        if C4_UNLIKELY(!m_tree->m_tag_directives.add(handle, prefix, m_curr_doc))
            RYML_ERR_PARSE_CB_(m_stack.m_callbacks, m_curr->pos, "too many %TAG directives");
    }

    /** @} */

public:

    /** @name arena functions */
    /** @{ */

    substr arena()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        return m_tree->m_arena.first(m_tree->m_arena_pos);
    }
    substr arena_rem()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        return m_tree->m_arena.sub(m_tree->m_arena_pos);
    }
    substr alloc_arena(size_t len) // NOLINT(readability-make-member-function-const)
    {
        return m_tree->alloc_arena(len);
    }

    /** @} */

public:

    /** @cond dev */
    void _reset_parser_state(state* st, id_type parse_root, id_type node)
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        _set_state_(st, node);
        const NodeType type = m_tree->type(node);
        #ifdef RYML_DBG
        char flagbuf[80];
        _c4dbgpf("resetting state: initial flags={}", detail::_parser_flags_to_str(flagbuf, st->flags));
        #endif
        if(type == NOTYPE)
        {
            _c4dbgpf("node[{}] is notype", node);
            if(m_tree->is_root(parse_root))
            {
                _c4dbgpf("node[{}] is root", node);
                st->flags |= RUNK|RTOP;
            }
            else
            {
                _c4dbgpf("node[{}] is not root. setting USTY", node);
                st->flags |= USTY;
            }
        }
        else if(type.is_map())
        {
            _c4dbgpf("node[{}] is map", node);
            st->flags |= RMAP|USTY;
        }
        else if(type.is_seq())
        {
            _c4dbgpf("node[{}] is map", node);
            st->flags |= RSEQ|USTY;
        }
        else if(type.has_key())
        {
            _c4dbgpf("node[{}] has key. setting USTY", node);
            st->flags |= USTY;
        }
        else
        {
            RYML_ERR_VISIT_CB_(m_tree->callbacks(), m_tree, node, "cannot append to node"); // LCOV_EXCL_LINE
        }
        if(type.is_doc())
        {
            _c4dbgpf("node[{}] is doc", node);
            st->flags |= RDOC;
        }
        #ifdef RYML_DBG
        _c4dbgpf("resetting state: final flags={}", detail::_parser_flags_to_str(flagbuf, st->flags));
        #endif
    }

    /** push a new parent, add a child to the new parent, and set the
     * child as the current node */
    void _push()
    {
        _stack_push();
        NodeData const* prev = m_tree->m_buf; // watch out against relocation of the tree nodes
        m_curr->node_id = m_tree->_append_child__unprotected(m_parent->node_id);
        m_curr->tr_data = m_tree->_p(m_curr->node_id);
        if(prev != m_tree->m_buf)
            _refresh_after_relocation();
        _c4dbgpf("pushed! level={}. top is now node={} (parent={})", m_curr->level, m_curr->node_id, m_parent ? m_parent->node_id : NONE);
    }
    /** end the current scope */
    void _pop()
    {
        _remove_speculative_with_parent();
        _stack_pop();
    }

public:

    C4_ALWAYS_INLINE void enable_(type_bits bits) noexcept
    {
        m_curr->tr_data->m_type.m_bits |= bits;
    }
    template<type_bits bits> C4_HOT C4_ALWAYS_INLINE void enable_() noexcept
    {
        m_curr->tr_data->m_type.m_bits |= bits;
    }
    template<type_bits bits> C4_HOT C4_ALWAYS_INLINE void disable_() noexcept
    {
        m_curr->tr_data->m_type.m_bits &= ~bits;
    }
    template<type_bits bits> C4_HOT C4_ALWAYS_INLINE bool has_any_() const noexcept
    {
        return (m_curr->tr_data->m_type.m_bits & bits) != 0;
    }

public:

    C4_ALWAYS_INLINE void _set_state_(state *C4_RESTRICT s, id_type id) const noexcept
    {
        s->node_id = id;
        s->tr_data = m_tree->_p(id);
    }
    void _refresh_after_relocation()
    {
        _c4dbgp("tree: refreshing stack data after tree data relocation");
        for(auto &st : m_stack)
            st.tr_data = m_tree->_p(st.node_id);
    }

    void _set_root_as_stream()
    {
        _c4dbgp("set root as stream");
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->root_id() == 0u, m_tree, m_tree->root_id());
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_curr->node_id == 0u, m_tree, m_curr->node_id);
        m_tree->set_root_as_stream();
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_stream(m_tree->root_id()), m_tree, m_tree->root_id());
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->has_children(m_tree->root_id()), m_tree, m_tree->root_id());
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->is_doc(m_tree->first_child(m_tree->root_id())), m_tree, m_tree->root_id());
        _set_state_(m_curr, m_tree->root_id());
    }

    static NodeData _val2key_(NodeData const& C4_RESTRICT d) noexcept
    {
        NodeData r = d;
        r.m_key = d.m_val;
        r.m_val = {};
        r.m_type = d.m_type;
        static_assert((VALMASK_ >> 1u) == KEYMASK_, "required for this function to work");
        static_assert((VAL_STYLE >> 1u) == KEY_STYLE, "required for this function to work");
        r.m_type.m_bits = ((d.m_type.m_bits & (VALMASK_|VAL_STYLE)) >> 1u);
        r.m_type.m_bits = (r.m_type.m_bits & ~(VALMASK_|VAL_STYLE));
        r.m_type.m_bits = (r.m_type.m_bits | KEY);
        if(d.m_type.m_bits & VALNIL)
            r.m_type.m_bits = (r.m_type.m_bits | KEYNIL);
        return r;
    }

    void _remove_speculative()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), !m_tree->empty());
        const id_type last_added = m_tree->size() - 1;
        const NodeData *C4_RESTRICT d = m_tree->_p(last_added);
        if(d->m_parent != NONE && d->m_type == NOTYPE)
        {
            _c4dbgpf("remove speculative: currparent={} node={} parent(node)={}", m_parent->node_id, last_added, d->m_parent);
            m_tree->remove(last_added);
            --m_curr->node_id;
        }
    }

    void _remove_speculative_with_parent()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), !m_tree->empty());
        const id_type last_added = m_tree->size() - 1;
        RYML_ASSERT_VISIT_CB_(m_tree->callbacks(), m_tree->has_parent(last_added), m_tree, last_added);
        if(m_tree->_p(last_added)->m_type == NOTYPE)
        {
            _c4dbgpf("remove speculative node with parent. parent={} node={} parent(node)={}", m_parent->node_id, last_added, m_tree->parent(last_added));
            m_tree->remove(last_added);
            --m_curr->node_id;
        }
    }

    C4_ALWAYS_INLINE void _save_loc()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_tree);
        RYML_ASSERT_BASIC_CB_(m_tree->callbacks(), m_tree->_p(m_curr->node_id)->m_val.scalar.len == 0);
        m_tree->_p(m_curr->node_id)->m_val.scalar.str = m_curr->line_contents.rem.str;
    }

#undef ryml_enable_
#undef ryml_disable_
#undef ryml_has_any_

    /** @endcond */
};

/** @} */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)
C4_SUPPRESS_WARNING_MSVC_POP
C4_SUPPRESS_WARNING_GCC_POP

#endif /* C4_YML_EVENT_HANDLER_TREE_HPP_ */
