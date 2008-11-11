#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include "portability.h"
#include "os.h"
#include <dirent.h>

int verbose;

static int walk(const char *dirname) {
    DIR *d;
    struct dirent *dirent;
    int dotfound = 0, dotdotfound = 0, otherfound = 0;

    d = opendir(dirname);
    if (d == NULL)
        return -1;
    while ((dirent = readdir(d))) {
        if (verbose)
            printf("%p %s\n", dirent, dirent->d_name);
        if (strcmp(dirent->d_name, ".") == 0)
            dotfound++;
        else if (strcmp(dirent->d_name, "..") == 0)
            dotdotfound++;
        else
            otherfound++;
    }
    closedir(d);
    assert(dotfound == 1 && dotdotfound == 1);
    return otherfound;
}

int main(int argc, char *argv[]) {
    int i;
    int found;
    int fd;
    int r;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
    }

    system("rm -rf " TESTDIR);

    // try to walk a directory that does not exist
    found = walk(TESTDIR);
    assert(found == -1);

    // try to walk an empty directory
    r = os_mkdir(TESTDIR, 0777); assert(r==0);
    found = walk(TESTDIR);
    assert(found == 0);
    //Try to delete the empty directory
    system("rm -rf " TESTDIR);
    
    r = os_mkdir(TESTDIR, 0777); assert(r==0);
    // walk a directory with a bunch of files in it
#define N 100
    for (i=0; i<N; i++) {
        char fname[256];
        sprintf(fname, TESTDIR "/%d", i);
        if (verbose)
            printf("%s\n", fname);
        // fd = creat(fname, 0777);
        fd = open(fname, O_CREAT+O_RDWR, 0777);
        assert(fd >= 0);
        close(fd);
    }
    found = walk(TESTDIR);
    assert(found == N);

    // walk and remove files
    
    return 0;
}

