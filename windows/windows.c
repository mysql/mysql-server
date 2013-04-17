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


int
toku_os_get_file_size(int fildes, int64_t *sizep) {
    int r;
    int64_t size = _filelengthi64(fildes);
    if (size<0) r = errno;
    else {
        r = 0;
        *sizep = size;
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
    if (r!=0) errno = r;
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
toku_os_get_max_process_data_size(uint64_t *maxdata) {
#ifdef _WIN32
    // the process gets 1/2 of the 32 bit address space.  
    // we are ignoring the 3GB feature for now.
    *maxdata = 1ULL << 31;
    return 0;
#else
#ifdef _WIN64
    *maxdata = ~0ULL;
    return 0;
#else
    return EINVAL;
#endif
#endif
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

int
toku_os_mkdir(const char *pathname, mode_t mode) {
    int r = mkdir(pathname);
    UNUSED_WARNING(mode);
    if (r!=0) r = errno;
    return r;
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

int 
toku_os_is_absolute_name(const char* path) {
    return (path[0] == '\\' || 
            (isalpha(path[0]) && path[1]==':' && path[2]=='\\'));
}
