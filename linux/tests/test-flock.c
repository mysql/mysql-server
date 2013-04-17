#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include "toku_portability.h"

int main(void) {
    int fd = toku_os_lock_file(__FILE__);
    assert(fd != -1);
    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0) {
        int fd2 = toku_os_lock_file(__FILE__);
        assert(fd2 == -1);
	return 0;
    } else {
        int status;
        pid_t wpid = waitpid(-1, &status, 0);
	assert(wpid == pid);
	assert(status == 0);
    }

    int r = toku_os_unlock_file(fd);
    assert(r == 0);

    return 0;
}
