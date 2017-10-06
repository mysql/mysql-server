/*
   Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef SHA2_PASSWORD_INCLUDED
#define SHA2_PASSWORD_INCLUDED

#include <stddef.h>

bool validate_sha256_scramble(const unsigned char *scramble,
                              size_t scramble_size,
                              const unsigned char *known, size_t known_size,
                              const unsigned char *rnd, size_t rnd_size);

#endif // SHA2_PASSWORD_INCLUDED
