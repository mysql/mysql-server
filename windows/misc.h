#ifndef _MISC_H
#define _MISC_H
#include "os.h"
#include <sys/stat.h>

//These are functions that really exist in windows but are named
//something else.
//TODO: Sort these into some .h file that makes sense.

int fsync(int fildes);

int gettimeofday(struct timeval *tv, struct timezone *tz);

long long int strtoll(const char *nptr, char **endptr, int base);


//TODO: Enforce use of these macros. Otherwise, open, creat, and chmod may fail
//os_mkdir actually ignores the permissions, so it won't fail.

//Permissions
//User permissions translate to global
//Execute bit does not exist
//TODO: Determine if we need to use BINARY mode for opening.
#define S_IRWXU     S_IRUSR | S_IWUSR | S_IXUSR
#define S_IRUSR     S_IREAD
#define S_IWUSR     S_IWRITE
//Execute bit does not exist
#define S_IXUSR     (0)

//Group permissions thrown away.
#define S_IRWXG     S_IRGRP | S_IWGRP | S_IXGRP
#define S_IRGRP     (0)
#define S_IWGRP     (0)
#define S_IXGRP     (0)

//Other permissions thrown away. (Except for read)
//MySQL defines S_IROTH as S_IREAD.  Avoid the warning.
#if defined(S_IROTH)
#undef S_IROTH
#endif
#define S_IRWXO     S_IROTH | S_IWOTH | S_IXOTH
#define S_IROTH     S_IREAD
#define S_IWOTH     (0)
#define S_IXOTH     (0)

long int random(void);
void srandom(unsigned int seed);

//snprintf
//TODO: Put in its own file, or define a snprintf function based on _vsnprintf
//in its own file
#define snprintf(str, size, fmt, ...) _snprintf(str, size, fmt, __VA_ARGS__)

//strtoll has a different name in windows.
#define strtoll     _strtoi64
#define strtoull    _strtoui64

//rmdir has a different name in windows.
#define rmdir       _rmdir

#ifndef PATH_MAX
#define PATH_MAX 1
#endif

char *realpath(const char *path, char *resolved_path);


int unsetenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);

#endif

