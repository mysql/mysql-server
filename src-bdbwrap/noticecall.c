#include <db.h>
#include <pthread.h>

#define MANAGER_BERKELEY_LOG_CLEANUP    (1L << 0)

ulong volatile manager_status;
pthread_mutex_t LOCK_manager;
pthread_cond_t COND_manager;

void berkeley_noticecall(DB_ENV *db_env, db_notices notice)
{
  switch (notice)
  {
  case DB_NOTICE_LOGFILE_CHANGED: /* purecov: tested */
    pthread_mutex_lock(&LOCK_manager);
    manager_status |= MANAGER_BERKELEY_LOG_CLEANUP;
    pthread_mutex_unlock(&LOCK_manager);
    pthread_cond_signal(&COND_manager);
    break;
  }
}

#define uint5korr(A)	((unsigned long long)(((unsigned long long) ((unsigned char) (A)[0])) +\
				    (((unsigned long long) ((unsigned char) (A)[1])) << 8) +\
				    (((unsigned long long) ((unsigned char) (A)[2])) << 16) +\
				    (((unsigned long long) ((unsigned char) (A)[3])) << 24)) +\
				    (((unsigned long long) ((unsigned char) (A)[4])) << 32))

int
berkeley_cmp_hidden_key(DB* file, const DBT *new_key, const DBT *saved_key)
{
  unsigned long long a=uint5korr((char*) new_key->data);
  unsigned long long b=uint5korr((char*) saved_key->data);
  return  a < b ? -1 : (a > b ? 1 : 0);
}
