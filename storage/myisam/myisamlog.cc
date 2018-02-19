/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* write whats in isam.log */

#include "my_config.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_tree.h"
#include "print_version.h"
#include "storage/myisam/myisamdef.h"
#include "welcome_copyright_notice.h"
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#define FILENAME(A) (A ? A->show_name : "Unknown")

struct file_info {
  long process;
  int filenr, id;
  uint rnd;
  char *name, *show_name;
  uchar *record;
  MI_INFO *isam;
  bool closed, used;
  ulong accessed;
};

struct test_if_open_param {
  char *name;
  int max_id;
};

struct st_access_param {
  ulong min_accessed;
  struct file_info *found;
};

#define NO_FILEPOS (ulong) ~0L

extern int main(int argc, char **argv);
static void get_options(int *argc, char ***argv);
static int examine_log(char *file_name, char **table_names);
static int read_string(IO_CACHE *file, uchar **to, uint length);
static int file_info_compare(const void *cmp_arg, const void *a, const void *b);
static int test_if_open(struct file_info *key, element_count count,
                        struct test_if_open_param *param);
static void fix_blob_pointers(MI_INFO *isam, uchar *record);
static int test_when_accessed(struct file_info *key, element_count count,
                              struct st_access_param *access_param);
static void file_info_free(struct file_info *info);
static int close_some_file(TREE *tree);
static int reopen_closed_file(TREE *tree, struct file_info *file_info);
static int find_record_with_key(struct file_info *file_info, uchar *record);
static void printf_log(const char *str, ...)
    MY_ATTRIBUTE((format(printf, 1, 2)));
static bool cmp_filename(struct file_info *file_info, char *name);

static uint verbose = 0, update = 0, test_info = 0, max_files = 0,
            re_open_count = 0, recover = 0, prefix_remove = 0,
            opt_processes = 0;
static char *log_filename = 0, *filepath = 0, *write_filename = 0;
static char *record_pos_file = 0;
static ulong com_count[10][3], number_of_commands = (ulong)~0L, isamlog_process;
static my_off_t isamlog_filepos, start_offset = 0, record_pos = HA_OFFSET_ERROR;
static const char *command_name[] = {"open",       "write", "update", "delete",
                                     "close",      "extra", "lock",   "re-open",
                                     "delete-all", NullS};

extern st_keycache_thread_var *keycache_thread_var() {
  return &main_thread_keycache_var;
}

int main(int argc, char **argv) {
  int error, i, first;
  ulong total_count, total_error, total_recover;
  MY_INIT(argv[0]);

  memset(&main_thread_keycache_var, 0, sizeof(st_keycache_thread_var));
  mysql_cond_init(PSI_NOT_INSTRUMENTED, &main_thread_keycache_var.suspend);

  log_filename = myisam_log_filename;
  get_options(&argc, &argv);
  /* Number of MyISAM files we can have open at one time */
  max_files = (my_set_max_open_files(MY_MIN(max_files, 8)) - 6) / 2;
  if (update)
    printf("Trying to %s MyISAM files according to log '%s'\n",
           (recover ? "recover" : "update"), log_filename);
  error = examine_log(log_filename, argv);
  if (update && !error) puts("Tables updated successfully");
  total_count = total_error = total_recover = 0;
  for (i = first = 0; command_name[i]; i++) {
    if (com_count[i][0]) {
      if (!first++) {
        if (verbose || update) puts("");
        puts("Commands   Used count    Errors   Recover errors");
      }
      printf("%-12s%9ld%10ld%17ld\n", command_name[i], com_count[i][0],
             com_count[i][1], com_count[i][2]);
      total_count += com_count[i][0];
      total_error += com_count[i][1];
      total_recover += com_count[i][2];
    }
  }
  if (total_count)
    printf("%-12s%9ld%10ld%17ld\n", "Total", total_count, total_error,
           total_recover);
  if (re_open_count)
    printf("Had to do %d re-open because of too few possibly open files\n",
           re_open_count);
  (void)mi_panic(HA_PANIC_CLOSE);
  my_free_open_file_info();
  my_end(test_info ? MY_CHECK_ERROR | MY_GIVE_INFO : MY_CHECK_ERROR);
  mysql_cond_destroy(&main_thread_keycache_var.suspend);
  exit(error);
} /* main */

static void get_options(int *argc, char ***argv) {
  int help, version;
  const char *pos, *usage;
  char option;

  help = 0;
  usage =
      "Usage: %s [-?iruvDIV] [-c #] [-f #] [-F filepath/] [-o #] [-R file "
      "recordpos] [-w write_file] [log-filename [table ...]] \n";
  pos = "";

  while (--*argc > 0 && *(pos = *(++*argv)) == '-') {
    while (*++pos) {
      version = 0;
      switch ((option = *pos)) {
        case '#':
          DBUG_PUSH(++pos);
          pos = " "; /* Skip rest of arg */
          break;
        case 'c':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          number_of_commands = (ulong)atol(pos);
          pos = " ";
          break;
        case 'u':
          update = 1;
          break;
        case 'f':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          max_files = (uint)atoi(pos);
          pos = " ";
          break;
        case 'i':
          test_info = 1;
          break;
        case 'o':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          start_offset = (my_off_t)my_strtoll(pos, NULL, 10);
          pos = " ";
          break;
        case 'p':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          prefix_remove = atoi(pos);
          break;
        case 'r':
          update = 1;
          recover++;
          break;
        case 'P':
          opt_processes = 1;
          break;
        case 'R':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          record_pos_file = (char *)pos;
          if (!--*argc) goto err;
          record_pos = (my_off_t)my_strtoll(*(++*argv), NULL, 10);
          pos = " ";
          break;
        case 'v':
          verbose++;
          break;
        case 'w':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          write_filename = (char *)pos;
          pos = " ";
          break;
        case 'F':
          if (!*++pos) {
            if (!--*argc)
              goto err;
            else
              pos = *(++*argv);
          }
          filepath = (char *)pos;
          pos = " ";
          break;
        case 'V':
          version = 1;
          /* Fall through */
        case 'I':
        case '?':
          print_version();
          puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
          if (version) break;
          puts("Write info about whats in a MyISAM log file.");
          printf("If no file name is given %s is used\n", log_filename);
          puts("");
          printf(usage, my_progname);
          puts("");
          puts(
              "Options: -? or -I \"Info\"     -V \"version\"   -c \"do only # "
              "commands\"");
          puts(
              "         -f \"max open files\" -F \"filepath\"  -i \"extra "
              "info\"");
          puts(
              "         -o \"offset\"         -p # \"remove # components from "
              "path\"");
          puts("         -r \"recover\"        -R \"file recordposition\"");
          puts(
              "         -u \"update\"         -v \"verbose\"   -w \"write "
              "file\"");
          puts("         -D \"myisam compiled with DBUG\"   -P \"processes\"");
          puts("\nOne can give a second and a third '-v' for more verbose.");
          puts("Normaly one does a update (-u).");
          puts(
              "If a recover is done all writes and all possibly updates and "
              "deletes is done\nand errors are only counted.");
          puts(
              "If one gives table names as arguments only these tables will be "
              "updated\n");
          help = 1;
          break;
        default:
          printf("illegal option: \"-%c\"\n", *pos);
          break;
      }
    }
  }
  if (!*argc) {
    if (help) exit(0);
    (*argv)++;
  }
  if (*argc >= 1) {
    log_filename = (char *)pos;
    (*argc)--;
    (*argv)++;
  }
  return;
err:
  (void)fprintf(stderr, "option \"%c\" used without or with wrong argument\n",
                option);
  exit(1);
}

static int examine_log(char *file_name, char **table_names) {
  uint command, result, files_open;
  ulong access_time, length;
  my_off_t filepos;
  int lock_command, mi_result;
  char isam_file_name[FN_REFLEN], llbuff[21], llbuff2[21];
  uchar head[20];
  uchar *buff;
  struct test_if_open_param open_param;
  IO_CACHE cache;
  File file;
  FILE *write_file;
  enum ha_extra_function extra_command;
  TREE tree;
  struct file_info file_info, *curr_file_info;
  DBUG_ENTER("examine_log");

  if ((file = my_open(file_name, O_RDONLY, MYF(MY_WME))) < 0) DBUG_RETURN(1);
  write_file = 0;
  if (write_filename) {
    if (!(write_file = my_fopen(write_filename, O_WRONLY, MYF(MY_WME)))) {
      my_close(file, MYF(0));
      DBUG_RETURN(1);
    }
  }

  init_io_cache(&cache, file, 0, READ_CACHE, start_offset, 0, MYF(0));
  memset(com_count, 0, sizeof(com_count));
  init_tree(&tree, 0, 0, sizeof(file_info), file_info_compare, 1,
            (tree_element_free)file_info_free, NULL);
  (void)init_key_cache(dflt_key_cache, KEY_CACHE_BLOCK_SIZE, KEY_CACHE_SIZE, 0,
                       0);

  files_open = 0;
  access_time = 0;
  while (access_time++ != number_of_commands &&
         !my_b_read(&cache, (uchar *)head, 9)) {
    isamlog_filepos = my_b_tell(&cache) - 9L;
    file_info.filenr = mi_uint2korr(head + 1);
    isamlog_process = file_info.process = (long)mi_uint4korr(head + 3);
    if (!opt_processes) file_info.process = 0;
    result = mi_uint2korr(head + 7);
    if ((curr_file_info = (struct file_info *)tree_search(&tree, &file_info,
                                                          tree.custom_arg))) {
      curr_file_info->accessed = access_time;
      if (update && curr_file_info->used && curr_file_info->closed) {
        if (reopen_closed_file(&tree, curr_file_info)) {
          command = sizeof(com_count) / sizeof(com_count[0][0]) / 3;
          result = 0;
          goto com_err;
        }
      }
    }
    command = (uint)head[0];
    if (command < sizeof(com_count) / sizeof(com_count[0][0]) / 3 &&
        (!table_names[0] || (curr_file_info && curr_file_info->used))) {
      com_count[command][0]++;
      if (result) com_count[command][1]++;
    }
    switch ((enum myisam_log_commands)command) {
      case MI_LOG_OPEN:
        if (!table_names[0]) {
          com_count[command][0]--; /* Must be counted explicite */
          if (result) com_count[command][1]--;
        }

        if (curr_file_info)
          printf(
              "\nWarning: %s is opened with same process and filenumber\n"
              "Maybe you should use the -P option ?\n",
              curr_file_info->show_name);
        if (my_b_read(&cache, (uchar *)head, 2)) goto err;
        buff = 0;
        file_info.name = 0;
        file_info.show_name = 0;
        file_info.record = 0;
        if (read_string(&cache, &buff, (uint)mi_uint2korr(head))) goto err;
        {
          uint i;
          char *pos, *to;

          /* Fix if old DOS files to new format */
          for (pos = file_info.name = (char *)buff; (pos = strchr(pos, '\\'));
               pos++)
            *pos = '/';

          pos = file_info.name;
          for (i = 0; i < prefix_remove; i++) {
            char *next;
            if (!(next = strchr(pos, '/'))) break;
            pos = next + 1;
          }
          to = isam_file_name;
          if (filepath) to = convert_dirname(isam_file_name, filepath, NullS);
          my_stpcpy(to, pos);
          fn_ext(isam_file_name)[0] = 0; /* Remove extension */
        }
        open_param.name = file_info.name;
        open_param.max_id = 0;
        (void)tree_walk(&tree, (tree_walk_action)test_if_open,
                        (void *)&open_param, left_root_right);
        file_info.id = open_param.max_id + 1;
        /*
         * In the line below +10 is added to accomodate '<' and '>' chars
         * plus '\0' at the end, so that there is place for 7 digits.
         * It is  improbable that same table can have that many entries in
         * the table cache.
         * The additional space is needed for the sprintf commands two lines
         * below.
         */
        file_info.show_name =
            (char *)my_memdup(PSI_NOT_INSTRUMENTED, isam_file_name,
                              (uint)strlen(isam_file_name) + 10, MYF(MY_WME));
        if (file_info.id > 1)
          sprintf(strend(file_info.show_name), "<%d>", file_info.id);
        file_info.closed = 1;
        file_info.accessed = access_time;
        file_info.used = 1;
        if (table_names[0]) {
          char **name;
          file_info.used = 0;
          for (name = table_names; *name; name++) {
            if (!strcmp(*name, isam_file_name))
              file_info.used = 1; /* Update/log only this */
          }
        }
        if (update && file_info.used) {
          if (files_open >= max_files) {
            if (close_some_file(&tree)) goto com_err;
            files_open--;
          }
          if (!(file_info.isam =
                    mi_open(isam_file_name, O_RDWR, HA_OPEN_WAIT_IF_LOCKED)))
            goto com_err;
          if (!(file_info.record = (uchar *)my_malloc(
                    PSI_NOT_INSTRUMENTED, file_info.isam->s->base.reclength,
                    MYF(MY_WME))))
            goto end;
          files_open++;
          file_info.closed = 0;
        }
        (void)tree_insert(&tree, (uchar *)&file_info, 0, tree.custom_arg);
        if (file_info.used) {
          if (verbose && !record_pos_file)
            printf_log("%s: open -> %d", file_info.show_name, file_info.filenr);
          com_count[command][0]++;
          if (result) com_count[command][1]++;
        }
        break;
      case MI_LOG_CLOSE:
        if (verbose && !record_pos_file &&
            (!table_names[0] || (curr_file_info && curr_file_info->used)))
          printf_log("%s: %s -> %d", FILENAME(curr_file_info),
                     command_name[command], result);
        if (curr_file_info) {
          if (!curr_file_info->closed) files_open--;
          (void)tree_delete(&tree, (uchar *)curr_file_info, 0, tree.custom_arg);
        }
        break;
      case MI_LOG_EXTRA:
        if (my_b_read(&cache, (uchar *)head, 1)) goto err;
        extra_command = (enum ha_extra_function)head[0];
        if (verbose && !record_pos_file &&
            (!table_names[0] || (curr_file_info && curr_file_info->used)))
          printf_log("%s: %s(%d) -> %d", FILENAME(curr_file_info),
                     command_name[command], (int)extra_command, result);
        if (update && curr_file_info && !curr_file_info->closed) {
          if (mi_extra(curr_file_info->isam, extra_command, 0) != (int)result) {
            fflush(stdout);
            (void)fprintf(
                stderr, "Warning: error %d, expected %d on command %s at %s\n",
                my_errno(), result, command_name[command],
                llstr(isamlog_filepos, llbuff));
            fflush(stderr);
          }
        }
        break;
      case MI_LOG_DELETE:
        if (my_b_read(&cache, (uchar *)head, 8)) goto err;
        filepos = mi_sizekorr(head);
        if (verbose &&
            (!record_pos_file ||
             ((record_pos == filepos || record_pos == NO_FILEPOS) &&
              !cmp_filename(curr_file_info, record_pos_file))) &&
            (!table_names[0] || (curr_file_info && curr_file_info->used)))
          printf_log("%s: %s at %ld -> %d", FILENAME(curr_file_info),
                     command_name[command], (long)filepos, result);
        if (update && curr_file_info && !curr_file_info->closed) {
          if (mi_rrnd(curr_file_info->isam, curr_file_info->record, filepos)) {
            if (!recover) goto com_err;
            if (verbose)
              printf_log("error: Didn't find row to delete with mi_rrnd");
            com_count[command][2]++; /* Mark error */
          }
          mi_result = mi_delete(curr_file_info->isam, curr_file_info->record);
          if ((mi_result == 0 && result) ||
              (mi_result && (uint)my_errno() != result)) {
            if (!recover) goto com_err;
            if (mi_result) com_count[command][2]++; /* Mark error */
            if (verbose)
              printf_log("error: Got result %d from mi_delete instead of %d",
                         mi_result, result);
          }
        }
        break;
      case MI_LOG_WRITE:
      case MI_LOG_UPDATE:
        if (my_b_read(&cache, (uchar *)head, 12)) goto err;
        filepos = mi_sizekorr(head);
        length = mi_uint4korr(head + 8);
        buff = 0;
        if (read_string(&cache, &buff, (uint)length)) goto err;
        if ((!record_pos_file ||
             ((record_pos == filepos || record_pos == NO_FILEPOS) &&
              !cmp_filename(curr_file_info, record_pos_file))) &&
            (!table_names[0] || (curr_file_info && curr_file_info->used))) {
          if (write_file && (my_fwrite(write_file, buff, length,
                                       MYF(MY_WAIT_IF_FULL | MY_NABP))))
            goto end;
          if (verbose)
            printf_log("%s: %s at %ld, length=%ld -> %d",
                       FILENAME(curr_file_info), command_name[command],
                       (long)filepos, length, result);
        }
        if (update && curr_file_info && !curr_file_info->closed) {
          if (curr_file_info->isam->s->base.blobs)
            fix_blob_pointers(curr_file_info->isam, buff);
          if ((enum myisam_log_commands)command == MI_LOG_UPDATE) {
            if (mi_rrnd(curr_file_info->isam, curr_file_info->record,
                        filepos)) {
              if (!recover) {
                result = 0;
                goto com_err;
              }
              if (verbose)
                printf_log("error: Didn't find row to update with mi_rrnd");
              if (recover == 1 || result ||
                  find_record_with_key(curr_file_info, buff)) {
                com_count[command][2]++; /* Mark error */
                break;
              }
            }
            mi_result =
                mi_update(curr_file_info->isam, curr_file_info->record, buff);
            if ((mi_result == 0 && result) ||
                (mi_result && (uint)my_errno() != result)) {
              if (!recover) goto com_err;
              if (verbose)
                printf_log("error: Got result %d from mi_update instead of %d",
                           mi_result, result);
              if (mi_result) com_count[command][2]++; /* Mark error */
            }
          } else {
            mi_result = mi_write(curr_file_info->isam, buff);
            if ((mi_result == 0 && result) ||
                (mi_result && (uint)my_errno() != result)) {
              if (!recover) goto com_err;
              if (verbose)
                printf_log("error: Got result %d from mi_write instead of %d",
                           mi_result, result);
              if (mi_result) com_count[command][2]++; /* Mark error */
            }
            if (!recover && filepos != curr_file_info->isam->lastpos) {
              printf("error: Wrote at position: %s, should have been %s",
                     llstr(curr_file_info->isam->lastpos, llbuff),
                     llstr(filepos, llbuff2));
              goto end;
            }
          }
        }
        my_free(buff);
        break;
      case MI_LOG_LOCK:
        if (my_b_read(&cache, (uchar *)head, sizeof(lock_command))) goto err;
        memcpy(&lock_command, head, sizeof(lock_command));
        if (verbose && !record_pos_file &&
            (!table_names[0] || (curr_file_info && curr_file_info->used)))
          printf_log("%s: %s(%d) -> %d\n", FILENAME(curr_file_info),
                     command_name[command], lock_command, result);
        if (update && curr_file_info && !curr_file_info->closed) {
          if (mi_lock_database(curr_file_info->isam, lock_command) !=
              (int)result)
            goto com_err;
        }
        break;
      case MI_LOG_DELETE_ALL:
        if (verbose && !record_pos_file &&
            (!table_names[0] || (curr_file_info && curr_file_info->used)))
          printf_log("%s: %s -> %d\n", FILENAME(curr_file_info),
                     command_name[command], result);
        break;
      default:
        fflush(stdout);
        (void)fprintf(stderr,
                      "Error: found unknown command %d in logfile, aborted\n",
                      command);
        fflush(stderr);
        goto end;
    }
  }
  end_key_cache(dflt_key_cache, 1);
  delete_tree(&tree);
  (void)end_io_cache(&cache);
  (void)my_close(file, MYF(0));
  if (write_file && my_fclose(write_file, MYF(MY_WME))) DBUG_RETURN(1);
  DBUG_RETURN(0);

err:
  fflush(stdout);
  (void)fprintf(stderr, "Got error %d when reading from logfile\n", my_errno());
  fflush(stderr);
  goto end;
com_err:
  fflush(stdout);
  (void)fprintf(stderr, "Got error %d, expected %d on command %s at %s\n",
                my_errno(), result, command_name[command],
                llstr(isamlog_filepos, llbuff));
  fflush(stderr);
end:
  end_key_cache(dflt_key_cache, 1);
  delete_tree(&tree);
  (void)end_io_cache(&cache);
  (void)my_close(file, MYF(0));
  if (write_file) (void)my_fclose(write_file, MYF(MY_WME));
  DBUG_RETURN(1);
}

static int read_string(IO_CACHE *file, uchar **to, uint length) {
  DBUG_ENTER("read_string");

  if (*to) my_free(*to);
  if (!(*to = (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, length + 1,
                                 MYF(MY_WME))) ||
      my_b_read(file, (uchar *)*to, length)) {
    if (*to) my_free(*to);
    *to = 0;
    DBUG_RETURN(1);
  }
  *((uchar *)*to + length) = '\0';
  DBUG_RETURN(0);
} /* read_string */

static int file_info_compare(const void *cmp_arg MY_ATTRIBUTE((unused)),
                             const void *a, const void *b) {
  long lint;

  if ((lint =
           ((struct file_info *)a)->process - ((struct file_info *)b)->process))
    return lint < 0L ? -1 : 1;
  return ((struct file_info *)a)->filenr - ((struct file_info *)b)->filenr;
}

/* ARGSUSED */

static int test_if_open(struct file_info *key,
                        element_count count MY_ATTRIBUTE((unused)),
                        struct test_if_open_param *param) {
  if (!strcmp(key->name, param->name) && key->id > param->max_id)
    param->max_id = key->id;
  return 0;
}

static void fix_blob_pointers(MI_INFO *info, uchar *record) {
  uchar *pos;
  MI_BLOB *blob, *end;

  pos = record + info->s->base.reclength;
  for (end = info->blobs + info->s->base.blobs, blob = info->blobs; blob != end;
       blob++) {
    memcpy(record + blob->offset + blob->pack_length, &pos, sizeof(char *));
    pos += _mi_calc_blob_length(blob->pack_length, record + blob->offset);
  }
}

/* close the file with hasn't been accessed for the longest time */
/* ARGSUSED */

static int test_when_accessed(struct file_info *key,
                              element_count count MY_ATTRIBUTE((unused)),
                              struct st_access_param *access_param) {
  if (key->accessed < access_param->min_accessed && !key->closed) {
    access_param->min_accessed = key->accessed;
    access_param->found = key;
  }
  return 0;
}

static void file_info_free(struct file_info *fileinfo) {
  DBUG_ENTER("file_info_free");
  if (update) {
    if (!fileinfo->closed) (void)mi_close(fileinfo->isam);
    if (fileinfo->record) my_free(fileinfo->record);
  }
  my_free(fileinfo->name);
  my_free(fileinfo->show_name);
  DBUG_VOID_RETURN;
}

static int close_some_file(TREE *tree) {
  struct st_access_param access_param;

  access_param.min_accessed = LONG_MAX;
  access_param.found = 0;

  (void)tree_walk(tree, (tree_walk_action)test_when_accessed,
                  (void *)&access_param, left_root_right);
  if (!access_param.found)
    return 1; /* No open file that is possibly to close */
  if (mi_close(access_param.found->isam)) return 1;
  access_param.found->closed = 1;
  return 0;
}

static int reopen_closed_file(TREE *tree, struct file_info *fileinfo) {
  char name[FN_REFLEN];
  if (close_some_file(tree)) return 1; /* No file to close */
  my_stpcpy(name, fileinfo->show_name);
  if (fileinfo->id > 1) *strrchr(name, '<') = '\0'; /* Remove "<id>" */

  if (!(fileinfo->isam = mi_open(name, O_RDWR, HA_OPEN_WAIT_IF_LOCKED)))
    return 1;
  fileinfo->closed = 0;
  re_open_count++;
  return 0;
}

/* Try to find record with uniq key */

static int find_record_with_key(struct file_info *file_info, uchar *record) {
  uint key;
  MI_INFO *info = file_info->isam;
  uchar tmp_key[MI_MAX_KEY_BUFF];

  for (key = 0; key < info->s->base.keys; key++) {
    if (mi_is_key_active(info->s->state.key_map, key) &&
        info->s->keyinfo[key].flag & HA_NOSAME) {
      (void)_mi_make_key(info, key, tmp_key, record, 0L);
      return mi_rkey(info, file_info->record, (int)key, tmp_key, 0,
                     HA_READ_KEY_EXACT);
    }
  }
  return 1;
}

static void printf_log(const char *format, ...) {
  char llbuff[21];
  va_list args;
  va_start(args, format);
  if (verbose > 2) printf("%9s:", llstr(isamlog_filepos, llbuff));
  if (verbose > 1) printf("%5ld ", isamlog_process); /* Write process number */
  (void)vprintf((char *)format, args);
  putchar('\n');
  va_end(args);
}

static bool cmp_filename(struct file_info *file_info, char *name) {
  if (!file_info) return 1;
  return strcmp(file_info->name, name) ? 1 : 0;
}

#include "storage/myisam/mi_extrafunc.h"
