/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "util/require.h"
#include "util/ndb_zlib.h"

#include <assert.h> // assert()
#include <cstdint>
#include <stdlib.h> // abort()

#include "zlib.h"

#ifndef REQUIRE
#define REQUIRE(r) do { if (unlikely(!(r))) { \
  fprintf(stderr, "\nYYY: %s: %u: %s: r = %d\n", __FILE__, __LINE__, __func__, (r)); \
  require((r)); \
} } while (0)
#endif

#define RETURN(rv) return(rv)
//#define RETURN(rv) REQUIRE(rv)

ndb_zlib::ndb_zlib()
: mem_begin(nullptr),
  mem_top(nullptr),
  mem_end(nullptr),
  m_op_mode(NO_OP),
  pkcs_padded(false),
  padding(0),
  padding_left(0)
{
  file.zalloc = Z_NULL;
  file.zfree = Z_NULL;
  file.opaque = Z_NULL;
}

ndb_zlib::~ndb_zlib()
{
  require(mem_begin == mem_top);
}

void ndb_zlib::reset()
{
  m_op_mode = NO_OP;
  pkcs_padded = false;
  padding = 0;
  padding_left = 0;
}

int ndb_zlib::set_memory(void* mem, size_t size)
{
  require(mem != nullptr);
  require(size >= MEMORY_NEED);

  require(m_op_mode == NO_OP);
  require(mem_begin == nullptr);
  require(mem_top == nullptr);
  require(mem_end == nullptr);

  mem_begin = static_cast<byte*>(mem);
  mem_top = mem_begin;
  mem_end = mem_begin + size;

  file.zalloc = alloc;
  file.zfree = free;
  file.opaque = static_cast<void*>(this);

  return 0;
}

void *ndb_zlib::alloc(void *opaque, unsigned items, unsigned size)
{
  ndb_zlib *ths = static_cast<ndb_zlib *>(opaque);
  size_t sz = size_t{items} * size_t{size};
  if (ths->mem_top + sz <= ths->mem_end)
  {
    void *p = ths->mem_top;
    ths->mem_top += sz;
    return p;
  }
  return nullptr;
}

void ndb_zlib::free(void *opaque, void *address)
{
  ndb_zlib *ths = static_cast<ndb_zlib *>(opaque);
  REQUIRE(ths->mem_begin <= address);
  REQUIRE(address < ths->mem_top);
  REQUIRE(address < ths->mem_end);
  ths->mem_top = (byte *)address;
}

int ndb_zlib::deflate_init()
{
  assert(m_op_mode == NO_OP);
  if (m_op_mode != NO_OP)
    return -1;

  assert(padding == 0);
  if (padding != 0)
    return -1;

  int err = ::deflateInit2(&file, level, method, zlib_windowBits, memLevel, strategy);
  switch (err)
  {
    case Z_OK:
      m_op_mode = DEFLATE;
      return 0;
    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
    case Z_VERSION_ERROR:
      return -1;
      break;
    default:
      RETURN(-1);
  }
}

int ndb_zlib::deflate_end()
{
  if (m_op_mode != DEFLATE)
    RETURN(-1);
  int err = ::deflateEnd(&file);
  switch (err)
  {
    case Z_OK:
      require(mem_top == mem_begin);
      m_op_mode = NO_OP;
      if (padding != 0)
        RETURN(-1);
      return 0;
    case Z_DATA_ERROR:
    case Z_STREAM_ERROR:
      m_op_mode = NO_OP;
      RETURN(-1);
    default:
      RETURN(-1);
  }
}

int ndb_zlib::inflate_init()
{
  assert(m_op_mode == NO_OP);
  if (m_op_mode != NO_OP)
    return -1;
  assert(padding == 0);
  if (padding != 0)
    return -1;

  file.next_in = nullptr;
  file.avail_in = 0;
  int err = ::inflateInit2(&file, zlib_windowBits);
  switch (err)
  {
    case Z_OK:
      m_op_mode = INFLATE;
      return 0;
      break;
    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
    case Z_VERSION_ERROR:
      return -1;
    default:
      RETURN(-1);
  }
}

int ndb_zlib::inflate_end()
{
  if (m_op_mode == NO_OP)
    return 0;
  if (m_op_mode != INFLATE)
  {
    return -1;
  }
  if (padding_left != 0)
  {
    // Not all padding processed
    RETURN(-1);
  }
  if (pkcs_padded && padding == 0)
  {
    // No padding processed
    RETURN(-1);
  }
  int err = ::inflateEnd(&file);
  switch (err)
  {
    case Z_OK:
      require(mem_begin == mem_top);
      m_op_mode = NO_OP;
      return 0;
    case Z_STREAM_ERROR:
      m_op_mode = NO_OP;
      return -1;
    default:
      RETURN(-1);
  }
}

/*
 * return
 *  0 - finished
 *  1 - need more input
 *  2 - have more output
 * -1 - unrecoverable error
 */
int ndb_zlib::deflate(output_iterator* out, input_iterator* in)
{
  assert(m_op_mode == DEFLATE);
  if (m_op_mode != DEFLATE)
    return -1;

  if (padding == 0)
  {
    size_t in_size = in->size();
    size_t out_size = out->size();

    file.next_in = const_cast<byte*>(in->cbegin());
    file.avail_in = in_size;
    file.next_out = out->begin();
    file.avail_out = out_size;
    int flush_mode = in->last() ? Z_FINISH : Z_NO_FLUSH;
    int err = ::deflate(&file, flush_mode);

    size_t in_advance = in_size - file.avail_in;
    size_t out_advance = out_size - file.avail_out;

    in->advance(in_advance);
    require(file.next_in == const_cast<byte*>(in->cbegin()));
    out->advance(out_advance);
    require(file.next_out == out->begin());

    switch (err)
    {
    case Z_OK:
    case Z_BUF_ERROR:
    {
      if (out->empty()) return have_more_output;
      return need_more_input;
    }
    case Z_STREAM_END:
      require(file.avail_in == 0);
      require(in->last());
      if (pkcs_padded)
      {
        padding = 16 - file.total_out % 16;
        padding_left = padding;
        break; // handle padding
      }
      out->set_last();
      return 0;
    default:
      RETURN(-1);
    }
  }
  // padding if Z_STREAM_END
  require(pkcs_padded);
  require(padding_left > 0);
  while (!out->empty() && padding_left > 0)
  {
    *out->begin()= padding;
    out->advance(1);
    padding_left--;
  }
  if (padding_left == 0)
  {
    padding = 0;
    out->set_last();
    return 0;
  }
  return have_more_output;
}

int ndb_zlib::inflate(output_iterator* out, input_iterator* in)
{
  assert(m_op_mode == INFLATE);
  if (m_op_mode != INFLATE)
  {
    return -1;
  }

  if (padding_left == 0)
  {
    size_t in_size = in->size();
    size_t out_size = out->size();

    file.next_in = const_cast<byte*>(in->cbegin());
    file.avail_in = in_size;
    file.next_out = out->begin();
    file.avail_out = out_size;
    int flush_mode = in->last() ? Z_FINISH : Z_NO_FLUSH;
    int err = Z_OK;
    if (file.avail_in || file.avail_out)
    {
      err = ::inflate(&file, flush_mode);
    }

    size_t in_advance = in_size - file.avail_in;
    size_t out_advance = out_size - file.avail_out;

    in->advance(in_advance);
    out->advance(out_advance);

    switch (err)
    {
    case Z_OK:
    case Z_BUF_ERROR:  // no progress
      if (out->empty()) return have_more_output;
      return need_more_input;
    case Z_STREAM_END:
      if (!pkcs_padded)
      {
        out->set_last();
        return 0;
      }
      if (in->empty() && in->last() && padding != 0 && padding_left == 0)
      {
        // All padding already processed
        out->set_last();
        return 0;
      }
      if (file.avail_in > 0)
      {
        padding = file.next_in[0];
        assert(padding == (16 - (file.total_in % 16)));
        if (padding != (16 - (file.total_in % 16)))
        {
          RETURN(-1);
        }
      }
      else
      {
        padding = 16 - (file.total_in % 16);
      }
      require(padding > 0);
      require(padding <= 16);
      padding_left = padding;
    break;
    case Z_MEM_ERROR:
    case Z_STREAM_ERROR:
    case Z_DATA_ERROR:
      RETURN(-1);
    default:
      RETURN(-1);
    }
  }
  // padding if Z_STREAM_END
  require(pkcs_padded);
  require(padding_left > 0);
  while (!in->empty() && padding_left > 0)
  {
    if (*in->cbegin() != padding)
    {
      RETURN(-1);
    }
    in->advance(1);
    padding_left--;
  }
  if (padding_left == 0)
  {
    require(in->last());
    out->set_last();
    return 0;
  }
  return need_more_input;
}

#if 0

void *Ndbxfrm_zlib::probe_alloc(void *opaque,
                                     unsigned items,
                                     unsigned size)
{
  size_t *extra = static_cast<size_t *>(opaque);
  extra[0] += items * size;
  extra[1] += 1;
  return new byte[items * size];
}

void Ndbxfrm_zlib::probe_free(void *opaque, void *address)
{
  size_t *extra = static_cast<size_t *>(opaque);
  byte *ptr = static_cast<byte *>(address);
  (void)extra;  // TODO use
  delete[] ptr;
}

size_t Ndbxfrm_zlib::max_write_memory()
{
  /* The memory requirements for deflate are (in bytes):
              (1 << (windowBits+2)) +  (1 << (memLevel+9))
   that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
   plus a few kilobytes for small objects. For example, if you want to reduce
   the default memory requirements from 256K to 128K, compile with
       make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
   Of course this will generally degrade compression (there's no free lunch).
  */
  size_t extra[2] = {0, 0};
  z_stream file;
  file.zalloc = &probe_alloc;
  file.zfree = &probe_free;
  file.opaque = static_cast<void *>(&extra);
  file.next_in = nullptr;
  file.avail_in = 0;
  file.next_out = nullptr;
  file.avail_out = 0;
  int err = deflateInit2(&file, level, method, -windowBits, memLevel, strategy);
  err = deflateEnd(&file);
  // window and mem is included in probed extra for deflate
  //  return (1UL << (windowBits + 2)) +
  //       (1UL << (memLevel + 9)) +
  // TODO what alignment?
  return extra[0];
}

size_t Ndbxfrm_zlib::max_read_memory()
{
  /*
     The memory requirements for inflate are (in bytes) 1 << windowBits
   that is, 32K for windowBits=15 (default value) plus about 7 kilobytes
   for small objects.
  */
  size_t extra[2] = {0, 0};
  z_stream file;
  file.zalloc = &probe_alloc;
  file.zfree = &probe_free;
  file.opaque = static_cast<void *>(&extra);
  file.next_in = nullptr;
  file.avail_in = 0;
  file.next_out = nullptr;
  file.avail_out = 0;
  int err = inflateInit2(&file, -windowBits);
  err = inflateEnd(&file);
  // window memory not included in probed for inflate
  return (1UL << windowBits) + extra[0];
}

#endif

#ifdef TEST_NDB_ZLIB
#include <cstdio>

void require_fn(bool cc, const char cc_str[], const char file[], int line, const char func[])
{
  if (cc) return;
  fprintf(stderr,"YYY: %s: %u: %s: require(%s) failed.\n", file, line, func, cc_str);
  abort();
}

int main()
{
  using byte = unsigned char;

  ndb_zlib zlib;
  
  byte ibuf[32768];
  byte obuf[32768];

#if defined __GNUC__ && (__GNUC__ >= 11)
  /*
   * Hack to to avoid some GNU compiler versions to generate a false
   * -Wmaybe-uninitialized warning. Possibly it is an instance of the following
   * GCC Bug 104060 - [11/12/13 Regression] -Wmaybe-uninitialized false alarm
   * on address of local array.
   * See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104060
   */
  ibuf[0] = 0;
#endif

  ndbxfrm_input_iterator in{ibuf, ibuf, true};
  ndbxfrm_output_iterator out{obuf, obuf + 32768, false};
  require(zlib.deflate_init() == 0);
  // Compress empty buffer should fit 32KB buffer
  require(zlib.deflate(&out, &in) == 0);
  require(zlib.deflate_end() == 0);
  require(in.empty());
  require(out.last());
  fprintf(stderr, "zlib.deflate() empty input generate %zu bytes out\n",
          out.begin() - obuf);
  require(out.begin() == obuf + 2);

  in = ndbxfrm_input_iterator{obuf, out.begin(), out.last()};
  out = ndbxfrm_output_iterator{ibuf, ibuf + 32768, false};
  require(zlib.inflate_init() == 0);
  require(zlib.inflate(&out, &in) == 0);
  require(zlib.inflate_end() == 0);
  require(in.empty());
  require(out.last());
  fprintf(stderr, "zlib.inflate() back to %zu bytes\n", out.begin() - ibuf);
  require(out.begin() == ibuf);

  in = ndbxfrm_input_iterator{ibuf, ibuf, true};
  out = ndbxfrm_output_iterator{obuf, obuf + 1, false};
  require(zlib.deflate_init() == 0);
  for(int err = ndbxfrm_progress::need_more_input; err > 0;)
  {
    err = zlib.deflate(&out, &in);
    fprintf(stderr, "zlib.deflate() = %d total output %zu bytes last %d\n",
            err, out.begin() - obuf, out.last());
    if (err == ndbxfrm_progress::have_more_output && out.empty())
    {
      byte* oend = out.begin();
      require(oend != obuf + sizeof(obuf));
      out = ndbxfrm_output_iterator{oend, oend + 1, false};
    }
  }
  require(zlib.deflate_end() == 0);
  require(in.empty());
  require(out.last());
  fprintf(stderr, "zlib.deflate() empty input generate %zu bytes out\n",
          out.begin() - obuf);
  require(out.begin() == obuf + 2);
  fprintf(stderr, "zlib total in %jd out %jd\n",
          std::intmax_t{zlib.get_input_position()},
          std::intmax_t{zlib.get_output_position()});
  require(zlib.get_input_position() == 0);
  require(zlib.get_output_position() == 2);
  return 0;
}
#endif
