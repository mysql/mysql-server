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


/*  SIMPLE HASH TABLE.  Maps a null-terminated string to a pointer.
    Creates and manages its own copy of the string. 
*/
#include <stdio.h>
#include <string.h>

template<typename T> class LookupTable {
public:
  int elements;
  bool do_free_values;

  LookupTable(int sz = 128) : 
    elements(0),
    do_free_values(false),
    size(sz), 
    symtab(new Entry *[sz]) 
  {
    for(int i = 0 ; i < size ; i++) 
      symtab[i] = 0;
  }


  ~LookupTable() {
    for(int i = 0 ; i < size ; i++) {
      Entry *sym = symtab[i];
      while(sym) {
        if(do_free_values) free((void *) sym->value);
        Entry *next = sym->next;
        delete sym;
        sym = next;
      }
    }
    delete[] symtab;
  }


  T * find(const char *name) {
    Uint32 h = do_hash(name) % size;
    for(Entry *sym = symtab[h] ; sym != 0 ; sym = sym->next) 
      if(strcmp(name, sym->key) == 0) 
        return sym->value;
    return 0;
  }
  
  void insert(const char *name, T * value) { 
    Uint32 h = do_hash(name) % size;
    Entry *sym = new Entry(name, value);    
    sym->next = symtab[h];
    symtab[h] = sym;
    elements++;
  }

    
private:
  class Entry {
  public:
    char *key;
    T * value;
    Entry *next;

    Entry(const char *name, T * val) {
      key = strdup(name);
      value = val;
    }
    
    ~Entry() {
      free(key);
    }    
  };
  
  int size;
  Entry ** symtab;

  Uint32 do_hash(const char *string) {
    Uint32 h = 0;
    for(const char *s = string; *s != 0; s++) h = 37 * h + *s;
    return h;
  }
};

