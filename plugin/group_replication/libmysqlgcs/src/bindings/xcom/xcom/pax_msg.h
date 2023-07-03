/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PAX_MSG_H
#define PAX_MSG_H

#include "xcom/site_struct.h"
#include "xdr_gen/xcom_vp.h"

#ifdef PAX_MSG_SANITY_CHECK
#define PAX_MSG_SANITY_CHECK(p)                      \
  {                                                  \
    if ((p) && !(p)->a && (p)->msg_type == normal) { \
      assert((p)->op != client_msg);                 \
      assert((p)->op != ack_prepare_op);             \
      assert((p)->op != accept_op);                  \
      assert((p)->op != learn_op);                   \
    }                                                \
  }
#else
#define PAX_MSG_SANITY_CHECK(p)
#endif

#define CLONE_PAX_MSG(target, msg) \
  replace_pax_msg((&target), clone_pax_msg_no_app(msg))

int eq_ballot(ballot x, ballot y);
int gt_ballot(ballot x, ballot y);
int ref_msg(pax_msg *p);
int unref_msg(pax_msg **pp);
pax_msg *clone_pax_msg_no_app(pax_msg *msg);
pax_msg *clone_pax_msg(pax_msg *msg);
ballot *init_ballot(ballot *bal, int cnt, node_no node);
pax_msg *pax_msg_new(synode_no synode, site_def const *site);
pax_msg *pax_msg_new_0(synode_no synode);
void dbg_ballot(ballot const *p, char *s);
void add_ballot_event(ballot const bal);
char *dbg_pax_msg(pax_msg const *p);
void delete_pax_msg(pax_msg *p);
/* void replace_pax_msg(pax_msg **target, pax_msg *p); */
void unchecked_replace_pax_msg(pax_msg **target, pax_msg *p);

#define replace_pax_msg(target, p)        \
  {                                       \
    PAX_MSG_SANITY_CHECK(p);              \
    unchecked_replace_pax_msg(target, p); \
  }

#endif
