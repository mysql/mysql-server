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

#ifdef OPEN_SSL
   puts("\
   --ssl                Use SSL for connection (automatically set with other flags\n\
   --ssl-key            X509 key in PEM format (implies --ssl)\n\
   --ssl-cert           X509 cert in PEM format (implies --ssl)\n\
   --ssl-ca             CA file in PEM format (check OpenSSL docs, implies --ssl)\n\
   --ssl-capath         CA directory (check OpenSSL docs, implies --ssl)");
#endif
