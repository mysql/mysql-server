#ifndef _TOKU_DIRENT_H
#define _TOKU_DIRENT_H

#if defined(__cplusplus)
extern "C" {
#endif

//The DIR functions do not exist in windows, but the Linux API ends up
//just using a wrapper.  We might convert these into an toku_os_* type api.

DIR *opendir(const char *name);

struct dirent *readdir(DIR *dir);

int closedir(DIR *dir);

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#if defined(__cplusplus)
};
#endif

#endif

