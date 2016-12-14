/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <stdlib.h>
#include "xcom_cfg.h"

void init_cfg_app_xcom()
{
  if (!the_app_xcom_cfg)
    the_app_xcom_cfg= (cfg_app_xcom_st*) malloc(sizeof(cfg_app_xcom_st));

  the_app_xcom_cfg->m_poll_spin_loops= 0;
}
void deinit_cfg_app_xcom()
{
  free(the_app_xcom_cfg);
  the_app_xcom_cfg= NULL;
}
