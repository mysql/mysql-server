/* Does the OS already support struct timespec */
#undef _OS_HAS_TIMESPEC

/* For networking code: an integral type the size of an IP address (4
   octets).  Determined by examining return values from certain
   functions.  */
#undef pthread_ipaddr_type

/* For networking code: an integral type the size of an IP port number
   (2 octets).  Determined by examining return values from certain
   functions.  */
#undef pthread_ipport_type

/* type of clock_t, from system header files */
#undef pthread_clock_t

/* Specially named so grep processing will find it and put it into the
   generated ac-types.h.  */
#undef pthread_have_va_list_h

/* type of size_t, from system header files */
#undef pthread_size_t

/* type of ssize_t, from system header files */
#undef pthread_ssize_t

/* type of time_t, from system header files */
#undef pthread_time_t

/* type of fpos_t, from system header files */
#undef pthread_fpos_t

/* type of off_t, from system header files */
#undef pthread_off_t

/* type of va_list, from system header files */
#undef pthread_va_list

/* type of sigset_t, from system header files */
#undef pthread_sigset_t

/* I don't know why the native compiler definitions aren't sufficient
   for this.  */
#undef sunos4

/* define if the linker hauls in certain static data from libc even when
   you don't want it to.  yes, this description is bogus, but chris added
   the need for this, without describing the problem.  */
#undef LD_LINKS_STATIC_DATA

/* define if the system reissues the SIGCHLD if the handler reinstalls
 * itself before calling wait()
 */
#undef BROKEN_SIGNALS

/* where are terminal devices to be found? */
#undef _PATH_PTY

/* what directory holds the time zone info on this system? */
#undef _PATH_TZDIR

/* what file indicates the local time zone? */
#undef _PATH_TZFILE

/* Paths for various networking support files.  */
#undef _PATH_RESCONF
#undef _PATH_HOSTS
#undef _PATH_NETWORKS
#undef _PATH_PROTOCOLS
#undef _PATH_SERVICES

/* Path for Bourne shell.  */
#undef _PATH_BSHELL
