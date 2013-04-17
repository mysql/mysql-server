/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
///////////////////////////////////////////////////////////////////////////////
//
/// \file       test_index.c
/// \brief      Tests functions handling the lzma_index structure
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "tests.h"

#define MEMLIMIT (LZMA_VLI_C(1) << 20)


static lzma_index *
create_empty(void)
{
	lzma_index *i = lzma_index_init(NULL, NULL);
	expect(i != NULL);
	return i;
}


static lzma_index *
create_small(void)
{
	lzma_index *i = lzma_index_init(NULL, NULL);
	expect(i != NULL);
	expect(lzma_index_append(i, NULL, 101, 555) == LZMA_OK);
	expect(lzma_index_append(i, NULL, 602, 777) == LZMA_OK);
	expect(lzma_index_append(i, NULL, 804, 999) == LZMA_OK);
	return i;
}


static lzma_index *
create_big(void)
{
	lzma_index *i = lzma_index_init(NULL, NULL);
	expect(i != NULL);

	lzma_vli total_size = 0;
	lzma_vli uncompressed_size = 0;

	// Add pseudo-random sizes (but always the same size values).
	const size_t count = 5555;
	uint32_t n = 11;
	for (size_t j = 0; j < count; ++j) {
		n = 7019 * n + 7607;
		const uint32_t t = n * 3011;
		expect(lzma_index_append(i, NULL, t, n) == LZMA_OK);
		total_size += (t + 3) & ~LZMA_VLI_C(3);
		uncompressed_size += n;
	}

	expect(lzma_index_count(i) == count);
	expect(lzma_index_total_size(i) == total_size);
	expect(lzma_index_uncompressed_size(i) == uncompressed_size);
	expect(lzma_index_total_size(i) + lzma_index_size(i)
				+ 2 * LZMA_STREAM_HEADER_SIZE
			== lzma_index_stream_size(i));

	return i;
}


static void
test_equal(void)
{
	lzma_index *a = create_empty();
	lzma_index *b = create_small();
	lzma_index *c = create_big();
	expect(a && b && c);

	expect(lzma_index_equal(a, a));
	expect(lzma_index_equal(b, b));
	expect(lzma_index_equal(c, c));

	expect(!lzma_index_equal(a, b));
	expect(!lzma_index_equal(a, c));
	expect(!lzma_index_equal(b, c));

	lzma_index_end(a, NULL);
	lzma_index_end(b, NULL);
	lzma_index_end(c, NULL);
}


static void
test_overflow(void)
{
	// Integer overflow tests
	lzma_index *i = create_empty();

	expect(lzma_index_append(i, NULL, LZMA_VLI_MAX - 5, 1234)
			== LZMA_DATA_ERROR);

	// TODO

	lzma_index_end(i, NULL);
}


static void
test_copy(const lzma_index *i)
{
	lzma_index *d = lzma_index_dup(i, NULL);
	expect(d != NULL);
	lzma_index_end(d, NULL);
}


static void
test_read(lzma_index *i)
{
	lzma_index_record record;

	// Try twice so we see that rewinding works.
	for (size_t j = 0; j < 2; ++j) {
		lzma_vli total_size = 0;
		lzma_vli uncompressed_size = 0;
		lzma_vli stream_offset = LZMA_STREAM_HEADER_SIZE;
		lzma_vli uncompressed_offset = 0;
		uint32_t count = 0;

		while (!lzma_index_read(i, &record)) {
			++count;

			total_size += record.total_size;
			uncompressed_size += record.uncompressed_size;

			expect(record.stream_offset == stream_offset);
			expect(record.uncompressed_offset
					== uncompressed_offset);

			stream_offset += record.total_size;
			uncompressed_offset += record.uncompressed_size;
		}

		expect(lzma_index_total_size(i) == total_size);
		expect(lzma_index_uncompressed_size(i) == uncompressed_size);
		expect(lzma_index_count(i) == count);

		lzma_index_rewind(i);
	}
}


static void
test_code(lzma_index *i)
{
	const size_t alloc_size = 128 * 1024;
	uint8_t *buf = malloc(alloc_size);
	expect(buf != NULL);

	// Encode
	lzma_stream strm = LZMA_STREAM_INIT;
	expect(lzma_index_encoder(&strm, i) == LZMA_OK);
	const lzma_vli index_size = lzma_index_size(i);
	succeed(coder_loop(&strm, NULL, 0, buf, index_size,
			LZMA_STREAM_END, LZMA_RUN));

	// Decode
	lzma_index *d;
	expect(lzma_index_decoder(&strm, &d, MEMLIMIT) == LZMA_OK);
	succeed(decoder_loop(&strm, buf, index_size));

	expect(lzma_index_equal(i, d));

	lzma_index_end(d, NULL);
	lzma_end(&strm);

	// Decode with hashing
	lzma_index_hash *h = lzma_index_hash_init(NULL, NULL);
	expect(h != NULL);
	lzma_index_rewind(i);
	lzma_index_record r;
	while (!lzma_index_read(i, &r))
		expect(lzma_index_hash_append(h, r.unpadded_size,
				r.uncompressed_size) == LZMA_OK);
	size_t pos = 0;
	while (pos < index_size - 1)
		expect(lzma_index_hash_decode(h, buf, &pos, pos + 1)
				== LZMA_OK);
	expect(lzma_index_hash_decode(h, buf, &pos, pos + 1)
			== LZMA_STREAM_END);

	lzma_index_hash_end(h, NULL);

	// Encode buffer
	size_t buf_pos = 1;
	expect(lzma_index_buffer_encode(i, buf, &buf_pos, index_size)
			== LZMA_BUF_ERROR);
	expect(buf_pos == 1);

	succeed(lzma_index_buffer_encode(i, buf, &buf_pos, index_size + 1));
	expect(buf_pos == index_size + 1);

	// Decode buffer
	buf_pos = 1;
	uint64_t memlimit = MEMLIMIT;
	expect(lzma_index_buffer_decode(&d, &memlimit, NULL, buf, &buf_pos,
			index_size) == LZMA_DATA_ERROR);
	expect(buf_pos == 1);
	expect(d == NULL);

	succeed(lzma_index_buffer_decode(&d, &memlimit, NULL, buf, &buf_pos,
			index_size + 1));
	expect(buf_pos == index_size + 1);
	expect(lzma_index_equal(i, d));

	lzma_index_end(d, NULL);

	free(buf);
}


static void
test_many(lzma_index *i)
{
	test_copy(i);
	test_read(i);
	test_code(i);
}


static void
test_cat(void)
{
	lzma_index *a, *b, *c;

	// Empty Indexes
	a = create_empty();
	b = create_empty();
	expect(lzma_index_cat(a, b, NULL, 0) == LZMA_OK);
	expect(lzma_index_count(a) == 0);
	expect(lzma_index_stream_size(a) == 2 * LZMA_STREAM_HEADER_SIZE + 8);
	expect(lzma_index_file_size(a)
			== 2 * (2 * LZMA_STREAM_HEADER_SIZE + 8));

	b = create_empty();
	expect(lzma_index_cat(a, b, NULL, 0) == LZMA_OK);
	expect(lzma_index_count(a) == 0);
	expect(lzma_index_stream_size(a) == 2 * LZMA_STREAM_HEADER_SIZE + 8);
	expect(lzma_index_file_size(a)
			== 3 * (2 * LZMA_STREAM_HEADER_SIZE + 8));

	b = create_empty();
	c = create_empty();
	expect(lzma_index_cat(b, c, NULL, 4) == LZMA_OK);
	expect(lzma_index_count(b) == 0);
	expect(lzma_index_stream_size(b) == 2 * LZMA_STREAM_HEADER_SIZE + 8);
	expect(lzma_index_file_size(b)
			== 2 * (2 * LZMA_STREAM_HEADER_SIZE + 8) + 4);

	expect(lzma_index_cat(a, b, NULL, 8) == LZMA_OK);
	expect(lzma_index_count(a) == 0);
	expect(lzma_index_stream_size(a) == 2 * LZMA_STREAM_HEADER_SIZE + 8);
	expect(lzma_index_file_size(a)
			== 5 * (2 * LZMA_STREAM_HEADER_SIZE + 8) + 4 + 8);

	lzma_index_end(a, NULL);

	// Small Indexes
	a = create_small();
	lzma_vli stream_size = lzma_index_stream_size(a);
	b = create_small();
	expect(lzma_index_cat(a, b, NULL, 4) == LZMA_OK);
	expect(lzma_index_file_size(a) == stream_size * 2 + 4);
	expect(lzma_index_stream_size(a) > stream_size);
	expect(lzma_index_stream_size(a) < stream_size * 2);

	b = create_small();
	c = create_small();
	expect(lzma_index_cat(b, c, NULL, 8) == LZMA_OK);
	expect(lzma_index_cat(a, b, NULL, 12) == LZMA_OK);
	expect(lzma_index_file_size(a) == stream_size * 4 + 4 + 8 + 12);

	lzma_index_end(a, NULL);

	// Big Indexes
	a = create_big();
	stream_size = lzma_index_stream_size(a);
	b = create_big();
	expect(lzma_index_cat(a, b, NULL, 4) == LZMA_OK);
	expect(lzma_index_file_size(a) == stream_size * 2 + 4);
	expect(lzma_index_stream_size(a) > stream_size);
	expect(lzma_index_stream_size(a) < stream_size * 2);

	b = create_big();
	c = create_big();
	expect(lzma_index_cat(b, c, NULL, 8) == LZMA_OK);
	expect(lzma_index_cat(a, b, NULL, 12) == LZMA_OK);
	expect(lzma_index_file_size(a) == stream_size * 4 + 4 + 8 + 12);

	lzma_index_end(a, NULL);
}


static void
test_locate(void)
{
	lzma_index_record r;
	lzma_index *i = lzma_index_init(NULL, NULL);
	expect(i != NULL);

	// Cannot locate anything from an empty Index.
	expect(lzma_index_locate(i, &r, 0));
	expect(lzma_index_locate(i, &r, 555));

	// One empty Record: nothing is found since there's no uncompressed
	// data.
	expect(lzma_index_append(i, NULL, 16, 0) == LZMA_OK);
	expect(lzma_index_locate(i, &r, 0));

	// Non-empty Record and we can find something.
	expect(lzma_index_append(i, NULL, 32, 5) == LZMA_OK);
	expect(!lzma_index_locate(i, &r, 0));
	expect(r.total_size == 32);
	expect(r.uncompressed_size == 5);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 16);
	expect(r.uncompressed_offset == 0);

	// Still cannot find anything past the end.
	expect(lzma_index_locate(i, &r, 5));

	// Add the third Record.
	expect(lzma_index_append(i, NULL, 40, 11) == LZMA_OK);

	expect(!lzma_index_locate(i, &r, 0));
	expect(r.total_size == 32);
	expect(r.uncompressed_size == 5);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 16);
	expect(r.uncompressed_offset == 0);

	expect(!lzma_index_read(i, &r));
	expect(r.total_size == 40);
	expect(r.uncompressed_size == 11);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 16 + 32);
	expect(r.uncompressed_offset == 5);

	expect(!lzma_index_locate(i, &r, 2));
	expect(r.total_size == 32);
	expect(r.uncompressed_size == 5);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 16);
	expect(r.uncompressed_offset == 0);

	expect(!lzma_index_locate(i, &r, 5));
	expect(r.total_size == 40);
	expect(r.uncompressed_size == 11);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 16 + 32);
	expect(r.uncompressed_offset == 5);

	expect(!lzma_index_locate(i, &r, 5 + 11 - 1));
	expect(r.total_size == 40);
	expect(r.uncompressed_size == 11);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 16 + 32);
	expect(r.uncompressed_offset == 5);

	expect(lzma_index_locate(i, &r, 5 + 11));
	expect(lzma_index_locate(i, &r, 5 + 15));

	// Large Index
	i = lzma_index_init(i, NULL);
	expect(i != NULL);

	for (size_t n = 4; n <= 4 * 5555; n += 4)
		expect(lzma_index_append(i, NULL, n + 8, n) == LZMA_OK);

	expect(lzma_index_count(i) == 5555);

	// First Record
	expect(!lzma_index_locate(i, &r, 0));
	expect(r.total_size == 4 + 8);
	expect(r.uncompressed_size == 4);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE);
	expect(r.uncompressed_offset == 0);

	expect(!lzma_index_locate(i, &r, 3));
	expect(r.total_size == 4 + 8);
	expect(r.uncompressed_size == 4);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE);
	expect(r.uncompressed_offset == 0);

	// Second Record
	expect(!lzma_index_locate(i, &r, 4));
	expect(r.total_size == 2 * 4 + 8);
	expect(r.uncompressed_size == 2 * 4);
	expect(r.stream_offset == LZMA_STREAM_HEADER_SIZE + 4 + 8);
	expect(r.uncompressed_offset == 4);

	// Last Record
	expect(!lzma_index_locate(i, &r, lzma_index_uncompressed_size(i) - 1));
	expect(r.total_size == 4 * 5555 + 8);
	expect(r.uncompressed_size == 4 * 5555);
	expect(r.stream_offset == lzma_index_total_size(i)
			+ LZMA_STREAM_HEADER_SIZE - 4 * 5555 - 8);
	expect(r.uncompressed_offset
			== lzma_index_uncompressed_size(i) - 4 * 5555);

	// Allocation chunk boundaries. See INDEX_GROUP_SIZE in
	// liblzma/common/index.c.
	const size_t group_multiple = 256 * 4;
	const size_t radius = 8;
	const size_t start = group_multiple - radius;
	lzma_vli ubase = 0;
	lzma_vli tbase = 0;
	size_t n;
	for (n = 1; n < start; ++n) {
		ubase += n * 4;
		tbase += n * 4 + 8;
	}

	while (n < start + 2 * radius) {
		expect(!lzma_index_locate(i, &r, ubase + n * 4));

		expect(r.stream_offset == tbase + n * 4 + 8
				+ LZMA_STREAM_HEADER_SIZE);
		expect(r.uncompressed_offset == ubase + n * 4);

		tbase += n * 4 + 8;
		ubase += n * 4;
		++n;

		expect(r.total_size == n * 4 + 8);
		expect(r.uncompressed_size == n * 4);
	}

	// Do it also backwards since lzma_index_locate() uses relative search.
	while (n > start) {
		expect(!lzma_index_locate(i, &r, ubase + (n - 1) * 4));

		expect(r.total_size == n * 4 + 8);
		expect(r.uncompressed_size == n * 4);

		--n;
		tbase -= n * 4 + 8;
		ubase -= n * 4;

		expect(r.stream_offset == tbase + n * 4 + 8
				+ LZMA_STREAM_HEADER_SIZE);
		expect(r.uncompressed_offset == ubase + n * 4);
	}

	// Test locating in concatend Index.
	i = lzma_index_init(i, NULL);
	expect(i != NULL);
	for (n = 0; n < group_multiple; ++n)
		expect(lzma_index_append(i, NULL, 8, 0) == LZMA_OK);
	expect(lzma_index_append(i, NULL, 16, 1) == LZMA_OK);
	expect(!lzma_index_locate(i, &r, 0));
	expect(r.total_size == 16);
	expect(r.uncompressed_size == 1);
	expect(r.stream_offset
			== LZMA_STREAM_HEADER_SIZE + group_multiple * 8);
	expect(r.uncompressed_offset == 0);

	lzma_index_end(i, NULL);
}


static void
test_corrupt(void)
{
	const size_t alloc_size = 128 * 1024;
	uint8_t *buf = malloc(alloc_size);
	expect(buf != NULL);
	lzma_stream strm = LZMA_STREAM_INIT;

	lzma_index *i = create_empty();
	expect(lzma_index_append(i, NULL, 0, 1) == LZMA_PROG_ERROR);
	lzma_index_end(i, NULL);

	// Create a valid Index and corrupt it in different ways.
	i = create_small();
	expect(lzma_index_encoder(&strm, i) == LZMA_OK);
	succeed(coder_loop(&strm, NULL, 0, buf, 20,
			LZMA_STREAM_END, LZMA_RUN));
	lzma_index_end(i, NULL);

	// Wrong Index Indicator
	buf[0] ^= 1;
	expect(lzma_index_decoder(&strm, &i, MEMLIMIT) == LZMA_OK);
	succeed(decoder_loop_ret(&strm, buf, 1, LZMA_DATA_ERROR));
	buf[0] ^= 1;

	// Wrong Number of Records and thus CRC32 fails.
	--buf[1];
	expect(lzma_index_decoder(&strm, &i, MEMLIMIT) == LZMA_OK);
	succeed(decoder_loop_ret(&strm, buf, 10, LZMA_DATA_ERROR));
	++buf[1];

	// Padding not NULs
	buf[15] ^= 1;
	expect(lzma_index_decoder(&strm, &i, MEMLIMIT) == LZMA_OK);
	succeed(decoder_loop_ret(&strm, buf, 16, LZMA_DATA_ERROR));

	lzma_end(&strm);
	free(buf);
}


int
main(void)
{
	test_equal();

	test_overflow();

	lzma_index *i = create_empty();
	test_many(i);
	lzma_index_end(i, NULL);

	i = create_small();
	test_many(i);
	lzma_index_end(i, NULL);

	i = create_big();
	test_many(i);
	lzma_index_end(i, NULL);

	test_cat();

	test_locate();

	test_corrupt();

	return 0;
}
