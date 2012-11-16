/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#include <stdio.h>

#include "KeyPrefix.h"


/* constructor */
KeyPrefix::KeyPrefix(const char *name) : 
  prefix(strdup(name)) , prefix_len(strlen(name))
{};

/* copy constructor */
KeyPrefix::KeyPrefix(const KeyPrefix &k) : 
  table(k.table),
  info(k.info),
  prefix(strdup(k.prefix)),   /* deep copy */
  prefix_len(k.prefix_len) 
{}; 

/* destructor */
KeyPrefix::~KeyPrefix() {
  free((void *) prefix);
}


void KeyPrefix::dump(FILE *f) const {
  fprintf(f,"   Prefix %d: \"%s\" [len:%lu], cluster %d, usable: %s \n", 
          info.prefix_id, prefix, prefix_len, info.cluster_id, 
          info.usable ? "Yes" : "No");
  if(table) {
    fprintf(f,"   Table: %s.%s (%d key%s;%d value%s)\n", 
            table->schema_name, table->table_name, 
            table->nkeycols, table->nkeycols == 1 ? "" : "s",
            table->nvaluecols,table->nvaluecols == 1 ? "" : "s");
    fprintf(f,"   Key0: %s, Value0: %s, Math: %s\n", table->key_columns[0],
            table->value_columns[0], table->math_column);
  }
  fprintf(f,"   READS   [mc/db]: %d %d\n", info.do_mc_read, info.do_db_read);
  fprintf(f,"   WRITES  [mc/db]: %d %d\n", info.do_mc_write, info.do_db_write);
  fprintf(f,"   DELETES [mc/db]: %d %d\n", info.do_mc_delete, info.do_db_delete);          
  fprintf(f, "\n");
}
