#ifndef C4_YML_EVENT_HANDLER_STACK_HPP_
#define C4_YML_EVENT_HANDLER_STACK_HPP_

#ifndef C4_YML_DETAIL_STACK_HPP_
#include "c4/yml/detail/stack.hpp"
#endif

#ifndef C4_YML_NODE_TYPE_HPP_
#include "c4/yml/node_type.hpp"
#endif

#ifndef C4_YML_DETAIL_DBGPRINT_HPP_
#include "c4/yml/detail/dbgprint.hpp"
#endif

#ifndef C4_YML_PARSER_STATE_HPP_
#include "c4/yml/parser_state.hpp"
#endif

#ifdef RYML_DBG
#ifndef C4_YML_DETAIL_PRINT_HPP_
#include "c4/yml/detail/print.hpp"
#endif
#endif

// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** @addtogroup doc_event_handlers
 * @{ */

/** Use this class a base of implementations of event handler to
 * simplify the stack logic. */
template<class HandlerImpl, class HandlerState>
struct EventHandlerStack
{
    static_assert(std::is_base_of<ParserState, HandlerState>::value,
                  "ParserState must be a base of HandlerState");

    using state = HandlerState;

public:

    detail::stack<state> m_stack;
    state *C4_RESTRICT   m_curr;    ///< current stack level: top of the stack. cached here for easier access.
    state *C4_RESTRICT   m_parent;  ///< parent of the current stack level.
    substr               m_src;

protected:

    EventHandlerStack() : m_stack(), m_curr(), m_parent(), m_src() {} // NOLINT
    EventHandlerStack(Callbacks const& cb) : m_stack(cb), m_curr(), m_parent(), m_src() {} // NOLINT

protected:

    void _stack_start_parse(const char *filename, substr ymlsrc)
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_curr != nullptr);
        m_curr->start_parse(filename, m_curr->node_id);
        m_src = ymlsrc;
    }

    void _stack_finish_parse()
    {
    }

protected:

    void _stack_reset_root()
    {
        m_stack.clear();
        m_stack.push({});
        m_parent = nullptr;
        m_curr = &m_stack.top();
    }

    void _stack_reset_non_root()
    {
        m_stack.clear();
        m_stack.push({}); // parent
        m_stack.push({}); // node
        m_parent = &m_stack.top(1);
        m_curr = &m_stack.top();
    }

    void _stack_push()
    {
        m_stack.push_top();
        m_parent = &m_stack.top(1); // don't use m_curr. watch out for relocations inside the prev push
        m_curr = &m_stack.top();
        m_curr->reset_after_push();
    }

    void _stack_pop()
    {
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_parent);
        RYML_ASSERT_BASIC_CB_(m_stack.m_callbacks, m_stack.size() > 1);
        m_parent->reset_before_pop(*m_curr);
        m_stack.pop();
        m_parent = m_stack.size() > 1 ? &m_stack.top(1) : nullptr;
        m_curr = &m_stack.top();
        #ifdef RYML_DBG
        if(m_parent)
            _c4dbgpf("popped! top is now node={} (parent={})", m_curr->node_id, m_parent->node_id);
        else
            _c4dbgpf("popped! top is now node={} @ ROOT", m_curr->node_id);
        #endif
    }

protected:

    // undefined below
    #define ryml_has_any_(bits) (static_cast<HandlerImpl const* C4_RESTRICT>(this)->template has_any_<bits>())

    // FIXME. Not happy about where these functions are. They should
    // be defined and called by the parser, passing the bool result to
    // begin_doc()/end_doc() as well as begin_doc_expl()/end_doc_expl().

    bool _stack_should_push_on_begin_doc() const
    {
        const bool is_root = (m_stack.size() == 1u);
        return is_root && (m_curr->has_children || ryml_has_any_(DOC|VAL|MAP|SEQ));
    }

    bool _stack_should_pop_on_end_doc() const
    {
        const bool is_root = (m_stack.size() == 1u);
        return !is_root && ryml_has_any_(DOC);
    }

    #undef ryml_has_any_

};

/** @} */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)

#endif /* C4_YML_EVENT_HANDLER_STACK_HPP_ */
