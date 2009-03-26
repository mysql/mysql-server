/* Do not remove this file even though it is empty.
This file is included in univ.i and will cause compilation failure
if not present.
A custom check has been added in the generated
storage/innobase/Makefile.in that is shipped with with the InnoDB Plugin
source archive. This check tries to compile a test program and if
successful then adds "#define HAVE_ATOMIC_PTHREAD_T" to this file.
This is a hack that has been developed in order to check for pthread_t
atomicity without the need to regenerate the ./configure script that is
distributed in the MySQL 5.1 official source archives.
If by any chance Makefile.in and ./configure are regenerated and thus
the hack from Makefile.in wiped away then the "real" check from plug.in
will take over.
*/
/* This is temprary fix for http://bugs.mysql.com/43740 */
/* force to enable */
#ifdef HAVE_GCC_ATOMIC_BUILTINS
#define HAVE_ATOMIC_PTHREAD_T
#endif
