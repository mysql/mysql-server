#include <my_pthread.h>

enum ma_service_thread_state {THREAD_RUNNING, THREAD_DYING, THREAD_DEAD};

typedef struct st_ma_service_thread_control
{
  /** 'kill' flag for the background thread */
  enum ma_service_thread_state status;
  /** if thread module was inited or not */
  my_bool inited;
  /** for killing the background thread */
  pthread_mutex_t *LOCK_control;
  /** for killing the background thread */
  pthread_cond_t *COND_control;
} MA_SERVICE_THREAD_CONTROL;


int ma_service_thread_control_init(MA_SERVICE_THREAD_CONTROL *control);
void ma_service_thread_control_end(MA_SERVICE_THREAD_CONTROL *control);
my_bool my_service_thread_sleep(MA_SERVICE_THREAD_CONTROL *control,
                                ulonglong sleep_time);
void my_service_thread_signal_end(MA_SERVICE_THREAD_CONTROL *control);
