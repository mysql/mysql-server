<<<<<<< HEAD:mysys/my_aes_impl.h
/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231:mysys_ssl/my_aes_impl.h

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.
<<<<<<< HEAD
=======

Without limiting anything contained in the foregoing, this file,
which is part of C Driver for MySQL (Connector/C), is also subject to the
Universal FOSS Exception, version 1.0, a copy of which can be found at
http://oss.oracle.com/licenses/universal-foss-exception.
>>>>>>> upstream/cluster-7.6

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_aes_impl.h
*/

/** Maximum supported key length */
#define MAX_AES_KEY_LENGTH 256

/* TODO: remove in a future version */
/* Guard against using an old export control restriction #define */
#ifdef AES_USE_KEY_BITS
#error AES_USE_KEY_BITS not supported
#endif

extern uint *my_aes_opmode_key_sizes;

void my_aes_create_key(const unsigned char *key, uint key_length, uint8 *rkey,
                       enum my_aes_opmode opmode);
