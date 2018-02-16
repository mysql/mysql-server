/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_DETECTOR_H
#define XCOM_DETECTOR_H

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DETECTOR_LIVE_TIMEOUT 5.0

typedef double detector_state[NSERVERS];
struct site_def;

void note_detected(struct site_def const *site, node_no node);
int may_be_dead(detector_state const ds, node_no i, double seconds);
void init_detector(detector_state ds);
void invalidate_detector_sites(struct site_def *site);
void update_detected(struct site_def *site);

#ifdef __cplusplus
}
#endif

#endif
