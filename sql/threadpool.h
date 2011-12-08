
/* Threadpool parameters */
#ifdef _WIN32
extern uint threadpool_min_threads;  /* Minimum threads in pool */
#else
extern uint threadpool_idle_timeout; /* Shutdown idle worker threads  after this timeout */
extern uint threadpool_size; /* Number of parallel executing threads */
extern uint threadpool_stall_limit;  /* time interval in 10 ms units for stall checks*/
#endif
extern uint threadpool_max_threads;  /* Maximum threads in pool */

/*
  Threadpool statistics
*/
struct TP_STATISTICS
{
  /* Current number of worker thread. */
  volatile int num_worker_threads;
  /* Current number of idle threads. */
  volatile int num_waiting_threads;
  /* Number of login requests are queued but not yet processed. */
  volatile int pending_login_requests;
  /* Number of threads that are starting. */
  volatile int pending_thread_starts;
  /* Number of threads that are being shut down */
  volatile int pending_thread_shutdowns;
  /* Time (in milliseconds) since pool is blocked (num_waiting_threads is 0) */
  ulonglong pool_block_duration;
  /* Maximum duration of the pending login, im milliseconds. */
  ulonglong pending_login_duration; 
  /* Time since last thread was created */
  ulonglong time_since_last_thread_creation;
  /* Number of requests processed since pool monitor run last time. */
  volatile int requests_dequeued;
  volatile int requests_completed;
};

extern TP_STATISTICS tp_stats;


/* Functions to set threadpool parameters */
extern void tp_set_min_threads(uint val);
extern void tp_set_max_threads(uint val);

/* Activate threadpool scheduler */
extern void tp_scheduler(void);

