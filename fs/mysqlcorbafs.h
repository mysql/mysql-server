/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "CorbaFS.h"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"

#define QUOTE_CHAR '`'
/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2
#define EX_CONSCHECK 3
#define EX_EOM 4

#define CORBAFS_VERSION "0.01"

typedef struct
{
   POA_CorbaFS_Inode servant;
   PortableServer_POA poa;

        CORBA_char *path;
        CORBA_unsigned_long inodeNum;
#if 0
        CORBA_unsigned_short mode;
        CORBA_unsigned_long uid;
        CORBA_unsigned_long gid;
        CORBA_unsigned_long size;
        CORBA_unsigned_short numLinks;
        CORBA_long atime;
        CORBA_long mtime;
        CORBA_long ctime;
#endif
}
impl_POA_CorbaFS_Inode;

typedef struct
{
   POA_CorbaFS_FileSystem servant;
   PortableServer_POA poa;

}
impl_POA_CorbaFS_FileSystem;

/*** Implementation stub prototypes ***/
CorbaFS_FileSystem 
impl_FileSystem__create(PortableServer_POA poa, CORBA_Environment * ev);

static void 
impl_Inode__destroy(impl_POA_CorbaFS_Inode * servant,
					CORBA_Environment * ev);
static void
impl_Inode_getStatus(impl_POA_CorbaFS_Inode * servant,
   CORBA_unsigned_short * mode,
   CORBA_unsigned_long * uid,
   CORBA_unsigned_long * gid,
   CORBA_unsigned_long * size,
   CORBA_unsigned_long * inodeNum,
   CORBA_unsigned_short * numLinks,
   CORBA_long * atime,
   CORBA_long * mtime,
   CORBA_long * ctime, CORBA_Environment * ev);

static void
impl_Inode_readpage(impl_POA_CorbaFS_Inode * servant,
   CorbaFS_Buffer ** buffer,
   CORBA_long size,
   CORBA_long offset, CORBA_Environment * ev);

static void
impl_Inode_release(impl_POA_CorbaFS_Inode * servant,
   CORBA_Environment * ev);

static void impl_FileSystem__destroy(impl_POA_CorbaFS_FileSystem *
   servant, CORBA_Environment * ev);

static CorbaFS_Inode
impl_FileSystem_getInode(impl_POA_CorbaFS_FileSystem * servant,
   CORBA_char * path, CORBA_Environment * ev);

static CorbaFS_DirEntSeq *
impl_FileSystem_readdir(impl_POA_CorbaFS_FileSystem * servant,
   CORBA_char * path,
   CORBA_Environment * ev);

static CORBA_char *
impl_FileSystem_readlink(impl_POA_CorbaFS_FileSystem * servant,
   CORBA_char * filename,
   CORBA_Environment * ev);

static my_bool verbose,opt_compress;
static uint opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static int   first_error=0;
static MYSQL connection, *sock=0;

extern uint opt_mysql_port;
extern my_string opt_mysql_unix_port,host,user,password;



static struct format {
      char *tablestart;

      char *headerrowstart;
      char *headercellstart;
      char *headercellseparator;
      char *headercellend;
      char *headerrowend;
      int  headerformat; /* 0 - simple, 1 - left padded, 2 - right padded */

      char *contentrowstart;
      char *contentcellstart;
      char *contentcellseparator;
      char *contentcellend;
      char *contentrowend;
      int  contentformat;

      char *footerrowstart;
      char *footercellstart;
      char *footercellseparator;
      char *footercellend;
      char *footerrowend;
      int  footerformat;

      char *tableend;

      char *leftuppercorner;
      char *rightuppercorner;
      char *leftdowncorner;
      char *rightdowncorner;
      char *leftcross;
      char *rightcross;
      char *topcross;
      char *middlecross;
      char *bottomcross;

      
} Human, HTML, CSF, XML;


