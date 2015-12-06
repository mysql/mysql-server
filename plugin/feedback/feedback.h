/* Copyright (C) 2010 Sergei Golubchik and Monty Program Ab

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

#define MYSQL_SERVER 1
#include <sql_class.h>

namespace feedback {

int fill_feedback(THD *thd, TABLE_LIST *tables, COND *cond);
int fill_plugin_version(THD *thd, TABLE_LIST *tables);
int fill_misc_data(THD *thd, TABLE_LIST *tables);
int fill_linux_info(THD *thd, TABLE_LIST *tables);

static const int SERVER_UID_SIZE= 29;
extern char server_uid_buf[SERVER_UID_SIZE+1], *user_info;
int calculate_server_uid(char *);
int prepare_linux_info();

extern ST_SCHEMA_TABLE *i_s_feedback;

extern ulong send_timeout, send_retry_wait;

pthread_handler_t background_thread(void *arg);

/**
  The class for storing urls to send report data to.

  Constructors are private, the object should be created with create() method.
  send() method does the actual sending.
*/
class Url {
  protected:
  Url(LEX_STRING &url_arg) : full_url(url_arg) {}
  const LEX_STRING full_url;

  public:
  virtual ~Url() { my_free(full_url.str); }

  const char *url()   { return full_url.str; }
  size_t url_length() { return full_url.length; }
  virtual int send(const char* data, size_t data_length) =  0;

  static Url* create(const char *url, size_t url_length);
};

extern Url **urls;
extern uint url_count;

extern ulong startup_interval;
extern ulong first_interval;
extern ulong interval;

/* these are used to communicate with the background thread */
extern mysql_mutex_t sleep_mutex;
extern mysql_cond_t sleep_condition;
extern volatile bool shutdown_plugin;

} // namespace feedback

