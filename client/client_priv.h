/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* Common defines for all clients */

#include <my_global.h>
#include <my_sys.h> 
#include <m_string.h>
#include <mysql.h>
#include <errmsg.h>
#include <getopt.h>

/* We have to define 'enum options' identical in all files to keep OS2 happy */

enum options { OPT_CHARSETS_DIR=256, OPT_DEFAULT_CHARSET,
	       OPT_PAGER, OPT_NOPAGER, OPT_TEE, OPT_NOTEE,
	       OPT_LOW_PRIORITY, OPT_AUTO_REPAIR, OPT_COMPRESS,
	       OPT_FTB, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC, OPT_TABLES};
