/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef INCLUDE_MYSQL_STRINGS_M_CTYPE_H_
#define INCLUDE_MYSQL_STRINGS_M_CTYPE_H_

/**
  @file include/mysql/strings/m_ctype.h
  A better implementation of the UNIX ctype(3) library.
*/

#include <sys/types.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>

#include "mysql/attribute.h"
#include "mysql/my_loglevel.h"
#include "mysql/strings/api.h"
#include "template_utils.h"

constexpr int MY_CS_NAME_SIZE = 32;

constexpr const char *CHARSET_DIR = "charsets/";

typedef int myf; /* Type of MyFlags in my_funcs */

/**
  Our own version of wchar_t, ie., a type that holds a single Unicode code point
  ("wide character"). unsigned long is always big enough to hold any character
  in the BMP.
*/
typedef unsigned long my_wc_t;

static inline void MY_PUT_MB2(unsigned char *s, uint16_t code) {
  s[0] = code >> 8;
  s[1] = code & 0xFF;
}

struct MY_UNICASE_CHARACTER {
  uint32_t toupper;
  uint32_t tolower;
  uint32_t sort;
};

struct MY_UNICASE_INFO {
  my_wc_t maxchar;
  const MY_UNICASE_CHARACTER **page;
};

struct MY_UCA_INFO;

struct MY_UNI_CTYPE {
  uint8_t pctype;
  uint8_t *ctype;
};

/* wm_wc and wc_mb return codes */
/* clang-format off */
static constexpr int
       MY_CS_ILSEQ = 0;        /* Wrong by sequence: wb_wc                   */
static constexpr int
       MY_CS_ILUNI = 0;        /* Cannot encode Unicode to charset: wc_mb    */
static constexpr int
       MY_CS_TOOSMALL  = -101; /* Need at least one byte:    wc_mb and mb_wc */
static constexpr int
       MY_CS_TOOSMALL2 = -102; /* Need at least two bytes:   wc_mb and mb_wc */
static constexpr int
       MY_CS_TOOSMALL3 = -103; /* Need at least three bytes: wc_mb and mb_wc */

/* These following three are currently not really used */
static constexpr int
       MY_CS_TOOSMALL4 = -104; /* Need at least 4 bytes: wc_mb and mb_wc */
static constexpr int
       MY_CS_TOOSMALL5 = -105; /* Need at least 5 bytes: wc_mb and mb_wc */
static constexpr int
       MY_CS_TOOSMALL6 = -106; /* Need at least 6 bytes: wc_mb and mb_wc */
/* clang-format on */

static constexpr int MY_SEQ_INTTAIL = 1;
static constexpr int MY_SEQ_SPACES = 2;

/* CHARSET_INFO::state flags */
/* clang-format off */
static constexpr uint32_t
       MY_CHARSET_UNDEFINED     = 0;       // for unit testing
static constexpr uint32_t
       MY_CS_COMPILED           = 1 << 0;  // compiled-in charsets
static constexpr uint32_t
       MY_CS_CONFIG_UNUSED      = 1 << 1;  // unused bitmask
static constexpr uint32_t
       MY_CS_INDEX_UNUSED       = 1 << 2;  // unused bitmask
static constexpr uint32_t
       MY_CS_LOADED             = 1 << 3;  // charsets that are currently loaded
static constexpr uint32_t
       MY_CS_BINSORT            = 1 << 4;  // if binary sort order
static constexpr uint32_t
       MY_CS_PRIMARY            = 1 << 5;  // if primary collation
static constexpr uint32_t
       MY_CS_STRNXFRM           = 1 << 6;  // if _not_ set, sort_order will
                                           // give same result as strnxfrm --
                                           // all new collations should have
                                           // this flag set,
                                           // do not check it in new code
static constexpr uint32_t
       MY_CS_UNICODE            = 1 << 7;  // if a charset is BMP Unicode
static constexpr uint32_t
       MY_CS_READY              = 1 << 8;  // if a charset is initialized
static constexpr uint32_t
       MY_CS_AVAILABLE          = 1 << 9;  // if either compiled-in or loaded
static constexpr uint32_t
       MY_CS_CSSORT             = 1 << 10; // if case sensitive sort order
static constexpr uint32_t
       MY_CS_HIDDEN             = 1 << 11; // don't display in SHOW
static constexpr uint32_t
       MY_CS_PUREASCII          = 1 << 12; // if a charset is pure ascii
static constexpr uint32_t
       MY_CS_NONASCII           = 1 << 13; // if not ASCII-compatible
static constexpr uint32_t
       MY_CS_UNICODE_SUPPLEMENT = 1 << 14; // Non-BMP Unicode characters
static constexpr uint32_t
       MY_CS_LOWER_SORT         = 1 << 15; // if use lower case as weight
static constexpr uint32_t
       MY_CS_INLINE             = 1 << 16; // CS definition is C++ source

/* Character repertoire flags */
static constexpr uint32_t
       MY_REPERTOIRE_ASCII = 1;     /* Pure ASCII            U+0000..U+007F */
static constexpr uint32_t
       MY_REPERTOIRE_EXTENDED = 2;  /* Extended characters:  U+0080..U+FFFF */
static constexpr uint32_t
       MY_REPERTOIRE_UNICODE30 = 3; /* ASCII | EXTENDED:     U+0000..U+FFFF */

/* Flags for strxfrm */
static constexpr uint32_t
       MY_STRXFRM_PAD_TO_MAXLEN = 0x00000080; /* if pad tail(for filesort) */

/* clang-format on */

struct MY_UNI_IDX {
  uint16_t from;
  uint16_t to;
  const uint8_t *tab;
};

struct my_match_t {
  unsigned beg;
  unsigned end;
  unsigned mb_len;
};

struct CHARSET_INFO;

/**
  Helper structure to return error messages from collation parser/initializer.
*/
struct MY_CHARSET_ERRMSG {
  static constexpr int errmsg_size = 192;
  unsigned errcode{0};         ///< See include/mysys_err.h
  char errarg[errmsg_size]{};  ///< Error message text
};

/**
  User-specified callback interface for collation parser/initializer
*/
class MY_CHARSET_LOADER {
 public:
  MY_CHARSET_LOADER() = default;
  virtual ~MY_CHARSET_LOADER();

  MY_CHARSET_LOADER(const MY_CHARSET_LOADER &) = delete;
  MY_CHARSET_LOADER(const MY_CHARSET_LOADER &&) = delete;

  MY_CHARSET_LOADER &operator=(const MY_CHARSET_LOADER &) = delete;
  MY_CHARSET_LOADER &operator=(const MY_CHARSET_LOADER &&) = delete;

  /**
    Intercepts error messages from collation parser/initializer

    @param loglevel     ERROR_LEVEL or WARNING_LEVEL
    @param errcode      See include/mysys_err.h
  */
  virtual void reporter(enum loglevel loglevel, unsigned errcode, ...) = 0;

  /**
    Loads a file by its OS path into collation parser/initializer

    @param path         '\0'-terminated file path to load
    @param size         Byte size of @p path

    @returns Pointer to file data on success, otherwise nullptr.
             This is a caller's responsibility to free this pointer
             with free().
  */
  virtual void *read_file(const char *path, size_t *size) = 0;

  /**
    Collation parser helper function (not overloadable).

    @param cs   New collation object to register in the collation library

    @return MY_XML_OK on success, otherwise MY_XML_ERROR
  */
  int add_collation(CHARSET_INFO *cs);

  /**
    Allocate-and-forget version of malloc().
  */
  virtual void *once_alloc(size_t);

  virtual void *mem_malloc(size_t size) { return malloc(size); }
  virtual void mem_free(void *ptr) { free(ptr); }

 private:
  std::deque<void *> m_delete_list;
};

extern MYSQL_STRINGS_EXPORT int (*my_string_stack_guard)(int);

enum Pad_attribute { PAD_SPACE, NO_PAD };

/* See strings/CHARSET_INFO.txt for information about this structure  */
struct MY_COLLATION_HANDLER {
  bool (*init)(CHARSET_INFO *, MY_CHARSET_LOADER *, MY_CHARSET_ERRMSG *);
  void (*uninit)(CHARSET_INFO *, MY_CHARSET_LOADER *);
  /* Collation routines */
  int (*strnncoll)(const CHARSET_INFO *, const uint8_t *, size_t,
                   const uint8_t *, size_t, bool);
  /**
    Compare the two strings under the pad rules given by the collation.

    Thus, for NO PAD collations, this is identical to strnncoll with is_prefix
    set to false. For PAD SPACE collations, the two strings are conceptually
    extended infinitely at the end using space characters (0x20) and then
    compared under the collation's normal comparison rules, so that e.g 'a' is
    equal to 'a '.
  */
  int (*strnncollsp)(const CHARSET_INFO *, const uint8_t *, size_t,
                     const uint8_t *, size_t);
  /**
    Transform the string into a form such that memcmp() between transformed
    strings yields the correct collation order.

    @param [out] dst Buffer for the transformed string.
    @param [out] dstlen Number of bytes available in dstlen.
      Must be even.
    @param num_codepoints Treat the string as if it were of type
      CHAR(num_codepoints). In particular, this means that if the
      collation is a pad collation (pad_attribute is PAD_SPACE) and
      string has fewer than "num_codepoints" codepoints, the string
      will be transformed as if it ended in (num_codepoints-n) extra spaces.
      If the string has more than "num_codepoints" codepoints,
      behavior is undefined; may truncate, may crash, or do something
      else entirely. Note that MY_STRXFRM_PAD_TO_MAXLEN overrides this;
      if it is given for a PAD SPACE collation, this value is taken to be
      effectively infinity.
    @param src The source string, in the required character set
      for the collation.
    @param srclen Number of bytes in src.
    @param flags ORed bitmask of MY_STRXFRM_* flags.

    @return Number of bytes written to dst.
  */
  size_t (*strnxfrm)(const CHARSET_INFO *, uint8_t *dst, size_t dstlen,
                     unsigned num_codepoints, const uint8_t *src, size_t srclen,
                     unsigned flags);

  /**
    Return the maximum number of output bytes needed for strnxfrm()
    to output all weights for any string of the given input length.
    You can use this to e.g. size buffers for sort keys.

    @param num_bytes Number of bytes in the input string. Note that for
      multibyte character sets, this _must_ be a pessimistic estimate,
      ie., one that's cs->mbmaxlen * max_num_codepoints. So for e.g.
      the utf8mb4 string "foo", you will need to give in 12, not 3.
  */
  size_t (*strnxfrmlen)(const CHARSET_INFO *, size_t num_bytes);
  bool (*like_range)(const CHARSET_INFO *, const char *s, size_t s_length,
                     char w_prefix, char w_one, char w_many, size_t res_length,
                     char *min_str, char *max_str, size_t *min_len,
                     size_t *max_len);
  int (*wildcmp)(const CHARSET_INFO *, const char *str, const char *str_end,
                 const char *wildstr, const char *wildend, int escape,
                 int w_one, int w_many);

  int (*strcasecmp)(const CHARSET_INFO *, const char *, const char *);

  unsigned (*strstr)(const CHARSET_INFO *, const char *b, size_t b_length,
                     const char *s, size_t s_length, my_match_t *match,
                     unsigned nmatch);

  /**
    Compute a sort hash for the given key. This hash must preserve equality
    under the given collation, so that a=b => H(a)=H(b). Note that this hash
    is used for hash-based partitioning (PARTITION KEY), so you cannot change
    it except when writing a new collation; it needs to be unchanged across
    releases, so that the on-disk format does not change. (It is also used
    for testing equality in the MEMORY storage engine.)

    nr1 and nr2 are both in/out parameters. nr1 is the actual hash value;
    nr2 holds extra state between invocations.
  */
  void (*hash_sort)(const CHARSET_INFO *cs, const uint8_t *key, size_t len,
                    uint64_t *nr1, uint64_t *nr2);
  bool (*propagate)(const CHARSET_INFO *cs, const uint8_t *str, size_t len);
};

/* Some typedef to make it easy for C++ to make function pointers */
typedef int (*my_charset_conv_mb_wc)(const CHARSET_INFO *, my_wc_t *,
                                     const uint8_t *, const uint8_t *);
typedef int (*my_charset_conv_wc_mb)(const CHARSET_INFO *, my_wc_t, uint8_t *,
                                     uint8_t *);
typedef size_t (*my_charset_conv_case)(const CHARSET_INFO *, char *, size_t,
                                       char *, size_t);

/* See strings/CHARSET_INFO.txt about information on this structure  */
struct MY_CHARSET_HANDLER {
  bool (*init)(CHARSET_INFO *, MY_CHARSET_LOADER *loader, MY_CHARSET_ERRMSG *);
  /* Multibyte routines */
  unsigned (*ismbchar)(const CHARSET_INFO *, const char *, const char *);
  unsigned (*mbcharlen)(const CHARSET_INFO *, unsigned c);
  size_t (*numchars)(const CHARSET_INFO *, const char *b, const char *e);

  /**
    Return at which byte codepoint number "pos" begins, relative to
    the start of the string. If the string is shorter than or is
    exactly "pos" codepoints long, returns a value equal or greater to
    (e-b).
  */
  size_t (*charpos)(const CHARSET_INFO *, const char *b, const char *e,
                    size_t pos);
  size_t (*well_formed_len)(const CHARSET_INFO *, const char *b, const char *e,
                            size_t nchars, int *error);
  /**
    Given a pointer and a length in bytes, returns a new length in bytes where
    all trailing space characters are stripped. This holds even for NO PAD
    collations.

    Exception: The "binary" collation, which is used behind-the-scenes to
    implement the BINARY type (by mapping it to CHAR(n) COLLATE "binary"),
    returns just the length back with no stripping. It's done that way so that
    Field_string (implementing CHAR(n)) returns the full padded width on read
    (as opposed to a normal CHAR, where we usually strip the spaces on read),
    but it's suboptimal, since lengthsp() is also used in a number of other
    places, e.g. stripping trailing spaces from enum values given in by the
    user. If you call this function, be aware of this special exception and
    consider the implications.
  */
  size_t (*lengthsp)(const CHARSET_INFO *, const char *ptr, size_t length);
  size_t (*numcells)(const CHARSET_INFO *, const char *b, const char *e);

  /* Unicode conversion */
  my_charset_conv_mb_wc mb_wc;
  my_charset_conv_wc_mb wc_mb;

  /* CTYPE scanner */
  int (*ctype)(const CHARSET_INFO *cs, int *ctype, const uint8_t *s,
               const uint8_t *e);

  /* Functions for case and sort conversion */
  size_t (*caseup_str)(const CHARSET_INFO *, char *);
  size_t (*casedn_str)(const CHARSET_INFO *, char *);

  my_charset_conv_case caseup;
  my_charset_conv_case casedn;

  /* Charset dependant snprintf() */
  size_t (*snprintf)(const CHARSET_INFO *, char *to, size_t n, const char *fmt,
                     ...) MY_ATTRIBUTE((format(printf, 4, 5)));

  size_t (*long10_to_str)(const CHARSET_INFO *, char *to, size_t n, int radix,
                          long int val);
  size_t (*longlong10_to_str)(const CHARSET_INFO *, char *to, size_t n,
                              int radix, long long val);

  void (*fill)(const CHARSET_INFO *, char *to, size_t len, int fill);

  /* String-to-number conversion routines */
  long (*strntol)(const CHARSET_INFO *, const char *s, size_t l, int base,
                  const char **e, int *err);
  unsigned long (*strntoul)(const CHARSET_INFO *, const char *s, size_t l,
                            int base, const char **e, int *err);
  long long (*strntoll)(const CHARSET_INFO *, const char *s, size_t l, int base,
                        const char **e, int *err);
  unsigned long long (*strntoull)(const CHARSET_INFO *, const char *s, size_t l,
                                  int base, const char **e, int *err);
  double (*strntod)(const CHARSET_INFO *, const char *s, size_t l,
                    const char **e, int *err);
  long long (*strtoll10)(const CHARSET_INFO *cs, const char *nptr,
                         const char **endptr, int *error);
  unsigned long long (*strntoull10rnd)(const CHARSET_INFO *cs, const char *str,
                                       size_t length, int unsigned_fl,
                                       const char **endptr, int *error);
  size_t (*scan)(const CHARSET_INFO *, const char *b, const char *e, int sq);
};

/* See strings/CHARSET_INFO.txt about information on this structure  */
struct CHARSET_INFO {
  unsigned number;
  unsigned primary_number;
  unsigned binary_number;
  unsigned state;
  const char *csname;
  const char *m_coll_name;
  const char *comment;
  const char *tailoring;
  struct Coll_param *coll_param;
  const uint8_t *ctype;
  const uint8_t *to_lower;
  const uint8_t *to_upper;
  const uint8_t *sort_order;
  struct MY_UCA_INFO *uca; /* This can be changed in apply_one_rule() */
  const uint16_t *tab_to_uni;
  const MY_UNI_IDX *tab_from_uni;
  const MY_UNICASE_INFO *caseinfo;
  const struct lex_state_maps_st *state_maps; /* parser internal data */
  const uint8_t *ident_map;                   /* parser internal data */
  unsigned strxfrm_multiply;
  uint8_t caseup_multiply;
  uint8_t casedn_multiply;
  unsigned mbminlen;
  unsigned mbmaxlen;
  unsigned mbmaxlenlen;
  my_wc_t min_sort_char;
  my_wc_t max_sort_char; /* For LIKE optimization */
  uint8_t pad_char;
  bool escape_with_backslash_is_dangerous;
  uint8_t levels_for_compare;

  MY_CHARSET_HANDLER *cset;
  MY_COLLATION_HANDLER *coll;

  /**
    If this collation is PAD_SPACE, it collates as if all inputs were
    padded with a given number of spaces at the end (see the "num_codepoints"
    flag to strnxfrm). NO_PAD simply compares unextended strings.

    Note that this is fundamentally about the behavior of coll->strnxfrm.
  */
  enum Pad_attribute pad_attribute;
};

/*
  NOTE: You cannot use a CHARSET_INFO without it having been initialized first.
  In particular, they are not initialized when a unit test starts; do not use
  these globals indiscriminately from there, and do not add more. Instead,
  initialize them using my_collation_get_by_name().
*/

extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_bin;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_latin1;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_filename;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb4_0900_ai_ci;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb4_0900_bin;

extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_latin1_bin;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf32_unicode_ci;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb3_general_ci;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb3_tolower_ci;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb3_unicode_ci;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb3_bin;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb4_bin;
extern MYSQL_STRINGS_EXPORT CHARSET_INFO my_charset_utf8mb4_general_ci;

/**
  @note Deprecated function, please call cs->coll->wildcmp(cs...) instead.
*/
MYSQL_STRINGS_EXPORT int my_wildcmp_mb_bin(const CHARSET_INFO *cs,
                                           const char *str, const char *str_end,
                                           const char *wildstr,
                                           const char *wildend, int escape,
                                           int w_one, int w_many);

MYSQL_STRINGS_EXPORT extern size_t my_strcspn(const CHARSET_INFO *cs,
                                              const char *str, const char *end,
                                              const char *reject,
                                              size_t reject_length);

MYSQL_STRINGS_EXPORT unsigned my_string_repertoire(const CHARSET_INFO *cs,
                                                   const char *str, size_t len);

MYSQL_STRINGS_EXPORT bool my_charset_is_ascii_based(const CHARSET_INFO *cs);

/**
  Detect whether a character set is ASCII compatible.
*/
inline bool my_charset_is_ascii_based(const CHARSET_INFO *cs) {
  return (cs->state & MY_CS_NONASCII) == 0;
}

inline bool my_charset_same(const CHARSET_INFO *cs1, const CHARSET_INFO *cs2) {
  assert(0 != strcmp(cs1->csname, "utf8"));
  assert(0 != strcmp(cs2->csname, "utf8"));
  return ((cs1 == cs2) || !strcmp(cs1->csname, cs2->csname));
}

MYSQL_STRINGS_EXPORT unsigned my_charset_repertoire(const CHARSET_INFO *cs);

MYSQL_STRINGS_EXPORT unsigned my_strxfrm_flag_normalize(unsigned flags);

MYSQL_STRINGS_EXPORT size_t my_convert(char *to, size_t to_length,
                                       const CHARSET_INFO *to_cs,
                                       const char *from, size_t from_length,
                                       const CHARSET_INFO *from_cs,
                                       unsigned *errors);

MYSQL_STRINGS_EXPORT unsigned my_mbcharlen_ptr(const CHARSET_INFO *cs,
                                               const char *s, const char *e);

MYSQL_STRINGS_EXPORT bool my_is_prefixidx_cand(const CHARSET_INFO *cs,
                                               const char *wildstr,
                                               const char *wildend, int escape,
                                               int w_many, size_t *prefix_len);

/* clang-format off */
static constexpr uint8_t MY_CHAR_U   =   01; /* Upper case */
static constexpr uint8_t MY_CHAR_L   =   02; /* Lower case */
static constexpr uint8_t MY_CHAR_NMR =   04; /* Numeral (digit) */
static constexpr uint8_t MY_CHAR_SPC =  010; /* Spacing character */
static constexpr uint8_t MY_CHAR_PNT =  020; /* Punctuation */
static constexpr uint8_t MY_CHAR_CTR =  040; /* Control character */
static constexpr uint8_t MY_CHAR_B   = 0100; /* Blank */
static constexpr uint8_t MY_CHAR_X   = 0200; /* heXadecimal digit */
/* clang-format on */

/* The following functions make sense only for one-byte character sets.
They will not fail for multibyte character sets, but will not produce
the expected results. They may have some limited usability like
e.g. for utf8mb3/utf8mb4, meaningful results will be produced for
values < 0x7F. */

inline bool my_isascii(char ch) { return (ch & ~0177) == 0; }

inline char my_toupper(const CHARSET_INFO *cs, char ch) {
  return static_cast<char>(cs->to_upper[static_cast<uint8_t>(ch)]);
}

inline char my_tolower(const CHARSET_INFO *cs, char ch) {
  return static_cast<char>(cs->to_lower[static_cast<uint8_t>(ch)]);
}

inline bool my_isalpha(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] &
          (MY_CHAR_U | MY_CHAR_L)) != 0;
}

inline bool my_isupper(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_U) != 0;
}

inline bool my_islower(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_L) != 0;
}

inline bool my_isdigit(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_NMR) != 0;
}

inline bool my_isxdigit(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_X) != 0;
}

inline bool my_isalnum(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] &
          (MY_CHAR_U | MY_CHAR_L | MY_CHAR_NMR)) != 0;
}

inline bool my_isspace(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_SPC) != 0;
}

inline bool my_ispunct(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_PNT) != 0;
}

inline bool my_isgraph(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] &
          (MY_CHAR_PNT | MY_CHAR_U | MY_CHAR_L | MY_CHAR_NMR)) != 0;
}

inline bool my_iscntrl(const CHARSET_INFO *cs, char ch) {
  return ((cs->ctype + 1)[static_cast<uint8_t>(ch)] & MY_CHAR_CTR) != 0;
}

inline bool my_isvar(const CHARSET_INFO *cs, char ch) {
  return my_isalnum(cs, ch) || (ch == '_');
}

inline bool my_isvar_start(const CHARSET_INFO *cs, char ch) {
  return my_isalpha(cs, ch) || (ch == '_');
}

// Properties of character sets.
inline bool my_binary_compare(const CHARSET_INFO *cs) {
  return (cs->state & MY_CS_BINSORT) != 0;
}

inline bool use_strnxfrm(const CHARSET_INFO *cs) {
  return (cs->state & MY_CS_STRNXFRM) != 0;
}

// Interfaces to member functions.
inline size_t my_strnxfrm(const CHARSET_INFO *cs, uint8_t *dst, size_t dstlen,
                          const uint8_t *src, size_t srclen) {
  return cs->coll->strnxfrm(cs, dst, dstlen, dstlen, src, srclen, 0);
}

inline int my_strnncoll(const CHARSET_INFO *cs, const uint8_t *a,
                        size_t a_length, const uint8_t *b, size_t b_length) {
  return cs->coll->strnncoll(cs, a, a_length, b, b_length, false);
}

inline bool my_like_range(const CHARSET_INFO *cs, const char *s,
                          size_t s_length, char w_prefix, char w_one,
                          char w_many, size_t res_length, char *min_str,
                          char *max_str, size_t *min_len, size_t *max_len) {
  return cs->coll->like_range(cs, s, s_length, w_prefix, w_one, w_many,
                              res_length, min_str, max_str, min_len, max_len);
}

inline int my_wildcmp(const CHARSET_INFO *cs, const char *str,
                      const char *str_end, const char *wildstr,
                      const char *wildend, int escape, int w_one, int w_many) {
  return cs->coll->wildcmp(cs, str, str_end, wildstr, wildend, escape, w_one,
                           w_many);
}

inline int my_strcasecmp(const CHARSET_INFO *cs, const char *s1,
                         const char *s2) {
  return cs->coll->strcasecmp(cs, s1, s2);
}

inline size_t my_charpos(const CHARSET_INFO *cs, const char *beg,
                         const char *end, size_t pos) {
  return cs->cset->charpos(cs, beg, end, pos);
}

inline size_t my_charpos(const CHARSET_INFO *cs, const unsigned char *beg,
                         const unsigned char *end, size_t pos) {
  return cs->cset->charpos(cs, pointer_cast<const char *>(beg),
                           pointer_cast<const char *>(end), pos);
}

inline bool use_mb(const CHARSET_INFO *cs) {
  return cs->cset->ismbchar != nullptr;
}

inline unsigned my_ismbchar(const CHARSET_INFO *cs, const char *str,
                            const char *strend) {
  return cs->cset->ismbchar(cs, str, strend);
}

inline unsigned my_ismbchar(const CHARSET_INFO *cs, const uint8_t *str,
                            const uint8_t *strend) {
  return cs->cset->ismbchar(cs, pointer_cast<const char *>(str),
                            pointer_cast<const char *>(strend));
}

inline unsigned my_mbcharlen(const CHARSET_INFO *cs, unsigned first_byte) {
  return cs->cset->mbcharlen(cs, first_byte);
}

/**
  Get the length of gb18030 code by the given two leading bytes

  @param[in] cs charset_info
  @param[in] first_byte first byte of gb18030 code
  @param[in] second_byte second byte of gb18030 code
  @return    the length of gb18030 code starting with given two bytes,
             the length would be 2 or 4 for valid gb18030 code,
             or 0 for invalid gb18030 code
*/
inline unsigned my_mbcharlen_2(const CHARSET_INFO *cs, uint8_t first_byte,
                               uint8_t second_byte) {
  return cs->cset->mbcharlen(cs,
                             ((first_byte & 0xFF) << 8) + (second_byte & 0xFF));
}

/**
  Get the maximum length of leading bytes needed to determine the length of a
  multi-byte gb18030 code

  @param[in] cs charset_info
  @return    number of leading bytes we need, would be 2 for gb18030
             and 1 for all other charsets
*/
inline unsigned my_mbmaxlenlen(const CHARSET_INFO *cs) {
  return cs->mbmaxlenlen;
}

/**
  Judge if the given byte is a possible leading byte for a charset.
  For gb18030 whose mbmaxlenlen is 2, we can't determine the length of
  a multi-byte character by looking at the first byte only

  @param[in] cs charset_info
  @param[in] leading_byte possible leading byte
  @return    true if it is, otherwise false
*/
inline bool my_ismb1st(const CHARSET_INFO *cs, unsigned leading_byte) {
  return my_mbcharlen(cs, leading_byte) > 1 ||
         (my_mbmaxlenlen(cs) == 2 && my_mbcharlen(cs, leading_byte) == 0);
}

inline size_t my_caseup_str(const CHARSET_INFO *cs, char *str) {
  return cs->cset->caseup_str(cs, str);
}

inline size_t my_casedn_str(const CHARSET_INFO *cs, char *str) {
  return cs->cset->casedn_str(cs, str);
}

inline long my_strntol(const CHARSET_INFO *cs, const char *str, size_t length,
                       int base, const char **end, int *err) {
  return cs->cset->strntol(cs, str, length, base, end, err);
}

inline unsigned long my_strntoul(const CHARSET_INFO *cs, const char *str,
                                 size_t length, int base, const char **end,
                                 int *err) {
  return cs->cset->strntoul(cs, str, length, base, end, err);
}

inline int64_t my_strntoll(const CHARSET_INFO *cs, const char *str,
                           size_t length, int base, const char **end,
                           int *err) {
  return cs->cset->strntoll(cs, str, length, base, end, err);
}

inline uint64_t my_strntoull(const CHARSET_INFO *cs, const char *str,
                             size_t length, int base, const char **end,
                             int *err) {
  return cs->cset->strntoull(cs, str, length, base, end, err);
}

inline double my_strntod(const CHARSET_INFO *cs, const char *str, size_t length,
                         const char **end, int *err) {
  return cs->cset->strntod(cs, str, length, end, err);
}

inline bool is_supported_parser_charset(const CHARSET_INFO *cs) {
  return (cs->mbminlen == 1);
}

#endif  // INCLUDE_MYSQL_STRINGS_M_CTYPE_H_
