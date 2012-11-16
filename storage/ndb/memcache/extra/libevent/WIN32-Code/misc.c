#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <sys/timeb.h>
#include <time.h>

#ifdef __GNUC__
/*our prototypes for timeval and timezone are in here, just in case the above
  headers don't have them*/
#include "misc.h"
#endif

/****************************************************************************
 *
 * Function: gettimeofday(struct timeval *, struct timezone *)
 *
 * Purpose:  Get current time of day.
 *
 * Arguments: tv => Place to store the curent time of day.
 *            tz => Ignored.
 *
 * Returns: 0 => Success.
 *
 ****************************************************************************/

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval *tv, struct timezone *tz) {
	struct _timeb tb;

	if(tv == NULL)
		return -1;

	_ftime(&tb);
	tv->tv_sec = (long) tb.time;
	tv->tv_usec = ((int) tb.millitm) * 1000;
	return 0;
}
#endif

#if 0
int
win_read(int fd, void *buf, unsigned int length)
{
	DWORD dwBytesRead;
	int res = ReadFile((HANDLE) fd, buf, length, &dwBytesRead, NULL);
	if (res == 0) {
		DWORD error = GetLastError();
		if (error == ERROR_NO_DATA)
			return (0);
		return (-1);
	} else
		return (dwBytesRead);
}

int
win_write(int fd, void *buf, unsigned int length)
{
	DWORD dwBytesWritten;
	int res = WriteFile((HANDLE) fd, buf, length, &dwBytesWritten, NULL);
	if (res == 0) {
		DWORD error = GetLastError();
		if (error == ERROR_NO_DATA)
			return (0);
		return (-1);
	} else
		return (dwBytesWritten);
}

int
socketpair(int d, int type, int protocol, int *sv)
{
	static int count;
	char buf[64];
	HANDLE fd;
	DWORD dwMode;
	sprintf(buf, "\\\\.\\pipe\\levent-%d", count++);
	/* Create a duplex pipe which will behave like a socket pair */
	fd = CreateNamedPipe(buf, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_NOWAIT, 
		PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);
	if (fd == INVALID_HANDLE_VALUE)
		return (-1);
	sv[0] = (int)fd;

	fd = CreateFile(buf, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == INVALID_HANDLE_VALUE)
		return (-1);
	dwMode = PIPE_NOWAIT;
	SetNamedPipeHandleState(fd, &dwMode, NULL, NULL);
	sv[1] = (int)fd;

	return (0);
}
#endif
