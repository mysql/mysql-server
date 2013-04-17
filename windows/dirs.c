#include <toku_portability.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <toku_assert.h>
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

#define SUPPORT_CYGWIN_STYLE_STAT 0
#define CYGWIN_ROOT_DIR_PREFIX "c:/cygwin"
#define CYGDRIVE_PREFIX        "/cygdrive/"

int
toku_stat(const char *name, toku_struct_stat *statbuf) {
    char new_name[strlen(name) + sizeof(CYGWIN_ROOT_DIR_PREFIX)];
    int bytes;
#if SUPPORT_CYGWIN_STYLE_STAT
    if (name[0] == '/') {
        char *cygdrive = strstr(name, CYGDRIVE_PREFIX);
        if (cygdrive==name && isalpha(name[strlen(CYGDRIVE_PREFIX)]))
             bytes = snprintf(new_name, sizeof(new_name), "%c:%s", name[strlen(CYGDRIVE_PREFIX)], name+strlen(CYGDRIVE_PREFIX)+1); //handle /cygdrive/DRIVELETTER
        else bytes = snprintf(new_name, sizeof(new_name), "%s%s", CYGWIN_ROOT_DIR_PREFIX, name);                  //handle /usr/local (for example)
    }
    else
#endif
             bytes = snprintf(new_name, sizeof(new_name), "%s", name);                                            //default
    //Verify no overflow
    assert(bytes>=0);
    assert((size_t)bytes < sizeof(new_name));
    int needdir = 0;
    if (bytes>1 && new_name[bytes-1]=='/') {
        //Strip trailing '/', but this implies it is a directory.
        new_name[bytes-1] = '\0';
        needdir = 1;
    }
    toku_struct_stat temp;
    int r = _stati64(new_name, &temp);
    if (r==0 && needdir && !(temp.st_mode&_S_IFDIR)) {
        r = -1;
        errno = ENOENT;
    }
    if (r==0) *statbuf = temp;
    return r;
}

int
toku_fstat(int fd, toku_struct_stat *statbuf) {
    int r = _fstati64(fd, statbuf);
    return r;
}

int
toku_fsync_dirfd_without_accounting(DIR *dirp) {
    //Not supported in windows.
    //Possibly not needed
    return 0;
}

int
toku_fsync_directory(const char *UU(fname)) {
    return 0; // toku_fsync_dirfd
}
