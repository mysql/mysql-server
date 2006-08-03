#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H

#if (defined(_SCO_DS) || defined(UNIXWARE_7)) && !defined(SHUT_RDWR)
/*
   SHUT_* functions are defined only if
   "(defined(_XOPEN_SOURCE) && _XOPEN_SOURCE_EXTENDED - 0 >= 1)"
*/
#define SHUT_RDWR 2
#endif

#ifdef __WIN__

#define vsnprintf _vsnprintf
#define snprintf _snprintf

#define SIGKILL 9
#define SHUT_RDWR 0x2

/*TODO:  fix this */
#define PROTOCOL_VERSION 10

typedef int pid_t;

#undef popen
#define popen(A,B) _popen(A,B)

#endif /* __WIN__ */

#endif  /* INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H */


