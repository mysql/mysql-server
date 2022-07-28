/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "hash_item_util.h"

#include <arpa/inet.h>

char *hash_item_get_key(const hash_item *item) {
  char *ret = (char *)(item + 1);
  if (item->iflag & ITEM_WITH_CAS) {
    ret += sizeof(uint64_t);
  }

  return ret;
}

char *hash_item_get_data(const hash_item *item) {
  return ((char *)hash_item_get_key(item)) + item->nkey;
}

uint16_t hash_item_get_key_len(const hash_item *item) { return item->nkey; }

uint32_t hash_item_get_data_len(const hash_item *item) { return item->nbytes; }

uint64_t hash_item_get_cas(const hash_item *item) {
  if (item->iflag & ITEM_WITH_CAS) {
    return *(uint64_t *)(item + 1);
  }
  return 0;
}

void hash_item_set_cas(hash_item *item, uint64_t cas) {
  if (item->iflag & ITEM_WITH_CAS) {
    *(uint64_t *)(item + 1) = cas;
  }
  return;
}

uint64_t hash_item_get_exp(const hash_item *item) { return item->exptime; }

uint32_t hash_item_get_flag(const hash_item *item) {
  return htonl(item->flags);
}

void hash_item_set_flag(hash_item *item, uint32_t value) {
  item->flags = ntohl(value);
  return;
}

uint64_t *hash_item_get_cas_ptr(const hash_item *item) {
  if (item->iflag & ITEM_WITH_CAS) {
    return (uint64_t *)(item + 1);
  }
  return 0;
}
