#define _CRT_SECURE_NO_DEPRECATE

//rand_s requires _CRT_RAND_S be defined before including stdlib
#define _CRT_RAND_S
#include <stdlib.h>

#include <toku_portability.h>
#include <windows.h>
#include <dirent.h>
#include <toku_assert.h>
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

static int
toku_malloc_init(void) {
    int r = 0;
#if TOKU_WINDOWS_32
    //Set the heap (malloc/free/realloc) to use the low fragmentation mode.
    ULONG  HeapFragValue = 2;

    int success;
    success = HeapSetInformation(GetProcessHeap(),
                           HeapCompatibilityInformation,
                           &HeapFragValue,
                           sizeof(HeapFragValue));
    //if (success==0) //Do some error output if necessary.
    if (!success)
        r = GetLastError();
#endif
    return r;
}

int
toku_portability_init(void) {
    int r = 0;
    if (r==0)
        r = toku_malloc_init();
    if (r==0)
        r = toku_pthread_win32_init();
    if (r==0)
        r = toku_fsync_init();
    if (r==0)
        r = toku_mkstemp_init();
    if (r==0)
        _fmode = _O_BINARY; //Default open file is BINARY
    return r;
}

int
toku_portability_destroy(void) {
    int r = 0;
    if (r==0)
        r = toku_mkstemp_destroy();
    if (r==0)
        r = toku_fsync_destroy();
    if (r==0)
        r = toku_pthread_win32_destroy();
    return r;
}

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
    memset(&info, 0, sizeof(info));
    filehandle = (HANDLE)_get_osfhandle(fildes);
    if (filehandle==INVALID_HANDLE_VALUE) {
        r = errno; assert(r!=0);
        goto cleanup;
    }
    r = GetFileInformationByHandle(filehandle, &info);
    if (r==0) { //0 is error here.
        r = GetLastError(); assert(r!=0);
        if (r==ERROR_INVALID_FUNCTION && info.dwVolumeSerialNumber == 0 &&
            info.nFileIndexHigh == 0 &&  info.nFileIndexLow        == 0) {
            //"NUL" will return this.
            //TODO: Remove this hack somehow
            r = 0;
            goto continue_dev_null;
        }
        goto cleanup;
    }
    //Make sure only "NUL" returns all zeros.
    assert(info.dwVolumeSerialNumber!=0 || info.nFileIndexHigh!=0 || info.nFileIndexLow!=0);
continue_dev_null: //Skip zeros check (its allowed here).
    id->st_dev     = info.dwVolumeSerialNumber;
    id->st_ino     = info.nFileIndexHigh;
    id->st_ino   <<= 32;
    id->st_ino    |= info.nFileIndexLow;
    r = 0;
cleanup:
    if (r!=0) {
        errno = r;
        r = -1;
    }
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
#if TOKU_WINDOWS_32
    // the process gets 1/2 of the 32 bit address space.  
    // we are ignoring the 3GB feature for now.
    *maxdata = 1ULL << 31;
    return 0;
#elif TOKU_WINDOWS_64
    *maxdata = ~0ULL;
    return 0;
#else
#error
    return EINVAL;
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
            path[0] == '/' || 
            (isalpha(path[0]) && path[1]==':' && path[2]=='\\') ||
            (isalpha(path[0]) && path[1]==':' && path[2]=='/'));
}

int
vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    int r = _vsnprintf(str, size, format, ap);
    if (str && size>0) {
        str[size-1] = '\0';         //Always null terminate.
        if (r<0 && errno==ERANGE) {
            r = strlen(str)+1;      //Mimic linux return value.
                                    //May be too small, but it does
                                    //at least indicate overflow
        }
    }
    return r;
}

int
snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vsnprintf(str, size, format, ap);
    va_end(ap);
    return r;
}

#include <PowrProf.h>
#define TOKU_MICROSOFT_DID_NOT_DEFINE_PROCESSOR_POWER_INFORMATION 1
#if TOKU_MICROSOFT_DID_NOT_DEFINE_PROCESSOR_POWER_INFORMATION 
//From MSDN: (As of Windows 2000)
//  Note that this structure definition was accidentally omitted from WinNT.h.
//  This error will be corrected in the future. In the meantime, to compile your
//  application, include the structure definition contained in this topic in your
//  source code.
typedef struct _PROCESSOR_POWER_INFORMATION {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
}PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif

int
toku_os_get_processor_frequency(uint64_t *hzret) {

    SYSTEM_INFO sys_info;
    // find out how many processors we have in the system
    GetSystemInfo(&sys_info);
    PROCESSOR_POWER_INFORMATION infos[sys_info.dwNumberOfProcessors];
    memset(infos, 0, sizeof(infos));

    NTSTATUS r = CallNtPowerInformation(ProcessorInformation, NULL, 0, &infos[0], sizeof(infos));
    assert(r==ERROR_SUCCESS);

    uint64_t mhz = infos[0].MaxMhz;
    *hzret = mhz * 1000000ULL;
    return 0;
}

int
toku_dup2(int fd, int fd2) {
    int r;
    r = _dup2(fd, fd2);
    if (r==0) //success
        r = fd2;
    return r;

}

// for now, just return zeros 
int 
toku_get_filesystem_sizes(const char *path, uint64_t *avail_size, uint64_t *free_size, uint64_t *total_size) {
    int r;
    ULARGE_INTEGER free_bytes_for_user;
    ULARGE_INTEGER free_bytes_total;
    ULARGE_INTEGER total_bytes;

    BOOL success = GetDiskFreeSpaceEx(path,
                                      &free_bytes_for_user,
                                      &total_bytes,
                                      &free_bytes_total);
    if (success) {
        r = 0;
        if (avail_size) *avail_size = free_bytes_for_user.QuadPart;
        if (free_size)  *free_size  = free_bytes_total.QuadPart;
        if (total_size) *total_size = total_bytes.QuadPart;
    }
    else {
        r = GetLastError();
    }
    return r;
}

