/* Dump the log from stdin to stdout. */
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <zlib.h>
#include <assert.h>

#include "brttypes.h"
#include "log-internal.h"
#include "log_header.h"

u_int32_t crc=0;
u_int32_t actual_len=0;

int get_char(void) {
    int v = getchar();
    if (v==EOF) return v;
    unsigned char c = v;
    crc=toku_crc32(crc, &c, 1);
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

void transcribe_filenum(void) {
    u_int32_t value = get_uint32();
    printf(" filenum=%d", value);
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
	
void transcribe_header (void) {
    printf(" {size=%d", get_uint32());
    printf(" flags=%d", get_uint32());
    printf(" nodesize=%d", get_uint32());
    printf(" freelist=%lld", get_uint64());
    printf(" unused_memory=%lld",get_uint64());
    int n_roots=get_uint32();
    printf(" n_named_roots=%d", n_roots);
    if (n_roots>0) {
	abort();
    } else {
	printf(" root=%lld", get_uint64());
    }
    printf("}");
}

void read_and_print_magic (void) {
    {
	char magic[8];
	int r=fread(magic, 1, 8, stdin);
	if (r!=8) {
	    fprintf(stderr, "Couldn't read the magic\n");
	    exit(1);
	}
	if (memcmp(magic, "tokulogg", 8)!=0) {
	    fprintf(stderr, "Magic is wrong.\n");
	    exit(1);
	}
    }
    {
	int version;
    	int r=fread(&version, 1, 4, stdin);
	if (r!=4) {
	    fprintf(stderr, "Couldn't read the version\n");
	    exit(1);
	}
	printf("tokulog v.%d\n", ntohl(version));
    }
}

static void newmain (int count) {
    int i;
    read_and_print_magic();
    for (i=0; i!=count; i++) {
	int r = toku_logprint_one_record(stdout, stdin);
	if (r==EOF) break;
	if (r!=0) {
	    fflush(stdout);
	    fprintf(stderr, "Problem in log err=%d\n", r);
	    exit(1);
	}
    }
}

static void oldmain (int count) {
    int cmd;
    int i;
    read_and_print_magic();
    for (i=0;
	 i!=count && (crc=0,actual_len=0,cmd=get_char())!=EOF;
	 i++) {
	switch ((enum lt_command)cmd) {
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
	    goto next;
    
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
	    goto next;

	case LT_FCREATE:
	    printf("FCREATE:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_key_or_data("fname");
	    transcribe_mode();
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    goto next;

	case LT_FOPEN:
	    printf("FOPEN:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_key_or_data("fname");
	    transcribe_filenum();
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    goto next;
	    
	case LT_COMMIT:
	    printf("COMMIT:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    goto next;

	case LT_FHEADER:
	    printf("HEADER:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_filenum();
	    transcribe_header();
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    goto next;

	case LT_NEWBRTNODE:
	    printf("NEWBRTNODE:");
	    transcribe_lsn();
	    transcribe_txnid();
	    transcribe_filenum();
	    transcribe_diskoff();
	    printf(" height=%d", get_uint32());
	    printf(" nodesize=%d", get_uint32());
	    printf(" is_dup_sort=%d", get_char());
	    printf(" rand=%u", get_uint32());
	    transcribe_crc32();
	    transcribe_len();
	    printf("\n");
	    goto next;

	case LT_UNLINK:
	case LT_BLOCK_RENAME:
	case LT_CHECKPOINT:
	    fprintf(stderr, "Cannot handle this command yet: '%c'\n", cmd);
	    break;
	}
	/* The default is to fall out the bottom.  That way we can get a compiler warning if we forget one of the enums, but we can also
	 * get a runtime warning if the actual value isn't one of the enums. */
	fprintf(stderr, "Huh?, found command '%c'\n", cmd);
	assert(0);
    next: ; /*nothing*/
    }
}

int main (int argc, char *argv[]) {
    int count=-1;
    int oldcode=0;
    while (argc>1) {
	if (strcmp(argv[1], "--oldcode")==0) {
	    oldcode=1;
	} else {
	    count = atoi(argv[1]);
	}
	argc--; argv++;
    }
    if (oldcode) oldmain(count);
    else newmain(count);
    return 0;
}

