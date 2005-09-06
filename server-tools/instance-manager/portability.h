#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H

#ifdef __WIN__

#define vsnprintf _vsnprintf

#define SIGKILL 9
#define SHUT_RDWR 0x2

/*TODO:  fix this */
#define DEFAULT_MONITORING_INTERVAL 20
#define DEFAULT_PORT 2273
#define PROTOCOL_VERSION 10

typedef int pid_t;

#undef popen
#define popen(A,B) _popen(A,B)

#endif /* __WIN__ */

#endif  /* INCLUDES_MYSQL_INSTANCE_MANAGER_PORTABILITY_H */


