/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <errno.h>
#include <stdio.h>

#include "duk_node_fs.h"

duk_ret_t duk_node_fs_read_file_sync(duk_context *ctx) {
  char *buf;
  size_t len;
  size_t off;
  int rc;
  FILE *f;

  const char *fn = duk_require_string(ctx, 0);

  f = fopen(fn, "rb");
  if (!f) {
    return duk_error(ctx, DUK_ERR_TYPE_ERROR,
                     "cannot open file %s for reading, errno %ld: %s", fn,
                     (long)errno, strerror(errno));
  }

  rc = fseek(f, 0, SEEK_END);
  if (rc < 0) {
    (void)fclose(f);
    return duk_error(ctx, DUK_ERR_TYPE_ERROR,
                     "fseek() failed for %s, errno %ld: %s", fn, (long)errno,
                     strerror(errno));
  }
  len = (size_t)ftell(f);
  rc = fseek(f, 0, SEEK_SET);
  if (rc < 0) {
    (void)fclose(f);
    return duk_error(ctx, DUK_ERR_TYPE_ERROR,
                     "fseek() failed for %s, errno %ld: %s", fn, (long)errno,
                     strerror(errno));
  }

  buf = (char *)duk_push_fixed_buffer(ctx, (duk_size_t)len);
  for (off = 0; off < len;) {
    size_t got;
    got = fread((void *)(buf + off), 1, len - off, f);
    if (ferror(f)) {
      (void)fclose(f);
      return duk_error(ctx, DUK_ERR_TYPE_ERROR, "error while reading %s", fn);
    }
    if (got == 0) {
      if (feof(f)) {
        break;
      } else {
        (void)fclose(f);
        return duk_error(ctx, DUK_ERR_TYPE_ERROR, "error while reading %s", fn);
      }
    }
    off += got;
  }

  if (f) {
    (void)fclose(f);
  }

  return 1;
}
