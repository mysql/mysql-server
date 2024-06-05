/* Copyright (c) 2009, 2024, Oracle and/or its affiliates.

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

#include <HashMap.hpp>

#ifdef TEST_HASH

#include <BaseString.hpp>
#include <NdbTap.hpp>

struct NodePair {
  Uint32 node1;
  Uint32 node2;
};

TAPTEST(HashMap) {
  OK(my_init() == 0);  // Init mysys

  printf("int -> int\n");
  {
    HashMap<int, int> hash1;
    for (int i = 0; i < 100; i++) OK(hash1.insert(i, i * 34));

    int int_val;
    for (int i = 0; i < 100; i++) {
      OK(hash1.search(i, int_val));
      OK(int_val == i * 34);

      OK(!hash1.search(i + 100, int_val));
    }

    // Duplicate key insert disallowed
    OK(!hash1.insert(32, 32));

    // Value should not have changed
    OK(hash1.search(32, int_val));
    OK(int_val == 32 * 34);

    // Duplicate key insert with replace flag
    OK(hash1.insert(32, 37, true));

    // Value should now have changed
    OK(hash1.search(32, int_val));
    OK(int_val == 37);
  }

  printf("int -> BaseString\n");
  {
    HashMap<int, BaseString> hash2;

    // Insert value with key 32
    BaseString str1("hej");
    OK(hash2.insert(32, str1));

    // Retrieve value with key 32 and check it's the same
    BaseString str2;
    OK(hash2.search(32, str2));
    OK(str1 == str2);

    // no value with key 33 inserted
    OK(!hash2.search(33, str2));

    for (int i = 100; i < 200; i++) {
      BaseString str;
      str.assfmt("magnus%d", i);
      OK(hash2.insert(i, str));
    }

    for (int i = 100; i < 200; i++) {
      BaseString str;
      OK(hash2.search(i, str));
    }

    // Delete every second entry
    for (int i = 100; i < 200; i += 2) OK(hash2.remove(i));

    BaseString str3;
    OK(!hash2.search(102, str3));
    OK(hash2.search(103, str3));
  }

  printf("struct NodePair -> Uint32\n");
  {
    HashMap<NodePair, Uint32> lookup;
    NodePair pk;
    pk.node1 = 1;
    pk.node2 = 2;
    OK(lookup.insert(pk, 37));

    // Duplicate insert
    OK(!lookup.insert(pk, 38));

    Uint32 value;
    OK(lookup.search(pk, value));
    OK(value == 37);
  }

  printf("BaseString -> int\n");
  {
    HashMap<BaseString, int, BaseString_get_key> string_hash;
    OK(string_hash.insert("magnus", 1));
    OK(string_hash.insert("mas", 2));
    int value;
    OK(string_hash.search("mas", value));
    OK(value == 2);

    OK(string_hash.entries() == 2);

    // Remove entry
    OK(string_hash.remove("mas"));

    // Check it does not exist
    OK(!string_hash.search("mas", value));

    OK(string_hash.entries() == 1);
  }

  my_end(0);  // Bye mysys

  return 1;  // OK
}

#endif
