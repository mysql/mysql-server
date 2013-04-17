#if !defined(TOKU_OS_INTERFACE_H)
#define TOKU_OS_INTERFACE_H

#if defined __cplusplus
extern "C" {
#endif

#include "toku_os_types.h"

// Returns: the current process id
int toku_os_getpid(void);

// Returns: the current thread id
int toku_os_gettid(void);

// Returns: the number of processors in the system
int toku_os_get_number_processors(void);

// Returns: the number of active processors in the system
int toku_os_get_number_active_processors(void);

// Returns: the system page size
int toku_os_get_pagesize(void);

// Returns: the total number of bytes of physical memory
uint64_t toku_os_get_phys_memory_size(void);

// Returns: 0 on success
// sets fsize to the number of bytes in a file
int toku_os_get_file_size(int fildes, int64_t *fsize);

// Returns: 0 on success
// Initializes id as a unique fileid for fildes on success.
int toku_os_get_unique_file_id(int fildes, struct fileid *id);

//Locks a file (should not be open to begin with).
//Returns: file descriptor (or -1 on error)
int toku_os_lock_file(char *name);

//Unlocks and closes a file locked by toku_os_lock_on_file
int toku_os_unlock_file(int fildes);

int toku_os_mkdir(const char *pathname, mode_t mode) __attribute__((__visibility__("default")));

// Get the current process user and kernel use times
int toku_os_get_process_times(struct timeval *usertime, struct timeval *kerneltime);

// Get the current in memory size (in bytes) of the current process
int toku_os_get_rss(int64_t *rss);

// Get the maximum in memory size (in bytes) of the current process
int toku_os_get_max_rss(int64_t *maxrss);

int toku_os_initialize_settings(int verbosity)  __attribute__((__visibility__("default")));

//
// this int acts like a bool, returns 0 for false, 1 for true
//
int toku_os_is_absolute_name(const char* path);

#if defined __cplusplus
};
#endif

#endif

