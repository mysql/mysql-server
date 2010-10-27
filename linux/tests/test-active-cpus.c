#include <stdio.h>
#include <stdlib.h>
#include <toku_stdint.h>
#include <unistd.h>
#include <toku_assert.h>
#include "toku_os.h"
#include <sched.h>

static void set_cpuset(cpu_set_t *cpuset, int ncpus) {
    CPU_ZERO(cpuset);
    for (int i = 0; i < ncpus; i++)
        CPU_SET(i, cpuset);
}

int main(void) {
    int r;

    int max_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    assert(toku_os_get_number_active_processors() == max_cpus);

    // change the processor affinity and verify that the correct number is computed
    for (int ncpus = 1; ncpus <= max_cpus; ncpus++) {
        cpu_set_t cpuset; 
        set_cpuset(&cpuset, ncpus);
        r = sched_setaffinity(getpid(), sizeof cpuset, &cpuset);
        assert(r == 0);

        assert(toku_os_get_number_active_processors() == ncpus);
    }

    // change the TOKU_NCPUS env variable and verify that the correct number is computed
    for (int ncpus = 1; ncpus <= max_cpus; ncpus++) {
        char ncpus_str[32];
        sprintf(ncpus_str, "%d", ncpus);
        r = setenv("TOKU_NCPUS", ncpus_str, 1);
        assert(r == 0);

        assert(toku_os_get_number_active_processors() == ncpus);
    }

    return 0;
}
