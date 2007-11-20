/* Dump the log from stdin to stdout. */
#include <stdio.h>
#include "brttypes.h"
#include "log-internal.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <zlib.h>
#include <assert.h>


u_int32_t crc=0;
u_int32_t actual_len=0;

int get_char(void) {
    int v = getchar();
    if (v==EOF) return v;
    unsigned char c = v;
    crc=crc32(crc, &c, 1);
    actual_len++;
    return v;
}

u_int32_t get_uint32 (void) {
    u_int32_t a = get_char();
    u_int32_t b = get_char();
    u_int32_t c = get_char();
    u_int32_t d = get_char();
    return (a<<24)|(b<<16)|(c<<8)|d;
}

u_int64_t get_uint64 (void) {
    u_int32_t hi = get_uint32();
    u_int32_t lo = get_uint32();
    return ((((long long)hi) << 32)
	    |
	    lo);
}

void transcribe_lsn (void) {
    long long value = get_uint64();
    printf(" lsn=%lld", value);
}

void transcribe_txnid (void) {
    long long value = get_uint64();
    printf(" txnid=%lld", value);
}

void transcribe_fileid (void) {
    u_int32_t value = get_uint32();
    printf(" fileid=%d", value);
}    


void transcribe_diskoff (void) {
    long long value = get_uint64();
    printf(" diskoff=%lld", value);
}    

void transcribe_crc32 (void) {
    u_int32_t oldcrc=crc;
    u_int32_t l = get_uint32();
    printf(" crc=%08x", l);
    assert(l==oldcrc);
}

void transcribe_mode (void) {
    u_int32_t value = get_uint32();
    printf(" mode=0%o", value);
}

void transcribe_len (void) {
    u_int32_t l = get_uint32();
    printf(" len=%d", l);
    if (l!=actual_len) printf(" actual_len=%d", actual_len);
    assert(l==actual_len);
}

void transcribe_key_or_data (char *what) {
    u_int32_t l = get_uint32();
    unsigned int i;
    printf(" %s(%d):\"", what, l);
    for (i=0; i<l; i++) {
	u_int32_t c = get_char();
	if (c=='\\') printf("\\\\");
	else if (c=='\n') printf("\\n");
	else if (c==' ') printf("\\ ");
	else if (c=='"') printf("\"\"");
	else if (isprint(c)) printf("%c", c);
	else printf("\\%02x", c);
    }
    printf("\"");
}
	

int main (int argc, char *argv[]) {
    int cmd;
    int count=-1;
    int i;
    if (argc>1) {
	count = atoi(argv[1]);
    }
    for (i=0;
	 i!=count && (crc=0,actual_len=0,cmd=get_char())!=EOF;
	 i++) {
	switch (cmd) {
	case LT_INSERT_WITH_NO_OVERWRITE:
	    printf("INSERT_WITH_NO_OVERWRITE:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_fileid();
	    transcribe_diskoff();
	    transcribe_key_or_data("key");
	    transcribe_key_or_data("data");
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    break;
    
	case LT_DELETE:
	    printf("DELETE:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_fileid();
	    transcribe_diskoff();
	    transcribe_key_or_data("key");
	    transcribe_key_or_data("data");
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    break;

	case LT_FCREATE:
	    printf("FCREATE:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_key_or_data("fname");
	    transcribe_mode();
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    break;

	case LT_COMMIT:
	    printf("COMMIT:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    break;

	default:
	    fprintf(stderr, "Huh?, found command %c\n", cmd);
	    assert(0);
	}
    }
    return 0;
}

