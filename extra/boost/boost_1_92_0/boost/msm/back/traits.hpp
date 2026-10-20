// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACK_TRAITS_H
#define BOOST_MSM_BACK_TRAITS_H

#include <boost/mpl/has_xxx.hpp>

// Metafunctions
BOOST_MPL_HAS_XXX_TRAIT_DEF(explicit_creation)
BOOST_MPL_HAS_XXX_TRAIT_DEF(pseudo_entry)
BOOST_MPL_HAS_XXX_TRAIT_DEF(pseudo_exit)
BOOST_MPL_HAS_XXX_TRAIT_DEF(concrete_exit_state)
BOOST_MPL_HAS_XXX_TRAIT_DEF(composite_tag)
BOOST_MPL_HAS_XXX_TRAIT_DEF(not_real_row_tag)
BOOST_MPL_HAS_XXX_TRAIT_DEF(event_blocking_flag)
BOOST_MPL_HAS_XXX_TRAIT_DEF(explicit_entry_state)
BOOST_MPL_HAS_XXX_TRAIT_DEF(completion_event)
BOOST_MPL_HAS_XXX_TRAIT_DEF(no_exception_thrown)
BOOST_MPL_HAS_XXX_TRAIT_DEF(no_message_queue)
BOOST_MPL_HAS_XXX_TRAIT_DEF(activate_deferred_events)
BOOST_MPL_HAS_XXX_TRAIT_DEF(wrapped_entry)
BOOST_MPL_HAS_XXX_TRAIT_DEF(active_state_switch_policy)

// State machine
BOOST_MPL_HAS_XXX_TRAIT_DEF(accept_sig)
BOOST_MPL_HAS_XXX_TRAIT_DEF(no_automatic_create)
BOOST_MPL_HAS_XXX_TRAIT_DEF(non_forwarding_flag)
BOOST_MPL_HAS_XXX_TRAIT_DEF(direct_entry)
BOOST_MPL_HAS_XXX_TRAIT_DEF(initial_event)
BOOST_MPL_HAS_XXX_TRAIT_DEF(final_event)
BOOST_MPL_HAS_XXX_TRAIT_DEF(do_serialize)
BOOST_MPL_HAS_XXX_TRAIT_DEF(history_policy)
BOOST_MPL_HAS_XXX_TRAIT_DEF(fsm_check)
BOOST_MPL_HAS_XXX_TRAIT_DEF(compile_policy)
BOOST_MPL_HAS_XXX_TRAIT_DEF(queue_container_policy)
BOOST_MPL_HAS_XXX_TRAIT_DEF(using_declared_table)
BOOST_MPL_HAS_XXX_TRAIT_DEF(event_queue_before_deferred_queue)

// Dispatch table
BOOST_MPL_HAS_XXX_TRAIT_DEF(is_frow)

#endif // BOOST_MSM_BACK_TRAITS_H