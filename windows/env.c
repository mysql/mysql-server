#include <windows.h>
#include <toku_stdlib.h>

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

