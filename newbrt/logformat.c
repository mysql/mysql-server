/* This file defines the logformat in an executable fashion.
 * This code is used to generate
 *   The code that writes into the log.
 *   The code that reads the log and prints it to stdout (the log_print utility)
 *   The code that reads the log for recovery.
 *   The struct definitions.
 *   The Latex documentation.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

int logformat_version_number = 0;

const struct logtype logtypes[] = {
    {"commit", 'C', FA{{"TXNID", "txnid"},NULLFIELD}},
    {"delete", 'D', FA{{"FILENUM", "filenum"},
		       {"DISKOFF", "diskoff"},
		       {"BYTESTRING", "key"},
		       {"BYTESTRING", "data"},
		       NULLFIELD}},
    {"fcreate", 'F', FA{{"TXNID",      "txnid"},
			{"BYTESTRING", "fname"},
			{"u_int32_t",  "mode"},
			NULLFIELD}},
    {"fheader", 'H',    FA{{"TXNID",      "txnid"},
			   {"FILENUM",    "filenum"},
			   {"LOGGEDBRTHEADER",  "header"},
			   NULLFIELD}},
    {"newbrtnode", 'N', FA{{"TXNID",   "txnid"},
			   {"FILENUM", "filenum"},
			   {"DISKOFF", "diskoff"},
			   {"u_int32_t", "height"},
			   {"u_int32_t", "nodesize"},
			   {"u_int8_t", "is_dup_sort"},
			   {"u_int32_t", "rand4fingerprint"},
			   NULLFIELD}},
    {"fopen",   'O', FA{{"TXNID",      "txnid"},
			{"BYTESTRING", "fname"},
			{"FILENUM",    "filenum"},
			NULLFIELD}},
    
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

void fprintf2 (FILE *f1, FILE *f2, const char *format, ...) {
    va_list ap;
    int r;
    va_start(ap, format);
    r=vfprintf(f1, format, ap); assert(r>=0);
    r=vfprintf(f2, format, ap); assert(r>=0);
    va_end(ap);
}

FILE *hf=0, *cf=0;

void generate_lt_enum (void) {
    int count=0;
    fprintf(hf, "enum lt_cmd {");
    DO_LOGTYPES(lt,
		({
		    if (count!=0) fprintf(hf, ",");
		    count++;
		    fprintf(hf, "\n");
		    fprintf(hf,"    LT_%-16s = '%c'", lt->name, lt->command);
		}));
    fprintf(hf, "\n};\n\n");
}

void generate_log_struct (void) {
    DO_LOGTYPES(lt,
		({  fprintf(hf, "struct logtype_%s {\n", lt->name);
		    fprintf(hf, "  %-16s lsn;\n", "LSN");
		    DO_FIELDS(ft, lt,
			      fprintf(hf, "  %-16s %s;\n", ft->type, ft->name));
		    fprintf(hf, "  %-16s crc;\n", "u_int32_t");
		    fprintf(hf, "  %-16s len;\n", "u_int32_t");
		    fprintf(hf, "};\n");
		}));
    fprintf(hf, "struct log_entry {\n");
    fprintf(hf, "  enum lt_cmd cmd;\n");
    fprintf(hf, "  union {\n");
    DO_LOGTYPES(lt, fprintf(hf,"    struct logtype_%s %s;\n", lt->name, lt->name));
    fprintf(hf, "  } u;\n");
    fprintf(hf, "};\n");
}

void generate_log_writer (void) {
    DO_LOGTYPES(lt, ({
			fprintf2(cf, hf, "int toku_log_%s (TOKUTXN txn", lt->name);
			DO_FIELDS(ft, lt,
				  fprintf2(cf, hf, ", %s %s", ft->type, ft->name));
			fprintf(hf, ");\n");
			fprintf(cf, ") {\n");
			fprintf(cf, "  const unsigned int buflen= (1 // log command\n");
			fprintf(cf, "                              +8 // lsn\n");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "                              +toku_logsizeof_%s(%s)\n", ft->type, ft->name));
			fprintf(cf, "                              +8 // crc + len\n");
			fprintf(cf, "                     );\n");
			fprintf(cf, "  struct wbuf wbuf;\n");
			fprintf(cf, "  char *buf = toku_malloc(buflen);\n");
			fprintf(cf, "  if (buf==0) return errno;\n");
			fprintf(cf, "  wbuf_init(&wbuf, buf, buflen);\n");
			fprintf(cf, "  wbuf_char(&wbuf, '%c');\n", lt->command);
			fprintf(cf, "  wbuf_LSN(&wbuf, txn->logger->lsn);\n");
			fprintf(cf, "  txn->logger->lsn.lsn++;;\n");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "  wbuf_%s(&wbuf, %s);\n", ft->type, ft->name));
			fprintf(cf, "  int r= tokulogger_finish(txn->logger, &wbuf);\n");
			fprintf(cf, "  assert(wbuf.ndone==buflen);\n");
			fprintf(cf, "  toku_free(buf);\n");
			if (lt->command=='C') {
			    fprintf(cf, "  if (r!=0) return r;\n");
			    fprintf(cf, "  // commit has some extra work to do.\n");
			    fprintf(cf, "  if (txn->parent) return 0; // don't fsync if there is a parent.\n");
			    fprintf(cf, "  else return tokulogger_fsync(txn->logger);\n");
			} else {
			    fprintf(cf, "  return r;\n");
			}
			fprintf(cf, "}\n\n");
		    }));

}

void generate_log_reader (void) {
    DO_LOGTYPES(lt, ({
			fprintf2(cf, hf, "int tokulog_fread_%s (FILE *infile, struct logtype_%s *data, char cmd)", lt->name, lt->name);
			fprintf(hf, ";\n");
			fprintf(cf, " {\n");
			fprintf(cf, "  int r=0;\n");
			fprintf(cf, "  u_int32_t actual_len=1;\n");
			fprintf(cf, "  u_int32_t crc = toku_crc32(0, &cmd, 1);\n");
			fprintf(cf, "  r=toku_fread_%-16s(infile, &data->%-16s, &crc, &actual_len); if (r!=0) return r;\n", "LSN", "lsn");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "  r=toku_fread_%-16s(infile, &data->%-16s, &crc, &actual_len); if (r!=0) return r;\n", ft->type, ft->name));
			fprintf(cf, "  u_int32_t crc_in_file, len_in_file;\n");
			fprintf(cf, "  r=toku_fread_u_int32_t_nocrclen(infile, &crc_in_file);  actual_len+=4;   if (r!=0) return r;\n");
			fprintf(cf, "  r=toku_fread_u_int32_t_nocrclen(infile, &len_in_file);  actual_len+=4;   if (r!=0) return r;\n");
			fprintf(cf, "  if (crc_in_file!=crc || len_in_file!=actual_len) return DB_BADFORMAT;\n");
			fprintf(cf, "  return 0;\n");
			fprintf(cf, "}\n\n");
		    }));
    fprintf2(cf, hf, "int tokulog_fread (FILE *infile, struct log_entry *le)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n");
    fprintf(cf, "  int cmd=fgetc(infile);\n");
    fprintf(cf, "  if (cmd==EOF) return EOF;\n");
    fprintf(cf, "  le->cmd=cmd;\n");
    fprintf(cf, "  switch ((enum lt_cmd)cmd) {\n");
    DO_LOGTYPES(lt, ({
			fprintf(cf, "  case LT_%s:\n", lt->name);
			fprintf(cf, "    return tokulog_fread_%s (infile, &le->u.%s, cmd);\n", lt->name, lt->name);
		    }));
    fprintf(cf, "  };\n");
    fprintf(cf, "  return DB_BADFORMAT;\n"); // Should read past the record using the len field.
    fprintf(cf, "}\n\n");
}

void generate_logprint (void) {
    fprintf2(cf, hf, "int toku_logprint_one_record(FILE *outf, FILE *f)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n");
    fprintf(cf, "    int cmd, r;\n");
    fprintf(cf, "    cmd=fgetc(f);\n");
    fprintf(cf, "    if (cmd==EOF) return EOF;\n");
    fprintf(cf, "    u_int32_t len_in_file, len=1;\n");
    fprintf(cf, "    char charcmd = cmd;\n");
    fprintf(cf, "    u_int32_t crc_in_file, crc = toku_crc32(0, &charcmd, 1);\n");
    fprintf(cf, "    switch ((enum lt_cmd)cmd) {\n");
    DO_LOGTYPES(lt, ({
			fprintf(cf, "    case LT_%s: \n", lt->name);
			// We aren't using the log reader here because we want better diagnostics as soon as things go wrong.
			fprintf(cf, "        fprintf(outf, \"%%s:\", \"%s\");\n", lt->name); 
			fprintf(cf, "        r = toku_logprint_%-16s(outf, f, \"lsn\", &crc, &len);     if (r!=0) return r;\n", "LSN");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "        r = toku_logprint_%-16s(outf, f, \"%s\", &crc, &len);     if (r!=0) return r;\n", ft->type, ft->name));
			fprintf(cf, "        r = toku_fread_u_int32_t_nocrclen (f, &crc_in_file); len+=4; if (r!=0) return r;\n");
			fprintf(cf, "        fprintf(outf, \" crc=%%d\", crc_in_file);\n");
			fprintf(cf, "        if (crc_in_file!=crc) fprintf(outf, \" actual_crc=%%d\", crc);\n");
			fprintf(cf, "        r = toku_fread_u_int32_t_nocrclen (f, &len_in_file); len+=4; if (r!=0) return r;\n");
			fprintf(cf, "        fprintf(outf, \" len=%%d\", len_in_file);\n");
			fprintf(cf, "        if (len_in_file!=len) fprintf(outf, \" actual_len=%%d\", len);\n");
			fprintf(cf, "        if (len_in_file!=len || crc_in_file!=crc) return DB_BADFORMAT;\n");
			fprintf(cf, "        fprintf(outf, \"\\n\");\n");
			fprintf(cf, "        return 0;;\n\n");
		    }));
    fprintf(cf, "    }\n");
    fprintf(cf, "    fprintf(outf, \"Unknown command %%d ('%%c')\", cmd, cmd);\n");
    fprintf(cf, "    return DB_BADFORMAT;\n");
    fprintf(cf, "}\n\n");
}

const char *codepath = "log_code.c";
const char *headerpath = "log_header.h";
int main (int argc __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    unlink(codepath);
    unlink(headerpath);
    cf = fopen(codepath, "w");      assert(cf!=0);
    hf = fopen(headerpath, "w");     assert(hf!=0);
    fprintf2(cf, hf, "/* Do not edit this file.  This code generated by logformat.c.  Copyright 2007 Tokutek.    */\n");
    fprintf(cf, "#include <stdio.h>\n");
    fprintf(hf, "#include \"brt-internal.h\"\n");
    fprintf(cf, "#include \"log_header.h\"\n");
    fprintf(cf, "#include \"wbuf.h\"\n");
    fprintf(cf, "#include \"log-internal.h\"\n");
    generate_lt_enum();
    generate_log_struct();
    generate_log_writer();
    generate_log_reader();
    generate_logprint();
    {
	int r=fclose(hf);
	assert(r==0);
	r=fclose(cf);
	assert(r==0);
	// Make it tougher to modify by mistake
	chmod(codepath, S_IRUSR|S_IRGRP|S_IROTH);
	chmod(headerpath, S_IRUSR|S_IRGRP|S_IROTH);
    }
    return 0;
}
   
