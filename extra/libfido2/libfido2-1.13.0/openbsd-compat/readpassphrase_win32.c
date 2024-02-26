/*
* Author: Manoj Ampalam <manoj.ampalam@microsoft.com>
*
* Author: Bryan Berns <berns@uwalumni.com>
*   Modified group detection use s4u token information 
*
* Copyright(c) 2016 Microsoft Corp.
* All rights reserved
*
* Misc Unix POSIX routine implementations for Windows
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met :
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and / or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define UMDF_USING_NTSTATUS 
#define SECURITY_WIN32
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <shlwapi.h>
#include <conio.h>
#include <lm.h>
#include <sddl.h>
#include <aclapi.h>
#include <ntsecapi.h>
#include <security.h>
#include <ntstatus.h>
#include <wchar.h>

#include "openbsd-compat.h"

#ifndef HAVE_READPASSPHRASE

/*on error returns NULL and sets errno*/
static wchar_t *
utf8_to_utf16(const char *utf8)
{
	int needed = 0;
	wchar_t* utf16 = NULL;
	if ((needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0)) == 0 ||
	    (utf16 = malloc(needed * sizeof(wchar_t))) == NULL ||
	    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, needed) == 0) {
		/* debug3("failed to convert utf8 payload:%s error:%d", utf8, GetLastError()); */
		errno = ENOMEM;
		return NULL;
	}

	return utf16;
}

char *
readpassphrase(const char *prompt, char *outBuf, size_t outBufLen, int flags)
{
	size_t current_index = 0;
	char ch;
	wchar_t* wtmp = NULL;

	if (outBufLen == 0) {
		errno = EINVAL;
		return NULL;
	}

	while (_kbhit()) (void)_getch();

	wtmp = utf8_to_utf16(prompt);
	if (wtmp == NULL)
		errx(1, "unable to alloc memory");

	_cputws(wtmp);
	free(wtmp);

	while (current_index < outBufLen - 1) {
		ch = (char)_getch();
		
		if (ch == '\r') {
			if (_kbhit()) (void)_getch(); /* read linefeed if its there */
			break;
		} else if (ch == '\n') {
			break;
		} else if (ch == '\b') { /* backspace */
			if (current_index > 0) {
				if (flags & RPP_ECHO_ON)
					printf_s("%c \b", ch);

				current_index--; /* overwrite last character */
			}
		} else if (ch == '\003') { /* exit on Ctrl+C */
			errx(1, "");
		} else {
			if (flags & RPP_SEVENBIT)
				ch &= 0x7f;

			if (isalpha((unsigned char)ch)) {
				if(flags & RPP_FORCELOWER)
					ch = (char)tolower((unsigned char)ch);
				if(flags & RPP_FORCEUPPER)
					ch = (char)toupper((unsigned char)ch);
			}

			outBuf[current_index++] = ch;
			if(flags & RPP_ECHO_ON)
				printf_s("%c", ch);
		}
	}

	outBuf[current_index] = '\0';
	_cputs("\n");

	return outBuf;
}

#endif /* HAVE_READPASSPHRASE */
