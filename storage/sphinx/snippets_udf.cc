//
// $Id: snippets_udf.cc 2058 2009-11-07 04:01:57Z shodan $
//

//
// Copyright (c) 2001-2008, Andrew Aksyonoff. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#include <mysql_version.h>

#if MYSQL_VERSION_ID>50100
#include "mysql_priv.h"
#include <mysql/plugin.h>
#else
#include "../mysql_priv.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/un.h>
#include <netdb.h>

#include <mysys_err.h>
#include <my_sys.h>

#if MYSQL_VERSION_ID>=50120
typedef uchar byte;
#endif

/// partially copy-pasted stuff that should be moved elsewhere

#if UNALIGNED_RAM_ACCESS

/// pass-through wrapper
template < typename T > inline T sphUnalignedRead ( const T & tRef )
{
	return tRef;
}

/// pass-through wrapper
template < typename T > void sphUnalignedWrite ( void * pPtr, const T & tVal )
{
	*(T*)pPtr = tVal;
}

#else

/// unaligned read wrapper for some architectures (eg. SPARC)
template < typename T >
inline T sphUnalignedRead ( const T & tRef )
{
	T uTmp;
	byte * pSrc = (byte *) &tRef;
	byte * pDst = (byte *) &uTmp;
	for ( int i=0; i<(int)sizeof(T); i++ )
		*pDst++ = *pSrc++;
	return uTmp;
}

/// unaligned write wrapper for some architectures (eg. SPARC)
template < typename T >
void sphUnalignedWrite ( void * pPtr, const T & tVal )
{
	byte * pDst = (byte *) pPtr;
	byte * pSrc = (byte *) &tVal;
	for ( int i=0; i<(int)sizeof(T); i++ )
		*pDst++ = *pSrc++;
}

#endif

#define SPHINXSE_MAX_ALLOC			(16*1024*1024)

#define SafeDelete(_arg)		{ if ( _arg ) delete ( _arg );		(_arg) = NULL; }
#define SafeDeleteArray(_arg)	{ if ( _arg ) delete [] ( _arg );	(_arg) = NULL; }

#define Min(a,b) ((a)<(b)?(a):(b))

typedef unsigned int DWORD;

inline DWORD sphF2DW ( float f ) { union { float f; uint32 d; } u; u.f = f; return u.d; }

static char * sphDup ( const char * sSrc, int iLen=-1 )
{
	if ( !sSrc )
		return NULL;

	if ( iLen<0 )
		iLen = strlen(sSrc);

	char * sRes = new char [ 1+iLen ];
	memcpy ( sRes, sSrc, iLen );
	sRes[iLen] = '\0';
	return sRes;
}

static inline void sphShowErrno ( const char * sCall )
{
	char sError[256];
	snprintf ( sError, sizeof(sError), "%s() failed: [%d] %s", sCall, errno, strerror(errno) );
	my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), sError );
}

static const bool sphReportErrors = true;

static bool sphSend ( int iFd, const char * pBuffer, int iSize, bool bReportErrors = false )
{
	assert ( pBuffer );
	assert ( iSize > 0 );

	const int iResult = send ( iFd, pBuffer, iSize, 0 );
	if ( iResult != iSize )
	{
		if ( bReportErrors ) sphShowErrno("send");
		return false;
	}
	return true;
}

static bool sphRecv ( int iFd, char * pBuffer, int iSize, bool bReportErrors = false )
{
	assert ( pBuffer );
	assert ( iSize > 0 );
	
	while ( iSize )
	{
		const int iResult = recv ( iFd, pBuffer, iSize, 0 );
		if ( iResult > 0 )
		{
			iSize -= iResult;
			pBuffer += iSize;
		}
		else if ( iResult == 0 )
		{
			if ( bReportErrors )
				my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), "recv() failed: disconnected" );
			return false;
		}
		else
		{
			if ( bReportErrors ) sphShowErrno("recv");
			return false;
		}
	}
	return true;
}

enum
{
	SPHINX_SEARCHD_PROTO		= 1,

	SEARCHD_COMMAND_SEARCH		= 0,
	SEARCHD_COMMAND_EXCERPT		= 1,

	VER_COMMAND_SEARCH		= 0x116,
	VER_COMMAND_EXCERPT		= 0x100,
};

/// known answers
enum
{
	SEARCHD_OK		= 0,	///< general success, command-specific reply follows
	SEARCHD_ERROR	= 1,	///< general failure, error message follows
	SEARCHD_RETRY	= 2,	///< temporary failure, error message follows, client should retry later
	SEARCHD_WARNING	= 3		///< general success, warning message and command-specific reply follow
};

#define SPHINXSE_DEFAULT_SCHEME		"sphinx"
#define SPHINXSE_DEFAULT_HOST		"127.0.0.1"
#define SPHINXSE_DEFAULT_PORT		9312
#define SPHINXSE_DEFAULT_INDEX		"*"

class CSphBuffer
{
private:
	bool m_bOverrun;
	int m_iSize;
	int m_iLeft;
	char * m_pBuffer;
	char * m_pCurrent;

public:
	CSphBuffer ( const int iSize )
		: m_bOverrun ( false )
		, m_iSize ( iSize )
		, m_iLeft ( iSize )
	{
		assert ( iSize > 0 );
		m_pBuffer = new char[iSize];
		m_pCurrent = m_pBuffer;
	}

	~CSphBuffer ()
	{
		SafeDelete ( m_pBuffer );
	}

	const char * Ptr() const { return m_pBuffer; }

	bool Finalize()
	{
		return !( m_bOverrun || m_iLeft != 0 || m_pCurrent - m_pBuffer != m_iSize );
	}
	
	void SendBytes ( const void * pBytes, int iBytes );
	
	void SendWord ( short int v )					{ v = ntohs(v); SendBytes ( &v, sizeof(v) ); }
	void SendInt ( int v )							{ v = ntohl(v); SendBytes ( &v, sizeof(v) ); }
	void SendDword ( DWORD v )						{ v = ntohl(v) ;SendBytes ( &v, sizeof(v) ); }
	void SendUint64 ( ulonglong v )					{ SendDword ( uint(v>>32) ); SendDword ( uint(v&0xFFFFFFFFUL) ); }
	void SendString ( const char * v )				{ SendString ( v, strlen(v) ); }
	void SendString ( const char * v, int iLen )	{ SendDword(iLen); SendBytes ( v, iLen ); }
	void SendFloat ( float v )						{ SendDword ( sphF2DW(v) ); }
};

void CSphBuffer::SendBytes ( const void * pBytes, int iBytes )
{
	if ( m_iLeft < iBytes )
	{
		m_bOverrun = true;
		return;
	}

	memcpy ( m_pCurrent, pBytes, iBytes );

	m_pCurrent += iBytes;
	m_iLeft -= iBytes;
}

struct CSphUrl
{
	char * m_sBuffer;
	char * m_sFormatted;
	
	char * m_sScheme;
	char * m_sHost;
	char * m_sIndex;
	
	int m_iPort;
	
	CSphUrl()
		: m_sBuffer ( NULL )
		, m_sFormatted ( NULL )
		, m_sScheme ( (char*) SPHINXSE_DEFAULT_SCHEME )
		, m_sHost ( (char*) SPHINXSE_DEFAULT_HOST )
		, m_sIndex ( (char*) SPHINXSE_DEFAULT_INDEX )
		, m_iPort ( SPHINXSE_DEFAULT_PORT )
	{}
	
	~CSphUrl()
	{
		SafeDeleteArray ( m_sFormatted );
		SafeDeleteArray ( m_sBuffer );
	}
	
	bool Parse ( const char * sUrl, int iLen );
	int Connect();
	const char * Format();
};

const char * CSphUrl::Format()
{
	if ( !m_sFormatted )
	{
		int iSize = 15 + strlen(m_sHost) + strlen(m_sIndex);
		m_sFormatted = new char [ iSize ];
		if ( m_iPort )
			snprintf ( m_sFormatted, iSize, "inet://%s:%d/%s", m_sHost, m_iPort, m_sIndex );
		else
			snprintf ( m_sFormatted, iSize, "unix://%s/%s", m_sHost, m_sIndex );
	}
	return m_sFormatted;
}

// the following scheme variants are recognized
//
// inet://host/index
// inet://host:port/index
// unix://unix/domain/socket:index
// unix://unix/domain/socket
bool CSphUrl::Parse ( const char * sUrl, int iLen )
{
	bool bOk = true;
	while ( iLen )
	{
		bOk = false;
		
		m_sBuffer = sphDup ( sUrl, iLen );
		m_sScheme = m_sBuffer;
		
		m_sHost = strstr ( m_sBuffer, "://" );
		if ( !m_sHost )
			break;
		m_sHost[0] = '\0';
		m_sHost += 2;
		
		if ( !strcmp ( m_sScheme, "unix" ) )
		{
			// unix-domain socket
			m_iPort = 0;
			if (!( m_sIndex = strrchr ( m_sHost, ':' ) ))
				m_sIndex = (char*) SPHINXSE_DEFAULT_INDEX;
			else
			{
				*m_sIndex++ = '\0';
				if ( !*m_sIndex )
					m_sIndex = (char*) SPHINXSE_DEFAULT_INDEX;
			}
			bOk = true;
			break;
		}
		if( strcmp ( m_sScheme, "sphinx" ) != 0 && strcmp ( m_sScheme, "inet" ) != 0 )
			break;

		// inet
		m_sHost++;
		char * sPort = strchr ( m_sHost, ':' );
		if ( sPort )
		{
			*sPort++ = '\0';
			if ( *sPort )
			{
				m_sIndex = strchr ( sPort, '/' );
				if ( m_sIndex )
					*m_sIndex++ = '\0'; 
				else
					m_sIndex = (char*) SPHINXSE_DEFAULT_INDEX;
				
				m_iPort = atoi(sPort);
				if ( !m_iPort )
					m_iPort = SPHINXSE_DEFAULT_PORT;
			}
		} else
		{
			m_sIndex = strchr ( m_sHost, '/' );
			if ( m_sIndex )
				*m_sIndex++ = '\0';
			else
				m_sIndex = (char*) SPHINXSE_DEFAULT_INDEX;
		}

		bOk = true;
		break;
	}
	
	return bOk;
}

int CSphUrl::Connect()
{
	struct sockaddr_in sin;
#ifndef __WIN__
	struct sockaddr_un saun;
#endif

	int iDomain = 0;
	int iSockaddrSize = 0;
	struct sockaddr * pSockaddr = NULL;

	in_addr_t ip_addr;

	if ( m_iPort )
	{
		iDomain = AF_INET;
		iSockaddrSize = sizeof(sin);
		pSockaddr = (struct sockaddr *) &sin;

		memset ( &sin, 0, sizeof(sin) );
		sin.sin_family = AF_INET;
		sin.sin_port = htons(m_iPort);
		
		// resolve address
		if ( (int)( ip_addr=inet_addr(m_sHost) ) != (int)INADDR_NONE )
			memcpy ( &sin.sin_addr, &ip_addr, sizeof(ip_addr) );
		else
		{
			int tmp_errno;
			struct hostent tmp_hostent, *hp;
			char buff2 [ GETHOSTBYNAME_BUFF_SIZE ];
			
			hp = my_gethostbyname_r ( m_sHost, &tmp_hostent,
									  buff2, sizeof(buff2), &tmp_errno );
			if ( !hp )
			{ 
				my_gethostbyname_r_free();
				
				char sError[256];
				snprintf ( sError, sizeof(sError), "failed to resolve searchd host (name=%s)", m_sHost );
				
				my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), sError );
				return -1;
			}
			
			memcpy ( &sin.sin_addr, hp->h_addr, Min ( sizeof(sin.sin_addr), (size_t)hp->h_length ) );
			my_gethostbyname_r_free();
		}
	}
	else
	{
#ifndef __WIN__
		iDomain = AF_UNIX;
		iSockaddrSize = sizeof(saun);
		pSockaddr = (struct sockaddr *) &saun;

		memset ( &saun, 0, sizeof(saun) );
		saun.sun_family = AF_UNIX;
		strncpy ( saun.sun_path, m_sHost, sizeof(saun.sun_path)-1 );
#else
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), "Unix-domain sockets are not supported on Windows" );
		return -1;
#endif
	}

	// connect to searchd and exchange versions
	uint uServerVersion;
	uint uClientVersion = htonl ( SPHINX_SEARCHD_PROTO );
	int iSocket = -1;
        const char * pError = NULL;
	do
	{
		iSocket = socket ( iDomain, SOCK_STREAM, 0 );
		if ( iSocket == -1 )
		{
			pError = "Failed to create client socket";
			break;
		}
	
		if ( connect ( iSocket, pSockaddr, iSockaddrSize ) == -1)
		{
			pError = "Failed to connect to searchd";
			break;
		}

		if ( !sphRecv ( iSocket, (char *)&uServerVersion, sizeof(uServerVersion) ) )
		{
			pError = "Failed to receive searchd version";
			break;
		}
		
		if ( !sphSend ( iSocket, (char *)&uClientVersion, sizeof(uClientVersion) ) )
		{
			pError = "Failed to send client version";
			break;
		}
	}
	while(0);

	// fixme: compare versions?

	if ( pError )
	{
		char sError[1024];
		snprintf ( sError, sizeof(sError), "%s [%d] %s", Format(), errno, strerror(errno) );
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), sError );

		if ( iSocket != -1 )
			close ( iSocket );
		
		return -1;
	}

	return iSocket;
}

struct CSphResponse
{
	char * m_pBuffer;
	char * m_pBody;

	CSphResponse ()
		: m_pBuffer ( NULL )
		, m_pBody ( NULL )
	{}

	CSphResponse ( DWORD uSize )
		: m_pBody ( NULL )
	{
		m_pBuffer = new char[uSize];
	}

	~CSphResponse ()
	{
		SafeDeleteArray ( m_pBuffer );
	}
	
	static CSphResponse * Read ( int iSocket, int iClientVersion );
};

CSphResponse *
CSphResponse::Read ( int iSocket, int iClientVersion )
{
	char sHeader[8];
	if ( !sphRecv ( iSocket, sHeader, sizeof(sHeader) ) )
		return NULL;

	int iStatus   = ntohs ( sphUnalignedRead ( *(short int *) &sHeader[0] ) );
	int iVersion  = ntohs ( sphUnalignedRead ( *(short int *) &sHeader[2] ) );
	DWORD uLength = ntohl ( sphUnalignedRead ( *(DWORD *)     &sHeader[4] ) );

	if ( iVersion < iClientVersion ) // fixme: warn
        {}

	if ( uLength <= SPHINXSE_MAX_ALLOC )
	{
		CSphResponse * pResponse = new CSphResponse ( uLength );
		if ( !sphRecv ( iSocket, pResponse->m_pBuffer, uLength ) )
		{
			SafeDelete ( pResponse );
			return NULL;
		}

		pResponse->m_pBody = pResponse->m_pBuffer;
		if ( iStatus != SEARCHD_OK )
		{
			DWORD uSize = ntohl ( *(DWORD *)pResponse->m_pBuffer );
			if ( iStatus == SEARCHD_WARNING )
				pResponse->m_pBody += uSize; // fixme: report the warning somehow
			else
			{
				char * sMessage = sphDup ( pResponse->m_pBuffer + sizeof(DWORD), uSize );
				my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), sMessage );
				SafeDelete ( sMessage );
				SafeDelete ( pResponse );
				return NULL;
			}
		}
		return pResponse;
	}
	return NULL;
}

/// udf

extern "C"
{
	my_bool sphinx_snippets_init ( UDF_INIT * pUDF, UDF_ARGS * pArgs, char * sMessage );
	void sphinx_snippets_deinit ( UDF_INIT * pUDF );
	char * sphinx_snippets ( UDF_INIT * pUDF, UDF_ARGS * pArgs, char * sResult, unsigned long * pLength, char * pIsNull, char * sError );
};

#define MAX_MESSAGE_LENGTH 255
#define MAX_RESULT_LENGTH 255

struct CSphSnippets
{
	CSphUrl m_tUrl;
	CSphResponse * m_pResponse;

	int m_iBeforeMatch;
	int m_iAfterMatch;
	int m_iChunkSeparator;
	int m_iLimit;
	int m_iAround;
	int m_iFlags;

	CSphSnippets()
		: m_pResponse(NULL)
		, m_iBeforeMatch(0)
		, m_iAfterMatch(0)
		, m_iChunkSeparator(0)
		  // defaults
		, m_iLimit(256)
		, m_iAround(5)
		, m_iFlags(1)
	{
	}

	~CSphSnippets()
	{
		SafeDelete ( m_pResponse );
	}
};

#define KEYWORD(NAME) else if ( strncmp ( NAME, pArgs->attributes[i], pArgs->attribute_lengths[i] ) == 0 )

#define CHECK_TYPE(TYPE)											\
	if ( pArgs->arg_type[i] != TYPE )								\
	{																\
		snprintf ( sMessage, MAX_MESSAGE_LENGTH,					\
				   "%.*s argument must be a string",				\
				   (int)pArgs->attribute_lengths[i],				\
				   pArgs->attributes[i] );							\
		bFail = true;												\
		break;														\
	}																\
	if ( TYPE == STRING_RESULT && !pArgs->args[i] )					\
	{																\
		snprintf ( sMessage, MAX_MESSAGE_LENGTH,					\
				   "%.*s argument must be constant (and not NULL)",	\
				   (int)pArgs->attribute_lengths[i],				\
				   pArgs->attributes[i] );							\
		bFail = true;												\
		break;														\
	}

#define STRING CHECK_TYPE(STRING_RESULT)
#define INT CHECK_TYPE(INT_RESULT); int iValue = *(long long *)pArgs->args[i]

my_bool sphinx_snippets_init ( UDF_INIT * pUDF, UDF_ARGS * pArgs, char * sMessage )
{
	if ( pArgs->arg_count < 3 )
	{
		strncpy ( sMessage, "insufficient arguments", MAX_MESSAGE_LENGTH );
		return 1;
	}

	bool bFail = false;
	CSphSnippets * pOpts = new CSphSnippets;
	for ( uint i = 0; i < pArgs->arg_count; i++ )
	{
		if ( i < 3 )
		{
			if ( pArgs->arg_type[i] != STRING_RESULT )
			{
				strncpy ( sMessage, "first three arguments must be of string type", MAX_MESSAGE_LENGTH );
				bFail = true;
				break;
			}
		}
		KEYWORD("sphinx")
		{
			STRING;
			if ( !pOpts->m_tUrl.Parse ( pArgs->args[i], pArgs->lengths[i] ) )
			{
				strncpy ( sMessage, "failed to parse connection string", MAX_MESSAGE_LENGTH );
				bFail = true;
				break;
			}
		}
		KEYWORD("before_match")		{ STRING; pOpts->m_iBeforeMatch = i; }
		KEYWORD("after_match")		{ STRING; pOpts->m_iAfterMatch = i; }
		KEYWORD("chunk_separator")	{ STRING; pOpts->m_iChunkSeparator = i; }
		KEYWORD("limit")			{ INT; pOpts->m_iLimit = iValue; }
		KEYWORD("around")			{ INT; pOpts->m_iAround = iValue; }
		KEYWORD("exact_phrase")		{ INT; if ( iValue ) pOpts->m_iFlags |= 2; }
		KEYWORD("single_passage")	{ INT; if ( iValue ) pOpts->m_iFlags |= 4; }
		KEYWORD("use_boundaries")	{ INT; if ( iValue ) pOpts->m_iFlags |= 8; }
		KEYWORD("weight_order")		{ INT; if ( iValue ) pOpts->m_iFlags |= 16; }
		else
		{
			snprintf ( sMessage, MAX_MESSAGE_LENGTH, "unrecognized argument: %.*s",
					   (int)pArgs->attribute_lengths[i], pArgs->attributes[i] );
			bFail = true;
			break;
		}
	}
	
	if ( bFail )
	{
		SafeDelete ( pOpts );
		return 1;
	}
	pUDF->ptr = (char *)pOpts;
	return 0;
}

#undef STRING
#undef INT
#undef KEYWORD
#undef CHECK_TYPE

#define ARG(i) pArgs->args[i], pArgs->lengths[i]
#define ARG_LEN(VAR, LEN) ( VAR ? pArgs->lengths[VAR] : LEN )

#define SEND_STRING(INDEX, DEFAULT)							\
	if ( INDEX )											\
		tBuffer.SendString ( ARG(INDEX) );					\
	else													\
		tBuffer.SendString ( DEFAULT, sizeof(DEFAULT) - 1 );


char * sphinx_snippets ( UDF_INIT * pUDF, UDF_ARGS * pArgs, char * sResult, unsigned long * pLength, char * pIsNull, char * pError )
{
	CSphSnippets * pOpts = (CSphSnippets *)pUDF->ptr;
	assert ( pOpts );

	if ( !pArgs->args[0] || !pArgs->args[1] || !pArgs->args[2] )
	{
		*pIsNull = 1;
		return sResult;
	}

	const int iSize =
		8 + // header
		8 +
		4 + pArgs->lengths[1] + // index
		4 + pArgs->lengths[2] + // words
		4 + ARG_LEN ( pOpts->m_iBeforeMatch, 3 ) +
		4 + ARG_LEN ( pOpts->m_iAfterMatch, 4 ) +
		4 + ARG_LEN ( pOpts->m_iChunkSeparator, 5 ) +
		12 +
		4 + pArgs->lengths[0]; // document

	CSphBuffer tBuffer(iSize);

	tBuffer.SendWord ( SEARCHD_COMMAND_EXCERPT );
	tBuffer.SendWord ( VER_COMMAND_EXCERPT );
	tBuffer.SendDword ( iSize - 8 );

	tBuffer.SendDword ( 0 );
	tBuffer.SendDword ( pOpts->m_iFlags );

	tBuffer.SendString ( ARG(1) ); // index
	tBuffer.SendString ( ARG(2) ); // words

	SEND_STRING ( pOpts->m_iBeforeMatch, "<b>" );
	SEND_STRING ( pOpts->m_iAfterMatch, "</b>" );
	SEND_STRING ( pOpts->m_iChunkSeparator, " ... " );

	tBuffer.SendInt ( pOpts->m_iLimit );
	tBuffer.SendInt ( pOpts->m_iAround );

	// single document
	tBuffer.SendInt ( 1 );
	tBuffer.SendString ( ARG(0) );

	int iSocket = -1;
	do
	{
		if ( !tBuffer.Finalize() )
		{
			my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: failed to build request" );
			break;
		}
		
		iSocket = pOpts->m_tUrl.Connect();
		if ( iSocket == -1 ) break;
		if ( !sphSend ( iSocket, tBuffer.Ptr(), iSize, sphReportErrors ) ) break;

		CSphResponse * pResponse = CSphResponse::Read ( iSocket, 0x100 );
		if ( !pResponse ) break;

		close ( iSocket );
		pOpts->m_pResponse = pResponse;
		*pLength = ntohl( *(DWORD *)pResponse->m_pBody );
		return pResponse->m_pBody + sizeof(DWORD);
	}
	while(0);

	if ( iSocket != -1 )
		close ( iSocket );

	*pError = 1;
	return sResult;
}

#undef SEND_STRING
#undef ARG_LEN	
#undef ARG

void sphinx_snippets_deinit ( UDF_INIT * pUDF )
{
	CSphSnippets * pOpts = (CSphSnippets *)pUDF->ptr;
	SafeDelete ( pOpts );
}

//
// $Id: snippets_udf.cc 2058 2009-11-07 04:01:57Z shodan $
//
