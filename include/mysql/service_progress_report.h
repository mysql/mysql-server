#ifndef MYSQL_SERVICE_PROGRESS_REPORT_INCLUDED
/* Copyright (C) 2011 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file
  This service allows plugins to report progress of long running operations
  to the server. The progress report is visible in SHOW PROCESSLIST,
  INFORMATION_SCHEMA.PROCESSLIST, and is sent to the client
  if requested.

  The functions are documented at
  http://kb.askmonty.org/en/progress-reporting#how-to-add-support-for-progress-reporting-to-a-storage-engine
*/

#ifdef __cplusplus
extern "C" {
#endif

#define thd_proc_info(thd, msg)  set_thd_proc_info(thd, msg, \
                                                   __func__, __FILE__, __LINE__)

extern struct progress_report_service_st {
  void (*thd_progress_init_func)(MYSQL_THD thd, unsigned int max_stage);
  void (*thd_progress_report_func)(MYSQL_THD thd,
                                   unsigned long long progress,
                                   unsigned long long max_progress);
  void (*thd_progress_next_stage_func)(MYSQL_THD thd);
  void (*thd_progress_end_func)(MYSQL_THD thd);
  const char *(*set_thd_proc_info_func)(MYSQL_THD, const char *info,
                                        const char *func,
                                        const char *file,
                                        unsigned int line);
} *progress_report_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define thd_progress_init(thd,max_stage) (progress_report_service->thd_progress_init_func((thd),(max_stage)))
#define thd_progress_report(thd, progress, max_progress) (progress_report_service->thd_progress_report_func((thd), (progress), (max_progress)))
#define thd_progress_next_stage(thd) (progress_report_service->thd_progress_next_stage_func(thd))
#define thd_progress_end(thd) (progress_report_service->thd_progress_end_func(thd))
#define set_thd_proc_info(thd,info,func,file,line) (progress_report_service->set_thd_proc_info_func((thd),(info),(func),(file),(line)))

#else

/**
   Report progress for long running operations 

   @param thd            User thread connection handle
   @param progress       Where we are now
   @param max_progress   Progress will continue up to this
*/
void thd_progress_init(MYSQL_THD thd, unsigned int max_stage);
void thd_progress_report(MYSQL_THD thd,
                         unsigned long long progress,
                         unsigned long long max_progress);
void thd_progress_next_stage(MYSQL_THD thd);
void thd_progress_end(MYSQL_THD thd);
const char *set_thd_proc_info(MYSQL_THD, const char * info, const char *func,
                              const char *file, unsigned int line);

#endif

#ifdef __cplusplus
}
#endif

#define MYSQL_SERVICE_PROGRESS_REPORT_INCLUDED
#endif

