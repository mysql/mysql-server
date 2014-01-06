/* Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <ndb_global.h>
#include <ndb_opts.h>

#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>

static struct my_option
my_long_options[] =
{
  { "help", '?',
    "Display this help and exit.",
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 },
  { 0, 0,
    0,
    0, 0, 0,
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }
};

const char*
load_default_groups[]= { 0 };

static void
short_usage_sub(void)
{
  ndb_short_usage_sub("*.frm ...");
}

static void
usage()
{
  printf("%s: pack and dump *.frm as C arrays\n", my_progname);
  ndb_usage(short_usage_sub, load_default_groups, my_long_options);
}

static void
dodump(const char* name, const uchar* frm_data, uint frm_len)
{
  printf("const uint g_%s_frm_len = %u;\n\n", name, frm_len);
  printf("const uint8 g_%s_frm_data[%u] =\n{\n", name, frm_len);
  uint n = 0;
  while (n < frm_len) {
    if (n % 8 == 0) {
      if (n != 0) {
        printf("\n");
      }
      printf("  ");
    }
    printf("0x%02x", frm_data[n]);
    if (n + 1 < frm_len) {
            printf(",");
    }
    n++;
  }
  if (n % 8 != 0) {
    printf("\n");
  }
  printf("};\n");
}

static int
dofile(const char* file)
{
  struct stat st;
  size_t size = 0;
  uchar* data = 0;
  int fd = -1;
  uchar* pack_data = 0;
  size_t pack_len = 0;
  char* namebuf = 0;
  int ret = -1;
  do
  {
    if (stat(file, &st) == -1)
    {
      fprintf(stderr, "%s: stat: %s\n", file, strerror(errno));
      break;
    }
    size = st.st_size;
    if ((data = (uchar*)malloc(size)) == 0)
    {
      fprintf(stderr, "%s: malloc %u: %s\n", file, (uint)size, strerror(errno));
      break;
    }
    if ((fd = open(file, O_RDONLY)) == -1)
    {
      fprintf(stderr, "%s: open: %s\n", file, strerror(errno));
      break;
    }
    ssize_t size2;
    if ((size2 = read(fd, data, size)) == -1)
    {
      fprintf(stderr, "%s: read: %s\n", file, strerror(errno));
      break;
    }
    if ((size_t)size2 != size)
    {
      fprintf(stderr, "%s: short read: %u != %u\n", file, (uint)size2, (uint)size);
      break;
    }
    int error;
    if ((error = packfrm(data, size, &pack_data, &pack_len)) != 0)
    {
      fprintf(stderr, "%s: packfrm: error %d\n", file, error);
      break;
    }
    namebuf = strdup(file);
    if (namebuf == 0)
    {
      fprintf(stderr, "%s: strdup: %s\n", file, strerror(errno));
      break;
    }
    char* name = namebuf;
    if (strchr(name, '/') != 0)
      name = strrchr(name, '/') + 1;
    char* dot;
    if ((dot = strchr(name, '.')) != 0)
      *dot = 0;
    printf("\n/*\n");
    printf("  name: %s\n", name);
    printf("  orig: %u\n", (uint)size);
    printf("  pack: %u\n", (uint)pack_len);
    printf("*/\n\n");
    dodump(name, pack_data, pack_len);
    ret = 0;
  }
  while (0);
  if (namebuf != 0)
    free(namebuf);
  if (pack_data != 0)
    my_free(pack_data); // Free data returned from packfrm with my_free
  if (fd != -1)
    (void)close(fd);
  if (data != 0)
    free(data);
  return ret;
}

int
main(int argc, char** argv)
{
  my_progname = "ndb_pack_frm";
  int ret;

  ndb_init();
  ndb_opt_set_usage_funcs(short_usage_sub, usage);
  ret = handle_options(&argc, &argv, my_long_options, ndb_std_get_one_option);
 if (ret != 0)
    return NDBT_WRONGARGS;

  for (int i = 0; i < argc; i++)
  {
    ret = dofile(argv[i]);
    if (ret != 0)
      return NDBT_FAILED;
  }

  return NDBT_OK;
}
