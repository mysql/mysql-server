/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PAX_MSG_H
#define PAX_MSG_H

#ifdef __cplusplus
extern "C" {
#endif


#if 0
#define PAX_MSG_SANITY_CHECK(p) {                       \
    if((p) && !(p)->a && (p)->msg_type == normal){      \
      assert((p)->op != client_msg);                    \
      assert((p)->op != ack_prepare_op);                \
      assert((p)->op != accept_op);                     \
      assert((p)->op != learn_op);                      \
    }                                                   \
  }
#else
#define PAX_MSG_SANITY_CHECK(p)
#endif

#define CLONE_PAX_MSG(target, msg) replace_pax_msg((&target), clone_pax_msg_no_app(msg))

int	eq_ballot(ballot x, ballot y);
int	gt_ballot(ballot x, ballot y);
int	ref_msg(pax_msg *p);
int	unref_msg(pax_msg **pp);
pax_msg *clone_pax_msg_no_app(pax_msg *msg);
pax_msg *clone_pax_msg(pax_msg *msg);
ballot *init_ballot(ballot *bal, int cnt, node_no node);
pax_msg *pax_msg_new(synode_no synode, site_def const *site);
pax_msg *pax_msg_new_0(synode_no synode);
void dbg_ballot(ballot const *p, char *s);
char *dbg_pax_msg(pax_msg const *p);
void delete_pax_msg(pax_msg *p);
/* void replace_pax_msg(pax_msg **target, pax_msg *p); */
void unchecked_replace_pax_msg(pax_msg **target, pax_msg *p);

#define replace_pax_msg(target, p) { PAX_MSG_SANITY_CHECK(p); unchecked_replace_pax_msg(target, p);}

#ifdef __cplusplus
}
#endif

#endif

