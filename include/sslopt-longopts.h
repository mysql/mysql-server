/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#ifdef HAVE_OPENSSL

#define OPT_SSL_SSL	200
#define OPT_SSL_KEY	201
#define OPT_SSL_CERT	202
#define OPT_SSL_CA	203
#define OPT_SSL_CAPATH  204
#define OPT_SSL_CIPHER  205
  {"ssl",           no_argument,           0, OPT_SSL_SSL},
  {"ssl-key",       required_argument,     0, OPT_SSL_KEY},
  {"ssl-cert",      required_argument,     0, OPT_SSL_CERT},
  {"ssl-ca",        required_argument,     0, OPT_SSL_CA},
  {"ssl-capath",    required_argument,     0, OPT_SSL_CAPATH},
  {"ssl-cipher",    required_argument,     0, OPT_SSL_CIPHER},

#endif /* HAVE_OPENSSL */
