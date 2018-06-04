/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/



#include "hash_item_util.h"

#include <arpa/inet.h>

char * hash_item_get_key(const hash_item* item) {
  char *ret = (void*)(item + 1);
  if (item->iflag & ITEM_WITH_CAS) {
    ret += sizeof(uint64_t);
  }
  
  return ret;
}


char * hash_item_get_data(const hash_item* item) {
  return ((char*) hash_item_get_key(item)) + item->nkey;
}


uint16_t hash_item_get_key_len(const hash_item *item) {
  return item->nkey;
}


uint32_t hash_item_get_data_len(const hash_item *item) {
  return item->nbytes;
}


uint64_t hash_item_get_cas(const hash_item* item)
{
  if (item->iflag & ITEM_WITH_CAS) {
    return *(uint64_t*)(item + 1);
  }
  return 0;
}

void hash_item_set_cas(hash_item* item, uint64_t cas)
{
  if (item->iflag & ITEM_WITH_CAS) {
    *(uint64_t*)(item + 1) = cas;
  }
  return;
}

uint64_t hash_item_get_exp(const hash_item* item)
{
	return item->exptime;
}

uint32_t hash_item_get_flag(const hash_item* item)
{
	return htonl(item->flags);
}

void hash_item_set_flag(hash_item* item, uint32_t value)
{
	item->flags = ntohl(value);
	return;
}

uint64_t * hash_item_get_cas_ptr(const hash_item* item)
{
  if (item->iflag & ITEM_WITH_CAS) {
    return (uint64_t*)(item + 1);
  }
  return 0;
}

