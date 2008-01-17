/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* This file defines the logformat in an executable fashion.
 * This code is used to generate
 *   The code that writes into the log.
 *   The code that reads the log and prints it to stdout (the log_print utility)
 *   The code that reads the log for recovery.
 *   The struct definitions.
 *   The Latex documentation.
 */
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct field {
    char *type;
    char *name;
    char *format; // optional format string
} F;

#define NULLFIELD {0,0,0}
#define FA (F[])

struct logtype {
    char *name;
    char command;
    struct field *fields;
};

// In the fields, don't mention the command, the LSN, the CRC or the trailing LEN.

int logformat_version_number = 0;

const struct logtype logtypes[] = {
    {"commit", 'C', FA{{"TXNID", "txnid", 0},NULLFIELD}},
    {"delete", 'D', FA{{"FILENUM", "filenum", 0},
		       {"DISKOFF", "diskoff", 0},
		       {"BYTESTRING", "key", 0},
		       {"BYTESTRING", "data", 0},
		       NULLFIELD}},
    {"fcreate", 'F', FA{{"TXNID",      "txnid", 0},
			{"BYTESTRING", "fname", 0},
			{"u_int32_t",  "mode",  "0%o"},
			NULLFIELD}},
    {"fheader", 'H',    FA{{"TXNID",      "txnid", 0},
			   {"FILENUM",    "filenum", 0},
			   {"LOGGEDBRTHEADER",  "header", 0},
			   NULLFIELD}},
    {"newbrtnode", 'N', FA{{"TXNID",   "txnid", 0},
			   {"FILENUM", "filenum", 0},
			   {"DISKOFF", "diskoff", 0},
			   {"u_int32_t", "height", 0},
			   {"u_int32_t", "nodesize", 0},
			   {"u_int8_t", "is_dup_sort", 0},
			   {"u_int32_t", "rand4fingerprint", 0},
			   NULLFIELD}},
    {"insertchild", 'i', FA{{"TXNID",   "txnid", 0},
			    {"FILENUM", "filenum", 0},
			    {"DISKOFF", "diskoff", 0},
			    {"u_int32_t", "childnum", 0},
			    {"DISKOFF",  "child", 0},
			    NULLFIELD}},
    {"insertpivot", 'k', FA{{"TXNID",   "txnid", 0},
			    {"FILENUM", "filenum", 0},
			    {"DISKOFF", "diskoff", 0},
			    {"u_int32_t", "childnum", 0},
			    {"BYTESTRING", "pivotkey", 0},
			    NULLFIELD}},
    {"fopen",   'O', FA{{"TXNID",      "txnid", 0},
			{"BYTESTRING", "fname", 0},
			{"FILENUM",    "filenum", 0},
			NULLFIELD}},
    {"insertinleaf", 'I', FA{{"TXNID",      "txnid", 0},
			     {"FILENUM",    "filenum", 0},
			     {"DISKOFF",    "diskoff", 0},
			     {"u_int32_t",  "pmaidx", 0},
			     {"BYTESTRING", "key", 0},
			     {"BYTESTRING", "data", 0},
                             NULLFIELD}},
    {"resizepma", 'R', FA{{"TXNID", "txnid", 0},
			  {"FILENUM",    "filenum", 0},
			  {"DISKOFF",    "diskoff", 0},
			  {"u_int32_t",  "oldsize", 0},
			  {"u_int32_t",  "newsize", 0},
			  NULLFIELD}},
    {"pmadistribute", 'M', FA{{"TXNID", "txnid", 0},
			      {"FILENUM",    "filenum", 0},
			      {"DISKOFF",    "old_diskoff", 0},
			      {"DISKOFF",    "new_diskoff", 0},
			      {"INTPAIRARRAY",   "fromto",    0},
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
    va_end(ap);
    va_start(ap, format);
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
		    fprintf(hf, "void toku_recover_%s (struct logtype_%s *);\n", lt->name, lt->name);
		    fprintf(hf, "int toku_rollback_%s (struct logtype_%s *, TOKUTXN);\n", lt->name, lt->name);
		}));
    fprintf(hf, "struct log_entry {\n");
    fprintf(hf, "  enum lt_cmd cmd;\n");
    fprintf(hf, "  union {\n");
    DO_LOGTYPES(lt, fprintf(hf,"    struct logtype_%s %s;\n", lt->name, lt->name));
    fprintf(hf, "  } u;\n");
    fprintf(hf, "  struct log_entry *next; /* for in-memory list of log entries */\n");
    fprintf(hf, "  struct log_entry *tmp;  /* This will be a back pointer, but it is only created if needed (e.g., when abort is called. */\n");
    fprintf(hf, "};\n");
}

void generate_dispatch (void) {
    fprintf(hf, "#define logtype_dispatch(s, funprefix) ({ switch((s)->cmd) {\\\n");
    DO_LOGTYPES(lt, fprintf(hf, "  case LT_%s: funprefix ## %s (&(s)->u.%s); break;\\\n", lt->name, lt->name, lt->name));
    fprintf(hf, " }})\n");

    fprintf(hf, "#define logtype_dispatch_assign(s, funprefix, var, args...) ({ switch((s)->cmd) {\\\n");
    DO_LOGTYPES(lt, fprintf(hf, "  case LT_%s: var = funprefix ## %s (&(s)->u.%s, ## args); break;\\\n", lt->name, lt->name, lt->name));
    fprintf(hf, " }})\n");

}
		
void generate_log_free(void) {
    DO_LOGTYPES(lt, ({
			fprintf2(cf, hf, "void toku_free_logtype_%s(struct logtype_%s *e)", lt->name, lt->name);
			fprintf(hf, ";\n");
			fprintf(cf, " {\n");
			DO_FIELDS(ft, lt, fprintf(cf, "  toku_free_%s(e->%s);\n", ft->type, ft->name));
			fprintf(cf, "}\n");
		    }));
}

void generate_log_writer (void) {
    DO_LOGTYPES(lt, ({
			fprintf2(cf, hf, "int toku_log_%s (TOKUTXN txn", lt->name);
			DO_FIELDS(ft, lt,
				  fprintf2(cf, hf, ", %s %s", ft->type, ft->name));
			if (lt->command=='C') {
			    fprintf2(cf,hf, ", int nosync");
			}
			fprintf(hf, ");\n");
			fprintf(cf, ") {\n");
			fprintf(cf, "  if (txn==0) return 0;\n");
			fprintf(cf, "  const unsigned int buflen= (+4 // len at the beginning\n");
			fprintf(cf, "                              +1 // log command\n");
			fprintf(cf, "                              +8 // lsn\n");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "                              +toku_logsizeof_%s(%s)\n", ft->type, ft->name));
			fprintf(cf, "                              +8 // crc + len\n");
			fprintf(cf, "                     );\n");
			fprintf(cf, "  struct wbuf wbuf;\n");
			fprintf(cf, "  char *buf = toku_malloc(buflen);\n");
			fprintf(cf, "  if (buf==0) return errno;\n");
			fprintf(cf, "  wbuf_init(&wbuf, buf, buflen);\n");
			fprintf(cf, "  wbuf_int(&wbuf, buflen);\n");
			fprintf(cf, "  wbuf_char(&wbuf, '%c');\n", lt->command);
			fprintf(cf, "  wbuf_LSN(&wbuf, txn->logger->lsn);\n");
			fprintf(cf, "  txn->last_lsn = txn->logger->lsn;\n");
			fprintf(cf, "  txn->logger->lsn.lsn++;\n");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "  wbuf_%s(&wbuf, %s);\n", ft->type, ft->name));
			fprintf(cf, "  int r= toku_logger_finish(txn->logger, &wbuf);\n");
			fprintf(cf, "  assert(wbuf.ndone==buflen);\n");
			fprintf(cf, "  toku_free(buf);\n");
			if (lt->command=='C') {
			    fprintf(cf, "  if (r!=0) return r;\n");
			    fprintf(cf, "  // commit has some extra work to do.\n");
			    fprintf(cf, "  if (nosync) return 0;\n");
			    fprintf(cf, "  if (txn->parent) { // do not fsync if there is a parent.  Instead append the log entries onto the parent.\n");
			    fprintf(cf, "    if (txn->parent->oldest_logentry) txn->parent->newest_logentry->next = txn->oldest_logentry;\n");
			    fprintf(cf, "    else                              txn->parent->oldest_logentry       = txn->oldest_logentry;\n");
			    fprintf(cf, "    if (txn->newest_logentry) txn->parent->newest_logentry = txn->newest_logentry;\n");
			    fprintf(cf, "    txn->newest_logentry = txn->oldest_logentry = 0;\n");
			    fprintf(cf, "  } else {\n");
			    fprintf(cf, "    r = toku_logger_fsync(txn->logger);\n");
			    fprintf(cf, "    if (r!=0) toku_logger_panic(txn->logger, r);\n");
			    fprintf(cf, "  }\n");
			    fprintf(cf, "  return 0;\n");
			} else {
			    int i=0;
			    fprintf(cf, "  struct log_entry *MALLOC(lentry);\n");
			    fprintf(cf, "  if (lentry==0) return errno;\n");
			    fprintf(cf, "  if (0) { died0: toku_free(lentry); return r; }\n");
			    fprintf(cf, "  lentry->cmd = %d;\n", lt->command);
			    DO_FIELDS(ft, lt,
				      ({
					  fprintf(cf, "  r=toku_copy_%s(&lentry->u.%s.%s, %s);\n", ft->type, lt->name, ft->name, ft->name);
					  fprintf(cf, "  if (r!=0) { if(0) { died%d: toku_free_%s(lentry->u.%s.%s); } goto died%d; }\n", i+1, ft->type, lt->name, ft->name, i);
					  i++;
				      }));
			    fprintf(cf, "  if (0) { goto died%d; }\n", i); // Need to use that label.
			    fprintf(cf, "  lentry->next = 0;\n");
			    fprintf(cf, "  if (txn->oldest_logentry==0) txn->oldest_logentry = lentry;\n");
			    fprintf(cf, "  else txn->newest_logentry->next = lentry;\n");
			    fprintf(cf, "  txn->newest_logentry = lentry;\n");
			    fprintf(cf, "  return r;\n");
			}
			fprintf(cf, "}\n\n");
		    }));

}

void generate_log_reader (void) {
    DO_LOGTYPES(lt, ({
			fprintf(cf, "static int toku_log_fread_%s (FILE *infile, struct logtype_%s *data, u_int32_t crc)", lt->name, lt->name);
			fprintf(cf, " {\n");
			fprintf(cf, "  int r=0;\n");
			fprintf(cf, "  u_int32_t actual_len=5; // 1 for the command, 4 for the first len.\n");
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
    fprintf2(cf, hf, "int toku_log_fread (FILE *infile, struct log_entry *le)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n");
    fprintf(cf, "  u_int32_t len1; int r;\n");
    fprintf(cf, "  u_int32_t crc=0,ignorelen=0;\n");
    fprintf(cf, "  r = toku_fread_u_int32_t(infile, &len1,&crc,&ignorelen); if (r!=0) return r;\n"); 
    fprintf(cf, "  int cmd=fgetc(infile);\n");
    fprintf(cf, "  if (cmd==EOF) return EOF;\n");
    fprintf(cf, "  char cmdchar = cmd;\n");
    fprintf(cf, "  crc = toku_crc32(crc, &cmdchar, 1);\n");
    fprintf(cf, "  le->cmd=cmd;\n");
    fprintf(cf, "  switch ((enum lt_cmd)cmd) {\n");
    DO_LOGTYPES(lt, ({
			fprintf(cf, "  case LT_%s:\n", lt->name);
			fprintf(cf, "    return toku_log_fread_%s (infile, &le->u.%s, crc);\n", lt->name, lt->name);
		    }));
    fprintf(cf, "  };\n");
    fprintf(cf, "  return DB_BADFORMAT;\n"); // Should read past the record using the len field.
    fprintf(cf, "}\n\n");
}

void generate_logprint (void) {
    unsigned maxnamelen=0;
    fprintf2(cf, hf, "int toku_logprint_one_record(FILE *outf, FILE *f)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n");
    fprintf(cf, "    int cmd, r;\n");
    fprintf(cf, "    u_int32_t len1, crc_in_file;\n");
    fprintf(cf, "    u_int32_t crc = 0, ignorelen=0;\n");
    fprintf(cf, "    r=toku_fread_u_int32_t(f, &len1, &crc, &ignorelen);\n");
    fprintf(cf, "    if (r==EOF) return EOF;\n");
    fprintf(cf, "    cmd=fgetc(f);\n");
    fprintf(cf, "    if (cmd==EOF) return DB_BADFORMAT;\n");
    fprintf(cf, "    u_int32_t len_in_file, len=1+4; // cmd + len1\n");
    fprintf(cf, "    char charcmd = cmd;\n");
    fprintf(cf, "    crc = toku_crc32(crc, &charcmd, 1);\n");
    fprintf(cf, "    switch ((enum lt_cmd)cmd) {\n");
    DO_LOGTYPES(lt, ({ if (strlen(lt->name)>maxnamelen) maxnamelen=strlen(lt->name); }));
    DO_LOGTYPES(lt, ({
			fprintf(cf, "    case LT_%s: \n", lt->name);
			// We aren't using the log reader here because we want better diagnostics as soon as things go wrong.
			fprintf(cf, "        fprintf(outf, \"%%-%ds \", \"%s\");\n", maxnamelen, lt->name);
			if (isprint(lt->command)) fprintf(cf,"        fprintf(outf, \" '%c':\");\n", lt->command);
			else                      fprintf(cf,"        fprintf(outf, \"0%03o:\");\n", lt->command);
			fprintf(cf, "        r = toku_logprint_%-16s(outf, f, \"lsn\", &crc, &len, 0);     if (r!=0) return r;\n", "LSN");
			DO_FIELDS(ft, lt,
				  ({
				      fprintf(cf, "        r = toku_logprint_%-16s(outf, f, \"%s\", &crc, &len,", ft->type, ft->name);
				      if (ft->format) fprintf(cf, "\"%s\"", ft->format);
				      else            fprintf(cf, "0");
				      fprintf(cf, "); if (r!=0) return r;\n");
				  }));
			fprintf(cf, "        r = toku_fread_u_int32_t_nocrclen (f, &crc_in_file); len+=4; if (r!=0) return r;\n");
			fprintf(cf, "        fprintf(outf, \" crc=%%08x\", crc_in_file);\n");
			fprintf(cf, "        if (crc_in_file!=crc) fprintf(outf, \" actual_crc=%%08x\", crc);\n");
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
    fprintf2(cf, hf, "#ident \"Copyright (c) 2007 Tokutek Inc.  All rights reserved.\"\n");
    fprintf(cf, "#include <stdio.h>\n");
    fprintf(hf, "#include \"brt-internal.h\"\n");
    fprintf(cf, "#include \"log_header.h\"\n");
    fprintf(cf, "#include \"wbuf.h\"\n");
    fprintf(cf, "#include \"log-internal.h\"\n");
    generate_lt_enum();
    generate_log_struct();
    generate_dispatch();
    generate_log_writer();
    generate_log_free();
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
   
