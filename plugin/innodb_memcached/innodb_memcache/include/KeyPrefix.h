#ifndef NDBMEMCACHE_KEYPREFIX_H
#define NDBMEMCACHE_KEYPREFIX_H

#include <stdio.h>

/***** This section defines a data structures available to C. ***/
/* The prefix_info_t is the compacted form of the parts of the 
   KeyPrefix that must be available to C code. 
*/

/* The value of 13 imposes a limit of 8,192 prefixes */ 
#define KEY_PREFIX_BITS 13
#define MAX_KEY_PREFIXES ( 1 << KEY_PREFIX_BITS )

/* The value of 4 imposes a limit of 16 clusters */
#define CLUSTER_ID_BITS 4
#define MAX_CLUSTERS ( 1 << CLUSTER_ID_BITS )

typedef struct ndb_prefix_bitfield {
  unsigned usable         : 1;
  unsigned use_ndb        : 1;
  unsigned _unused1       : 1;
  unsigned prefix_id      : KEY_PREFIX_BITS;  
 
  unsigned do_mc_read     : 1;  
  unsigned do_db_read     : 1;
  unsigned do_mc_write    : 1;
  unsigned do_db_write    : 1;
  unsigned do_mc_delete   : 1;
  unsigned do_db_delete   : 1;
  unsigned do_db_flush    : 1;
  unsigned has_cas_col    : 1;

  unsigned has_flags_col  : 1;  
  unsigned has_expire_col : 1;
  unsigned has_math_col   : 1;
  unsigned cluster_id     : CLUSTER_ID_BITS;
} prefix_info_t;


/***** This section is available to C++ only. ***/
#ifdef __cplusplus

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#include "TableSpec.h"

class KeyPrefix {
public: 
  /* public methods */
  KeyPrefix(const char *name);
  KeyPrefix(const KeyPrefix &);
  ~KeyPrefix();
  int cmp(const char *key, int nkey);
  void dump(FILE *) const;

  /* public instance variables */
  TableSpec *table;
  prefix_info_t info;
  const char *prefix; 
  const size_t prefix_len;
};


/**** Inline methods for KeyPrefix ****/
inline int KeyPrefix::cmp(const char *key, int nkey) {
  return strncmp(prefix, key, prefix_len);  
};

#endif

#endif

