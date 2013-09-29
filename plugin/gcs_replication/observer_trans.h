/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef OBSERVER_TRANS
#define OBSERVER_TRANS

#include "gcs_plugin.h"
#include <replication.h>

/*
  Transaction lifecycle events observers.
*/
int gcs_trans_before_commit(Trans_param *param);

int gcs_trans_before_rollback(Trans_param *param);

int gcs_trans_after_commit(Trans_param *param);

int gcs_trans_after_rollback(Trans_param *param);

extern Trans_observer trans_observer;

#endif /* OBSERVER_TRANS */
