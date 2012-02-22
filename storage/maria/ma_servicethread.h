/*
   Copyright (c) 2009, 2011, Monty Program Ab

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

#include <my_pthread.h>

enum ma_service_thread_state {THREAD_RUNNING, THREAD_DYING, THREAD_DEAD};

typedef struct st_ma_service_thread_control
{
  /** 'kill' flag for the background thread */
  enum ma_service_thread_state status;
  /** if thread module was inited or not */
  my_bool inited;
  /** for killing the background thread */
  mysql_mutex_t *LOCK_control;
  /** for killing the background thread */
  mysql_cond_t *COND_control;
} MA_SERVICE_THREAD_CONTROL;


int ma_service_thread_control_init(MA_SERVICE_THREAD_CONTROL *control);
void ma_service_thread_control_end(MA_SERVICE_THREAD_CONTROL *control);
my_bool my_service_thread_sleep(MA_SERVICE_THREAD_CONTROL *control,
                                ulonglong sleep_time);
void my_service_thread_signal_end(MA_SERVICE_THREAD_CONTROL *control);
