/* An example of using lzma.  Written by Bradley Tue Mar  6 2012 */
/* Read stdin in blocks of size up to 1MB, compress it and write it to stdout.
 * Each output block is preceeded by two 4-byte numbers that says how big the compressed block is and how big the uncompressed block is.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <lzma.h>

float tdiff (struct timeval *start, struct timeval *end) {
    return (end->tv_sec-start->tv_sec) +1e-6*(end->tv_usec - start->tv_usec);
}

int main (int argc, char *argv[] __attribute__((__unused__)))
{
    assert(argc==1);
    const size_t in_max = 1024*1024;
    uint8_t *in_data = malloc(in_max);
    uint8_t *reconstituted = malloc(in_max);
    assert(in_data);

    size_t out_current = 0;
    uint8_t *out_data = NULL;

    double compress_time = 0, decompress_time = 0;

    while (1) {
	size_t in_size = fread(in_data, 1, in_max, stdin);
	if (in_size==0) {
	    assert(feof(stdin));
	    break;
	}
	printf("Got %ld bytes\n", in_size);

	size_t out_bound = lzma_stream_buffer_bound(in_size);
	printf("out bound = %ld\n", out_bound);
	if (out_current < out_bound) {
	    out_data = realloc(out_data, out_bound);
	    out_current = out_bound;
	}
	
	size_t compressed_size;
	{
	    struct timeval tstart,tend;
	    gettimeofday(&tstart, NULL);
	    size_t out_pos = 0;
	    lzma_ret r = lzma_easy_buffer_encode(6, LZMA_CHECK_CRC32, NULL,
						 in_data, in_size,
						 out_data, &out_pos,  out_current);
	    gettimeofday(&tend, NULL);
	    compress_time += tdiff(&tstart, &tend);
	    printf("r=%d\n", r);
	    assert(r==LZMA_OK);
	    printf("out size = %ld\n", out_pos);
	    compressed_size = out_pos;
	}
	{
	    struct timeval tstart,tend;
	    gettimeofday(&tstart, NULL);
	    uint64_t memlimit = UINT64_MAX;
	    size_t compressed_pos = 0;
	    size_t reconstituted_pos = 0;
	    lzma_ret r = lzma_stream_buffer_decode(&memlimit,
						   0,
						   NULL,
						   out_data,
						   &compressed_pos, compressed_size,
						   reconstituted, &reconstituted_pos, in_max);
	    gettimeofday(&tend, NULL);
	    decompress_time += tdiff(&tstart, &tend);
	    printf("r=%d\n", r);
	    assert(r==LZMA_OK);
	}
	
    }

    printf("Compression time  = %9.6fs\n", compress_time);
    printf("Decmpression time = %9.6fs\n", decompress_time);
    free(in_data);
    free(out_data);
    free(reconstituted);
    return 0;
}
