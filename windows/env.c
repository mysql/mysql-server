#include <toku_portability.h>
#include <windows.h>
#include <toku_stdlib.h>
#include <toku_assert.h>

int
setenv(const char *name, const char *value, int overwrite) {
    char * current = getenv(name);
    BOOL exists = current!=NULL;

    int r;
    if (overwrite || !exists) {
        char setstring[sizeof("=") + strlen(name) + strlen(value)];
        int bytes = snprintf(setstring, sizeof(setstring), "%s=%s", name, value);
        assert(bytes>=0);
        assert((size_t)bytes < sizeof(setstring));
        r = _putenv(setstring);
        if (r==-1) {
            errno = GetLastError();
            goto cleanup;
        }
    }
    r = 0;
cleanup:
    return r;
}

int
unsetenv(const char *name) {
    int r = setenv(name, "", 1);
    return r;
}

