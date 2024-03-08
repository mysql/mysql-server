#include "hpack.h"
#include "config.h"
#include "hpack-data.h"

struct cno_header_table_t {
  struct cno_header_table_t *prev;
  struct cno_header_table_t *next;
  size_t k_size;
  size_t v_size;
  size_t refcnt;
  char data[];
};

void cno_hpack_free_header(struct cno_header_t *h) {
  if (h->flags & CNO_HEADER_OWNS_NAME) free((void *)h->name.data);
  if (h->flags & CNO_HEADER_OWNS_VALUE) free((void *)h->value.data);
  if (h->flags & CNO_HEADER_REFS_TABLE) {
    struct cno_header_table_t *entry =
        ((struct cno_header_table_t *)h->name.data) - 1;
    if (!--entry->refcnt) free(entry);
  }
  *h = CNO_HEADER_EMPTY;
}

void cno_hpack_init(struct cno_hpack_t *state, uint32_t limit) {
  state->last = state->first = (struct cno_header_table_t *)state;
  state->size = 0;
  state->limit = state->limit_upper = state->limit_update_min =
      state->limit_update_end = limit;
}

static void cno_hpack_evict(struct cno_hpack_t *state, uint32_t limit) {
  while (state->size > limit) {
    struct cno_header_table_t *entry = state->last;
    state->size -= entry->k_size + entry->v_size + 32;
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    if (!--entry->refcnt) free(entry);
  }
}

void cno_hpack_clear(struct cno_hpack_t *state) { cno_hpack_evict(state, 0); }

int cno_hpack_setlimit(struct cno_hpack_t *state, uint32_t limit) {
  if (limit > state->limit_upper)
    return CNO_ERROR(ASSERTION,
                     "dynamic table size limit higher than allowed by peer");
  // The update will be encoded the next time we send a header block.
  if (state->limit_update_min > limit)
    cno_hpack_evict(state, state->limit_update_min = limit);
  state->limit_update_end = limit;
  return CNO_OK;
}

static int cno_hpack_insert(struct cno_hpack_t *state,
                            const struct cno_header_t *h) {
  size_t recorded = h->name.size + h->value.size + 32;
  if (recorded > state->limit) {
    cno_hpack_evict(state, 0);
  } else {
    cno_hpack_evict(state, state->limit - recorded);

    size_t actual =
        h->name.size + h->value.size + sizeof(struct cno_header_table_t);
    struct cno_header_table_t *entry = malloc(actual);
    if (entry == NULL) return CNO_ERROR(NO_MEMORY, "%zu bytes", actual);

    memcpy(&entry->data[0], h->name.data, entry->k_size = h->name.size);
    memcpy(&entry->data[h->name.size], h->value.data,
           entry->v_size = h->value.size);
    entry->refcnt = 1;
    entry->prev = (struct cno_header_table_t *)state;
    entry->next = state->first;
    state->first->prev = entry;
    state->first = entry;
    state->size += recorded;
  }
  return CNO_OK;
}

static int cno_hpack_lookup(struct cno_hpack_t *state, size_t index,
                            struct cno_header_t *out) {
  if (index == 0) return CNO_ERROR(PROTOCOL, "header index 0 is reserved");

  if (index <= CNO_HPACK_STATIC_TABLE_SIZE) {
    out->name = CNO_HPACK_STATIC_TABLE[index - 1].name;
    out->value = CNO_HPACK_STATIC_TABLE[index - 1].value;
    return CNO_OK;
  }

  struct cno_header_table_t *hdr = (struct cno_header_table_t *)state;
  for (index -= CNO_HPACK_STATIC_TABLE_SIZE; index--;)
    if ((hdr = hdr->next) == (struct cno_header_table_t *)state)
      return CNO_ERROR(PROTOCOL, "dynamic table index out of bounds");

  out->name = (struct cno_buffer_t){&hdr->data[0], hdr->k_size};
  out->value = (struct cno_buffer_t){&hdr->data[hdr->k_size], hdr->v_size};
  out->flags |= CNO_HEADER_REFS_TABLE;
  hdr->refcnt++;
  return CNO_OK;
}

// Return value is either 0 (not found), index (name match), or -index (full
// match).
static int cno_hpack_lookup_inverse(struct cno_hpack_t *state,
                                    const struct cno_header_t *needle) {
#define TRY(k, v)                                   \
  do {                                              \
    if (!cno_buffer_eq(needle->name, k)) break;     \
    if (cno_buffer_eq(needle->value, v)) return -i; \
    if (possible == 0) possible = i;                \
  } while (0)

  int i = 1, possible = 0;
  for (const struct cno_header_t *h = CNO_HPACK_STATIC_TABLE;
       i <= CNO_HPACK_STATIC_TABLE_SIZE; ++h, ++i)
    TRY(h->name, h->value);
  for (const struct cno_header_table_t *t = state->first;
       t != (struct cno_header_table_t *)state; t = t->next, ++i)
    TRY(((struct cno_buffer_t){&t->data[0], t->k_size}),
        ((struct cno_buffer_t){&t->data[t->k_size], t->v_size}));
  return possible;

#undef TRY
}

// Format: the value as 1 byte if less than mask, else {mask, 0x80 | <7 bits>,
// ..., <last 7 bits>} (little-endian).
static int cno_hpack_decode_uint(struct cno_buffer_t *source, uint8_t mask,
                                 size_t *out) {
  if (!source->size) return CNO_ERROR(PROTOCOL, "expected uint, got EOF");

  const uint8_t *src = (const uint8_t *)source->data;
  const uint8_t head = *out = *src++ & mask;
  uint8_t size = 1;

  if (head == mask) {
    do {
      if (size == source->size)
        return CNO_ERROR(PROTOCOL, "truncated multi-byte uint");
      if (size == sizeof(size_t))
        return CNO_ERROR(PROTOCOL, "uint literal too large");
      *out += (*src & 0x7Ful) << (7 * size++ - 7);
    } while (*src++ & 0x80);
  }

  *source = cno_buffer_shift(*source, size);
  return CNO_OK;
}

// Format: 1 bit is a flag for Huffman encoding, then a varint for length, then
// raw data. In HPACK, mask is always 0xFF, i.e. a string starts on a byte
// boundary. QPACK allows unaligned strings.
static int cno_hpack_decode_string(struct cno_buffer_t *source, uint8_t mask,
                                   struct cno_buffer_t *out, int *borrow) {
  if (!source->size) return CNO_ERROR(PROTOCOL, "expected string, got EOF");
  const uint8_t huffman =
      (*(const uint8_t *)source->data) & (mask ^ (mask >> 1));

  size_t length = 0;
  if (cno_hpack_decode_uint(source, mask >> 1, &length)) return CNO_ERROR_UP();
  if (length > source->size)
    return CNO_ERROR(PROTOCOL, "expected %zu octets, got %zu", length,
                     source->size);

  if (length && huffman) {
    uint8_t *buf = malloc(length * 8 / CNO_HUFFMAN_MIN_BITS_PER_CHAR);
    uint8_t *ptr = buf;
    if (!buf) return CNO_ERROR(NO_MEMORY, "%zu bytes", length * 2);

    struct cno_huffman_state_t state = CNO_HUFFMAN_STATE_INIT;
    for (const uint8_t *p = (const uint8_t *)source->data, *e = length + p;
         p != e; p++) {
      state = CNO_HUFFMAN_STATE[state.next << 8 | *p];
      if (state.emit1) *ptr++ = state.byte1;
      if (state.emit2) *ptr++ = state.byte2;
    }
    if (!state.accept) {
      free(buf);
      return CNO_ERROR(PROTOCOL, "invalid or truncated Huffman code");
    }

    out->data = (char *)buf;
    out->size = ptr - buf;
  } else {
    out->data = source->data;
    out->size = length;
    *borrow = 1;
  }

  *source = cno_buffer_shift(*source, length);
  return CNO_OK;
}

static int cno_hpack_decode_one(struct cno_hpack_t *state,
                                struct cno_buffer_t *source,
                                struct cno_header_t *target) {
  *target = CNO_HEADER_EMPTY;

  const uint8_t head = *(const uint8_t *)source->data;
  size_t index = 0;
  if (head >= 0x80) {
    // 1....... -- name & value taken from the table
    return cno_hpack_decode_uint(source, 0x7F, &index) ||
           cno_hpack_lookup(state, index, target);
  } else if (head >= 0x40) {
    // 01...... -- name taken from the table, value included as a literal
    if (cno_hpack_decode_uint(source, 0x3F, &index)) return CNO_ERROR_UP();
  } else if (head >= 0x20) {
    // 001..... -- table size limit update; see cno_hpack_decode
    return CNO_ERROR(PROTOCOL, "unexpected table size limit update");
  } else {
    // 000x.... -- same as 0x40, but we shouldn't insert this header into the
    // table. `x` is set if proxies must use the same encoding.
    target->flags |= CNO_HEADER_NOT_INDEXED;
    if (cno_hpack_decode_uint(source, 0x0F, &index)) return CNO_ERROR_UP();
  }

  if (index == 0) {
    int borrow = 0;
    if (cno_hpack_decode_string(source, 0xFF, &target->name, &borrow))
      return CNO_ERROR_UP();
    if (!borrow) target->flags |= CNO_HEADER_OWNS_NAME;
  } else {
    if (cno_hpack_lookup(state, index, target)) return CNO_ERROR_UP();
  }

  int borrow = 0;
  if (cno_hpack_decode_string(source, 0xFF, &target->value, &borrow))
    return CNO_ERROR_UP();
  if (!borrow) target->flags |= CNO_HEADER_OWNS_VALUE;

  return target->flags & CNO_HEADER_NOT_INDEXED
             ? CNO_OK
             : cno_hpack_insert(state, target);
}

int cno_hpack_decode(struct cno_hpack_t *state, struct cno_buffer_t buf,
                     struct cno_header_t *rs, size_t *n) {
  while (buf.size && ((*(const uint8_t *)buf.data) & 0xE0) == 0x20) {
    // 001..... -- a new size limit for the table
    size_t limit = 0;
    if (cno_hpack_decode_uint(&buf, 0x1F, &limit)) return CNO_ERROR_UP();
    cno_hpack_evict(state, state->limit = limit);
  }

  if (state->limit > state->limit_upper)
    return CNO_ERROR(
        PROTOCOL,
        "current decoder state size limit is higher than the upper bound");

  size_t read = 0, limit = *n;
  while (buf.size && read < limit) {
    if (!cno_hpack_decode_one(state, &buf, &rs[read++])) continue;
    while (read) cno_hpack_free_header(&rs[--read]);
    return CNO_ERROR_UP();
  }
  if (buf.size) {
    while (read) cno_hpack_free_header(&rs[--read]);
    return CNO_ERROR(PROTOCOL, "header list too long");
  }
  *n = read;
  return CNO_OK;
}

static int cno_hpack_encode_uint(struct cno_buffer_dyn_t *buf, uint8_t prefix,
                                 uint8_t mask, size_t num) {
  if (num < mask) {
    prefix |= num;
    return cno_buffer_dyn_concat(buf,
                                 (struct cno_buffer_t){(char *)&prefix, 1});
  }

  uint8_t tmp[sizeof(num) * 2], *ptr = tmp;
  *ptr++ = prefix | mask;
  for (num -= mask; num > 0x7F; num >>= 7) *ptr++ = num | 0x80;
  *ptr++ = num;
  return cno_buffer_dyn_concat(buf,
                               (struct cno_buffer_t){(char *)tmp, ptr - tmp});
}

static int cno_hpack_encode_string(struct cno_buffer_dyn_t *buf, uint8_t prefix,
                                   uint8_t mask, const struct cno_buffer_t s) {
  size_t total = 0;
  for (const uint8_t *p = (const uint8_t *)s.data, *e = p + s.size; p != e; p++)
    total += CNO_HUFFMAN_LEN[*p];
  total = (total + 7) / 8;

  if (total >= s.size)
    return cno_hpack_encode_uint(buf, prefix, mask >> 1, s.size) ||
           cno_buffer_dyn_concat(buf, s);

  if (cno_hpack_encode_uint(buf, prefix | (mask ^ (mask >> 1)), mask >> 1,
                            total) ||
      cno_buffer_dyn_reserve(buf, buf->size + total))
    return CNO_ERROR_UP();

  uint8_t *out = (uint8_t *)buf->data + buf->size;
  uint64_t code = 0;
  int bits = 0;
  for (const uint8_t *p = (const uint8_t *)s.data, *e = p + s.size; p != e;
       p++) {
    code = CNO_HUFFMAN_ENC[*p] | code << CNO_HUFFMAN_LEN[*p];
    bits += CNO_HUFFMAN_LEN[*p];
    if (bits >= 32) {
      uint32_t part = code >> (bits -= 32);
      // FIXME? this is not compiled into a single move because of misalignment
      *out++ = part >> 24;
      *out++ = part >> 16;
      *out++ = part >> 8;
      *out++ = part;
    }
  }
  // At most 31 bits left to flush
  if (bits >= 8) *out++ = code >> (bits -= 8);
  if (bits >= 8) *out++ = code >> (bits -= 8);
  if (bits >= 8) *out++ = code >> (bits -= 8);
  // The remaining byte must be padded with ones
  if (bits) *out++ = 0xff >> bits | code << (8 - bits);

  buf->size += total;
  return CNO_OK;
}

static int cno_hpack_encode_one(struct cno_hpack_t *state,
                                struct cno_buffer_dyn_t *buf,
                                const struct cno_header_t *h) {
  int index = cno_hpack_lookup_inverse(state, h);
  if (index < 0) return cno_hpack_encode_uint(buf, 0x80, 0x7F, -index);

  if (h->flags & CNO_HEADER_NOT_INDEXED
          ? cno_hpack_encode_uint(buf, 0x10, 0x0F, index)
          : cno_hpack_encode_uint(buf, 0x40, 0x3F, index) ||
                cno_hpack_insert(state, h))
    return CNO_ERROR_UP();

  if (!index && cno_hpack_encode_string(buf, 0, 0xFF, h->name))
    return CNO_ERROR_UP();

  return cno_hpack_encode_string(buf, 0, 0xFF, h->value);
}

int cno_hpack_encode(struct cno_hpack_t *state, struct cno_buffer_dyn_t *buf,
                     const struct cno_header_t *headers, size_t n) {
  // Force the other side to evict the same number of entries first...
  if (state->limit != state->limit_update_min)
    if (cno_hpack_encode_uint(buf, 0x20, 0x1F,
                              state->limit = state->limit_update_min))
      return CNO_ERROR_UP();
  // ...then set the limit to its actual value.
  if (state->limit != state->limit_update_end)
    if (cno_hpack_encode_uint(
            buf, 0x20, 0x1F,
            state->limit = state->limit_update_min = state->limit_update_end))
      return CNO_ERROR_UP();

  while (n--)
    if (cno_hpack_encode_one(state, buf, headers++)) return CNO_ERROR_UP();
  return CNO_OK;
}
