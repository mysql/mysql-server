#ifndef __SYS_TIMERS_H__
#define __SYS_TIMERS_H__

struct timespec {
    time_t  tv_sec;
    long    tv_nsec; 
}; 

#define TIMEVAL_TO_TIMESPEC(tv, ts) {                                   \
        (ts)->tv_sec = (tv)->tv_sec;                                    \
        (ts)->tv_nsec = (tv)->tv_usec * 1000;                           \
}
#define TIMESPEC_TO_TIMEVAL(tv, ts) {                                   \
        (tv)->tv_sec = (ts)->tv_sec;                                    \
        (tv)->tv_usec = (ts)->tv_nsec / 1000;                           \
}

#endif /* !__SYS_TIMERS_H__ */
