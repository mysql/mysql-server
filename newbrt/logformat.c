/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* This file defines the logformat in an executable fashion.
 * This code is used to generate
 *   The code that writes into the log.
 *   The code that reads the log and prints it to stdout (the log_print utility)
 *   The code that reads the log for recovery.
 *   The struct definitions.
 *   The Latex documentation.
 */
#include "toku_assert.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
    unsigned int command_and_flags;
    struct field *fields;
};

// In the fields, don't mention the command, the LSN, the CRC or the trailing LEN.

int logformat_version_number = 0;

const struct logtype rollbacks[] = {
    {"fcreate", 'F', FA{{"TXNID", "xid", 0},
			{"BYTESTRING", "fname", 0},
			NULLFIELD}},
    {"cmdinsert", 'i', FA{{"TXNID", "xid", 0},
			  {"FILENUM", "filenum", 0},
			  {"BYTESTRING", "key", 0},
			  {"BYTESTRING", "data", 0},
			  NULLFIELD}},
    {"cmddeleteboth", 'D', FA{{"TXNID", "xid", 0},
			      {"FILENUM", "filenum", 0},
			      {"BYTESTRING", "key", 0},
			      {"BYTESTRING", "data", 0},
			      NULLFIELD}},
    {"cmddelete", 'd', FA{{"TXNID", "xid", 0},
			  {"FILENUM", "filenum", 0},
			  {"BYTESTRING", "key", 0},
			  NULLFIELD}},
    {"rollinclude", 'I', FA{{"BYTESTRING", "fname", 0},
                            NULLFIELD}},
//    {"fclose", 'c', FA{{"FILENUM", "filenum", 0},
//		       {"BYTESTRING", "fname", 0},
//		       NULLFIELD}},
//    {"deleteatleaf", 'd', FA{{"FILENUM", "filenum", 0}, // Note a delete for rollback.   The delete takes place in a leaf.
//			     {"BYTESTRING", "key", 0},
//			     {"BYTESTRING", "data", 0},
//			     NULLFIELD}},
//    {"insertatleaf", 'i', FA{{"FILENUM", "filenum", 0}, // Note an insert for rollback.   The insert takes place in a leaf.
//			     {"BYTESTRING", "key", 0},
//			     {"BYTESTRING", "data", 0},
//			     NULLFIELD}},
//    {"xactiontouchednonleaf", 'n', FA{{"FILENUM", "filenum", 0},
//				      {"DISKOFFARRAY", "parents", 0},
//				      {"DISKOFF", "diskoff", 0},
//				      NULLFIELD}},
    {0,0,FA{NULLFIELD}}
};

const struct logtype logtypes[] = {
    {"checkpoint", 'x', FA{NULLFIELD}},
    {"commit", 'C', FA{{"TXNID", "txnid", 0},NULLFIELD}},
    {"xabort", 'q', FA{{"TXNID", "txnid", 0},NULLFIELD}},
    {"xbegin", 'b', FA{{"TXNID", "parenttxnid", 0},NULLFIELD}},
#if 0
    {"tl_delete", 'D', FA{{"FILENUM", "filenum", 0}, // tl logentries can be used, by themselves, to rebuild the whole DB from scratch.
		       {"DISKOFF", "diskoff", 0},
		       {"BYTESTRING", "key", 0},
		       {"BYTESTRING", "data", 0},
		       NULLFIELD}},
#endif
    {"fcreate", 'F', FA{{"TXNID",      "txnid", 0},
			{"BYTESTRING", "fname", 0},
			{"u_int32_t",  "mode",  "0%o"},
			NULLFIELD}},
    {"fheader", 'H',  FA{{"TXNID",      "txnid", 0},
			 {"FILENUM",    "filenum", 0},
			 {"LOGGEDBRTHEADER",  "header", 0},
			 NULLFIELD}},
    {"newbrtnode", 'N', FA{{"FILENUM", "filenum", 0},
			   {"DISKOFF", "diskoff", 0},
			   {"u_int32_t", "height", 0},
			   {"u_int32_t", "nodesize", 0},
			   {"u_int8_t", "is_dup_sort", 0},
			   {"u_int32_t", "rand4fingerprint", "%08x"},
			   NULLFIELD}},
    {"changeunnamedroot", 'u', FA{{"FILENUM", "filenum", 0},
				  {"DISKOFF", "oldroot", 0},
				  {"DISKOFF", "newroot", 0},
				  NULLFIELD}},
    {"changenamedroot", 'n', FA{{"FILENUM", "filenum", 0},
				{"BYTESTRING", "name", 0},
				{"DISKOFF", "oldroot", 0},
				{"DISKOFF", "newroot", 0},
				NULLFIELD}},
    {"changeunusedmemory", 'm',  FA{{"FILENUM", "filenum", 0},
				    {"DISKOFF", "oldunused", 0},
				    {"DISKOFF", "newunused", 0},
				    NULLFIELD}},
    {"addchild", 'c', FA{{"FILENUM", "filenum", 0},
			 {"DISKOFF", "diskoff", 0},
			 {"u_int32_t", "childnum", 0}, // children scoot over
			 {"DISKOFF",   "child", 0},
			 {"u_int32_t", "childfingerprint", "%08x"},
			 NULLFIELD}},
    {"delchild", 'r', FA{{"FILENUM", "filenum", 0},
			 {"DISKOFF", "diskoff", 0},
			 {"u_int32_t", "childnum", 0}, // children scoot over
			 {"DISKOFF",   "child", 0},
			 {"u_int32_t", "childfingerprint", "%08x"},
			 {"BYTESTRING", "pivotkey", 0},
			 NULLFIELD}},
    {"setchild", 'i', FA{{"FILENUM",   "filenum", 0},
			 {"DISKOFF",   "diskoff", 0},
			 {"u_int32_t", "childnum", 0},
			 {"DISKOFF",   "oldchild", 0},
			 {"DISKOFF",   "newchild", 0},
			 NULLFIELD}},
    {"setpivot", 'k', FA{{"FILENUM", "filenum", 0},
			 {"DISKOFF", "diskoff", 0},
			 {"u_int32_t", "childnum", 0},
			 {"BYTESTRING", "pivotkey", 0},
			 NULLFIELD}},
    {"fopen",   'O', FA{{"TXNID",      "txnid", 0},
			{"BYTESTRING", "fname", 0},
			{"FILENUM",    "filenum", 0},
			NULLFIELD}},
    {"brtclose",   'e', FA{{"BYTESTRING", "fname", 0},   // brtclose is logged when a particular brt is closed
			   {"FILENUM",    "filenum", 0},
			   NULLFIELD}},
    {"cfclose",   'o', FA{{"BYTESTRING", "fname", 0},     // cfclose is logged when a cachefile actually closes ("cfclose" means cache file close)
			  {"FILENUM",    "filenum", 0},
			  NULLFIELD}},
    // Note that brtdeq and brtenq don't name the new size or fingerprint.  We can calculate them properly.
    {"brtdeq",       'U', FA{{"FILENUM",    "filenum", 0},
			     {"DISKOFF",    "diskoff", 0},
			     {"u_int32_t",  "childnum", 0},
			     NULLFIELD}},
    {"brtenq",       'Q', FA{{"FILENUM",    "filenum", 0},
			     {"DISKOFF",    "diskoff", 0},
			     {"u_int32_t",  "childnum", 0},
			     {"TXNID",      "xid", 0},
			     {"u_int32_t",  "typ", 0},
			     {"BYTESTRING", "key", 0},
			     {"BYTESTRING", "data", 0},
			     NULLFIELD}},
//    {"insertinleaf", 'I', FA{{"TXNID",      "txnid", 0},
//			     {"FILENUM",    "filenum", 0},
//			     {"DISKOFF",    "diskoff", 0},
//			     {"u_int32_t",  "pmaidx", 0},
//			     {"BYTESTRING", "key", 0},
//			     {"BYTESTRING", "data", 0},
//			     NULLFIELD}},
//    {"replaceleafentry", 'L', FA{{"FILENUM", "filenum", 0},
//				 {"DISKOFF", "diskoff", 0},
//				 {"u_int32_t", "pmaidx", 0},
//				 {"LEAFENTRY", "oldleafentry", 0},
//				 {"LEAFENTRY", "newleafentry", 0},
//				 NULLFIELD}},
    {"enqrootentry", 'a', FA{{"FILENUM", "filenum", 0},
				{"TXNID",   "xid", 0},
				{"u_int32_t", "typ", 0},
				{"BYTESTRING", "key", 0},
				{"BYTESTRING", "data", 0},
				NULLFIELD}},
    {"deqrootentry", 'A', FA{{"FILENUM", "filenum", 0},
			     NULLFIELD}},
    {"insertleafentry", 'I', FA{{"FILENUM", "filenum", 0},
				 {"DISKOFF", "diskoff", 0},
				 {"u_int32_t", "idx", 0},
				 {"LEAFENTRY", "newleafentry", 0},
				 NULLFIELD}},
    {"deleteleafentry", 'D', FA{{"FILENUM", "filenum", 0},
				{"DISKOFF", "diskoff", 0},
				{"u_int32_t", "idx", 0},
				NULLFIELD}},
    {"leafsplit",  's', FA{{"FILENUM", "filenum",     0}, // log the creation of a new node by splitting stuff out of an old node
			   {"DISKOFF", "old_diskoff", 0},
			   {"DISKOFF", "new_diskoff", 0},
			   {"u_int32_t", "old_n",     0},
			   {"u_int32_t", "split_at",  0},
			   {"u_int32_t", "new_nodesize", 0},
			   {"u_int32_t", "new_rand4", "%08x"},
			   {"u_int8_t",  "is_dupsort", 0},			   
			   NULLFIELD}},
    {0,0,FA{NULLFIELD}}
};

  
#define DO_STRUCTS(lt, array, body) ({ \
    const struct logtype *lt;    \
    for (lt=&array[0]; lt->name; lt++) {	\
	body; \
    } })

#define DO_ROLLBACKS(lt, body) DO_STRUCTS(lt, rollbacks, body)

#define DO_LOGTYPES(lt, body) DO_STRUCTS(lt, logtypes, body)

#define DO_LOGTYPES_AND_ROLLBACKS(lt, body) (DO_ROLLBACKS(lt,body), DO_LOGTYPES(lt, body))

#define DO_FIELDS(fld, lt, body) ({ \
    struct field *fld; \
    for (fld=lt->fields; fld->type; fld++) { \
        body; \
    } })


static void __attribute__((format (printf, 3, 4))) fprintf2 (FILE *f1, FILE *f2, const char *format, ...) {
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

void generate_enum_internal (char *enum_name, char *enum_prefix, const struct logtype *lts) {
    char used_cmds[256];
    int count=0;
    memset(used_cmds, 0, 256);
    fprintf(hf, "enum %s {", enum_name);
    DO_STRUCTS(lt, lts,
		({
		    unsigned char cmd = lt->command_and_flags&0xff;
		    if (count!=0) fprintf(hf, ",");
		    count++;
		    fprintf(hf, "\n");
		    fprintf(hf,"    %s_%-16s = '%c'", enum_prefix, lt->name, cmd);
		    if (used_cmds[cmd]!=0) { fprintf(stderr, "%s:%d: error: Command %d (%c) was used twice (second time for %s)\n", __FILE__, __LINE__, cmd, cmd, lt->name); abort(); }
		    used_cmds[cmd]=1;
		}));
    fprintf(hf, "\n};\n\n");
   
}

void generate_enum (void) {
    generate_enum_internal("lt_cmd", "LT", logtypes);
    generate_enum_internal("rt_cmd", "RT", rollbacks);
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
		    //fprintf(hf, "void toku_recover_%s (LSN lsn", lt->name);
		    //DO_FIELDS(ft, lt, fprintf(hf, ", %s %s", ft->type, ft->name));
		    //fprintf(hf, ");\n");
		}));
    DO_ROLLBACKS(lt,
		 ({  fprintf(hf, "struct rolltype_%s {\n", lt->name);
		     DO_FIELDS(ft, lt,
			       fprintf(hf, "  %-16s %s;\n", ft->type, ft->name));
		     fprintf(hf, "};\n");
		     fprintf(hf, "int toku_rollback_%s (", lt->name);
		     DO_FIELDS(ft, lt, fprintf(hf, "%s %s,", ft->type, ft->name));
		     fprintf(hf, "TOKUTXN txn);\n");
		     fprintf(hf, "int toku_commit_%s (", lt->name);
		     DO_FIELDS(ft, lt, fprintf(hf, "%s %s,", ft->type, ft->name));
		     fprintf(hf, "TOKUTXN txn);\n");
		 }));
    fprintf(hf, "struct log_entry {\n");
    fprintf(hf, "  enum lt_cmd cmd;\n");
    fprintf(hf, "  union {\n");
    DO_LOGTYPES(lt, fprintf(hf,"    struct logtype_%s %s;\n", lt->name, lt->name));
    fprintf(hf, "  } u;\n");
    fprintf(hf, "};\n");
    
    fprintf(hf, "struct roll_entry {\n");
    fprintf(hf, "  enum rt_cmd cmd;\n");
    fprintf(hf, "  union {\n");
    DO_ROLLBACKS(lt, fprintf(hf,"    struct rolltype_%s %s;\n", lt->name, lt->name));
    fprintf(hf, "  } u;\n");
    fprintf(hf, "  struct roll_entry *prev; /* for in-memory list of log entries.  Threads from newest to oldest. */\n");
    fprintf(hf, "  struct roll_entry *next; /* Points to a newer logentry.  Needed for flushing to disk, since we want to write the oldest one first. */\n");
    fprintf(hf, "};\n");

}

void generate_dispatch (void) {
    fprintf(hf, "#define rolltype_dispatch(s, funprefix) ({ switch((s)->cmd) {\\\n");
    DO_ROLLBACKS(lt, fprintf(hf, "  case RT_%s: funprefix ## %s (&(s)->u.%s); break;\\\n", lt->name, lt->name, lt->name));
    fprintf(hf, " }})\n");

    fprintf(hf, "#define logtype_dispatch_assign(s, funprefix, var, args...) ({ switch((s)->cmd) {\\\n");
    DO_LOGTYPES(lt, fprintf(hf, "  case LT_%s: var = funprefix ## %s (&(s)->u.%s, ## args); break;\\\n", lt->name, lt->name, lt->name));
    fprintf(hf, " }})\n");

    fprintf(hf, "#define rolltype_dispatch_assign(s, funprefix, var, args...) ({ \\\n");
    fprintf(hf, "  switch((s)->cmd) {\\\n");
    DO_ROLLBACKS(lt, ({
		    fprintf(hf, "  case RT_%s: var = funprefix ## %s (", lt->name, lt->name);
		    int fieldcount=0;
		    DO_FIELDS(ft, lt, ({
				if (fieldcount>0) fprintf(hf, ",");
				fprintf(hf, "(s)->u.%s.%s", lt->name, ft->name);
				fieldcount++;
			    }));
		    fprintf(hf, ",## args); break;\\\n");
		}));
    fprintf(hf, "  default: assert(0);} })\n");

    fprintf(hf, "#define logtype_dispatch_args(s, funprefix) ({ switch((s)->cmd) {\\\n");
    DO_LOGTYPES(lt,
		({
		    fprintf(hf, "  case LT_%s: funprefix ## %s ((s)->u.%s.lsn", lt->name, lt->name, lt->name);
		    DO_FIELDS(ft, lt, fprintf(hf, ",(s)->u.%s.%s", lt->name, ft->name));
		    fprintf(hf, "); break;\\\n");
		}));
    fprintf(hf, " }})\n");
}
		
void generate_log_writer (void) {
    DO_LOGTYPES(lt, ({
			fprintf2(cf, hf, "int toku_log_%s (TOKULOGGER logger, LSN *lsnp, int do_fsync", lt->name);
			DO_FIELDS(ft, lt, fprintf2(cf, hf, ", %s %s", ft->type, ft->name));
			fprintf(hf, ");\n");
			fprintf(cf, ") {\n");
			fprintf(cf, "  if (logger==0) return 0;\n");
			fprintf(cf, "  const unsigned int buflen= (+4 // len at the beginning\n");
			fprintf(cf, "                              +1 // log command\n");
			fprintf(cf, "                              +8 // lsn\n");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "                              +toku_logsizeof_%s(%s)\n", ft->type, ft->name));
			fprintf(cf, "                              +8 // crc + len\n");
			fprintf(cf, "                     );\n");
			fprintf(cf, "  struct wbuf wbuf;\n");
			fprintf(cf, "  struct logbytes *lbytes = MALLOC_LOGBYTES(buflen);\n");
			fprintf(cf, "  if (lbytes==0) return errno;\n");
			fprintf(cf, "  wbuf_init(&wbuf, &lbytes->bytes[0], buflen);\n");
			fprintf(cf, "  wbuf_int(&wbuf, buflen);\n");
			fprintf(cf, "  wbuf_char(&wbuf, '%c');\n", 0xff&lt->command_and_flags);
			fprintf(cf, "  ml_lock(&logger->input_lock);\n");
			fprintf(cf, "  logger->lsn.lsn++;\n");
			fprintf(cf, "  LSN lsn = logger->lsn;\n");
			fprintf(cf, "  wbuf_LSN(&wbuf, lsn);\n");
			fprintf(cf, "  lbytes->lsn = lsn;\n");
			fprintf(cf, "  if (lsnp) *lsnp=logger->lsn;\n");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "  wbuf_%s(&wbuf, %s);\n", ft->type, ft->name));
			fprintf(cf, "  int r= toku_logger_finish(logger, lbytes, &wbuf, do_fsync);\n");
			fprintf(cf, "  assert(wbuf.ndone==buflen);\n");
			fprintf(cf, "  return r;\n");
			fprintf(cf, "}\n\n");
		    }));
}

void generate_log_reader (void) {
    DO_LOGTYPES(lt, ({
			fprintf(cf, "static int toku_log_fread_%s (FILE *infile, struct logtype_%s *data, struct x1764 *checksum)", lt->name, lt->name);
			fprintf(cf, " {\n");
			fprintf(cf, "  int r=0;\n");
			fprintf(cf, "  u_int32_t actual_len=5; // 1 for the command, 4 for the first len.\n");
			fprintf(cf, "  r=toku_fread_%-16s(infile, &data->%-16s, checksum, &actual_len); if (r!=0) return r;\n", "LSN", "lsn");
			DO_FIELDS(ft, lt,
				  fprintf(cf, "  r=toku_fread_%-16s(infile, &data->%-16s, checksum, &actual_len); if (r!=0) return r;\n", ft->type, ft->name));
			fprintf(cf, "  u_int32_t checksum_in_file, len_in_file;\n");
			fprintf(cf, "  r=toku_fread_u_int32_t_nocrclen(infile, &checksum_in_file); actual_len+=4;   if (r!=0) return r;\n");
			fprintf(cf, "  r=toku_fread_u_int32_t_nocrclen(infile, &len_in_file);    actual_len+=4;   if (r!=0) return r;\n");
			fprintf(cf, "  if (checksum_in_file!=x1764_finish(checksum) || len_in_file!=actual_len) return DB_BADFORMAT;\n");
			fprintf(cf, "  return 0;\n");
			fprintf(cf, "}\n\n");
		    }));
    fprintf2(cf, hf, "int toku_log_fread (FILE *infile, struct log_entry *le)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n");
    fprintf(cf, "  u_int32_t len1; int r;\n");
    fprintf(cf, "  u_int32_t ignorelen=0;\n");
    fprintf(cf, "  struct x1764 checksum;\n");
    fprintf(cf, "  x1764_init(&checksum);\n");
    fprintf(cf, "  r = toku_fread_u_int32_t(infile, &len1, &checksum, &ignorelen); if (r!=0) return r;\n"); 
    fprintf(cf, "  int cmd=fgetc(infile);\n");
    fprintf(cf, "  if (cmd==EOF) return EOF;\n");
    fprintf(cf, "  char cmdchar = cmd;\n");
    fprintf(cf, "  x1764_add(&checksum, &cmdchar, 1);\n");
    fprintf(cf, "  le->cmd=cmd;\n");
    fprintf(cf, "  switch ((enum lt_cmd)cmd) {\n");
    DO_LOGTYPES(lt, ({
			fprintf(cf, "  case LT_%s:\n", lt->name);
			fprintf(cf, "    return toku_log_fread_%s (infile, &le->u.%s, &checksum);\n", lt->name, lt->name);
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
    fprintf(cf, "    u_int32_t ignorelen=0;\n");
    fprintf(cf, "    struct x1764 checksum;\n");
    fprintf(cf, "    x1764_init(&checksum);\n");
    fprintf(cf, "    r=toku_fread_u_int32_t(f, &len1, &checksum, &ignorelen);\n");
    fprintf(cf, "    if (r==EOF) return EOF;\n");
    fprintf(cf, "    cmd=fgetc(f);\n");
    fprintf(cf, "    if (cmd==EOF) return DB_BADFORMAT;\n");
    fprintf(cf, "    u_int32_t len_in_file, len=1+4; // cmd + len1\n");
    fprintf(cf, "    char charcmd = cmd;\n");
    fprintf(cf, "    x1764_add(&checksum, &charcmd, 1);\n");
    fprintf(cf, "    switch ((enum lt_cmd)cmd) {\n");
    DO_LOGTYPES(lt, ({ if (strlen(lt->name)>maxnamelen) maxnamelen=strlen(lt->name); }));
    DO_LOGTYPES(lt, ({
		unsigned char cmd = 0xff&lt->command_and_flags;
		fprintf(cf, "    case LT_%s: \n", lt->name);
		// We aren't using the log reader here because we want better diagnostics as soon as things go wrong.
		fprintf(cf, "        fprintf(outf, \"%%-%ds \", \"%s\");\n", maxnamelen, lt->name);
		if (isprint(cmd)) fprintf(cf,"        fprintf(outf, \" '%c':\");\n", cmd);
		else                      fprintf(cf,"        fprintf(outf, \"0%03o:\");\n", cmd);
		fprintf(cf, "        r = toku_logprint_%-16s(outf, f, \"lsn\", &checksum, &len, 0);     if (r!=0) return r;\n", "LSN");
		DO_FIELDS(ft, lt, ({
			    fprintf(cf, "        r = toku_logprint_%-16s(outf, f, \"%s\", &checksum, &len,", ft->type, ft->name);
			    if (ft->format) fprintf(cf, "\"%s\"", ft->format);
			    else            fprintf(cf, "0");
			    fprintf(cf, "); if (r!=0) return r;\n");
			}));
		fprintf(cf, "        {\n");
		fprintf(cf, "          u_int32_t actual_murmur = x1764_finish(&checksum);\n");
		fprintf(cf, "          r = toku_fread_u_int32_t_nocrclen (f, &crc_in_file); len+=4; if (r!=0) return r;\n");
		fprintf(cf, "          fprintf(outf, \" crc=%%08x\", crc_in_file);\n");
		fprintf(cf, "          if (crc_in_file!=actual_murmur) fprintf(outf, \" actual_fingerprint=%%08x\", actual_murmur);\n");
		fprintf(cf, "          r = toku_fread_u_int32_t_nocrclen (f, &len_in_file); len+=4; if (r!=0) return r;\n");
		fprintf(cf, "          fprintf(outf, \" len=%%d\", len_in_file);\n");
		fprintf(cf, "          if (len_in_file!=len) fprintf(outf, \" actual_len=%%d\", len);\n");
		fprintf(cf, "          if (len_in_file!=len || crc_in_file!=actual_murmur) return DB_BADFORMAT;\n");
		fprintf(cf, "        };\n");
		fprintf(cf, "        fprintf(outf, \"\\n\");\n");
		fprintf(cf, "        return 0;;\n\n");
	    }));
    fprintf(cf, "    }\n");
    fprintf(cf, "    fprintf(outf, \"Unknown command %%d ('%%c')\", cmd, cmd);\n");
    fprintf(cf, "    return DB_BADFORMAT;\n");
    fprintf(cf, "}\n\n");
}

void generate_rollbacks (void) {
    DO_ROLLBACKS(lt, ({
		    fprintf2(cf, hf, "int toku_logger_save_rollback_%s (TOKUTXN txn", lt->name);
		    DO_FIELDS(ft, lt, fprintf2(cf, hf, ", %s %s", ft->type, ft->name));
		    fprintf(hf, ");\n");
		    fprintf(cf, ") {\n");
		    {
			int count=0;
			fprintf(cf, "  u_int32_t rollback_fsize = toku_logger_rollback_fsize_%s(", lt->name);
			DO_FIELDS(ft, lt, fprintf(cf, "%s%s", (count++>0)?", ":"", ft->name));
			fprintf(cf, ");\n");
		    }
		    fprintf(cf, "  struct roll_entry *v = toku_malloc_in_rollback(txn, sizeof(*v));\n");
		    fprintf(cf, "  if (v==0) return errno;\n");
		    fprintf(cf, "  v->cmd = %d;\n", lt->command_and_flags&0xff);
		    DO_FIELDS(ft, lt, fprintf(cf, "  v->u.%s.%s = %s;\n", lt->name, ft->name, ft->name));
		    fprintf(cf, "  v->prev = txn->newest_logentry;\n");
		    fprintf(cf, "  v->next = 0;\n");
		    fprintf(cf, "  if (txn->oldest_logentry==0) txn->oldest_logentry=v;\n");
		    fprintf(cf, "  else txn->newest_logentry->next = v;\n");
		    fprintf(cf, "  txn->newest_logentry = v;\n");
		    fprintf(cf, "  txn->rollentry_resident_bytecount += rollback_fsize;\n");
		    fprintf(cf, "  return toku_maybe_spill_rollbacks(txn);\n}\n");
	    }));

    DO_ROLLBACKS(lt, ({
		fprintf2(cf, hf, "void toku_logger_rollback_wbufwrite_%s (struct wbuf *wbuf", lt->name);
		DO_FIELDS(ft, lt, fprintf2(cf, hf, ", %s %s", ft->type, ft->name));
		fprintf2(cf, hf, ")");
		fprintf(hf, ";\n");
		fprintf(cf, " {\n");
		fprintf(cf, "  u_int32_t ndone_at_start = wbuf->ndone;\n");
		fprintf(cf, "  wbuf_char(wbuf, '%c');\n", 0xff&lt->command_and_flags);
		DO_FIELDS(ft, lt, fprintf(cf, "  wbuf_%s(wbuf, %s);\n", ft->type, ft->name));
		fprintf(cf, "  wbuf_int(wbuf, 4+wbuf->ndone - ndone_at_start);\n");
		fprintf(cf, "}\n");
	    }));
    fprintf2(cf, hf, "void toku_logger_rollback_wbufwrite (struct wbuf *wbuf, struct roll_entry *r)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n  switch (r->cmd) {\n");
    DO_ROLLBACKS(lt, ({
		fprintf(cf, "    case RT_%s: toku_logger_rollback_wbufwrite_%s(wbuf", lt->name, lt->name);
		DO_FIELDS(ft, lt, fprintf(cf, ", r->u.%s.%s", lt->name, ft->name));
		fprintf(cf, "); return;\n");
	    }));
    fprintf(cf, "  }\n  assert(0);\n");
    fprintf(cf, "}\n");
    DO_ROLLBACKS(lt, ({
		fprintf2(cf, hf, "u_int32_t toku_logger_rollback_fsize_%s (", lt->name);
		int count=0;
		DO_FIELDS(ft, lt, fprintf2(cf, hf, "%s%s %s", (count++>0)?", ":"", ft->type, ft->name));
		fprintf(hf, ");\n");
		fprintf(cf, ") {\n");
		fprintf(cf, "  return 1 /* the cmd*/\n");
		fprintf(cf, "         + 4 /* the int at the end saying the size */");
		DO_FIELDS(ft, lt, 
			  fprintf(cf, "\n         + toku_logsizeof_%s(%s)", ft->type, ft->name));
		fprintf(cf, ";\n}\n");
	    }));
    fprintf2(cf, hf, "u_int32_t toku_logger_rollback_fsize(struct roll_entry *item)");
    fprintf(hf, ";\n");
    fprintf(cf, "{\n  switch(item->cmd) {\n");
    DO_ROLLBACKS(lt, ({
		fprintf(cf, "    case RT_%s: return toku_logger_rollback_fsize_%s(", lt->name, lt->name);
		int count=0;
		DO_FIELDS(ft, lt, fprintf(cf, "%sitem->u.%s.%s", (count++>0)?", ":"", lt->name, ft->name));
		fprintf(cf, ");\n");
	    }));
    fprintf(cf, "  }\n  assert(0);\n  return 0;\n");
    fprintf(cf, "}\n");

    fprintf2(cf, hf, "int toku_parse_rollback(unsigned char *buf, u_int32_t n_bytes, struct roll_entry **itemp, MEMARENA ma)");
    fprintf(hf, ";\n");
    fprintf(cf, " {\n  assert(n_bytes>0);\n  struct roll_entry *item = malloc_in_memarena(ma, sizeof(*item));\n  item->cmd=buf[0];\n");
    fprintf(cf, "  struct rbuf rc = {buf, n_bytes, 1};\n");
    fprintf(cf, "  switch(item->cmd) {\n");
    DO_ROLLBACKS(lt, ({
		fprintf(cf, "  case RT_%s:\n", lt->name);
		DO_FIELDS(ft, lt, fprintf(cf, "  rbuf_ma_%s(&rc, ma, &item->u.%s.%s);\n", ft->type, lt->name, ft->name));
		fprintf(cf, "    *itemp = item;\n");
		fprintf(cf, "    return 0;\n");
	    }));
    fprintf(cf, "  }\n  return EINVAL;\n}\n");
}


const char *codepath = "log_code.c";
const char *headerpath = "log_header.h";
int main (int argc __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    unlink(codepath);
    unlink(headerpath);
    cf = fopen(codepath, "w");      assert(cf!=0);
    hf = fopen(headerpath, "w");     assert(hf!=0);
    fprintf2(cf, hf, "/* Do not edit this file.  This code generated by logformat.c.  Copyright 2007, 2008 Tokutek.    */\n");
    fprintf2(cf, hf, "#ident \"Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved.\"\n");
    fprintf(cf, "#include <stdio.h>\n");
    fprintf(hf, "#include \"brt-internal.h\"\n");
    fprintf(hf, "#include \"memarena.h\"\n");
    fprintf(cf, "#include \"log_header.h\"\n");
    fprintf(cf, "#include \"wbuf.h\"\n");
    fprintf(cf, "#include \"log-internal.h\"\n");
    generate_enum();
    generate_log_struct();
    generate_dispatch();
    generate_log_writer();
    generate_log_reader();
    generate_logprint();
    generate_rollbacks();
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
   
