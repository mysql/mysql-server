/* Copyright (C) 2000 MySQL AB

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

/* This file is originally from the mysql distribution. Coded by monty */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#ifndef NOT_FIXED_DEC
#define NOT_FIXED_DEC			31
#endif

class String;
int sortcmp(const String *a,const String *b, CHARSET_INFO *cs);
String *copy_if_not_alloced(String *a,String *b,uint32 arg_length);
uint32 copy_and_convert(char *to, uint32 to_length, CHARSET_INFO *to_cs,
			const char *from, uint32 from_length,
			CHARSET_INFO *from_cs, uint *errors);

class String
{
  char *Ptr;
  uint32 str_length,Alloced_length;
  bool alloced;
  CHARSET_INFO *str_charset;
public:
  String()
  { 
    Ptr=0; str_length=Alloced_length=0; alloced=0; 
    str_charset= &my_charset_bin; 
  }
  String(uint32 length_arg)
  { 
    alloced=0; Alloced_length=0; (void) real_alloc(length_arg); 
    str_charset= &my_charset_bin;
  }
  String(const char *str, CHARSET_INFO *cs)
  { 
    Ptr=(char*) str; str_length=(uint) strlen(str); Alloced_length=0; alloced=0;
    str_charset=cs;
  }
  String(const char *str,uint32 len, CHARSET_INFO *cs)
  { 
    Ptr=(char*) str; str_length=len; Alloced_length=0; alloced=0;
    str_charset=cs;
  }
  String(char *str,uint32 len, CHARSET_INFO *cs)
  { 
    Ptr=(char*) str; Alloced_length=str_length=len; alloced=0;
    str_charset=cs;
  }
  String(const String &str)
  { 
    Ptr=str.Ptr ; str_length=str.str_length ;
    Alloced_length=str.Alloced_length; alloced=0; 
    str_charset=str.str_charset;
  }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr_arg,size_t size)
    {}
  static void operator delete(void *ptr_arg,size_t size, MEM_ROOT *mem_root)
    {}
  ~String() { free(); }

  inline void set_charset(CHARSET_INFO *charset) { str_charset= charset; }
  inline CHARSET_INFO *charset() const { return str_charset; }
  inline uint32 length() const { return str_length;}
  inline uint32 alloced_length() const { return Alloced_length;}
  inline char& operator [] (uint32 i) const { return Ptr[i]; }
  inline void length(uint32 len) { str_length=len ; }
  inline bool is_empty() { return (str_length == 0); }
  inline const char *ptr() const { return Ptr; }
  inline char *c_ptr()
  {
    if (!Ptr || Ptr[str_length])		/* Should be safe */
      (void) realloc(str_length);
    return Ptr;
  }
  inline char *c_ptr_quick()
  {
    if (Ptr && str_length < Alloced_length)
      Ptr[str_length]=0;
    return Ptr;
  }
  inline char *c_ptr_safe()
  {
    if (Ptr && str_length < Alloced_length)
      Ptr[str_length]=0;
    else
      (void) realloc(str_length);
    return Ptr;
  }

  void set(String &str,uint32 offset,uint32 arg_length)
  {
    DBUG_ASSERT(&str != this);
    free();
    Ptr=(char*) str.ptr()+offset; str_length=arg_length; alloced=0;
    if (str.Alloced_length)
      Alloced_length=str.Alloced_length-offset;
    else
      Alloced_length=0;
    str_charset=str.str_charset;
  }
  inline void set(char *str,uint32 arg_length, CHARSET_INFO *cs)
  {
    free();
    Ptr=(char*) str; str_length=Alloced_length=arg_length ; alloced=0;
    str_charset=cs;
  }
  inline void set(const char *str,uint32 arg_length, CHARSET_INFO *cs)
  {
    free();
    Ptr=(char*) str; str_length=arg_length; Alloced_length=0 ; alloced=0;
    str_charset=cs;
  }
  bool set_ascii(const char *str, uint32 arg_length);
  inline void set_quick(char *str,uint32 arg_length, CHARSET_INFO *cs)
  {
    if (!alloced)
    {
      Ptr=(char*) str; str_length=Alloced_length=arg_length;
    }
    str_charset=cs;
  }
  bool set(longlong num, CHARSET_INFO *cs);
  bool set(ulonglong num, CHARSET_INFO *cs);
  bool set(double num,uint decimals, CHARSET_INFO *cs);
  inline void free()
  {
    if (alloced)
    {
      alloced=0;
      Alloced_length=0;
      my_free(Ptr,MYF(0));
      Ptr=0;
      str_length=0;				/* Safety */
    }
  }
  inline bool alloc(uint32 arg_length)
  {
    if (arg_length < Alloced_length)
      return 0;
    return real_alloc(arg_length);
  }
  bool real_alloc(uint32 arg_length);			// Empties old string
  bool realloc(uint32 arg_length);
  inline void shrink(uint32 arg_length)		// Shrink buffer
  {
    if (arg_length < Alloced_length)
    {
      char *new_ptr;
      if (!(new_ptr=(char*) my_realloc(Ptr,arg_length,MYF(0))))
      {
	Alloced_length = 0;
	real_alloc(arg_length);
      }
      else
      {
	Ptr=new_ptr;
	Alloced_length=arg_length;
      }
    }
  }
  bool is_alloced() { return alloced; }
  inline String& operator = (const String &s)
  {
    if (&s != this)
    {
      free();
      Ptr=s.Ptr ; str_length=s.str_length ; Alloced_length=s.Alloced_length;
      alloced=0;
    }
    return *this;
  }

  bool copy();					// Alloc string if not alloced
  bool copy(const String &s);			// Allocate new string
  bool copy(const char *s,uint32 arg_length, CHARSET_INFO *cs);	// Allocate new string
  static bool needs_conversion(uint32 arg_length,
  			       CHARSET_INFO *cs_from, CHARSET_INFO *cs_to,
			       uint32 *offset);
  bool copy_aligned(const char *s, uint32 arg_length, uint32 offset,
		    CHARSET_INFO *cs);
  bool set_or_copy_aligned(const char *s, uint32 arg_length, CHARSET_INFO *cs);
  bool copy(const char*s,uint32 arg_length, CHARSET_INFO *csfrom,
	    CHARSET_INFO *csto, uint *errors);
  bool append(const String &s);
  bool append(const char *s);
  bool append(const char *s,uint32 arg_length);
  bool append(const char *s,uint32 arg_length, CHARSET_INFO *cs);
  bool append(IO_CACHE* file, uint32 arg_length);
  bool append_with_prefill(const char *s, uint32 arg_length, 
			   uint32 full_length, char fill_char);
  int strstr(const String &search,uint32 offset=0); // Returns offset to substring or -1
  int strrstr(const String &search,uint32 offset=0); // Returns offset to substring or -1
  bool replace(uint32 offset,uint32 arg_length,const char *to,uint32 length);
  bool replace(uint32 offset,uint32 arg_length,const String &to);
  inline bool append(char chr)
  {
    if (str_length < Alloced_length)
    {
      Ptr[str_length++]=chr;
    }
    else
    {
      if (realloc(str_length+1))
	return 1;
      Ptr[str_length++]=chr;
    }
    return 0;
  }
  bool fill(uint32 max_length,char fill);
  void strip_sp();
  inline void caseup() { my_caseup(str_charset,Ptr,str_length); }
  inline void casedn() { my_casedn(str_charset,Ptr,str_length); }
  friend int sortcmp(const String *a,const String *b, CHARSET_INFO *cs);
  friend int stringcmp(const String *a,const String *b);
  friend String *copy_if_not_alloced(String *a,String *b,uint32 arg_length);
  uint32 numchars();
  int charpos(int i,uint32 offset=0);

  int reserve(uint32 space_needed)
  {
    return realloc(str_length + space_needed);
  }
  int reserve(uint32 space_needed, uint32 grow_by);

  /*
    The following append operations do NOT check alloced memory
    q_*** methods writes values of parameters itself
    qs_*** methods writes string representation of value
  */
  void q_append(const char c)
  {
    Ptr[str_length++] = c;
  }
  void q_append(const uint32 n)
  {
    int4store(Ptr + str_length, n);
    str_length += 4;
  }
  void q_append(double d)
  {
    float8store(Ptr + str_length, d);
    str_length += 8;
  }
  void q_append(double *d)
  {
    float8store(Ptr + str_length, *d);
    str_length += 8;
  }
  void q_append(const char *data, uint32 data_len)
  {
    memcpy(Ptr + str_length, data, data_len);
    str_length += data_len;
  }

  void write_at_position(int position, uint32 value)
  {
    int4store(Ptr + position,value);
  }

  void qs_append(const char *str, uint32 len);
  void qs_append(double d);
  void qs_append(double *d);
  inline void qs_append(const char c)
  {
     Ptr[str_length]= c;
     str_length++;
  }

  /* Inline (general) functions used by the protocol functions */

  inline char *prep_append(uint32 arg_length, uint32 step_alloc)
  {
    uint32 new_length= arg_length + str_length;
    if (new_length > Alloced_length)
    {
      if (realloc(new_length + step_alloc))
        return 0;
    }
    uint32 old_length= str_length;
    str_length+= arg_length;
    return Ptr+ old_length;			/* Area to use */
  }

  inline bool append(const char *s, uint32 arg_length, uint32 step_alloc)
  {
    uint32 new_length= arg_length + str_length;
    if (new_length > Alloced_length && realloc(new_length + step_alloc))
      return TRUE;
    memcpy(Ptr+str_length, s, arg_length);
    str_length+= arg_length;
    return FALSE;
  }
  void print(String *print);

  /* Swap two string objects. Efficient way to exchange data without memcpy. */
  void swap(String &s);
};
