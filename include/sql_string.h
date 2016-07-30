#ifndef SQL_STRING_INCLUDED
#define SQL_STRING_INCLUDED

/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

/* This file is originally from the mysql distribution. Coded by monty */

#include "m_ctype.h"                            /* my_charset_bin */
#include "my_sys.h"              /* alloc_root, my_free, my_realloc */
#include "m_string.h"                           /* TRASH */

#ifdef MYSQL_SERVER
extern PSI_memory_key key_memory_String_value;
#define STRING_PSI_MEMORY_KEY key_memory_String_value
#else
#define STRING_PSI_MEMORY_KEY PSI_NOT_INSTRUMENTED
#endif

/**
  A wrapper class for null-terminated constant strings.
  Constructors make sure that the position of the '\0' terminating byte
  in m_str is always in sync with m_length.

  This class must stay as small as possible as we often 
  pass it and its descendants (such as Name_string) into functions
  using call-by-value evaluation.

  Don't add new members or virual methods into this class!
*/
class Simple_cstring
{
private:
  const char *m_str;
  size_t m_length;
protected:
  /**
    Initialize from a C string whose length is already known.
  */
  void set(const char *str_arg, size_t length_arg)
  {
    // NULL is allowed only with length==0
    DBUG_ASSERT(str_arg || length_arg == 0);
    // For non-NULL, make sure length_arg is in sync with '\0' terminator.
    DBUG_ASSERT(!str_arg || str_arg[length_arg] == '\0');
    m_str= str_arg;
    m_length= length_arg;
  }
public:
  Simple_cstring()
  {
    set(NULL, 0);
  }
  Simple_cstring(const char *str_arg, size_t length_arg)
  {
    set(str_arg, length_arg);
  }
  Simple_cstring(const LEX_STRING arg)
  {
    set(arg.str, arg.length);
  }
  void reset()
  {
    set(NULL, 0);
  }
  /**
    Set to a null-terminated string.
  */
  void set(const char *str)
  {
    set(str, str ? strlen(str) : 0);
  }
  /**
    Return string buffer.
  */
  const char *ptr() const { return m_str; }
  /**
    Check if m_ptr is set.
  */
  bool is_set() const { return m_str != NULL; }
  /**
    Return name length.
  */
  size_t length() const { return m_length; }
  /**
    Compare to another Simple_cstring.
  */
  bool eq_bin(const Simple_cstring other) const
  {
    return m_length == other.m_length &&
           memcmp(m_str, other.m_str, m_length) == 0;
  }
  /**
    Copy to the given buffer
  */
  void strcpy(char *buff) const
  {
    memcpy(buff, m_str, m_length);
    buff[m_length]= '\0';
  }
};


class String;
typedef struct charset_info_st CHARSET_INFO;
typedef struct st_io_cache IO_CACHE;
typedef struct st_mem_root MEM_ROOT;

int sortcmp(const String *a,const String *b, const CHARSET_INFO *cs);
String *copy_if_not_alloced(String *a, String *b, size_t arg_length);
inline size_t copy_and_convert(char *to, size_t to_length,
                               const CHARSET_INFO *to_cs,
                               const char *from, size_t from_length,
                               const CHARSET_INFO *from_cs, uint *errors)
{
  return my_convert(to, to_length, to_cs, from, from_length, from_cs, errors);
}
size_t well_formed_copy_nchars(const CHARSET_INFO *to_cs,
                               char *to, size_t to_length,
                               const CHARSET_INFO *from_cs,
                               const char *from, size_t from_length,
                               size_t nchars,
                               const char **well_formed_error_pos,
                               const char **cannot_convert_error_pos,
                               const char **from_end_pos);
size_t convert_to_printable(char *to, size_t to_len,
                            const char *from, size_t from_len,
                            const CHARSET_INFO *from_cs, size_t nbytes= 0);

size_t bin_to_hex_str(char *to, size_t to_len, char *from, size_t from_len);

class String
{
  char *m_ptr;
  size_t m_length;
  const CHARSET_INFO *m_charset;
  uint32 m_alloced_length; // should be size_t, but kept uint32 for size reasons
  bool m_is_alloced;
public:
  String()
    :m_ptr(NULL), m_length(0), m_charset(&my_charset_bin),
     m_alloced_length(0), m_is_alloced(false)
  { }
  String(size_t length_arg)
    :m_ptr(NULL), m_length(0), m_charset(&my_charset_bin),
     m_alloced_length(0), m_is_alloced(false)
  {
    (void) real_alloc(length_arg);
  }
  String(const char *str, const CHARSET_INFO *cs)
    :m_ptr(const_cast<char*>(str)), m_length(strlen(str)),
     m_charset(cs), m_alloced_length(0), m_is_alloced(false)
  { }
  String(const char *str, size_t len, const CHARSET_INFO *cs)
    :m_ptr(const_cast<char*>(str)), m_length(len),
     m_charset(cs), m_alloced_length(0), m_is_alloced(false)
  { }
  String(char *str, size_t len, const CHARSET_INFO *cs)
    :m_ptr(str), m_length(len), m_charset(cs),
     m_alloced_length(static_cast<uint32>(len)), m_is_alloced(false)
  { }
  String(const String &str)
    :m_ptr(str.m_ptr), m_length(str.m_length), m_charset(str.m_charset),
     m_alloced_length(static_cast<uint32>(str.m_alloced_length)),
     m_is_alloced(false)
  { }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr_arg, size_t size)
  {
    (void) ptr_arg;
    (void) size;
    TRASH(ptr_arg, size);
  }
  static void operator delete(void *, MEM_ROOT *)
  { /* never called */ }
  ~String() { mem_free(); }

  void set_charset(const CHARSET_INFO *charset_arg)
  { m_charset= charset_arg; }
  const CHARSET_INFO *charset() const { return m_charset; }
  size_t length() const { return m_length;}
  size_t alloced_length() const { return m_alloced_length;}
  char& operator [] (size_t i) const { return m_ptr[i]; }
  void length(size_t len) { m_length= len; }
  bool is_empty() const { return (m_length == 0); }
  void mark_as_const() { m_alloced_length= 0;}
  const char *ptr() const { return m_ptr; }
  char *c_ptr()
  {
    DBUG_ASSERT(!m_is_alloced || !m_ptr || !m_alloced_length ||
                (m_alloced_length >= (m_length + 1)));

    if (!m_ptr || m_ptr[m_length])		/* Should be safe */
      (void) mem_realloc(m_length);
    return m_ptr;
  }
  char *c_ptr_quick()
  {
    if (m_ptr && m_length < m_alloced_length)
      m_ptr[m_length]= 0;
    return m_ptr;
  }
  char *c_ptr_safe()
  {
    if (m_ptr && m_length < m_alloced_length)
      m_ptr[m_length]= 0;
    else
      (void) mem_realloc(m_length);
    return m_ptr;
  }
  LEX_STRING lex_string() const
  {
    LEX_STRING lex_string = { (char*) ptr(), length() };
    return lex_string;
  }

  LEX_CSTRING lex_cstring() const
  {
    LEX_CSTRING lex_cstring = { ptr(), length() };
    return lex_cstring;
  }

  void set(String &str,size_t offset, size_t arg_length)
  {
    DBUG_ASSERT(&str != this);
    mem_free();
    m_ptr= const_cast<char*>(str.ptr()) + offset;
    m_length= arg_length;
    m_is_alloced= false;
    if (str.m_alloced_length)
      m_alloced_length= str.m_alloced_length - static_cast<uint32>(offset);
    else
      m_alloced_length= 0;
    m_charset= str.m_charset;
  }


  /**
     Points the internal buffer to the supplied one. The old buffer is freed.
     @param str Pointer to the new buffer.
     @param arg_length Length of the new buffer in characters, excluding any 
            null character.
     @param cs Character set to use for interpreting string data.
     @note The new buffer will not be null terminated.
  */
  void set(char *str, size_t arg_length, const CHARSET_INFO *cs)
  {
    mem_free();
    m_ptr= str;
    m_length= m_alloced_length= static_cast<uint32>(arg_length);
    m_is_alloced= false;
    m_charset= cs;
  }
  void set(const char *str, size_t arg_length, const CHARSET_INFO *cs)
  {
    mem_free();
    m_ptr= const_cast<char*>(str);
    m_length= arg_length;
    m_alloced_length= 0;
    m_is_alloced= false;
    m_charset= cs;
  }
  bool set_ascii(const char *str, size_t arg_length);
  void set_quick(char *str, size_t arg_length, const CHARSET_INFO *cs)
  {
    if (!m_is_alloced)
    {
      m_ptr= str;
      m_length= arg_length;
      m_alloced_length= static_cast<uint32>(arg_length);
    }
    m_charset= cs;
  }
  bool set_int(longlong num, bool unsigned_flag, const CHARSET_INFO *cs);
  bool set(longlong num, const CHARSET_INFO *cs)
  { return set_int(num, false, cs); }
  bool set(ulonglong num, const CHARSET_INFO *cs)
  { return set_int((longlong)num, true, cs); }
  bool set_real(double num,uint decimals, const CHARSET_INFO *cs);

  /*
    PMG 2004.11.12
    This is a method that works the same as perl's "chop". It simply
    drops the last character of a string. This is useful in the case
    of the federated storage handler where I'm building a unknown
    number, list of values and fields to be used in a sql insert
    statement to be run on the remote server, and have a comma after each.
    When the list is complete, I "chop" off the trailing comma

    ex. 
      String stringobj; 
      stringobj.append("VALUES ('foo', 'fi', 'fo',");
      stringobj.chop();
      stringobj.append(")");

    In this case, the value of string was:

    VALUES ('foo', 'fi', 'fo',
    VALUES ('foo', 'fi', 'fo'
    VALUES ('foo', 'fi', 'fo')
      
  */
  void chop()
  {
    m_length--;
    m_ptr[m_length]= '\0';
    DBUG_ASSERT(strlen(m_ptr) == m_length);
  }

  void mem_claim()
  {
    if (m_is_alloced)
    {
      my_claim(m_ptr);
    }
  }

  void mem_free()
  {
    if (m_is_alloced)
    {
      m_is_alloced= false;
      m_alloced_length= 0;
      my_free(m_ptr);
      m_ptr= NULL;
      m_length= 0;				/* Safety */
    }
  }

  bool alloc(size_t arg_length)
  {
    if (arg_length < m_alloced_length)
      return false;
    return real_alloc(arg_length);
  }
  bool real_alloc(size_t arg_length);			// Empties old string
  bool mem_realloc(size_t arg_length, bool force_on_heap= false);

private:
  size_t next_realloc_exp_size(size_t sz);
  bool mem_realloc_exp(size_t arg_length);

public:
  // Shrink the buffer, but only if it is allocated on the heap.
  void shrink(size_t arg_length)
  {
    if (!is_alloced())
      return;
    if (arg_length < m_alloced_length)
    {
      char *new_ptr;
      if (!(new_ptr= static_cast<char*>(my_realloc(STRING_PSI_MEMORY_KEY,
                                                   m_ptr, arg_length, MYF(0)))))
      {
        m_alloced_length= 0;
        real_alloc(arg_length);
      }
      else
      {
        m_ptr= new_ptr;
        m_alloced_length= static_cast<uint32>(arg_length);
      }
    }
  }
  bool is_alloced() const { return m_is_alloced; }
  String& operator = (const String &s)
  {
    if (&s != this)
    {
      /*
        It is forbidden to do assignments like
        some_string = substring_of_that_string
       */
      DBUG_ASSERT(!s.uses_buffer_owned_by(this));
      mem_free();
      m_ptr= s.m_ptr;
      m_length= s.m_length;
      m_alloced_length= s.m_alloced_length;
      m_charset= s.m_charset;
      m_is_alloced= false;
    }
    return *this;
  }
  /**
    Takeover the buffer owned by another string.
    "this" becames the owner of the buffer and
    is further responsible to free it.
    The string "s" is detouched from the buffer (cleared).

    @param s - a String object to steal buffer from.
  */
  void takeover(String &s)
  {
    DBUG_ASSERT(this != &s);
    // Make sure buffers of the two Strings do not overlap
    DBUG_ASSERT(!s.uses_buffer_owned_by(this));
    mem_free();
    m_ptr= s.m_ptr;
    m_length= s.m_length;
    m_alloced_length= s.m_alloced_length;
    m_is_alloced= s.m_is_alloced;
    m_charset= s.m_charset;
    s.m_ptr= NULL;
    s.m_alloced_length= 0;
    s.m_length= 0;
    s.m_is_alloced= false;
  }

  bool copy();					// Alloc string if not alloced
  bool copy(const String &s);			// Allocate new string
  // Allocate new string
  bool copy(const char *s, size_t arg_length, const CHARSET_INFO *cs);
  static bool needs_conversion(size_t arg_length,
  			       const CHARSET_INFO *cs_from, const CHARSET_INFO *cs_to,
			       size_t *offset);
  static bool needs_conversion_on_storage(size_t arg_length,
                                          const CHARSET_INFO *cs_from,
                                          const CHARSET_INFO *cs_to);
  bool copy_aligned(const char *s, size_t arg_length, size_t offset,
		    const CHARSET_INFO *cs);
  bool set_or_copy_aligned(const char *s, size_t arg_length,
                           const CHARSET_INFO *cs);
  bool copy(const char*s, size_t arg_length, const CHARSET_INFO *csfrom,
	    const CHARSET_INFO *csto, uint *errors);
  bool append(const String &s);
  bool append(const char *s);
  bool append(LEX_STRING *ls)
  {
    return append(ls->str, ls->length);
  }
  bool append(Simple_cstring str)
  {
    return append(str.ptr(), str.length());
  }
  bool append(const char *s, size_t arg_length);
  bool append(const char *s, size_t arg_length, const CHARSET_INFO *cs);
  bool append_ulonglong(ulonglong val);
  bool append_longlong(longlong val);
  bool append(IO_CACHE* file, size_t arg_length);
  bool append_with_prefill(const char *s, size_t arg_length, 
			   size_t full_length, char fill_char);
  bool append_parenthesized(long nr, int radix= 10);
  int strstr(const String &search,size_t offset=0); // Returns offset to substring or -1
  int strrstr(const String &search,size_t offset=0); // Returns offset to substring or -1
  /**
   * Returns substring of given characters lenght, starting at given character offset.
   * Note that parameter indexes are character indexes and not byte indexes.
   */
  String substr(int offset, int count);

  bool replace(size_t offset, size_t arg_length,const char *to, size_t length);
  bool replace(size_t offset, size_t arg_length,const String &to);
  bool append(char chr)
  {
    if (m_length < m_alloced_length)
    {
      m_ptr[m_length++]= chr;
    }
    else
    {
      if (mem_realloc_exp(m_length + 1))
	return 1;
      m_ptr[m_length++]= chr;
    }
    return 0;
  }
  bool fill(size_t max_length,char fill);
  void strip_sp();
  friend int sortcmp(const String *a,const String *b, const CHARSET_INFO *cs);
  friend int stringcmp(const String *a,const String *b);
  friend String *copy_if_not_alloced(String *a,String *b, size_t arg_length);
  size_t numchars() const;
  size_t charpos(size_t i, size_t offset=0);

  int reserve(size_t space_needed)
  {
    return mem_realloc(m_length + space_needed);
  }
  int reserve(size_t space_needed, size_t grow_by);
  /*
    The following append operations do NOT check alloced memory
    q_*** methods writes values of parameters itself
    qs_*** methods writes string representation of value
  */
  void q_append(const char c)
  {
    m_ptr[m_length++] = c;
  }
  void q_append(const uint32 n)
  {
    int4store(m_ptr + m_length, n);
    m_length += 4;
  }
  void q_append(double d)
  {
    float8store(m_ptr + m_length, d);
    m_length += 8;
  }
  void q_append(double *d)
  {
    float8store(m_ptr + m_length, *d);
    m_length += 8;
  }
  void q_append(const char *data, size_t data_len)
  {
    memcpy(m_ptr + m_length, data, data_len);
    m_length += data_len;
  }

  void write_at_position(int position, uint32 value)
  {
    int4store(m_ptr + position,value);
  }

  void qs_append(const char *str, size_t len);
  void qs_append(double d, size_t len);
  void qs_append(const char c)
  {
     m_ptr[m_length]= c;
     m_length++;
  }
  void qs_append(int i);
  void qs_append(uint i);

  /* Inline (general) functions used by the protocol functions */

  char *prep_append(size_t arg_length, size_t step_alloc)
  {
    size_t new_length= arg_length + m_length;
    if (new_length > m_alloced_length)
    {
      if (mem_realloc(new_length + step_alloc))
        return NULL;
    }
    size_t old_length= m_length;
    m_length+= arg_length;
    return m_ptr+ old_length;			/* Area to use */
  }

  bool append(const char *s, size_t arg_length, size_t step_alloc)
  {
    size_t new_length= arg_length + m_length;
    if (new_length > m_alloced_length && mem_realloc_exp(new_length + step_alloc))
      return true;
    memcpy(m_ptr+m_length, s, arg_length);
    m_length+= arg_length;
    return false;
  }
  void print(String *print);

  /* Swap two string objects. Efficient way to exchange data without memcpy. */
  void swap(String &s);

  bool uses_buffer_owned_by(const String *s) const
  {
    return (s->m_is_alloced && m_ptr >= s->m_ptr && m_ptr < s->m_ptr + s->m_length);
  }
  bool is_ascii() const
  {
    if (length() == 0)
      return true;
    if (charset()->mbminlen > 1)
      return false;
    for (const char *c= ptr(), *end= c + length(); c < end; c++)
    {
      if (!my_isascii(*c))
        return false;
    }
    return true;
  }
  /**
    Make a zero-terminated copy of our value,allocated in the specified MEM_ROOT

    @param root         MEM_ROOT to allocate the result

    @return allocated string or NULL
  */
  char *dup(MEM_ROOT *root) const
  {
    if (m_length > 0 && m_ptr[m_length - 1] == 0)
      return static_cast<char *>(memdup_root(root, m_ptr, m_length));

    char *ret= static_cast<char*>(alloc_root(root, m_length + 1));
    if (ret != NULL)
    {
      memcpy(ret, m_ptr, m_length);
      ret[m_length]= 0;
    }
    return ret;
  }
};


/**
  String class wrapper with a preallocated buffer of size buff_sz

  This class allows to replace sequences of:
     char buff[12345];
     String str(buff, sizeof(buff));
     str.length(0);
  with a simple equivalent declaration:
     StringBuffer<12345> str;
*/

template<size_t buff_sz>
class StringBuffer : public String
{
  char buff[buff_sz];

public:
  StringBuffer() : String(buff, buff_sz, &my_charset_bin) { length(0); }
  explicit StringBuffer(const CHARSET_INFO *cs) : String(buff, buff_sz, cs)
  {
    length(0);
  }
  StringBuffer(const char *str, size_t length, const CHARSET_INFO *cs)
    : String(buff, buff_sz, cs)
  {
    set(str, length, cs);
  }
};


static inline bool check_if_only_end_space(const CHARSET_INFO *cs, char *str, 
                                           char *end)
{
  return str+ cs->cset->scan(cs, str, end, MY_SEQ_SPACES) == end;
}


inline LEX_CSTRING to_lex_cstring(const LEX_STRING &s)
{
  LEX_CSTRING cstr= { s.str, s.length };
  return cstr;
}


inline LEX_STRING to_lex_string(const LEX_CSTRING &s)
{
  LEX_STRING str= { const_cast<char *>(s.str),  s.length };
  return str;
}

inline LEX_CSTRING to_lex_cstring(const char *s)
{
  LEX_CSTRING cstr= { s, s != NULL ? strlen(s) : 0 };
  return cstr;
}

bool
validate_string(const CHARSET_INFO *cs, const char *str, uint32 length,
                size_t *valid_length, bool *length_error);
#endif /* SQL_STRING_INCLUDED */
