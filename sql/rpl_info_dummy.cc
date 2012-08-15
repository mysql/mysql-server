/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include "rpl_info_dummy.h"

Rpl_info_dummy::Rpl_info_dummy(const int nparam)
  :Rpl_info_handler(nparam)
{
}

int Rpl_info_dummy::do_init_info(uint instance __attribute__((unused)))
{
  return 0;
}

int Rpl_info_dummy::do_init_info()
{
  return 0;
}

int Rpl_info_dummy::do_prepare_info_for_read()
{
  DBUG_ASSERT(!abort);
  cursor= 0;
  return 0;
}

int Rpl_info_dummy::do_prepare_info_for_write()
{
  DBUG_ASSERT(!abort);
  cursor= 0;
  return 0;
}

enum_return_check Rpl_info_dummy::do_check_info()
{
  DBUG_ASSERT(!abort);
  return REPOSITORY_DOES_NOT_EXIST;
}

enum_return_check Rpl_info_dummy::do_check_info(uint instance __attribute__((unused)))
{
  DBUG_ASSERT(!abort);
  return REPOSITORY_DOES_NOT_EXIST;
}

int Rpl_info_dummy::do_flush_info(const bool force __attribute__((unused)))
{
  DBUG_ASSERT(!abort);
  return 0;
}

void Rpl_info_dummy::do_end_info()
{
  return;
}

int Rpl_info_dummy::do_remove_info()
{
  DBUG_ASSERT(!abort);
  return 0;
}

int Rpl_info_dummy::do_clean_info()
{
  DBUG_ASSERT(!abort);
  return 0;
}

uint Rpl_info_dummy::do_get_rpl_info_type()
{
  return INFO_REPOSITORY_DUMMY;
}

bool Rpl_info_dummy::do_set_info(const int pos __attribute__((unused)),
                                const char *value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos __attribute__((unused)),
                                const uchar *value __attribute__((unused)),
                                const size_t size __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos __attribute__((unused)),
                                const ulong value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos __attribute__((unused)),
                                const int value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos __attribute__((unused)),
                                const float value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos __attribute__((unused)),
                                const Dynamic_ids *value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos __attribute__((unused)),
                                char *value __attribute__((unused)),
                                const size_t size __attribute__((unused)),
                                const char *default_value __attribute__((unused)))
{
    DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos __attribute__((unused)),
                                uchar *value __attribute__((unused)),
                                const size_t size __attribute__((unused)),
                                const uchar *default_value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos __attribute__((unused)),
                                ulong *value __attribute__((unused)),
                                const ulong default_value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos __attribute__((unused)),
                                int *value __attribute__((unused)),
                                const int default_value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos __attribute__((unused)),
                                float *value __attribute__((unused)),
                                const float default_value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos __attribute__((unused)),
                                Dynamic_ids *value __attribute__((unused)),
                                const Dynamic_ids *default_value __attribute__((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

char* Rpl_info_dummy::do_get_description_info()
{
  DBUG_ASSERT(!abort);

  return NULL;
}

bool Rpl_info_dummy::do_is_transactional()
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_update_is_transactional()
{
  DBUG_ASSERT(!abort);

  return FALSE;
}
