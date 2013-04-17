#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <direct.h>
#include <dirent.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>

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
