/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */


#include "hash_item_util.h"

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

rel_time_t hash_item_get_exptime(const hash_item *item) {
  return item->exptime;
}

uint32_t hash_item_get_flags(const hash_item *item) {
  return item->flags;
}

uint64_t hash_item_get_cas(const hash_item* item)
{
  if (item->iflag & ITEM_WITH_CAS) {
    return *(uint64_t*)(item + 1);
  }
  return 0;
}

uint64_t * hash_item_get_cas_ptr(const hash_item* item)
{
  if (item->iflag & ITEM_WITH_CAS) {
    return (uint64_t*)(item + 1);
  }
  return 0;
}

void hash_item_set_cas(hash_item* item, uint64_t val)
{
  if (item->iflag & ITEM_WITH_CAS) {
    *(uint64_t*)(item + 1) = val;
  }
}

