/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef __BASE64_HPP_INCLUDED__
#define __BASE64_HPP_INCLUDED__

#include <UtilBuffer.hpp>
#include <BaseString.hpp>

int base64_encode(const UtilBuffer &src, BaseString &dst);
int base64_encode(const void * s, size_t src_len, BaseString &dst);
int base64_decode(const BaseString &src, UtilBuffer &dst);
int base64_decode(const char * s, size_t len, UtilBuffer &dst);

#endif /* !__BASE64_HPP_INCLUDED__ */
