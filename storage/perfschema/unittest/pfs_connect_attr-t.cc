/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#include <memory.h>
#include <string.h>
#include <sys/types.h>

#include <mysql/components/my_service.h>
#include "my_inttypes.h"
#include "my_thread.h"
#include "mysql/strings/m_ctype.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_server.h"
#include "unittest/mytap/tap.h"

/* test helpers, to inspect data */
bool read_nth_attr(const char *connect_attrs, uint connect_attrs_length,
                   const CHARSET_INFO *connect_attrs_cs, uint ordinal,
                   char *attr_name, uint max_attr_name, uint *attr_name_length,
                   char *attr_value, uint max_attr_value,
                   uint *attr_value_length);

static void test_blob_parser() {
  char name[100], value[4096];
  unsigned char packet[10000], *ptr;
  uint name_len, value_len, idx, packet_length;
  bool result;
  const CHARSET_INFO *cs = &my_charset_utf8mb3_bin;

  diag("test_blob_parser");

  result =
      read_nth_attr("", 0, cs, 0, name, 32, &name_len, value, 1024, &value_len);
  ok(result == false, "zero length blob");

  result = read_nth_attr("\x1", 1, cs, 0, name, 32, &name_len, value, 1024,
                         &value_len);
  ok(result == false, "invalid key length");

  result = read_nth_attr("\x2k1\x1", 4, cs, 0, name, 32, &name_len, value, 1024,
                         &value_len);
  ok(result == false, "invalid value length");

  result = read_nth_attr("\x2k1\x2v1", 6, cs, 0, name, 32, &name_len, value,
                         1024, &value_len);
  ok(result == true, "one pair return");
  ok(name_len == 2, "one pair attr name length");
  ok(!strncmp(name, "k1", name_len), "one pair attr name");
  ok(value_len == 2, "one pair value length");
  ok(!strncmp(value, "v1", value_len), "one pair value");

  result = read_nth_attr("\x2k1\x2v1", 6, cs, 1, name, 32, &name_len, value,
                         1024, &value_len);
  ok(result == false, "no second arg");

  result = read_nth_attr("\x2k1\x2v1\x2k2\x2v2", 12, cs, 1, name, 32, &name_len,
                         value, 1024, &value_len);
  ok(result == true, "two pairs return");
  ok(name_len == 2, "two pairs attr name length");
  ok(!strncmp(name, "k2", name_len), "two pairs attr name");
  ok(value_len == 2, "two pairs value length");
  ok(!strncmp(value, "v2", value_len), "two pairs value");

  result = read_nth_attr("\x2k1\xff\x2k2\x2v2", 12, cs, 1, name, 32, &name_len,
                         value, 1024, &value_len);
  ok(result == false, "two pairs first value bad return");

  result = read_nth_attr("\x2k1\x2v1\x2k2\x2v2", 10, cs, 1, name, 32, &name_len,
                         value, 1024, &value_len);
  ok(result == false, "two pairs wrong global length");

  result = read_nth_attr("\x21z123456789z123456789z123456789z12\x2v1", 37, cs,
                         0, name, 32, &name_len, value, 1024, &value_len);
  ok(result == true, "attr name overflow");
  ok(name_len == 32, "attr name overflow length");
  ok(!strncmp(name, "z123456789z123456789z123456789z1", name_len),
     "attr name overflow name");
  ok(value_len == 2, "attr name overflow value length");
  ok(!strncmp(value, "v1", value_len), "attr name overflow value");

  packet[0] = 2;
  packet[1] = 'k';
  packet[2] = '1';
  ptr = net_store_length(packet + 3, 1025);
  for (idx = 0; idx < 1025; idx++) *ptr++ = '0' + (idx % 10);
  packet_length = (uint)(ptr - packet);
  result = read_nth_attr((char *)packet, packet_length, cs, 0, name, 32,
                         &name_len, value, 1024, &value_len);
  ok(result == true, "attr value overflow");
  ok(name_len == 2, "attr value overflow length");
  ok(!strncmp(name, "k1", name_len), "attr value overflow name");
  ok(value_len == 1024, "attr value overflow value length");
  for (idx = 0; idx < 1024; idx++) {
    if (value[idx] != (char)('0' + (idx % 10))) break;
  }
  ok(idx == 1024, "attr value overflow value");

  result =
      read_nth_attr("\x21z123456789z123456789z123456789z12\x2v1\x2k2\x2v2", 43,
                    cs, 1, name, 32, &name_len, value, 1024, &value_len);
  ok(result == true, "prev attr name overflow");
  ok(name_len == 2, "prev attr name overflow length");
  ok(!strncmp(name, "k2", name_len), "prev attr name overflow name");
  ok(value_len == 2, "prev attr name overflow value length");
  ok(!strncmp(value, "v2", value_len), "prev attr name overflow value");

  packet[1] = 'k';
  packet[2] = '1';
  packet[3] = 2;
  packet[4] = 'v';
  packet[5] = '1';

  for (idx = 251; idx < 256; idx++) {
    packet[0] = idx;
    result = read_nth_attr((char *)packet, 6, cs, 0, name, 32, &name_len, value,
                           1024, &value_len);
    ok(result == false, "invalid string length %d", idx);
  }

  memset(packet, 0, sizeof(packet));
  for (idx = 0; idx < 1660 /* *6 = 9960 */; idx++)
    memcpy(packet + idx * 6, "\x2k1\x2v1", 6);
  result = read_nth_attr((char *)packet, 8192, cs, 1364, name, 32, &name_len,
                         value, 1024, &value_len);
  ok(result == true, "last valid attribute %d", 1364);
  result = read_nth_attr((char *)packet, 8192, cs, 1365, name, 32, &name_len,
                         value, 1024, &value_len);
  ok(result == false, "first attribute that's cut %d", 1365);
}

static void test_multibyte_lengths() {
  char name[100], value[4096];
  uint name_len, value_len;
  bool result;
  const CHARSET_INFO *cs = &my_charset_utf8mb3_bin;

  unsigned char var_len_packet[] = {
      252, 2, 0, 'k', '1', 253, 2, 0, 0, 'v', '1', 254, 2, 0, 0,   0,  0,
      0,   0, 0, 'k', '2', 254, 2, 0, 0, 0,   0,   0,   0, 0, 'v', '2'};

  result = read_nth_attr((char *)var_len_packet, sizeof(var_len_packet), cs, 0,
                         name, 32, &name_len, value, 1024, &value_len);
  ok(result == true, "multibyte lengths return");
  ok(name_len == 2, "multibyte lengths name length");
  ok(!strncmp(name, "k1", name_len), "multibyte lengths attr name");
  ok(value_len == 2, "multibyte lengths value length");
  ok(!strncmp(value, "v1", value_len), "multibyte lengths value");

  result = read_nth_attr((char *)var_len_packet, sizeof(var_len_packet), cs, 1,
                         name, 32, &name_len, value, 1024, &value_len);
  ok(result == true, "multibyte lengths second attr return");
  ok(name_len == 2, "multibyte lengths second attr name length");
  ok(!strncmp(name, "k2", name_len), "multibyte lengths second attr attr name");
  ok(value_len == 2, "multibyte lengths value length");
  ok(!strncmp(value, "v2", value_len), "multibyte lengths second attr value");
}

static void test_utf8mb3_parser() {
  /* utf8mb3 max byte length per character is 3 */
  char name[33 * 3], value[1024 * 3], packet[1500 * 3], *ptr;
  uint name_len, value_len;
  bool result;
  const CHARSET_INFO *cs = &my_charset_utf8mb3_bin;

  /* note : this is encoded in utf-8 */
  const char *attr1 = "Георги";
  const char *val1 = "Кодинов";
  const char *attr2 = "Пловдив";
  const char *val2 = "България";

  ptr = packet;
  *ptr++ = strlen(attr1);
  memcpy(ptr, attr1, strlen(attr1));
  ptr += strlen(attr1);
  *ptr++ = strlen(val1);
  memcpy(ptr, val1, strlen(val1));
  ptr += strlen(val1);

  *ptr++ = strlen(attr2);
  memcpy(ptr, attr2, strlen(attr2));
  ptr += strlen(attr2);
  *ptr++ = strlen(val2);
  memcpy(ptr, val2, strlen(val2));
  ptr += strlen(val2);

  diag("test_utf8mb3_parser attr pair #1");

  result =
      read_nth_attr((char *)packet, ptr - packet, cs, 0, name, sizeof(name),
                    &name_len, value, sizeof(value), &value_len);
  ok(result == true, "return");
  ok(name_len == strlen(attr1), "name length");
  ok(!strncmp(name, attr1, name_len), "attr name");
  ok(value_len == strlen(val1), "value length");
  ok(!strncmp(value, val1, value_len), "value");

  diag("test_utf8mb3_parser attr pair #2");
  result =
      read_nth_attr((char *)packet, ptr - packet, cs, 1, name, sizeof(name),
                    &name_len, value, sizeof(value), &value_len);
  ok(result == true, "return");
  ok(name_len == strlen(attr2), "name length");
  ok(!strncmp(name, attr2, name_len), "attr name");
  ok(value_len == strlen(val2), "value length");
  ok(!strncmp(value, val2, value_len), "value");
}

static void test_utf8mb3_parser_bad_encoding() {
  /* utf8mb3 max byte length per character is 3*/
  char name[33 * 3], value[1024 * 3], packet[1500 * 3], *ptr;
  uint name_len, value_len;
  bool result;
  const CHARSET_INFO *cs = &my_charset_utf8mb3_bin;

  /* note : this is encoded in utf-8 */
  const char *attr = "Георги";
  const char *val = "Кодинов";

  ptr = packet;
  *ptr++ = strlen(attr);
  memcpy(ptr, attr, strlen(attr));
  ptr[0] = (char)0xFA;  // invalid UTF8MB3 char
  ptr += strlen(attr);
  *ptr++ = strlen(val);
  memcpy(ptr, val, strlen(val));
  ptr += strlen(val);

  diag("test_utf8mb3_parser_bad_encoding");

  result =
      read_nth_attr((char *)packet, ptr - packet, cs, 0, name, sizeof(name),
                    &name_len, value, sizeof(value), &value_len);
  ok(result == false, "return");
}

const CHARSET_INFO *cs_cp1251;

static void test_cp1251_parser() {
  /* utf8mb3 max byte length per character is 3*/
  char name[33 * 3], value[1024 * 3], packet[1500 * 3], *ptr;
  uint name_len, value_len;
  bool result;

  /* note : this is Георги in windows-1251 */
  const char *attr1 = "\xc3\xe5\xee\xf0\xe3\xe8";
  /* note : this is Кодинов in windows-1251 */
  const char *val1 = "\xca\xee\xe4\xe8\xed\xee\xe2";
  /* note : this is Пловдив in windows-1251 */
  const char *attr2 = "\xcf\xeb\xee\xe2\xe4\xe8\xe2";
  /* note : this is България in windows-1251 */
  const char *val2 = "\xc1\xfa\xeb\xe3\xe0\xf0\xe8\xff";

  ptr = packet;
  *ptr++ = strlen(attr1);
  memcpy(ptr, attr1, strlen(attr1));
  ptr += strlen(attr1);
  *ptr++ = strlen(val1);
  memcpy(ptr, val1, strlen(val1));
  ptr += strlen(val1);

  *ptr++ = strlen(attr2);
  memcpy(ptr, attr2, strlen(attr2));
  ptr += strlen(attr2);
  *ptr++ = strlen(val2);
  memcpy(ptr, val2, strlen(val2));
  ptr += strlen(val2);

  diag("test_cp1251_parser attr pair #1");

  result =
      read_nth_attr((char *)packet, ptr - packet, cs_cp1251, 0, name,
                    sizeof(name), &name_len, value, sizeof(value), &value_len);
  ok(result == true, "return");
  /* need to compare to the UTF8MB3 equivalents */
  ok(name_len == strlen("Георги"), "name length");
  ok(!strncmp(name, "Георги", name_len), "attr name");
  ok(value_len == strlen("Кодинов"), "value length");
  ok(!strncmp(value, "Кодинов", value_len), "value");

  diag("test_cp1251_parser attr pair #2");
  result =
      read_nth_attr((char *)packet, ptr - packet, cs_cp1251, 1, name,
                    sizeof(name), &name_len, value, sizeof(value), &value_len);
  ok(result == true, "return");
  /* need to compare to the UTF8MB3 equivalents */
  ok(name_len == strlen("Пловдив"), "name length");
  ok(!strncmp(name, "Пловдив", name_len), "attr name");
  ok(value_len == strlen("България"), "value length");
  ok(!strncmp(value, "България", value_len), "value");
}

static void do_all_tests() {
  test_blob_parser();
  test_multibyte_lengths();
  test_utf8mb3_parser();
  test_utf8mb3_parser_bad_encoding();
  test_cp1251_parser();
}

int main(int, char **) {
  MY_INIT("pfs_connect_attr-t");

  cs_cp1251 = get_charset_by_csname("cp1251", MY_CS_PRIMARY, MYF(0));
  plan(69);
  do_all_tests();
  charset_uninit();
  const int retval = exit_status();
  my_end(0);
  return retval;
}
