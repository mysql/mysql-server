/* This file defines the logformat in an executable fashion.
 * This code is used to generate
 *   The code that writes into the log.
 *   The code that reads the log and prints it to stdout (the log_print utility)
 *   The code that reads the log for recovery.
 *   The struct definitions.
 *   The Latex documentation.
 */
#include <stdio.h>
#include <assert.h>

typedef struct field {
    char *type;
    char *name;
} F;

#define NULLFIELD {0,0}
#define FA (F[])

struct logtype {
    char *name;
    char command;
    struct field *fields;
};

// In the fields, don't mention the command, the LSN, the CRC or the trailing LEN.

const struct logtype logtypes[] = {
    {"delete", 'D', FA{{"FILENUM", "filenum"},
		       {"DISKOFF", "diskoff"},
		       {"BYTESTRING", "key"},
		       {"BYTESTRING", "data"},
		       NULLFIELD}},
    {"commit", 'C', FA{{"TXNID", "txnid"},NULLFIELD}},
    {0,0,FA{NULLFIELD}}
};
    
#define DO_LOGTYPES(lt, body) ({ \
    const struct logtype *lt;    \
    for (lt=&logtypes[0]; lt->name; lt++) { \
	body; \
    } })

#define DO_FIELDS(fld, lt, body) ({ \
    struct field *fld; \
    for (fld=lt->fields; fld->type; fld++) { \
        body; \
    } })

void generate_log_struct(void) {
    FILE *f = fopen("log_struct.h", "w");
    assert(f!=0);
    DO_LOGTYPES(lt,
		({  fprintf(f, "struct logtype_%s {\n", lt->name);
		    fprintf(f, "  %-16s lsn;\n", "LSN");
		    DO_FIELDS(ft, lt,
			      fprintf(f, "  %-16s %s;\n", ft->type, ft->name));
		    fprintf(f, "  %-16s crc;\n", "u_int_32");
		    fprintf(f, "  %-16s len;\n", "u_int_32");
		    fprintf(f, "};\n");
		}));
    int r=fclose(f);
    assert(r==0);
}

void generate_log_writer(void) {
    FILE *f = fopen("log_write.c", "w");
    assert(f!=0);
    DO_LOGTYPES(lt, ({
			fprintf(f, "int toku_log_%s (TOKULOGGE logger, LSN lsn", lt->name);
			DO_FIELDS(ft, lt,
				  fprintf(f, ", %s %s", ft->type, ft->name));
			fprintf(f, ") {\n");
			fprintf(f, "  const int buflen= (1 // log command\n");
			fprintf(f, "                     +8 // lsn\n");
			DO_FIELDS(ft, lt,
				  fprintf(f, "                     +toku_logsizeof_%s(%s)\n", ft->type, ft->name));
			fprintf(f, "                     +8 // crc + len\n");
			fprintf(f, "                     );\n");
			fprintf(f, "  struct wbuf wbuf;\n");
			fprintf(f, "  const char *buf = toku_malloc(buflen);\n");
			fprintf(f, "  if (buf==0) return errno;\n");
			fprintf(f, "  wbuf_init(&wbuf, buf, buflen)\n");
			fprintf(f, "  wbuf_char(&wbuf, '%c');\n", lt->command);
			fprintf(f, "  wbuf_lsn(&wbuf, lsn);\n");
			DO_FIELDS(ft, lt,
				  fprintf(f, "  wbuf_%s(&wbuf, %s);\n", ft->type, ft->name));
			fprintf(f, "  int r= tokulogger_finish(logger, &wbuf);\n");
			fprintf(f, "  assert(buf.ndone==buflen);\n");
			fprintf(f, "  toku_free(buf);\n");
			if (lt->command=='C') {
			    fprintf(f, "  if (r!=0) return r;\n");
			    fprintf(f, "  // commit has some extra work to do.\n");
			    fprintf(f, "  if (txn->parent) return 0; // don't fsync if there is a parent.\n");
			    fprintf(f, "  else return tokulogger_fsync(logger);\n");
			} else {
			    fprintf(f, "  return r;\n");
			}
			fprintf(f, "}\n\n");
		    }));

    int r=fclose(f);
    assert(r==0);
}


int main (int argc __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    generate_log_struct();
    generate_log_writer();
    return 0;
}
   
