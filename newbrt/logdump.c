/* Dump the log from stdin to stdout. */
#include <stdio.h>
#include "brttypes.h"
#include "log-internal.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>


u_int32_t get_uint32 (void) {
    u_int32_t a = getchar();
    u_int32_t b = getchar();
    u_int32_t c = getchar();
    u_int32_t d = getchar();
    return (a<<24)|(b<<16)|(c<<8)|d;
}

u_int64_t get_uint64 (void) {
    u_int32_t hi = get_uint32();
    u_int32_t lo = get_uint32();
    return ((((long long)hi) << 32)
	    |
	    lo);
}

void transcribe_txnid (void) {
    long long value = get_uint64();
    printf(" txnid=%lld", value);
}

void transcribe_diskoff (void) {
    long long value = get_uint64();
    printf(" diskoff=%lld", value);
}    

void transcribe_key_or_data (char *what) {
    u_int32_t l = get_uint32();
    unsigned int i;
    printf(" %s(%d):\"", what, l);
    for (i=0; i<l; i++) {
	u_int32_t c = getchar();
	if (c=='\\') printf("\\\\");
	else if (c=='\n') printf("\\n");
	else if (c==' ') printf("\\ ");
	else if (c=='"') printf("\"\"");
	else if (isprint(c)) printf("%c", c);
	else printf("\\%02x", c);
    }
    printf("\"");
}
	

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int cmd;
    while ((cmd=getchar())!=EOF) {
	switch (cmd) {
	case LT_INSERT_WITH_NO_OVERWRITE:
	    printf("INSERT_WITH_NO_OVERWRITE:");
	    transcribe_txnid();
	    transcribe_diskoff();
	    transcribe_key_or_data("key");
	    transcribe_key_or_data("data");
	    printf("\n");
	    break;
    
	case LT_DELETE:
	    printf("DELETE:");
	    transcribe_txnid();
	    transcribe_diskoff();
	    transcribe_key_or_data("key");
	    transcribe_key_or_data("data");
	    printf("\n");
	    break;

	case LT_COMMIT:
	    printf("COMMIT:");
	    transcribe_txnid();
	    printf("\n");
	    break;

	default:
	    printf("Huh?");
	    abort();
	}
    }
    return 0;
}
