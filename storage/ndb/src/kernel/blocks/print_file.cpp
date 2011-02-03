/*
   Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <NdbOut.hpp>
#include <UtilBuffer.hpp>
#include "diskpage.hpp"
#include <ndb_limits.h>
#include <dbtup/tuppage.hpp>

static void print_usage(const char*);
static int print_zero_page(int, void *, Uint32 sz);
static int print_extent_page(int, void*, Uint32 sz);
static int print_undo_page(int, void*, Uint32 sz);
static int print_data_page(int, void*, Uint32 sz);
static bool print_page(int page_no) 
{ 
  return false;
}

int g_verbosity = 1;
unsigned g_page_size = File_formats::NDB_PAGE_SIZE;
int (* g_print_page)(int count, void*, Uint32 sz) = print_zero_page;

File_formats::Undofile::Zero_page g_uf_zero;
File_formats::Datafile::Zero_page g_df_zero;

int main(int argc, char ** argv)
{
  for(int i = 1; i<argc; i++){
    if(!strncmp(argv[i], "-v", 2))
    {
      int pos= 2;
      do {
	g_verbosity++;
      } while(argv[i][pos++] == 'v');
      continue;
    }
    else if(!strcmp(argv[i], "-q"))
    {
      g_verbosity--;
      continue;
    }
    else if(!strcmp(argv[i], "-?") ||
	    !strcmp(argv[i], "--?") ||
	    !strcmp(argv[i], "-h") ||
	    !strcmp(argv[i], "--help"))
    {
      print_usage(argv[0]);
      exit(0);
    }
    
    const char * filename = argv[i];
    
    struct stat sbuf;
    if(stat(filename, &sbuf) != 0)
    {
      ndbout << "Could not find file: \"" << filename << "\"" << endl;
      continue;
    }
    //const Uint32 bytes = sbuf.st_size;
    
    UtilBuffer buffer;
    
    FILE * f = fopen(filename, "rb");
    if(f == 0){
      ndbout << "Failed to open file" << endl;
      continue;
    }
    
    Uint32 sz;
    Uint32 j = 0;
    do {
      buffer.grow(g_page_size);
      sz = (Uint32)fread(buffer.get_data(), 1, g_page_size, f);
      if((* g_print_page)(j++, buffer.get_data(), sz))
	break;
    } while(sz == g_page_size);
    
    fclose(f);
    continue;
  }
  return 0;
}

void
print_usage(const char* prg)
{
  ndbout << prg << " [-v]+ [-q]+ <file>+" << endl;
}

int
print_zero_page(int i, void * ptr, Uint32 sz){
  File_formats::Zero_page_header* page = (File_formats::Zero_page_header*)ptr;
  if(memcmp(page->m_magic, "NDBDISK", 8) != 0)
  {
    ndbout << "Invalid magic: file is not ndb disk data file" << endl;
    return 1;
  }
  
  if(page->m_byte_order != 0x12345678)
  {
    ndbout << "Unhandled byteorder" << endl;
    return 1;
  }

  switch(page->m_file_type)
  {
  case File_formats::FT_Datafile:
  {
    g_df_zero = (* (File_formats::Datafile::Zero_page*)ptr);
    ndbout << "-- Datafile -- " << endl;
    ndbout << g_df_zero << endl;
    g_print_page = print_extent_page;
    return 0;
  }
  break;
  case File_formats::FT_Undofile:
  {
    g_uf_zero = (* (File_formats::Undofile::Zero_page*)ptr);
    ndbout << "-- Undofile -- " << endl;
    ndbout << g_uf_zero << endl;
    g_print_page = print_undo_page;
    return 0;
  }
  break;
  default:
    ndbout << "Unhandled file type: " << page->m_file_type << endl;
    return 1;
  }
  
  if(page->m_page_size !=g_page_size)
  {
    /**
     * Todo 
     * lseek
     * g_page_size = page->m_page_size;
     */
    ndbout << "Unhandled page size: " << page->m_page_size << endl;
    return 1;
  }
  
  return 0;
}

NdbOut&
operator<<(NdbOut& out, const File_formats::Datafile::Extent_header& obj)
{
  if(obj.m_table == RNIL)
  {
    if(obj.m_next_free_extent != RNIL)
      out << " FREE, next free: " << obj.m_next_free_extent;
    else
      out << " FREE, next free: RNIL";
  }
  else 
  {
    out << "table: " << obj.m_table 
	<< " fragment: " << obj.m_fragment_id << " ";
    for(Uint32 i = 0; i<g_df_zero.m_extent_size; i++)
    {
      char t[2];
      BaseString::snprintf(t, sizeof(t), "%x", obj.get_free_bits(i));
      out << t;
    }
  }
  return out;
}

int
print_extent_page(int count, void* ptr, Uint32 sz){
  if((unsigned)count == g_df_zero.m_extent_pages)
  {
    g_print_page = print_data_page;
  }
  Uint32 header_words = 
    File_formats::Datafile::extent_header_words(g_df_zero.m_extent_size);
  Uint32 per_page = File_formats::Datafile::EXTENT_PAGE_WORDS / header_words;
  
  int no = count * per_page;
  
  const int max = count < int(g_df_zero.m_extent_pages) ? 
    per_page : g_df_zero.m_extent_count - (g_df_zero.m_extent_count - 1) * per_page;

  File_formats::Datafile::Extent_page * page = 
    (File_formats::Datafile::Extent_page*)ptr;

  ndbout << "Extent page: " << count
	 << ", lsn = [ " 
	 << page->m_page_header.m_page_lsn_hi << " " 
	 << page->m_page_header.m_page_lsn_lo << "] " 
	 << endl;
  for(int i = 0; i<max; i++)
  {
    ndbout << "  extent " << no+i << ": "
	   << (* page->get_header(i, g_df_zero.m_extent_size)) << endl;
  }
  return 0;
}

int
print_data_page(int count, void* ptr, Uint32 sz){

  File_formats::Datafile::Data_page * page = 
    (File_formats::Datafile::Data_page*)ptr;
  
  ndbout << "Data page: " << count
	 << ", lsn = [ " 
	 << page->m_page_header.m_page_lsn_hi << " " 
	 << page->m_page_header.m_page_lsn_lo << "]" ;

  if(g_verbosity > 1 || print_page(count))
  {
    switch(page->m_page_header.m_page_type){
    case File_formats::PT_Unallocated:
      break;
    case File_formats::PT_Tup_fixsize_page:
      ndbout << " fix ";
      if(g_verbosity > 2 || print_page(count))
	ndbout << (* (Tup_fixsize_page*)page);
      break;
    case File_formats::PT_Tup_varsize_page:
      ndbout << " var ";
      if(g_verbosity > 2 || print_page(count))
	ndbout << endl << (* (Tup_varsize_page*)page);
      break;
    default:
      ndbout << " unknown page type: %d" << page->m_page_header.m_page_type;
    }
  }
  ndbout << endl;
  return 0;
}

#define DBTUP_C
#include "dbtup/Dbtup.hpp"

int
print_undo_page(int count, void* ptr, Uint32 sz){
  if(count > int(g_uf_zero.m_undo_pages + 1))
  {
    ndbout_c(" ERROR to many pages in file!!");
    return 1;
  }

  File_formats::Undofile::Undo_page * page = 
    (File_formats::Undofile::Undo_page*)ptr;
  
  if(page->m_page_header.m_page_lsn_hi != 0 || 
     page->m_page_header.m_page_lsn_lo != 0)
  {
    ndbout << "Undo page: " << count
	   << ", lsn = [ " 
	   << page->m_page_header.m_page_lsn_hi << " " 
	   << page->m_page_header.m_page_lsn_lo << "] " 
	   << "words used: " << page->m_words_used << endl;
    
    Uint64 lsn= 0;
    lsn += page->m_page_header.m_page_lsn_hi;
    lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    lsn++;

    if(g_verbosity >= 3)
    {
      Uint32 pos= page->m_words_used - 1;
      while(pos + 1 != 0)
      {
	Uint32 len= page->m_data[pos] & 0xFFFF;
	Uint32 type= page->m_data[pos] >> 16;
	const Uint32* src= page->m_data + pos - len + 1;
	Uint32 next_pos= pos - len;
	if(type & File_formats::Undofile::UNDO_NEXT_LSN)
	{
	  type &= ~(Uint32)File_formats::Undofile::UNDO_NEXT_LSN;
	  lsn--;
	}
	else
	{
	  lsn = 0;
	  lsn += * (src - 2);
	  lsn <<= 32;
	  lsn += * (src - 1);
	  next_pos -= 2;
	}
	if(g_verbosity > 3)
	  printf(" %.4d - %.4d : ", pos - len + 1, pos);
	switch(type){
	case File_formats::Undofile::UNDO_LCP_FIRST:
	case File_formats::Undofile::UNDO_LCP:
	  printf("[ %lld LCP %d tab: %d frag: %d ]", lsn, 
		 src[0], src[1] >> 16, src[1] & 0xFFFF);
	  if(g_verbosity <= 3)
	    printf("\n");
	  break;
	case File_formats::Undofile::UNDO_TUP_ALLOC:
	  if(g_verbosity > 3)
	  {
	    Dbtup::Disk_undo::Alloc *req= (Dbtup::Disk_undo::Alloc*)src;
	    printf("[ %lld A %d %d %d ]",
		   lsn,
		   req->m_file_no_page_idx >> 16,
		   req->m_file_no_page_idx & 0xFFFF,
		   req->m_page_no);
	  }
	  break;
	case File_formats::Undofile::UNDO_TUP_UPDATE:
	  if(g_verbosity > 3)
	  {
	    Dbtup::Disk_undo::Update *req= (Dbtup::Disk_undo::Update*)src;
	    printf("[ %lld U %d %d %d gci: %d ]",
		   lsn,
		   req->m_file_no_page_idx >> 16,
		   req->m_file_no_page_idx & 0xFFFF,
		   req->m_page_no,
		   req->m_gci);
	  }
	  break;
	case File_formats::Undofile::UNDO_TUP_FREE:
	  if(g_verbosity > 3)
	  {
	    Dbtup::Disk_undo::Free *req= (Dbtup::Disk_undo::Free*)src;
	    printf("[ %lld F %d %d %d gci: %d ]",
		   lsn,
		   req->m_file_no_page_idx >> 16,
		   req->m_file_no_page_idx & 0xFFFF,
		   req->m_page_no,
		   req->m_gci);
	  }
	  break;
	case File_formats::Undofile::UNDO_TUP_CREATE:
	{
	  Dbtup::Disk_undo::Create *req = (Dbtup::Disk_undo::Create*)src;
	  printf("[ %lld Create %d ]", lsn, req->m_table);
	  if(g_verbosity <= 3)
	    printf("\n");
	  break;
	}
	case File_formats::Undofile::UNDO_TUP_DROP:
	{
	  Dbtup::Disk_undo::Drop *req = (Dbtup::Disk_undo::Drop*)src;
	  printf("[ %lld Drop %d ]", lsn, req->m_table);
	  if(g_verbosity <= 3)
	    printf("\n");
	  break;
	}
	case File_formats::Undofile::UNDO_TUP_ALLOC_EXTENT:
	{
	  Dbtup::Disk_undo::AllocExtent *req = (Dbtup::Disk_undo::AllocExtent*)src;
	  printf("[ %lld AllocExtent tab: %d frag: %d file: %d page: %d ]", 
		 lsn, 
		 req->m_table,
		 req->m_fragment,
		 req->m_file_no,
		 req->m_page_no);
	  if(g_verbosity <= 3)
	    printf("\n");
	  break;
	}
	case File_formats::Undofile::UNDO_TUP_FREE_EXTENT:
	{
	  Dbtup::Disk_undo::FreeExtent *req = (Dbtup::Disk_undo::FreeExtent*)src;
	  printf("[ %lld FreeExtent tab: %d frag: %d file: %d page: %d ]", 
		 lsn, 
		 req->m_table,
		 req->m_fragment,
		 req->m_file_no,
		 req->m_page_no);
	  if(g_verbosity <= 3)
	    printf("\n");
	  break;
	}
	default:
	  ndbout_c("[ Unknown type %d len: %d, pos: %d ]", type, len, pos);
	  if(!(len && type))
	  {
	    pos= 0;
	    while(pos < page->m_words_used)
	    {
	      printf("%.8x ", page->m_data[pos]);
	      if((pos + 1) % 7 == 0)
		ndbout_c("%s", "");
	      pos++;
	    }
	  }
	  assert(len && type);
	}
	pos = next_pos;
	if(g_verbosity > 3)
	  printf("\n");
      }
    }
  }
  
  if((unsigned)count == g_uf_zero.m_undo_pages + 1)
  {
  }
  
  return 0;
}

// Dummy implementations
Signal::Signal(){}
SimulatedBlock::Callback SimulatedBlock::TheEmptyCallback = {0, 0};
