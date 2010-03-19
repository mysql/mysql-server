#include "test.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "toku_time.h"

#define ENVDIR "dir."__FILE__


int verbose = 0;
static void
create_files(int N, int fds[N]) {
    int r;
    int i;
    char name[30];
    for (i = 0; i < N; i++) {
        snprintf(name, sizeof(name), "%d", i);
        fds[i] = open(name, O_CREAT|O_WRONLY);
        if (fds[i] < 0) {
            r = errno;
            CKERR(r);
        }
    }
}

static void
write_to_files(int N, int bytes, int fds[N]) {
    char junk[bytes];

    int i;
    for (i = 0; i < bytes; i++) {
        junk[i] = random() & 0xFF;
    }

    int r;
    for (i = 0; i < N; i++) {
        r = toku_os_write(fds[i], junk, bytes);
        CKERR(r);
    }
}

static void
time_many_fsyncs_one_file(int N, int bytes, int fds[N]) {
    if (verbose>1) {
        printf("Starting %s\n", __FUNCTION__);
        fflush(stdout);
    }
    struct timeval begin;
    struct timeval after_first;
    struct timeval end;
    write_to_files(1, bytes, fds);
    if (verbose>1) {
        printf("Done writing to os buffers\n");
        fflush(stdout);
    }
    int i;
    int r;

    r = gettimeofday(&begin, NULL);
    CKERR(r);
    r = fsync(fds[0]);
    CKERR(r);
    r = gettimeofday(&after_first, NULL);
    CKERR(r);
    for (i = 0; i < N; i++) {
        r = fsync(fds[0]);
        CKERR(r);
    }
    r = gettimeofday(&end, NULL);
    CKERR(r);

    if (verbose) {
        printf("Fsyncing one file %d times:\n"
               "\tFirst fsync took: [%f] seconds\n"
               "\tRemaining %d fsyncs took additional: [%f] seconds\n"
               "\tTotal time [%f] seconds\n",
               N + 1,
               toku_tdiff(&after_first, &begin),
               N, 
               toku_tdiff(&end, &after_first),
               toku_tdiff(&end, &begin));
        fflush(stdout);
    }
}

static void
time_fsyncs_many_files(int N, int bytes, int fds[N]) {
    if (verbose>1) {
        printf("Starting %s\n", __FUNCTION__);
        fflush(stdout);
    }
    write_to_files(N, bytes, fds);
    if (verbose>1) {
        printf("Done writing to os buffers\n");
        fflush(stdout);
    }
    struct timeval begin;
    struct timeval after_first;
    struct timeval end;
    int i;
    int r;

    r = gettimeofday(&begin, NULL);
    CKERR(r);
    for (i = 0; i < N; i++) {
        r = fsync(fds[i]);
        CKERR(r);
        if (i==0) {
            r = gettimeofday(&after_first, NULL);
            CKERR(r);
        }
        if (verbose>2) {
            printf("Done fsyncing %d\n", i);
            fflush(stdout);
        }
    }
    r = gettimeofday(&end, NULL);
    CKERR(r);
    if (verbose) {
        printf("Fsyncing %d files:\n"
               "\tFirst fsync took: [%f] seconds\n"
               "\tRemaining %d fsyncs took additional: [%f] seconds\n"
               "\tTotal time [%f] seconds\n",
               N,
               toku_tdiff(&after_first, &begin),
               N-1,
               toku_tdiff(&end, &after_first),
               toku_tdiff(&end, &begin));
        fflush(stdout);
    }
}

#if !TOKU_WINDOWS
//sync() does not appear to have an analogue on windows.
static void
time_sync_fsyncs_many_files(int N, int bytes, int fds[N]) {
    if (verbose>1) {
        printf("Starting %s\n", __FUNCTION__);
        fflush(stdout);
    }
    //TODO: timing
    write_to_files(N, bytes, fds);
    if (verbose>1) {
        printf("Done writing to os buffers\n");
        fflush(stdout);
    }
    int i;
    int r;
    struct timeval begin;
    struct timeval after_sync;
    struct timeval end;

    r = gettimeofday(&begin, NULL);
    CKERR(r);

    sync();

    r = gettimeofday(&after_sync, NULL);
    CKERR(r);
    if (verbose>1) {
        printf("Done with sync()\n");
        fflush(stdout);
    }

    for (i = 0; i < N; i++) {
        r = fsync(fds[i]);
        CKERR(r);
        if (verbose>2) {
            printf("Done fsyncing %d\n", i);
            fflush(stdout);
        }
    }
    r = gettimeofday(&end, NULL);
    CKERR(r);

    if (verbose) {
        printf("sync() then fsyncing %d files:\n"
               "\tsync() took: [%f] seconds\n"
               "\tRemaining %d fsyncs took additional: [%f] seconds\n"
               "\tTotal time [%f] seconds\n",
               N,
               toku_tdiff(&after_sync, &begin),
               N,
               toku_tdiff(&end, &after_sync),
               toku_tdiff(&end, &begin));
        fflush(stdout);
    }
}
#endif

int test_main(int argc, char *const argv[]) {
    int i;
    int r;
    int N = 1000;
    int bytes = 4096;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            if (verbose < 0) verbose = 0;
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            verbose = 0;
            continue;
        }
        if (strcmp(argv[i], "-b") == 0) {
            i++;
            if (i>=argc) exit(1);
            bytes = atoi(argv[i]);
            if (bytes <= 0)  exit(1);
            continue;
        }
        if (strcmp(argv[i], "-n") == 0) {
            i++;
            if (i>=argc) exit(1);
            N = atoi(argv[i]);
            if (N <= 0)  exit(1);
            continue;
        }
    }
    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = chdir(ENVDIR);
    CKERR(r);

    int fds[N];
    create_files(N, fds);

    time_many_fsyncs_one_file(N, bytes, fds);
    time_fsyncs_many_files(N, bytes, fds);
#if !TOKU_WINDOWS
    time_sync_fsyncs_many_files(N, bytes, fds);
#endif

    return 0;
}
