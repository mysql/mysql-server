/*
 * Copyright (c) 2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <winscard.h>

#include "mutator_aux.h"

static const struct blob *reader_list;
static int (*xread)(void *, u_char *, size_t, int);
static int (*xwrite)(void *, const u_char *, size_t);
static void (*xconsume)(const void *, size_t);

LONG __wrap_SCardEstablishContext(DWORD, LPCVOID, LPCVOID, LPSCARDCONTEXT);
LONG __wrap_SCardListReaders(SCARDCONTEXT, LPCSTR, LPSTR, LPDWORD);
LONG __wrap_SCardReleaseContext(SCARDCONTEXT);
LONG __wrap_SCardConnect(SCARDCONTEXT, LPCSTR, DWORD, DWORD, LPSCARDHANDLE,
    LPDWORD);
LONG __wrap_SCardDisconnect(SCARDHANDLE, DWORD);
LONG __wrap_SCardTransmit(SCARDHANDLE, const SCARD_IO_REQUEST *, LPCBYTE,
    DWORD, SCARD_IO_REQUEST *, LPBYTE, LPDWORD);

LONG
__wrap_SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
    LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	assert(dwScope == SCARD_SCOPE_SYSTEM);
	assert(pvReserved1 == NULL);
	assert(pvReserved2 == NULL);

	*phContext = 1;

	if (uniform_random(400) < 1)
		return SCARD_E_NO_SERVICE;
	if (uniform_random(400) < 1)
		return SCARD_E_NO_SMARTCARD;
	if (uniform_random(400) < 1)
		return SCARD_E_NO_MEMORY;
	if (uniform_random(400) < 1)
		*phContext = 0;

	return SCARD_S_SUCCESS;
}

LONG
__wrap_SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
    LPSTR mszReaders, LPDWORD pcchReaders)
{
	assert(hContext == 1);
	assert(mszGroups == NULL);
	assert(mszReaders != NULL);
	assert(pcchReaders != 0);

	if (reader_list == NULL || uniform_random(400) < 1)
		return SCARD_E_NO_READERS_AVAILABLE;
	if (uniform_random(400) < 1)
		return SCARD_E_NO_MEMORY;

	memcpy(mszReaders, reader_list->body, reader_list->len > *pcchReaders ?
	    *pcchReaders : reader_list->len);
	*pcchReaders = (DWORD)reader_list->len; /* on purpose */

	return SCARD_S_SUCCESS;
}

LONG
__wrap_SCardReleaseContext(SCARDCONTEXT hContext)
{
	assert(hContext == 1);

	return SCARD_S_SUCCESS;
}

LONG
__wrap_SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
    DWORD dwPreferredProtocols, LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	uint32_t r;

	assert(hContext == 1);
	xconsume(szReader, strlen(szReader) + 1);
	assert(dwShareMode == SCARD_SHARE_SHARED);
	assert(dwPreferredProtocols == SCARD_PROTOCOL_ANY);
	assert(phCard != NULL);
	assert(pdwActiveProtocol != NULL);

	if ((r = uniform_random(400)) < 1)
		return SCARD_E_UNEXPECTED;

	*phCard = 1;
	*pdwActiveProtocol = (r & 1) ? SCARD_PROTOCOL_T0 : SCARD_PROTOCOL_T1;

	if (uniform_random(400) < 1)
		*pdwActiveProtocol = SCARD_PROTOCOL_RAW;

	return SCARD_S_SUCCESS;
}

LONG
__wrap_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	assert(hCard == 1);
	assert(dwDisposition == SCARD_LEAVE_CARD);

	return SCARD_S_SUCCESS;
}

extern void consume(const void *body, size_t len);

LONG
__wrap_SCardTransmit(SCARDHANDLE hCard, const SCARD_IO_REQUEST *pioSendPci,
    LPCBYTE pbSendBuffer, DWORD cbSendLength, SCARD_IO_REQUEST *pioRecvPci,
    LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	void *ioh = (void *)NFC_DEV_HANDLE;
	int n;

	assert(hCard == 1);
	xconsume(pioSendPci, sizeof(*pioSendPci));
	xwrite(ioh, pbSendBuffer, cbSendLength);
	assert(pioRecvPci == NULL);

	if (uniform_random(400) < 1 ||
	    (n = xread(ioh, pbRecvBuffer, *pcbRecvLength, -1)) == -1)
		return SCARD_E_UNEXPECTED;
	*pcbRecvLength = (DWORD)n;

	return SCARD_S_SUCCESS;
}

void
set_pcsc_parameters(const struct blob *reader_list_ptr)
{
	reader_list = reader_list_ptr;
}

void
set_pcsc_io_functions(int (*read_f)(void *, u_char *, size_t, int),
    int (*write_f)(void *, const u_char *, size_t),
    void (*consume_f)(const void *, size_t))
{
	xread = read_f;
	xwrite = write_f;
	xconsume = consume_f;
}
