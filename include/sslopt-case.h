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
    case OPT_SSL_SSL:
      opt_use_ssl = 1;				//true
      break;
    case OPT_SSL_KEY:
      opt_use_ssl = 1;				//true
      my_free(opt_ssl_key, MYF(MY_ALLOW_ZERO_PTR));
      opt_ssl_key = my_strdup(optarg, MYF(0));
      break;
    case OPT_SSL_CERT:
      opt_use_ssl = 1;				//true
      my_free(opt_ssl_cert, MYF(MY_ALLOW_ZERO_PTR));
      opt_ssl_cert = my_strdup(optarg, MYF(0));
      break;
    case OPT_SSL_CA:
      opt_use_ssl = 1;				//true
      my_free(opt_ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
      opt_ssl_ca = my_strdup(optarg, MYF(0));
      break;
    case OPT_SSL_CAPATH:
      opt_use_ssl = 1;				//true
      my_free(opt_ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
      opt_ssl_ca = my_strdup(optarg, MYF(0));
      break;
#endif
