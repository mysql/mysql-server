/* If you need any special typedefs for function pointers &c to try
   testing for in configure.in, define them here.  */

/* According to ANSI, two struct types in the same module are not
   compatible types.  So there's no way to define a type for
   pthread_sigset_t that's compatible with sigset_t when they're
   structure types, if we assume we can't pull in a __sigset_t or
   something by itself from system header files.

   Since that was my main reason for creating this file, there isn't
   anything here now.  If after working on this code a bit longer we
   don't find anything else to put here, this file should just go
   away.  */
