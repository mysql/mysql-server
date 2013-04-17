#define _CRT_SECURE_NO_DEPRECATE

//rand_s requires _CRT_RAND_S be defined before including stdlib
#define _CRT_RAND_S
#include <stdlib.h>

#include <windows.h>
#include "toku_portability.h"
#include <dirent.h>
#include <assert.h>
#include <direct.h>
#include <errno.h>
#include <io.h>
#include <malloc.h>
#include <process.h>
#include <share.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <Crtdbg.h>

struct __toku_windir {
    struct dirent         ent;
    struct _finddatai64_t data;
    intptr_t              handle;
    BOOL                  finished;
};

DIR*
opendir(const char *name) {
    char *format = NULL;
    DIR *result = malloc(sizeof(*result));
    int r;
    if (!result) {
        r = ENOMEM;
        goto cleanup;
    }
    format = malloc(strlen(name)+2+1); //2 for /*, 1 for '\0'
    if (!format) {
        r = ENOMEM;
        goto cleanup;
    }
    strcpy(format, name);
    if (format[strlen(format)-1]=='/') format[strlen(format)-1]='\0';
    strcat(format, "/*");
    result->handle = _findfirsti64(format, &result->data);
    // printf("%s:%d %p %d\n", __FILE__, __LINE__, result->handle, errno); fflush(stdout);
    if (result->handle==-1L) {
        if (errno==ENOENT) {
            int64_t r_stat;
            //ENOENT can mean a good directory with no files, OR
            //a directory that does not exist.
            struct _stat64 buffer;
            format[strlen(format)-3] = '\0'; //Strip the "/*"
            r_stat = _stati64(format, &buffer);
            if (r_stat==0) {
                //Empty directory.
                result->finished = TRUE;
                r = 0;
                goto cleanup;
            }
        }
        r = errno;
        assert(r!=0);
        goto cleanup;
    }
    result->finished = FALSE;
    r = 0;
cleanup:
    if (r!=0) {
        if (result) free(result);
        result = NULL;
    }
    if (format) free(format);
    return result;
}

struct dirent*
readdir(DIR *dir) {
    struct dirent *result;
    int r;
    if (dir->finished) {
        errno = ENOENT;
        result = NULL;
        goto cleanup;
    }
    assert(dir->handle!=-1L);
    strcpy(dir->ent.d_name, dir->data.name);
    if (dir->data.attrib&_A_SUBDIR) dir->ent.d_type=DT_DIR;
    else                            dir->ent.d_type=DT_REG;
    r = _findnexti64(dir->handle, &dir->data);
    if (r==-1L) dir->finished = TRUE;
    result = &dir->ent;
cleanup:
    return result;
}

int
closedir(DIR *dir) {
    int r;
    if (dir->handle==-1L) r = 0;
    else r = _findclose(dir->handle);
    free(dir);
    return r;
}

int
fsync(int fildes) {
    int r = _commit(fildes);
    return r;
}

int
toku_os_get_file_size(int fildes, int64_t *size) {
    struct _stat64 sbuf;
    int r = _fstati64(fildes, &sbuf);
    if (r==0) {
        *size = sbuf.st_size;
    }
    return r;
}

uint64_t 
toku_os_get_phys_memory_size(void) {
    MEMORYSTATUS memory_status;
    GlobalMemoryStatus(&memory_status);
    return memory_status.dwTotalPhys;
}


int 
toku_os_get_number_processors(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

int 
toku_os_get_number_active_processors(void) {
    SYSTEM_INFO system_info;
    DWORD mask, n;
    GetSystemInfo(&system_info);
    mask = system_info.dwActiveProcessorMask;
    for (n=0; mask; mask >>= 1)
        n += mask & 1;
    return n;
}

int 
toku_os_get_pagesize(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

int
toku_os_get_unique_file_id(int fildes, struct fileid *id) {
    int r;
    BY_HANDLE_FILE_INFORMATION info;
    HANDLE filehandle;

    memset(id, 0, sizeof(*id));
    filehandle = (HANDLE)_get_osfhandle(fildes);
    if (filehandle==INVALID_HANDLE_VALUE) {
        r = errno; assert(r!=0);
        goto cleanup;
    }
    r = GetFileInformationByHandle(filehandle, &info);
    if (r==0) { //0 is error here.
        r = GetLastError(); assert(r!=0);
        goto cleanup;
    }
    id->st_dev     = info.dwVolumeSerialNumber;
    id->st_ino     = info.nFileIndexHigh;
    id->st_ino   <<= 32;
    id->st_ino    |= info.nFileIndexLow;
    id->st_creat   = info.ftCreationTime.dwHighDateTime;
    id->st_creat <<= 32;
    id->st_creat  |= info.ftCreationTime.dwLowDateTime;
    r = 0;
cleanup:
    return r;
}

static void
convert_filetime_timeval(FILETIME ft, struct timeval *tv) {
    ULARGE_INTEGER t;
    
    t.u.HighPart = ft.dwHighDateTime;
    t.u.LowPart = ft.dwLowDateTime;
    t.QuadPart /= 10;
    if (tv) {
        tv->tv_sec = t.QuadPart / 1000000;
        tv->tv_usec = t.QuadPart % 1000000;
    }
}

int
toku_os_get_process_times(struct timeval *usertime, struct timeval *kerneltime) {
    FILETIME w_createtime, w_exittime, w_usertime, w_kerneltime;

    if (GetProcessTimes(GetCurrentProcess(), &w_createtime, &w_exittime, &w_kerneltime, &w_usertime)) {
        convert_filetime_timeval(w_usertime, usertime);
        convert_filetime_timeval(w_kerneltime, kerneltime);
        return 0;
    }
    return GetLastError();
}

int
toku_os_getpid(void) {
#if 0
    return _getpid();
#else
    return GetCurrentProcessId();
#endif
}

int 
toku_os_gettid(void) {
    return GetCurrentThreadId();
}

int
toku_os_lock_file(char *name) {
    int fd = _sopen(name, O_CREAT, _SH_DENYRW, S_IREAD|S_IWRITE);
    return fd;
}

int
toku_os_unlock_file(int fildes) {
    int r = close(fildes);
    return r;
}

int64_t
pread(int fildes, void *buf, size_t nbyte, int64_t offset) {
    int64_t r = _lseeki64(fildes, offset, SEEK_SET);
    if (r>=0) {
        assert(r==offset);
        r = read(fildes, buf, nbyte);
    }
    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
    return r;
}

int64_t
pwrite(int fildes, const void *buf, size_t nbyte, int64_t offset) {
    int64_t r = _lseeki64(fildes, offset, SEEK_SET);
    if (r>=0) {
        assert(r==offset);
        r = write(fildes, buf, nbyte);
    }
    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
    return r;
}

int
toku_os_mkdir(const char *pathname, mode_t mode) {
    int r = mkdir(pathname);
    UNUSED_WARNING(mode);
    if (r!=0) r = errno;
    return r;
}

unsigned int
sleep(unsigned int seconds) {
    unsigned int m = seconds / 1000000;
    unsigned int n = seconds % 1000000;
    unsigned int i;
    for (i=0; i<m; i++)
        Sleep(1000000*1000);
    Sleep(n*1000);
    return 0;
}

int
usleep(unsigned int useconds) {
    unsigned int m = useconds / 1000;
    unsigned int n = useconds % 1000;
    if (m == 0 && n > 0)
        m = 1;
    Sleep(m);
    return 0;
}

static void printfParameterHandler(const wchar_t* expression,
    const wchar_t* function, const wchar_t* file, 
    unsigned int line, uintptr_t pReserved) {
    fwprintf(stderr, L"Invalid parameter detected in function %s."
                     L" File: %s Line: %d\n"
                     L"Expression: %s\n", function, file, line, expression);
}

static void ignoreParameterHandler(const wchar_t* expression,
    const wchar_t* function, const wchar_t* file, 
    unsigned int line, uintptr_t pReserved) {
    UNUSED_WARNING(expression);
    UNUSED_WARNING(function);
    UNUSED_WARNING(file);
    UNUSED_WARNING(line);
    UNUSED_WARNING(pReserved);
}

int
toku_os_initialize_settings(int verbosity) {
    int r;
    static int initialized = 0;
    assert(initialized==0);
    initialized=1;
    if (verbosity>0) 
        _set_invalid_parameter_handler(printfParameterHandler);
    else
        _set_invalid_parameter_handler(ignoreParameterHandler);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    r = 0;
    return r;
}

long int
random(void) {
    u_int32_t r;
    errno_t r_error = rand_s(&r);
    assert(r_error==0);
    //Should return 0 to 2**31-1 instead of 2**32-1
    r >>= 1;
    return r;
}

//TODO: Implement srandom to modify the way rand_s works (IF POSSIBLE).. or
//reimplement random.
void
srandom(unsigned int seed) {
    UNUSED_WARNING(seed);
}

int
setenv(const char *name, const char *value, int overwrite) {
    char buf[2]; //Need a dummy buffer
    BOOL exists = TRUE;
    int r = GetEnvironmentVariable(name, buf, sizeof(buf));
    if (r==0) {
        r = GetLastError();
        if (r==ERROR_ENVVAR_NOT_FOUND) exists = FALSE;
        else {
            errno = r;
            r = -1;
            goto cleanup;
        }
    } 
    if (overwrite || !exists) {
        r = SetEnvironmentVariable(name, value);
        if (r==0) {
            errno = GetLastError();
            r = -1;
            goto cleanup;
        }
    }
    r = 0;
cleanup:
    return r;
}

int
unsetenv(const char *name) {
    int r = SetEnvironmentVariable(name, NULL);
    if (r==0) { //0 is failure
        r = -1;
        errno = GetLastError();
    }
    else r = 0;
    return r;
}

int 
ftruncate(int fd, int64_t offset) {
    HANDLE h;
    BOOL b;
    int r;

    h = (HANDLE) _get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    r = _lseeki64(fd, 0, SEEK_SET);
    if (r != 0)
        return -2;
    b = SetEndOfFile(h);
    if (!b)
        return -3;
    return 0;
}

int 
toku_os_is_absolute_name(const char* path) {
    return (path[0] == '\\' || 
            (isalpha(path[0]) && path[1]==':' && path[2]=='\\'));
}
