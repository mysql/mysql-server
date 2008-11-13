#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <toku_os.h>

#define DO_MEMORY_INFO 1

#if DO_MEMORY_INFO
#include <psapi.h>

static int
get_memory_info(PROCESS_MEMORY_COUNTERS *meminfo) {
    int r;

    r = GetProcessMemoryInfo(GetCurrentProcess(), meminfo, sizeof *meminfo);
    if (r == 0)
        return GetLastError();
    return 0;
}

#endif

int
toku_os_get_rss(int64_t *rss) {
    int r;
#if DO_MEMORY_INFO
    PROCESS_MEMORY_COUNTERS meminfo;

    r = get_memory_info(&meminfo);
    if (r == 0)
        *rss = meminfo.WorkingSetSize;
#else
    r = 0;
    *rss = 0;
#endif
    return r;
}

int
toku_os_get_max_rss(int64_t *maxrss) {
    int r;
#if DO_MEMORY_INFO
    PROCESS_MEMORY_COUNTERS meminfo;

    r = get_memory_info(&meminfo);
    if (r == 0)
        *maxrss = meminfo.PeakWorkingSetSize;
#else
    r = 0;
    *maxrss = 0;
#endif
    return r;
}

