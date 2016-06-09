/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "rpl_info_dummy.h"

Rpl_info_dummy::Rpl_info_dummy(const int nparam)
  :Rpl_info_handler(nparam)
{
}

int Rpl_info_dummy::do_init_info(uint instance MY_ATTRIBUTE((unused)))
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

enum_return_check Rpl_info_dummy::do_check_info(uint instance MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);
  return REPOSITORY_DOES_NOT_EXIST;
}

int Rpl_info_dummy::do_flush_info(const bool force MY_ATTRIBUTE((unused)))
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

bool Rpl_info_dummy::do_set_info(const int pos MY_ATTRIBUTE((unused)),
                                const char *value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos MY_ATTRIBUTE((unused)),
                                const uchar *value MY_ATTRIBUTE((unused)),
                                const size_t size MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos MY_ATTRIBUTE((unused)),
                                const ulong value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos MY_ATTRIBUTE((unused)),
                                const int value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos MY_ATTRIBUTE((unused)),
                                const float value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_set_info(const int pos MY_ATTRIBUTE((unused)),
                                const Server_ids *value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos MY_ATTRIBUTE((unused)),
                                char *value MY_ATTRIBUTE((unused)),
                                const size_t size MY_ATTRIBUTE((unused)),
                                const char *default_value MY_ATTRIBUTE((unused)))
{
    DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos MY_ATTRIBUTE((unused)),
                                uchar *value MY_ATTRIBUTE((unused)),
                                const size_t size MY_ATTRIBUTE((unused)),
                                const uchar *default_value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos MY_ATTRIBUTE((unused)),
                                ulong *value MY_ATTRIBUTE((unused)),
                                const ulong default_value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos MY_ATTRIBUTE((unused)),
                                int *value MY_ATTRIBUTE((unused)),
                                const int default_value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos MY_ATTRIBUTE((unused)),
                                float *value MY_ATTRIBUTE((unused)),
                                const float default_value MY_ATTRIBUTE((unused)))
{
  DBUG_ASSERT(!abort);

  return FALSE;
}

bool Rpl_info_dummy::do_get_info(const int pos MY_ATTRIBUTE((unused)),
                                Server_ids *value MY_ATTRIBUTE((unused)),
                                const Server_ids *default_value MY_ATTRIBUTE((unused)))
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
