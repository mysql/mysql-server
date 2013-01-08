//
// $Id: ha_sphinx.cc 3133 2012-03-01 13:47:52Z shodan $
//

//
// Copyright (c) 2001-2012, Andrew Aksyonoff
// Copyright (c) 2008-2012, Sphinx Technologies Inc
// All rights reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation // gcc: Class implementation
#endif

#if _MSC_VER>=1400
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_NONSTDC_NO_DEPRECATE 1
#endif

#include <mysql_version.h>

#if MYSQL_VERSION_ID>=50515
#include "sql_class.h"
#include "sql_array.h"
#elif MYSQL_VERSION_ID>50100
#include "mysql_priv.h"
#include <mysql/plugin.h>
#else
#include "../mysql_priv.h"
#endif

#include <mysys_err.h>
#include <my_sys.h>
#include <mysql.h> // include client for INSERT table (sort of redoing federated..)

#ifndef __WIN__
	// UNIX-specific
	#include <my_net.h>
	#include <netdb.h>
	#include <sys/un.h>

	#define	RECV_FLAGS	MSG_WAITALL

	#define sphSockClose(_sock)	::close(_sock)
#else
	// Windows-specific
	#include <io.h>
	#define snprintf	_snprintf

	#define	RECV_FLAGS	0

	#define sphSockClose(_sock)	::closesocket(_sock)
#endif

#include <ctype.h>
#include "ha_sphinx.h"

#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif

#if _MSC_VER>=1400
#pragma warning(push,4)
#endif

/////////////////////////////////////////////////////////////////////////////

/// there might be issues with min() on different platforms (eg. Gentoo, they say)
#define Min(a,b) ((a)<(b)?(a):(b))

/// unaligned RAM accesses are forbidden on SPARC
#if defined(sparc) || defined(__sparc__)
#define UNALIGNED_RAM_ACCESS 0
#else
#define UNALIGNED_RAM_ACCESS 1
#endif


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

#if MYSQL_VERSION_ID>=50515

#define sphinx_hash_init my_hash_init
#define sphinx_hash_free my_hash_free
#define sphinx_hash_search my_hash_search
#define sphinx_hash_delete my_hash_delete

#else

#define sphinx_hash_init hash_init
#define sphinx_hash_free hash_free
#define sphinx_hash_search hash_search
#define sphinx_hash_delete hash_delete

#endif

/////////////////////////////////////////////////////////////////////////////

// FIXME! make this all dynamic
#define SPHINXSE_MAX_FILTERS		32

#define SPHINXAPI_DEFAULT_HOST		"127.0.0.1"
#define SPHINXAPI_DEFAULT_PORT		9312
#define SPHINXAPI_DEFAULT_INDEX		"*"

#define SPHINXQL_DEFAULT_PORT		9306

#define SPHINXSE_SYSTEM_COLUMNS		3

#define SPHINXSE_MAX_ALLOC			(16*1024*1024)
#define SPHINXSE_MAX_KEYWORDSTATS	4096

#define SPHINXSE_VERSION			"2.0.4-release"

// FIXME? the following is cut-n-paste from sphinx.h and searchd.cpp
// cut-n-paste is somewhat simpler that adding dependencies however..

enum
{
	SPHINX_SEARCHD_PROTO	= 1,
	SEARCHD_COMMAND_SEARCH	= 0,
	VER_COMMAND_SEARCH		= 0x119,
};

/// search query sorting orders
enum ESphSortOrder
{
	SPH_SORT_RELEVANCE		= 0,	///< sort by document relevance desc, then by date
	SPH_SORT_ATTR_DESC		= 1,	///< sort by document date desc, then by relevance desc
	SPH_SORT_ATTR_ASC		= 2,	///< sort by document date asc, then by relevance desc
	SPH_SORT_TIME_SEGMENTS	= 3,	///< sort by time segments (hour/day/week/etc) desc, then by relevance desc
	SPH_SORT_EXTENDED		= 4,	///< sort by SQL-like expression (eg. "@relevance DESC, price ASC, @id DESC")
	SPH_SORT_EXPR			= 5,	///< sort by expression

	SPH_SORT_TOTAL
};

/// search query matching mode
enum ESphMatchMode
{
	SPH_MATCH_ALL = 0,			///< match all query words
	SPH_MATCH_ANY,				///< match any query word
	SPH_MATCH_PHRASE,			///< match this exact phrase
	SPH_MATCH_BOOLEAN,			///< match this boolean query
	SPH_MATCH_EXTENDED,			///< match this extended query
	SPH_MATCH_FULLSCAN,			///< match all document IDs w/o fulltext query, apply filters
	SPH_MATCH_EXTENDED2,		///< extended engine V2

	SPH_MATCH_TOTAL
};

/// search query relevance ranking mode
enum ESphRankMode
{
	SPH_RANK_PROXIMITY_BM25		= 0,	///< default mode, phrase proximity major factor and BM25 minor one
	SPH_RANK_BM25				= 1,	///< statistical mode, BM25 ranking only (faster but worse quality)
	SPH_RANK_NONE				= 2,	///< no ranking, all matches get a weight of 1
	SPH_RANK_WORDCOUNT			= 3,	///< simple word-count weighting, rank is a weighted sum of per-field keyword occurence counts
	SPH_RANK_PROXIMITY			= 4,	///< phrase proximity
	SPH_RANK_MATCHANY			= 5,	///< emulate old match-any weighting
	SPH_RANK_FIELDMASK			= 6,	///< sets bits where there were matches
	SPH_RANK_SPH04				= 7,	///< codename SPH04, phrase proximity + bm25 + head/exact boost
	SPH_RANK_EXPR				= 8,	///< expression based ranker

	SPH_RANK_TOTAL,
	SPH_RANK_DEFAULT			= SPH_RANK_PROXIMITY_BM25
};

/// search query grouping mode
enum ESphGroupBy
{
	SPH_GROUPBY_DAY		= 0,	///< group by day
	SPH_GROUPBY_WEEK	= 1,	///< group by week
	SPH_GROUPBY_MONTH	= 2,	///< group by month
	SPH_GROUPBY_YEAR	= 3,	///< group by year
	SPH_GROUPBY_ATTR	= 4		///< group by attribute value
};

/// known attribute types
enum
{
	SPH_ATTR_NONE		= 0,			///< not an attribute at all
	SPH_ATTR_INTEGER	= 1,			///< this attr is just an integer
	SPH_ATTR_TIMESTAMP	= 2,			///< this attr is a timestamp
	SPH_ATTR_ORDINAL	= 3,			///< this attr is an ordinal string number (integer at search time, specially handled at indexing time)
	SPH_ATTR_BOOL		= 4,			///< this attr is a boolean bit field
	SPH_ATTR_FLOAT		= 5,
	SPH_ATTR_BIGINT		= 6,
	SPH_ATTR_STRING		= 7,			///< string (binary; in-memory)

	SPH_ATTR_UINT32SET		= 0x40000001UL,	///< this attr is multiple int32 values (0 or more)
	SPH_ATTR_UINT64SET		= 0x40000002UL	///< this attr is multiple int64 values (0 or more)
};

/// known answers
enum
{
	SEARCHD_OK		= 0,	///< general success, command-specific reply follows
	SEARCHD_ERROR	= 1,	///< general failure, error message follows
	SEARCHD_RETRY	= 2,	///< temporary failure, error message follows, client should retry later
	SEARCHD_WARNING	= 3		///< general success, warning message and command-specific reply follow
};

//////////////////////////////////////////////////////////////////////////////

#define SPHINX_DEBUG_OUTPUT		0
#define SPHINX_DEBUG_CALLS		0

#include <stdarg.h>

#if SPHINX_DEBUG_OUTPUT
inline void SPH_DEBUG ( const char * format, ... )
{
	va_list ap;
	va_start ( ap, format );
	fprintf ( stderr, "SphinxSE: " );
	vfprintf ( stderr, format, ap );
	fprintf ( stderr, "\n" );
	va_end ( ap );
}
#else
inline void SPH_DEBUG ( const char *, ... ) {}
#endif

#if SPHINX_DEBUG_CALLS

#define SPH_ENTER_FUNC() { SPH_DEBUG ( "enter %s", __FUNCTION__ ); }
#define SPH_ENTER_METHOD() { SPH_DEBUG ( "enter %s(this=%08x)", __FUNCTION__, this ); }
#define SPH_RET(_arg) { SPH_DEBUG ( "leave %s", __FUNCTION__ ); return _arg; }
#define SPH_VOID_RET() { SPH_DEBUG ( "leave %s", __FUNCTION__ ); return; }

#else

#define SPH_ENTER_FUNC()
#define SPH_ENTER_METHOD()
#define SPH_RET(_arg) { return(_arg); }
#define SPH_VOID_RET() { return; }

#endif


#define SafeDelete(_arg)		{ delete ( _arg ); (_arg) = NULL; }
#define SafeDeleteArray(_arg)	{ if ( _arg ) { delete [] ( _arg );	(_arg) = NULL; } }

//////////////////////////////////////////////////////////////////////////////

/// per-table structure that will be shared among all open Sphinx SE handlers
struct CSphSEShare
{
	pthread_mutex_t	m_tMutex;
	THR_LOCK		m_tLock;

	char *			m_sTable;
	char *			m_sScheme;		///< our connection string
	char *			m_sHost;		///< points into m_sScheme buffer, DO NOT FREE EXPLICITLY
	char *			m_sSocket;		///< points into m_sScheme buffer, DO NOT FREE EXPLICITLY
	char *			m_sIndex;		///< points into m_sScheme buffer, DO NOT FREE EXPLICITLY
	ushort			m_iPort;
	bool			m_bSphinxQL;	///< is this read-only SphinxAPI table, or write-only SphinxQL table?
	uint			m_iTableNameLen;
	uint			m_iUseCount;
	CHARSET_INFO *	m_pTableQueryCharset;

	int					m_iTableFields;
	char **				m_sTableField;
	enum_field_types *	m_eTableFieldType;

	CSphSEShare ()
		: m_sTable ( NULL )
		, m_sScheme ( NULL )
		, m_sHost ( NULL )
		, m_sSocket ( NULL )
		, m_sIndex ( NULL )
		, m_iPort ( 0 )
		, m_bSphinxQL ( false )
		, m_iTableNameLen ( 0 )
		, m_iUseCount ( 1 )
		, m_pTableQueryCharset ( NULL )

		, m_iTableFields ( 0 )
		, m_sTableField ( NULL )
		, m_eTableFieldType ( NULL )
	{
		thr_lock_init ( &m_tLock );
		pthread_mutex_init ( &m_tMutex, MY_MUTEX_INIT_FAST );
	}

	~CSphSEShare ()
	{
		pthread_mutex_destroy ( &m_tMutex );
		thr_lock_delete ( &m_tLock );

		SafeDeleteArray ( m_sTable );
		SafeDeleteArray ( m_sScheme );
		ResetTable ();
	}

	void ResetTable ()
	{
		for ( int i=0; i<m_iTableFields; i++ )
			SafeDeleteArray ( m_sTableField[i] );
		SafeDeleteArray ( m_sTableField );
		SafeDeleteArray ( m_eTableFieldType );
	}
};

/// schema attribute
struct CSphSEAttr
{
	char *			m_sName;		///< attribute name (received from Sphinx)
	uint32			m_uType;		///< attribute type (received from Sphinx)
	int				m_iField;		///< field index in current table (-1 if none)

	CSphSEAttr()
		: m_sName ( NULL )
		, m_uType ( SPH_ATTR_NONE )
		, m_iField ( -1 )
	{}

	~CSphSEAttr ()
	{
		SafeDeleteArray ( m_sName );
	}
};

/// word stats
struct CSphSEWordStats
{
	char *			m_sWord;
	int				m_iDocs;
	int				m_iHits;

	CSphSEWordStats ()
		: m_sWord ( NULL )
		, m_iDocs ( 0 )
		, m_iHits ( 0 )
	{}

	~CSphSEWordStats ()
	{
		SafeDeleteArray ( m_sWord );
	}
};

/// request stats
struct CSphSEStats
{
public:
	int					m_iMatchesTotal;
	int					m_iMatchesFound;
	int					m_iQueryMsec;
	int					m_iWords;
	CSphSEWordStats *	m_dWords;
	bool				m_bLastError;
	char				m_sLastMessage[1024];

	CSphSEStats()
		: m_dWords ( NULL )
	{
		Reset ();
	}

	void Reset ()
	{
		m_iMatchesTotal = 0;
		m_iMatchesFound = 0;
		m_iQueryMsec = 0;
		m_iWords = 0;
		m_bLastError = false;
		m_sLastMessage[0] = '\0';
                SafeDeleteArray ( m_dWords );
	}

	~CSphSEStats()
	{
		SafeDeleteArray ( m_dWords );
	}
};

/// thread local storage
struct CSphSEThreadData
{
	static const int	MAX_QUERY_LEN	= 262144; // 256k should be enough, right?

	bool				m_bStats;
	CSphSEStats			m_tStats;

	bool				m_bQuery;
	char				m_sQuery[MAX_QUERY_LEN];

	CHARSET_INFO *		m_pQueryCharset;

	bool				m_bReplace;		///< are we doing an INSERT or REPLACE

	bool				m_bCondId;		///< got a value from condition pushdown
	longlong			m_iCondId;		///< value acquired from id=value condition pushdown
	bool				m_bCondDone;	///< index_read() is now over

	CSphSEThreadData ()
		: m_bStats ( false )
		, m_bQuery ( false )
		, m_pQueryCharset ( NULL )
		, m_bReplace ( false )
		, m_bCondId ( false )
		, m_iCondId ( 0 )
		, m_bCondDone ( false )
	{}
};

/// filter types
enum ESphFilter
{
	SPH_FILTER_VALUES		= 0,	///< filter by integer values set
	SPH_FILTER_RANGE		= 1,	///< filter by integer range
	SPH_FILTER_FLOATRANGE	= 2		///< filter by float range
};


/// search query filter
struct CSphSEFilter
{
public:
	ESphFilter		m_eType;
	char *			m_sAttrName;
	longlong		m_uMinValue;
	longlong		m_uMaxValue;
	float			m_fMinValue;
	float			m_fMaxValue;
	int				m_iValues;
	longlong *		m_pValues;
	int				m_bExclude;

public:
	CSphSEFilter ()
		: m_eType ( SPH_FILTER_VALUES )
		, m_sAttrName ( NULL )
		, m_uMinValue ( 0 )
		, m_uMaxValue ( UINT_MAX )
		, m_fMinValue ( 0.0f )
		, m_fMaxValue ( 0.0f )
		, m_iValues ( 0 )
		, m_pValues ( NULL )
		, m_bExclude ( 0 )
	{
	}

	~CSphSEFilter ()
	{
		SafeDeleteArray ( m_pValues );
	}
};


/// float vs dword conversion
inline uint32 sphF2DW ( float f )	{ union { float f; uint32 d; } u; u.f = f; return u.d; }

/// dword vs float conversion
inline float sphDW2F ( uint32 d )	{ union { float f; uint32 d; } u; u.d = d; return u.f; }


/// client-side search query
struct CSphSEQuery
{
public:
	const char *	m_sHost;
	int				m_iPort;

private:
	char *			m_sQueryBuffer;

	const char *	m_sIndex;
	int				m_iOffset;
	int				m_iLimit;

	bool			m_bQuery;
        const char *		m_sQuery;
	uint32 *		m_pWeights;
	int				m_iWeights;
	ESphMatchMode	m_eMode;
	ESphRankMode	m_eRanker;
	const char *			m_sRankExpr;
	ESphSortOrder	m_eSort;
	const char *			m_sSortBy;
	int				m_iMaxMatches;
	int				m_iMaxQueryTime;
	uint32			m_iMinID;
	uint32			m_iMaxID;

	int				m_iFilters;
	CSphSEFilter	m_dFilters[SPHINXSE_MAX_FILTERS];

	ESphGroupBy		m_eGroupFunc;
	const char *			m_sGroupBy;
	const char *		m_sGroupSortBy;
	int				m_iCutoff;
	int				m_iRetryCount;
	int				m_iRetryDelay;
        const char *		m_sGroupDistinct;							///< points to query buffer; do NOT delete
	int				m_iIndexWeights;
	char *			m_sIndexWeight[SPHINXSE_MAX_FILTERS];		///< points to query buffer; do NOT delete
	int				m_iIndexWeight[SPHINXSE_MAX_FILTERS];
	int				m_iFieldWeights;
	char *			m_sFieldWeight[SPHINXSE_MAX_FILTERS];		///< points to query buffer; do NOT delete
	int				m_iFieldWeight[SPHINXSE_MAX_FILTERS];

	bool			m_bGeoAnchor;
	const char *		m_sGeoLatAttr;
	const char *		m_sGeoLongAttr;
	float			m_fGeoLatitude;
	float			m_fGeoLongitude;

	const char *		m_sComment;
	const char *		m_sSelect;

	struct Override_t
	{
		union Value_t
		{
			uint32		m_uValue;
			longlong	m_iValue64;
			float		m_fValue;
		};
		char *						m_sName; ///< points to query buffer
		int							m_iType;
		Dynamic_array<ulonglong>	m_dIds;
		Dynamic_array<Value_t>		m_dValues;
	};
	Dynamic_array<Override_t *> m_dOverrides;

public:
	char			m_sParseError[256];

public:
	CSphSEQuery ( const char * sQuery, int iLength, const char * sIndex );
	~CSphSEQuery ();

	bool			Parse ();
	int				BuildRequest ( char ** ppBuffer );

protected:
	char *			m_pBuf;
	char *			m_pCur;
	int				m_iBufLeft;
	bool			m_bBufOverrun;

	template < typename T > int ParseArray ( T ** ppValues, const char * sValue );
	bool			ParseField ( char * sField );

	void			SendBytes ( const void * pBytes, int iBytes );
	void			SendWord ( short int v )		{ v = ntohs(v); SendBytes ( &v, sizeof(v) ); }
	void			SendInt ( int v )				{ v = ntohl(v); SendBytes ( &v, sizeof(v) ); }
	void			SendDword ( uint v )			{ v = ntohl(v) ;SendBytes ( &v, sizeof(v) ); }
	void			SendUint64 ( ulonglong v )		{ SendDword ( (uint)(v>>32) ); SendDword ( (uint)(v&0xFFFFFFFFUL) ); }
	void			SendString ( const char * v )	{ int iLen = strlen(v); SendDword(iLen); SendBytes ( v, iLen ); }
	void			SendFloat ( float v )			{ SendDword ( sphF2DW(v) ); }
};

template int CSphSEQuery::ParseArray<uint32> ( uint32 **, const char * );
template int CSphSEQuery::ParseArray<longlong> ( longlong **, const char * );

//////////////////////////////////////////////////////////////////////////////

#if MYSQL_VERSION_ID>50100

#if MYSQL_VERSION_ID<50114
#error Sphinx SE requires MySQL 5.1.14 or higher if compiling for 5.1.x series!
#endif

static handler *	sphinx_create_handler ( handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root );
static int			sphinx_init_func ( void * p );
static int			sphinx_close_connection ( handlerton * hton, THD * thd );
static int			sphinx_panic ( handlerton * hton, enum ha_panic_function flag );
static bool			sphinx_show_status ( handlerton * hton, THD * thd, stat_print_fn * stat_print, enum ha_stat_type stat_type );

#else

static bool			sphinx_init_func_for_handlerton ();
static int			sphinx_close_connection ( THD * thd );
bool				sphinx_show_status ( THD * thd );

#endif // >50100

//////////////////////////////////////////////////////////////////////////////

static const char	sphinx_hton_name[]		= "SPHINX";
static const char	sphinx_hton_comment[]	= "Sphinx storage engine";

#if MYSQL_VERSION_ID<50100
handlerton sphinx_hton =
{
	#ifdef MYSQL_HANDLERTON_INTERFACE_VERSION
	MYSQL_HANDLERTON_INTERFACE_VERSION,
	#endif
	sphinx_hton_name,
	SHOW_OPTION_YES,
	sphinx_hton_comment,
	DB_TYPE_SPHINX_DB,
	sphinx_init_func_for_handlerton,
	0,							// slot
	0,							// savepoint size
	sphinx_close_connection,	// close_connection
	NULL,	// savepoint
	NULL,	// rollback to savepoint
	NULL,	// release savepoint
	NULL,	// commit
	NULL,	// rollback
	NULL,	// prepare
	NULL,	// recover
	NULL,	// commit_by_xid
	NULL,	// rollback_by_xid
	NULL,	// create_cursor_read_view
	NULL,	// set_cursor_read_view
	NULL,	// close_cursor_read_view
	HTON_CAN_RECREATE
};
#else
static handlerton * sphinx_hton_ptr = NULL;
#endif

//////////////////////////////////////////////////////////////////////////////

// variables for Sphinx shared methods
pthread_mutex_t		sphinx_mutex;		// mutex to init the hash
static int			sphinx_init = 0;	// flag whether the hash was initialized
static HASH			sphinx_open_tables;	// hash used to track open tables

//////////////////////////////////////////////////////////////////////////////
// INITIALIZATION AND SHUTDOWN
//////////////////////////////////////////////////////////////////////////////

// hashing function
#if MYSQL_VERSION_ID>=50120
typedef size_t GetKeyLength_t;
#else
typedef uint GetKeyLength_t;
#endif

static byte * sphinx_get_key ( const byte * pSharePtr, GetKeyLength_t * pLength, my_bool )
{
	CSphSEShare * pShare = (CSphSEShare *) pSharePtr;
	*pLength = (size_t) pShare->m_iTableNameLen;
	return (byte*) pShare->m_sTable;
}

#if MYSQL_VERSION_ID<50100
static int sphinx_init_func ( void * ) // to avoid unused arg warning
#else
static int sphinx_init_func ( void * p )
#endif
{
	SPH_ENTER_FUNC();
	if ( !sphinx_init )
	{
		sphinx_init = 1;
		void ( pthread_mutex_init ( &sphinx_mutex, MY_MUTEX_INIT_FAST ) );
		sphinx_hash_init ( &sphinx_open_tables, system_charset_info, 32, 0, 0,
			sphinx_get_key, 0, 0 );

		#if MYSQL_VERSION_ID > 50100
		handlerton * hton = (handlerton*) p;
		hton->state = SHOW_OPTION_YES;
		hton->db_type = DB_TYPE_AUTOASSIGN;
		hton->create = sphinx_create_handler;
		hton->close_connection = sphinx_close_connection;
		hton->show_status = sphinx_show_status;
		hton->panic = sphinx_panic;
		hton->flags = HTON_CAN_RECREATE;
		#endif
	}
	SPH_RET(0);
}


#if MYSQL_VERSION_ID<50100
static bool sphinx_init_func_for_handlerton ()
{
	return sphinx_init_func ( &sphinx_hton );
}
#endif


#if MYSQL_VERSION_ID>50100

static int sphinx_close_connection ( handlerton * hton, THD * thd )
{
	// deallocate common handler data
	SPH_ENTER_FUNC();
	void ** tmp = thd_ha_data ( thd, hton );
	CSphSEThreadData * pTls = (CSphSEThreadData*) (*tmp);
	SafeDelete ( pTls );
	*tmp = NULL;
	SPH_RET(0);
}


static int sphinx_done_func ( void * )
{
	SPH_ENTER_FUNC();

	if ( sphinx_init )
	{
		sphinx_init = 0;
#ifdef NOT_USED
		if ( sphinx_open_tables.records )
			error = 1;
#endif
		sphinx_hash_free ( &sphinx_open_tables );
		pthread_mutex_destroy ( &sphinx_mutex );
	}

	SPH_RET(0);
}


static int sphinx_panic ( handlerton * hton, enum ha_panic_function )
{
	return sphinx_done_func ( hton );
}

#else

static int sphinx_close_connection ( THD * thd )
{
	// deallocate common handler data
	SPH_ENTER_FUNC();
	CSphSEThreadData * pTls = (CSphSEThreadData*) thd->ha_data[sphinx_hton.slot];
	SafeDelete ( pTls );
	thd->ha_data[sphinx_hton.slot] = NULL;
	SPH_RET(0);
}

#endif // >50100

//////////////////////////////////////////////////////////////////////////////
// SHOW STATUS
//////////////////////////////////////////////////////////////////////////////

#if MYSQL_VERSION_ID>50100
static bool sphinx_show_status ( handlerton * hton, THD * thd, stat_print_fn * stat_print,
	enum ha_stat_type )
#else
bool sphinx_show_status ( THD * thd )
#endif
{
	SPH_ENTER_FUNC();

#if MYSQL_VERSION_ID<50100
	Protocol * protocol = thd->protocol;
	List<Item> field_list;
#endif

	char buf1[IO_SIZE];
	uint buf1len;
	char buf2[IO_SIZE];
	uint buf2len = 0;
	String words;

	buf1[0] = '\0';
	buf2[0] = '\0';


#if MYSQL_VERSION_ID>50100
	// 5.1.x style stats
	CSphSEThreadData * pTls = (CSphSEThreadData*) ( *thd_ha_data ( thd, hton ) );

#define LOC_STATS(_key,_keylen,_val,_vallen) \
	stat_print ( thd, sphinx_hton_name, strlen(sphinx_hton_name), _key, _keylen, _val, _vallen );

#else
	// 5.0.x style stats
	if ( have_sphinx_db!=SHOW_OPTION_YES )
	{
		my_message ( ER_NOT_SUPPORTED_YET,
			"failed to call SHOW SPHINX STATUS: --skip-sphinx was specified",
			MYF(0) );
		SPH_RET(TRUE);
	}
	CSphSEThreadData * pTls = (CSphSEThreadData*) thd->ha_data[sphinx_hton.slot];

	field_list.push_back ( new Item_empty_string ( "Type", 10 ) );
	field_list.push_back ( new Item_empty_string ( "Name", FN_REFLEN ) );
	field_list.push_back ( new Item_empty_string ( "Status", 10 ) );
	if ( protocol->send_fields ( &field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF ) )
		SPH_RET(TRUE);

#define LOC_STATS(_key,_keylen,_val,_vallen) \
	protocol->prepare_for_resend (); \
	protocol->store ( "SPHINX", 6, system_charset_info ); \
	protocol->store ( _key, _keylen, system_charset_info ); \
	protocol->store ( _val, _vallen, system_charset_info ); \
	if ( protocol->write() ) \
		SPH_RET(TRUE);

#endif


	// show query stats
	if ( pTls && pTls->m_bStats )
	{
		const CSphSEStats * pStats = &pTls->m_tStats;
		buf1len = my_snprintf ( buf1, sizeof(buf1),
			"total: %d, total found: %d, time: %d, words: %d",
			pStats->m_iMatchesTotal, pStats->m_iMatchesFound, pStats->m_iQueryMsec, pStats->m_iWords );

		LOC_STATS ( "stats", 5, buf1, buf1len );

		if ( pStats->m_iWords )
		{
			for ( int i=0; i<pStats->m_iWords; i++ )
			{
				CSphSEWordStats & tWord = pStats->m_dWords[i];
				buf2len = my_snprintf ( buf2, sizeof(buf2), "%s%s:%d:%d ",
					buf2, tWord.m_sWord, tWord.m_iDocs, tWord.m_iHits );
			}

			// convert it if we can
			const char * sWord = buf2;
			int iWord = buf2len;

			String sBuf3;
			if ( pTls->m_pQueryCharset )
			{
				uint iErrors;
				sBuf3.copy ( buf2, buf2len, pTls->m_pQueryCharset, system_charset_info, &iErrors );
				sWord = sBuf3.c_ptr();
				iWord = sBuf3.length();
			}

			LOC_STATS ( "words", 5, sWord, iWord );
		}
	}

	// show last error or warning (either in addition to stats, or on their own)
	if ( pTls && pTls->m_tStats.m_sLastMessage && pTls->m_tStats.m_sLastMessage[0] )
	{
		const char * sMessageType = pTls->m_tStats.m_bLastError ? "error" : "warning";

		LOC_STATS (
			sMessageType, strlen ( sMessageType ),
			pTls->m_tStats.m_sLastMessage, strlen ( pTls->m_tStats.m_sLastMessage ) );

	} else
	{
		// well, nothing to show just yet
#if MYSQL_VERSION_ID < 50100
		LOC_STATS ( "stats", 5, "no query has been executed yet", sizeof("no query has been executed yet")-1 );
#endif
	}

#if MYSQL_VERSION_ID < 50100
	send_eof(thd);
#endif

	SPH_RET(FALSE);
}

//////////////////////////////////////////////////////////////////////////////
// HELPERS
//////////////////////////////////////////////////////////////////////////////

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


static void sphLogError ( const char * sFmt, ... )
{
	// emit timestamp
#ifdef __WIN__
	SYSTEMTIME t;
	GetLocalTime ( &t );

	fprintf ( stderr, "%02d%02d%02d %2d:%02d:%02d SphinxSE: internal error: ",
		(int)t.wYear % 100, (int)t.wMonth, (int)t.wDay,
		(int)t.wHour, (int)t.wMinute, (int)t.wSecond );
#else
	// Unix version
	time_t tStamp;
	time ( &tStamp );

	struct tm * pParsed;
#ifdef HAVE_LOCALTIME_R
	struct tm tParsed;
	localtime_r ( &tStamp, &tParsed );
	pParsed = &tParsed;
#else
	pParsed = localtime ( &tStamp );
#endif // HAVE_LOCALTIME_R

	fprintf ( stderr, "%02d%02d%02d %2d:%02d:%02d SphinxSE: internal error: ",
		pParsed->tm_year % 100, pParsed->tm_mon + 1, pParsed->tm_mday,
		pParsed->tm_hour, pParsed->tm_min, pParsed->tm_sec);
#endif // __WIN__

	// emit message
	va_list ap;
	va_start ( ap, sFmt );
	vfprintf ( stderr, sFmt, ap );
	va_end ( ap );

	// emit newline
	fprintf ( stderr, "\n" );
}



// the following scheme variants are recognized
//
// sphinx://host[:port]/index
// sphinxql://host[:port]/index
// unix://unix/domain/socket[:index]
static bool ParseUrl ( CSphSEShare * share, TABLE * table, bool bCreate )
{
	SPH_ENTER_FUNC();

	if ( share )
	{
		// check incoming stuff
		if ( !table )
		{
			sphLogError ( "table==NULL in ParseUrl()" );
			return false;
		}
		if ( !table->s )
		{
			sphLogError ( "(table->s)==NULL in ParseUrl()" );
			return false;
		}

		// free old stuff
		share->ResetTable ();

		// fill new stuff
		share->m_iTableFields = table->s->fields;
		if ( share->m_iTableFields )
		{
			share->m_sTableField = new char * [ share->m_iTableFields ];
			share->m_eTableFieldType = new enum_field_types [ share->m_iTableFields ];

			for ( int i=0; i<share->m_iTableFields; i++ )
			{
				share->m_sTableField[i] = sphDup ( table->field[i]->field_name );
				share->m_eTableFieldType[i] = table->field[i]->type();
			}
		}
	}

	// defaults
	bool bOk = true;
	bool bQL = false;
	char * sScheme = NULL;
	char * sHost = (char*) SPHINXAPI_DEFAULT_HOST;
	char * sIndex = (char*) SPHINXAPI_DEFAULT_INDEX;
	int iPort = SPHINXAPI_DEFAULT_PORT;

	// parse connection string, if any
	while ( table->s->connect_string.length!=0 )
	{
		sScheme = sphDup ( table->s->connect_string.str, table->s->connect_string.length );

		sHost = strstr ( sScheme, "://" );
		if ( !sHost )
		{
			bOk = false;
			break;
		}
		sHost[0] = '\0';
		sHost += 3;

		/////////////////////////////
		// sphinxapi via unix socket
		/////////////////////////////

		if ( !strcmp ( sScheme, "unix" ) )
		{
			sHost--; // reuse last slash
			iPort = 0;
			if (!( sIndex = strrchr ( sHost, ':' ) ))
                          sIndex = (char*) SPHINXAPI_DEFAULT_INDEX;
			else
			{
				*sIndex++ = '\0';
				if ( !*sIndex )
                                  sIndex = (char*) SPHINXAPI_DEFAULT_INDEX;
			}
			bOk = true;
			break;
		}

		/////////////////////
		// sphinxapi via tcp
		/////////////////////

		if ( !strcmp ( sScheme, "sphinx" ) )
		{
			char * sPort = strchr ( sHost, ':' );
			if ( sPort )
			{
				*sPort++ = '\0';
				if ( *sPort )
				{
					sIndex = strchr ( sPort, '/' );
					if ( sIndex )
						*sIndex++ = '\0';
					else
                                          sIndex = (char*) SPHINXAPI_DEFAULT_INDEX;

					iPort = atoi(sPort);
					if ( !iPort )
						iPort = SPHINXAPI_DEFAULT_PORT;
				}
			} else
			{
				sIndex = strchr ( sHost, '/' );
				if ( sIndex )
					*sIndex++ = '\0';
				else
                                  sIndex = (char*) SPHINXAPI_DEFAULT_INDEX;
			}
			bOk = true;
			break;
		}

		////////////
		// sphinxql
		////////////

		if ( !strcmp ( sScheme, "sphinxql" ) )
		{
			bQL = true;
			iPort = SPHINXQL_DEFAULT_PORT;

			// handle port
			char * sPort = strchr ( sHost, ':' );
			sIndex = sHost; // starting point for index name search

			if ( sPort )
			{
				*sPort++ = '\0';
				sIndex = sPort;

				iPort = atoi(sPort);
				if ( !iPort )
				{
					bOk = false; // invalid port; can report ER_FOREIGN_DATA_STRING_INVALID
					break;
				}
			}

			// find index
			sIndex = strchr ( sIndex, '/' );
			if ( sIndex )
				*sIndex++ = '\0';

			// final checks
			// host and index names are required
			bOk = ( sHost && *sHost && sIndex && *sIndex );
			break;
		}

		// unknown case
		bOk = false;
		break;
	}

	if ( !bOk )
	{
		my_error ( bCreate ? ER_FOREIGN_DATA_STRING_INVALID_CANT_CREATE : ER_FOREIGN_DATA_STRING_INVALID,
			MYF(0), table->s->connect_string );
	} else
	{
		if ( share )
		{
			SafeDeleteArray ( share->m_sScheme );
			share->m_sScheme = sScheme;
			share->m_sHost = sHost;
			share->m_sIndex = sIndex;
			share->m_iPort = (ushort)iPort;
			share->m_bSphinxQL = bQL;
		}
	}
	if ( !bOk && !share )
		SafeDeleteArray ( sScheme );

	SPH_RET(bOk);
}


// Example of simple lock controls. The "share" it creates is structure we will
// pass to each sphinx handler. Do you have to have one of these? Well, you have
// pieces that are used for locking, and they are needed to function.
static CSphSEShare * get_share ( const char * table_name, TABLE * table )
{
	SPH_ENTER_FUNC();
	pthread_mutex_lock ( &sphinx_mutex );

	CSphSEShare * pShare = NULL;
	for ( ;; )
	{
		// check if we already have this share
#if MYSQL_VERSION_ID>=50120
		pShare = (CSphSEShare*) sphinx_hash_search ( &sphinx_open_tables, (const uchar *) table_name, strlen(table_name) );
#else
#ifdef __WIN__
		pShare = (CSphSEShare*) sphinx_hash_search ( &sphinx_open_tables, (const byte *) table_name, strlen(table_name) );
#else
		pShare = (CSphSEShare*) sphinx_hash_search ( &sphinx_open_tables, table_name, strlen(table_name) );
#endif // win
#endif // pre-5.1.20

		if ( pShare )
		{
			pShare->m_iUseCount++;
			break;
		}

		// try to allocate new share
		pShare = new CSphSEShare ();
		if ( !pShare )
			break;

		// try to setup it
		if ( !ParseUrl ( pShare, table, false ) )
		{
			SafeDelete ( pShare );
			break;
		}

		if ( !pShare->m_bSphinxQL )
			pShare->m_pTableQueryCharset = table->field[2]->charset();

		// try to hash it
		pShare->m_iTableNameLen = strlen(table_name);
		pShare->m_sTable = sphDup ( table_name );
		if ( my_hash_insert ( &sphinx_open_tables, (const byte *)pShare ) )
		{
			SafeDelete ( pShare );
			break;
		}

		// all seems fine
		break;
	}

	pthread_mutex_unlock ( &sphinx_mutex );
	SPH_RET(pShare);
}


// Free lock controls. We call this whenever we close a table. If the table had
// the last reference to the share then we free memory associated with it.
static int free_share ( CSphSEShare * pShare )
{
	SPH_ENTER_FUNC();
	pthread_mutex_lock ( &sphinx_mutex );

	if ( !--pShare->m_iUseCount )
	{
		sphinx_hash_delete ( &sphinx_open_tables, (byte *)pShare );
		SafeDelete ( pShare );
	}

	pthread_mutex_unlock ( &sphinx_mutex );
	SPH_RET(0);
}


#if MYSQL_VERSION_ID>50100
static handler * sphinx_create_handler ( handlerton * hton, TABLE_SHARE * table, MEM_ROOT * mem_root )
{
	sphinx_hton_ptr = hton;
	return new ( mem_root ) ha_sphinx ( hton, table );
}
#endif

//////////////////////////////////////////////////////////////////////////////
// CLIENT-SIDE REQUEST STUFF
//////////////////////////////////////////////////////////////////////////////

CSphSEQuery::CSphSEQuery ( const char * sQuery, int iLength, const char * sIndex )
	: m_sHost ( "" )
	, m_iPort ( 0 )
	, m_sIndex ( sIndex ? sIndex : "*" )
	, m_iOffset ( 0 )
	, m_iLimit ( 20 )
	, m_bQuery ( false )
	, m_sQuery ( "" )
	, m_pWeights ( NULL )
	, m_iWeights ( 0 )
	, m_eMode ( SPH_MATCH_ALL )
	, m_eRanker ( SPH_RANK_PROXIMITY_BM25 )
	, m_sRankExpr ( NULL )
	, m_eSort ( SPH_SORT_RELEVANCE )
	, m_sSortBy ( "" )
	, m_iMaxMatches ( 1000 )
	, m_iMaxQueryTime ( 0 )
	, m_iMinID ( 0 )
	, m_iMaxID ( 0 )
	, m_iFilters ( 0 )
	, m_eGroupFunc ( SPH_GROUPBY_DAY )
	, m_sGroupBy ( "" )
	, m_sGroupSortBy ( "@group desc" )
	, m_iCutoff ( 0 )
	, m_iRetryCount ( 0 )
	, m_iRetryDelay ( 0 )
	, m_sGroupDistinct ( "" )
	, m_iIndexWeights ( 0 )
	, m_iFieldWeights ( 0 )
	, m_bGeoAnchor ( false )
	, m_sGeoLatAttr ( "" )
	, m_sGeoLongAttr ( "" )
	, m_fGeoLatitude ( 0.0f )
	, m_fGeoLongitude ( 0.0f )
	, m_sComment ( "" )
	, m_sSelect ( "" )

	, m_pBuf ( NULL )
	, m_pCur ( NULL )
	, m_iBufLeft ( 0 )
	, m_bBufOverrun ( false )
{
	m_sQueryBuffer = new char [ iLength+2 ];
	memcpy ( m_sQueryBuffer, sQuery, iLength );
	m_sQueryBuffer[iLength] = ';';
	m_sQueryBuffer[iLength+1] = '\0';
}


CSphSEQuery::~CSphSEQuery ()
{
	SPH_ENTER_METHOD();
	SafeDeleteArray ( m_sQueryBuffer );
	SafeDeleteArray ( m_pWeights );
	SafeDeleteArray ( m_pBuf );
	for ( int i=0; i<m_dOverrides.elements(); i++ )
		SafeDelete ( m_dOverrides.at(i) );
	SPH_VOID_RET();
}


template < typename T >
int CSphSEQuery::ParseArray ( T ** ppValues, const char * sValue )
{
	SPH_ENTER_METHOD();

	assert ( ppValues );
	assert ( !(*ppValues) );

	const char * pValue;
	bool bPrevDigit = false;
	int iValues = 0;

	// count the values
	for ( pValue=sValue; *pValue; pValue++ )
	{
		bool bDigit = (*pValue)>='0' && (*pValue)<='9';
		if ( bDigit && !bPrevDigit )
			iValues++;
		bPrevDigit = bDigit;
	}
	if ( !iValues )
		SPH_RET(0);

	// extract the values
	T * pValues = new T [ iValues ];
	*ppValues = pValues;

	int iIndex = 0, iSign = 1;
	T uValue = 0;

	bPrevDigit = false;
	for ( pValue=sValue ;; pValue++ )
	{
		bool bDigit = (*pValue)>='0' && (*pValue)<='9';

		if ( bDigit )
		{
			if ( !bPrevDigit )
				uValue = 0;
			uValue = uValue*10 + ( (*pValue)-'0' );
		} else if ( bPrevDigit )
		{
			assert ( iIndex<iValues );
			pValues [ iIndex++ ] = uValue * iSign;
			iSign = 1;
		} else if ( *pValue=='-' )
			iSign = -1;

		bPrevDigit = bDigit;
		if ( !*pValue )
			break;
	}

	SPH_RET ( iValues );
}


static char * chop ( char * s )
{
	while ( *s && isspace(*s) )
		s++;

	char * p = s + strlen(s);
	while ( p>s && isspace ( p[-1] ) )
		p--;
	*p = '\0';

	return s;
}


static bool myisattr ( char c )
{
	return
		( c>='0' && c<='9' ) ||
		( c>='a' && c<='z' ) ||
		( c>='A' && c<='Z' ) ||
		c=='_';
}


bool CSphSEQuery::ParseField ( char * sField )
{
	SPH_ENTER_METHOD();

	// look for option name/value separator
	char * sValue = strchr ( sField, '=' );
	if ( !sValue || sValue==sField || sValue[-1]=='\\' )
	{
		// by default let's assume it's just query
		if ( sField[0] )
		{
			if ( m_bQuery )
			{
				snprintf ( m_sParseError, sizeof(m_sParseError), "search query already specified; '%s' is redundant", sField );
				SPH_RET(false);
			} else
			{
				m_sQuery = sField;
				m_bQuery = true;

				// unescape only 1st one
				char *s = sField, *d = sField;
				int iSlashes = 0;
				while ( *s )
				{
					iSlashes = ( *s=='\\' ) ? iSlashes+1 : 0;
					if ( ( iSlashes%2 )==0 ) *d++ = *s;
					s++;
				}
				*d = '\0';
			}
		}
		SPH_RET(true);
	}

	// split
	*sValue++ = '\0';
	sValue = chop ( sValue );
	int iValue = atoi ( sValue );

	// handle options
	char * sName = chop ( sField );

	if ( !strcmp ( sName, "query" ) )			m_sQuery = sValue;
	else if ( !strcmp ( sName, "host" ) )		m_sHost = sValue;
	else if ( !strcmp ( sName, "port" ) )		m_iPort = iValue;
	else if ( !strcmp ( sName, "index" ) )		m_sIndex = sValue;
	else if ( !strcmp ( sName, "offset" ) )		m_iOffset = iValue;
	else if ( !strcmp ( sName, "limit" ) )		m_iLimit = iValue;
	else if ( !strcmp ( sName, "weights" ) )	m_iWeights = ParseArray<uint32> ( &m_pWeights, sValue );
	else if ( !strcmp ( sName, "minid" ) )		m_iMinID = iValue;
	else if ( !strcmp ( sName, "maxid" ) )		m_iMaxID = iValue;
	else if ( !strcmp ( sName, "maxmatches" ) )	m_iMaxMatches = iValue;
	else if ( !strcmp ( sName, "maxquerytime" ) )	m_iMaxQueryTime = iValue;
	else if ( !strcmp ( sName, "groupsort" ) )	m_sGroupSortBy = sValue;
	else if ( !strcmp ( sName, "distinct" ) )	m_sGroupDistinct = sValue;
	else if ( !strcmp ( sName, "cutoff" ) )		m_iCutoff = iValue;
	else if ( !strcmp ( sName, "comment" ) )	m_sComment = sValue;
	else if ( !strcmp ( sName, "select" ) )		m_sSelect = sValue;

	else if ( !strcmp ( sName, "mode" ) )
	{
		m_eMode = SPH_MATCH_ALL;
		if ( !strcmp ( sValue, "any" ) )			m_eMode = SPH_MATCH_ANY;
		else if ( !strcmp ( sValue, "phrase" ) )	m_eMode = SPH_MATCH_PHRASE;
		else if ( !strcmp ( sValue, "boolean" ) )	m_eMode = SPH_MATCH_BOOLEAN;
		else if ( !strcmp ( sValue, "ext" ) )		m_eMode = SPH_MATCH_EXTENDED;
		else if ( !strcmp ( sValue, "extended" ) )	m_eMode = SPH_MATCH_EXTENDED;
		else if ( !strcmp ( sValue, "ext2" ) )		m_eMode = SPH_MATCH_EXTENDED2;
		else if ( !strcmp ( sValue, "extended2" ) )	m_eMode = SPH_MATCH_EXTENDED2;
		else if ( !strcmp ( sValue, "all" ) )		m_eMode = SPH_MATCH_ALL;
		else if ( !strcmp ( sValue, "fullscan" ) )	m_eMode = SPH_MATCH_FULLSCAN;
		else
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "unknown matching mode '%s'", sValue );
			SPH_RET(false);
		}
	} else if ( !strcmp ( sName, "ranker" ) )
	{
		m_eRanker = SPH_RANK_PROXIMITY_BM25;
		if ( !strcmp ( sValue, "proximity_bm25" ) )	m_eRanker = SPH_RANK_PROXIMITY_BM25;
		else if ( !strcmp ( sValue, "bm25" ) )		m_eRanker = SPH_RANK_BM25;
		else if ( !strcmp ( sValue, "none" ) )		m_eRanker = SPH_RANK_NONE;
		else if ( !strcmp ( sValue, "wordcount" ) )	m_eRanker = SPH_RANK_WORDCOUNT;
		else if ( !strcmp ( sValue, "proximity" ) )	m_eRanker = SPH_RANK_PROXIMITY;
		else if ( !strcmp ( sValue, "matchany" ) )	m_eRanker = SPH_RANK_MATCHANY;
		else if ( !strcmp ( sValue, "fieldmask" ) )	m_eRanker = SPH_RANK_FIELDMASK;
		else if ( !strcmp ( sValue, "sph04" ) )		m_eRanker = SPH_RANK_SPH04;
		else if ( !strncmp ( sValue, "expr:", 5 ) )
		{
			m_eRanker = SPH_RANK_EXPR;
			m_sRankExpr = sValue+5;
		} else
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "unknown ranking mode '%s'", sValue );
			SPH_RET(false);
		}
	} else if ( !strcmp ( sName, "sort" ) )
	{
		static const struct
		{
			const char *	m_sName;
			ESphSortOrder	m_eSort;
		} dSortModes[] =
		{
			{ "relevance",		SPH_SORT_RELEVANCE },
			{ "attr_desc:",		SPH_SORT_ATTR_DESC },
			{ "attr_asc:",		SPH_SORT_ATTR_ASC },
			{ "time_segments:",	SPH_SORT_TIME_SEGMENTS },
			{ "extended:",		SPH_SORT_EXTENDED },
			{ "expr:",			SPH_SORT_EXPR }
		};

		int i;
		const int nModes = sizeof(dSortModes)/sizeof(dSortModes[0]);
		for ( i=0; i<nModes; i++ )
			if ( !strncmp ( sValue, dSortModes[i].m_sName, strlen ( dSortModes[i].m_sName ) ) )
		{
			m_eSort = dSortModes[i].m_eSort;
			m_sSortBy = sValue + strlen ( dSortModes[i].m_sName );
			break;
		}
		if ( i==nModes )
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "unknown sorting mode '%s'", sValue );
			SPH_RET(false);
		}

	} else if ( !strcmp ( sName, "groupby" ) )
	{
		static const struct
		{
			const char *	m_sName;
			ESphGroupBy		m_eFunc;
		} dGroupModes[] =
		{
			{ "day:",	SPH_GROUPBY_DAY },
			{ "week:",	SPH_GROUPBY_WEEK },
			{ "month:",	SPH_GROUPBY_MONTH },
			{ "year:",	SPH_GROUPBY_YEAR },
			{ "attr:",	SPH_GROUPBY_ATTR },
		};

		int i;
		const int nModes = sizeof(dGroupModes)/sizeof(dGroupModes[0]);
		for ( i=0; i<nModes; i++ )
			if ( !strncmp ( sValue, dGroupModes[i].m_sName, strlen ( dGroupModes[i].m_sName ) ) )
		{
			m_eGroupFunc = dGroupModes[i].m_eFunc;
			m_sGroupBy = sValue + strlen ( dGroupModes[i].m_sName );
			break;
		}
		if ( i==nModes )
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "unknown groupby mode '%s'", sValue );
			SPH_RET(false);
		}

	} else if ( m_iFilters<SPHINXSE_MAX_FILTERS &&
		( !strcmp ( sName, "range" ) || !strcmp ( sName, "!range" ) || !strcmp ( sName, "floatrange" ) || !strcmp ( sName, "!floatrange" ) ) )
	{
		for ( ;; )
		{
			char * p = sName;
			CSphSEFilter & tFilter = m_dFilters [ m_iFilters ];
			tFilter.m_bExclude = ( *p=='!' ); if ( tFilter.m_bExclude ) p++;
			tFilter.m_eType = ( *p=='f' ) ? SPH_FILTER_FLOATRANGE : SPH_FILTER_RANGE;

			if (!( p = strchr ( sValue, ',' ) ))
				break;
			*p++ = '\0';

			tFilter.m_sAttrName = chop ( sValue );
			sValue = p;

			if (!( p = strchr ( sValue, ',' ) ))
				break;
			*p++ = '\0';

			if ( tFilter.m_eType==SPH_FILTER_RANGE )
			{
				tFilter.m_uMinValue = strtoll ( sValue, NULL, 0 );
				tFilter.m_uMaxValue = strtoll ( p, NULL, 0 );
			} else
			{
				tFilter.m_fMinValue = (float)atof(sValue);
				tFilter.m_fMaxValue = (float)atof(p);
			}

			// all ok
			m_iFilters++;
			break;
		}

	} else if ( m_iFilters<SPHINXSE_MAX_FILTERS &&
		( !strcmp ( sName, "filter" ) || !strcmp ( sName, "!filter" ) ) )
	{
		for ( ;; )
		{
			CSphSEFilter & tFilter = m_dFilters [ m_iFilters ];
			tFilter.m_eType = SPH_FILTER_VALUES;
			tFilter.m_bExclude = ( strcmp ( sName, "!filter" )==0 );

			// get the attr name
			while ( (*sValue) && !myisattr(*sValue) )
				sValue++;
			if ( !*sValue )
				break;

			tFilter.m_sAttrName = sValue;
			while ( (*sValue) && myisattr(*sValue) )
				sValue++;
			if ( !*sValue )
				break;
			*sValue++ = '\0';

			// get the values
			tFilter.m_iValues = ParseArray<longlong> ( &tFilter.m_pValues, sValue );
			if ( !tFilter.m_iValues )
			{
				assert ( !tFilter.m_pValues );
				break;
			}

			// all ok
			m_iFilters++;
			break;
		}

	} else if ( !strcmp ( sName, "indexweights" ) || !strcmp ( sName, "fieldweights" ) )
	{
		bool bIndex = !strcmp ( sName, "indexweights" );
		int * pCount = bIndex ? &m_iIndexWeights : &m_iFieldWeights;
		char ** pNames = bIndex ? &m_sIndexWeight[0] : &m_sFieldWeight[0];
		int * pWeights = bIndex ? &m_iIndexWeight[0] : &m_iFieldWeight[0];

		*pCount = 0;

		char * p = sValue;
		while ( *p && *pCount<SPHINXSE_MAX_FILTERS )
		{
			// extract attr name
			if ( !myisattr(*p) )
			{
				snprintf ( m_sParseError, sizeof(m_sParseError), "%s: index name expected near '%s'", sName, p );
				SPH_RET(false);
			}

			pNames[*pCount] = p;
			while ( myisattr(*p) ) p++;

			if ( *p!=',' )
			{
				snprintf ( m_sParseError, sizeof(m_sParseError), "%s: comma expected near '%s'", sName, p );
				SPH_RET(false);
			}
			*p++ = '\0';

			// extract attr value
			char * sVal = p;
			while ( isdigit(*p) ) p++;
			if ( p==sVal )
			{
				snprintf ( m_sParseError, sizeof(m_sParseError), "%s: integer weight expected near '%s'", sName, sVal );
				SPH_RET(false);
			}
			pWeights[*pCount] = atoi(sVal);
			(*pCount)++;

			if ( !*p )
				break;
			if ( *p!=',' )
			{
				snprintf ( m_sParseError, sizeof(m_sParseError), "%s: comma expected near '%s'", sName, p );
				SPH_RET(false);
			}
			p++;
		}

	} else if ( !strcmp ( sName, "geoanchor" ) )
	{
		m_bGeoAnchor = false;
		for ( ;; )
		{
			char * sLat = sValue;
			char * p = sValue;

			if (!( p = strchr ( p, ',' ) )) break; *p++ = '\0';
			char * sLong = p;

			if (!( p = strchr ( p, ',' ) )) break; *p++ = '\0';
			char * sLatVal = p;

			if (!( p = strchr ( p, ',' ) )) break; *p++ = '\0';
			char * sLongVal = p;

			m_sGeoLatAttr = chop(sLat);
			m_sGeoLongAttr = chop(sLong);
			m_fGeoLatitude = (float)atof ( sLatVal );
			m_fGeoLongitude = (float)atof ( sLongVal );
			m_bGeoAnchor = true;
			break;
		}
		if ( !m_bGeoAnchor )
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "geoanchor: parse error, not enough comma-separated arguments" );
			SPH_RET(false);
		}
	} else if ( !strcmp ( sName, "override" ) ) // name,type,id:value,id:value,...
	{
		char * sName = NULL;
		int iType = 0;
		CSphSEQuery::Override_t * pOverride = NULL;

		// get name and type
		char * sRest = sValue;
		for ( ;; )
		{
			sName = sRest;
			if ( !*sName )
				break;
			if (!( sRest = strchr ( sRest, ',' ) ))
				break;
			*sRest++ = '\0';
			char * sType = sRest;
			if (!( sRest = strchr ( sRest, ',' ) ))
				break;

			static const struct
			{
				const char *	m_sName;
				int				m_iType;
			}
			dAttrTypes[] =
			{
				{ "int",		SPH_ATTR_INTEGER },
				{ "timestamp",	SPH_ATTR_TIMESTAMP },
				{ "bool",		SPH_ATTR_BOOL },
				{ "float",		SPH_ATTR_FLOAT },
				{ "bigint",		SPH_ATTR_BIGINT }
			};
			for ( uint i=0; i<sizeof(dAttrTypes)/sizeof(*dAttrTypes); i++ )
				if ( !strncmp ( sType, dAttrTypes[i].m_sName, sRest - sType ) )
			{
				iType = dAttrTypes[i].m_iType;
				break;
			}
			break;
		}

		// fail
		if ( !sName || !*sName || !iType )
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "override: malformed query" );
			SPH_RET(false);
		}

		// grab id:value pairs
		sRest++;
		while ( sRest )
		{
			char * sId = sRest;
			if (!( sRest = strchr ( sRest, ':' ) )) break; *sRest++ = '\0';
			if (!( sRest - sId )) break;

			char * sValue = sRest;
			if ( ( sRest = strchr ( sRest, ',' ) )!=NULL )
				*sRest++ = '\0';
			if ( !*sValue )
				break;

			if ( !pOverride )
			{
				pOverride = new CSphSEQuery::Override_t;
				pOverride->m_sName = chop(sName);
				pOverride->m_iType = iType;
				m_dOverrides.append ( pOverride );
			}

			ulonglong uId = strtoull ( sId, NULL, 10 );
			CSphSEQuery::Override_t::Value_t tValue;
			if ( iType==SPH_ATTR_FLOAT )
				tValue.m_fValue = (float)atof(sValue);
			else if ( iType==SPH_ATTR_BIGINT )
				tValue.m_iValue64 = strtoll ( sValue, NULL, 10 );
			else
				tValue.m_uValue = (uint32)strtoul ( sValue, NULL, 10 );

			pOverride->m_dIds.append ( uId );
			pOverride->m_dValues.append ( tValue );
		}

		if ( !pOverride )
		{
			snprintf ( m_sParseError, sizeof(m_sParseError), "override: id:value mapping expected" );
			SPH_RET(false);
		}
		SPH_RET(true);
	} else
	{
		snprintf ( m_sParseError, sizeof(m_sParseError), "unknown parameter '%s'", sName );
		SPH_RET(false);
	}

	// !COMMIT handle syntax errors

	SPH_RET(true);
}


bool CSphSEQuery::Parse ()
{
	SPH_ENTER_METHOD();
	SPH_DEBUG ( "query [[ %s ]]", m_sQueryBuffer );

	m_bQuery = false;
	char * pCur = m_sQueryBuffer;
	char * pNext = pCur;

	while ( ( pNext = strchr ( pNext, ';' ) )!=NULL )
	{
		// handle escaped semicolons
		if ( pNext>m_sQueryBuffer && pNext[-1]=='\\' && pNext[1]!='\0' )
		{
			pNext++;
			continue;
		}

		// handle semicolon-separated clauses
		*pNext++ = '\0';
		if ( !ParseField ( pCur ) )
			SPH_RET(false);
		pCur = pNext;
	}

	SPH_DEBUG ( "q [[ %s ]]", m_sQuery );

	SPH_RET(true);
}


void CSphSEQuery::SendBytes ( const void * pBytes, int iBytes )
{
	SPH_ENTER_METHOD();
	if ( m_iBufLeft<iBytes )
	{
		m_bBufOverrun = true;
		SPH_VOID_RET();
	}

	memcpy ( m_pCur, pBytes, iBytes );

	m_pCur += iBytes;
	m_iBufLeft -= iBytes;
	SPH_VOID_RET();
}


int CSphSEQuery::BuildRequest ( char ** ppBuffer )
{
	SPH_ENTER_METHOD();

	// calc request length
	int iReqSize = 128 + 4*m_iWeights
		+ strlen ( m_sSortBy )
		+ strlen ( m_sQuery )
		+ strlen ( m_sIndex )
		+ strlen ( m_sGroupBy )
		+ strlen ( m_sGroupSortBy )
		+ strlen ( m_sGroupDistinct )
		+ strlen ( m_sComment )
		+ strlen ( m_sSelect );
	if ( m_eRanker==SPH_RANK_EXPR )
		iReqSize += 4 + strlen(m_sRankExpr);
	for ( int i=0; i<m_iFilters; i++ )
	{
		const CSphSEFilter & tFilter = m_dFilters[i];
		iReqSize += 12 + strlen ( tFilter.m_sAttrName ); // string attr-name; int type; int exclude-flag
		switch ( tFilter.m_eType )
		{
			case SPH_FILTER_VALUES:		iReqSize += 4 + 8*tFilter.m_iValues; break;
			case SPH_FILTER_RANGE:		iReqSize += 16; break;
			case SPH_FILTER_FLOATRANGE:	iReqSize += 8; break;
		}
	}
	if ( m_bGeoAnchor ) // 1.14+
		iReqSize += 16 + strlen ( m_sGeoLatAttr ) + strlen ( m_sGeoLongAttr );
	for ( int i=0; i<m_iIndexWeights; i++ ) // 1.15+
		iReqSize += 8 + strlen(m_sIndexWeight[i] );
	for ( int i=0; i<m_iFieldWeights; i++ ) // 1.18+
		iReqSize += 8 + strlen(m_sFieldWeight[i] );
	// overrides
	iReqSize += 4;
	for ( int i=0; i<m_dOverrides.elements(); i++ )
	{
		CSphSEQuery::Override_t * pOverride = m_dOverrides.at(i);
		const uint32 uSize = pOverride->m_iType==SPH_ATTR_BIGINT ? 16 : 12; // id64 + value
		iReqSize += strlen ( pOverride->m_sName ) + 12 + uSize*pOverride->m_dIds.elements();
	}
	// select
	iReqSize += 4;

	m_iBufLeft = 0;
	SafeDeleteArray ( m_pBuf );

	m_pBuf = new char [ iReqSize ];
	if ( !m_pBuf )
		SPH_RET(-1);

	m_pCur = m_pBuf;
	m_iBufLeft = iReqSize;
	m_bBufOverrun = false;
	(*ppBuffer) = m_pBuf;

	// build request
	SendWord ( SEARCHD_COMMAND_SEARCH ); // command id
	SendWord ( VER_COMMAND_SEARCH ); // command version
	SendInt ( iReqSize-8 ); // packet body length
	SendInt ( 0 ); // its a client

	SendInt ( 1 ); // number of queries
	SendInt ( m_iOffset );
	SendInt ( m_iLimit );
	SendInt ( m_eMode );
	SendInt ( m_eRanker ); // 1.16+
	if ( m_eRanker==SPH_RANK_EXPR )
		SendString ( m_sRankExpr );
	SendInt ( m_eSort );
	SendString ( m_sSortBy ); // sort attr
	SendString ( m_sQuery ); // query
	SendInt ( m_iWeights );
	for ( int j=0; j<m_iWeights; j++ )
		SendInt ( m_pWeights[j] ); // weights
	SendString ( m_sIndex ); // indexes
	SendInt ( 1 ); // id64 range follows
	SendUint64 ( m_iMinID ); // id/ts ranges
	SendUint64 ( m_iMaxID );

	SendInt ( m_iFilters );
	for ( int j=0; j<m_iFilters; j++ )
	{
		const CSphSEFilter & tFilter = m_dFilters[j];
		SendString ( tFilter.m_sAttrName );
		SendInt ( tFilter.m_eType );

		switch ( tFilter.m_eType )
		{
			case SPH_FILTER_VALUES:
				SendInt ( tFilter.m_iValues );
				for ( int k=0; k<tFilter.m_iValues; k++ )
					SendUint64 ( tFilter.m_pValues[k] );
				break;

			case SPH_FILTER_RANGE:
				SendUint64 ( tFilter.m_uMinValue );
				SendUint64 ( tFilter.m_uMaxValue );
				break;

			case SPH_FILTER_FLOATRANGE:
				SendFloat ( tFilter.m_fMinValue );
				SendFloat ( tFilter.m_fMaxValue );
				break;
		}

		SendInt ( tFilter.m_bExclude );
	}

	SendInt ( m_eGroupFunc );
	SendString ( m_sGroupBy );
	SendInt ( m_iMaxMatches );
	SendString ( m_sGroupSortBy );
	SendInt ( m_iCutoff ); // 1.9+
	SendInt ( m_iRetryCount ); // 1.10+
	SendInt ( m_iRetryDelay );
	SendString ( m_sGroupDistinct ); // 1.11+
	SendInt ( m_bGeoAnchor ); // 1.14+
	if ( m_bGeoAnchor )
	{
		SendString ( m_sGeoLatAttr );
		SendString ( m_sGeoLongAttr );
		SendFloat ( m_fGeoLatitude );
		SendFloat ( m_fGeoLongitude );
	}
	SendInt ( m_iIndexWeights ); // 1.15+
	for ( int i=0; i<m_iIndexWeights; i++ )
	{
		SendString ( m_sIndexWeight[i] );
		SendInt ( m_iIndexWeight[i] );
	}
	SendInt ( m_iMaxQueryTime ); // 1.17+
	SendInt ( m_iFieldWeights ); // 1.18+
	for ( int i=0; i<m_iFieldWeights; i++ )
	{
		SendString ( m_sFieldWeight[i] );
		SendInt ( m_iFieldWeight[i] );
	}
	SendString ( m_sComment );

	// overrides
	SendInt ( m_dOverrides.elements() );
	for ( int i=0; i<m_dOverrides.elements(); i++ )
	{
		CSphSEQuery::Override_t * pOverride = m_dOverrides.at(i);
		SendString ( pOverride->m_sName );
		SendDword ( pOverride->m_iType );
		SendInt ( pOverride->m_dIds.elements() );
		for ( int j=0; j<pOverride->m_dIds.elements(); j++ )
		{
			SendUint64 ( pOverride->m_dIds.at(j) );
			if ( pOverride->m_iType==SPH_ATTR_FLOAT )
				SendFloat ( pOverride->m_dValues.at(j).m_fValue );
			else if ( pOverride->m_iType==SPH_ATTR_BIGINT )
				SendUint64 ( pOverride->m_dValues.at(j).m_iValue64 );
			else
				SendDword ( pOverride->m_dValues.at(j).m_uValue );
		}
	}

	// select
	SendString ( m_sSelect );

	// detect buffer overruns and underruns, and report internal error
	if ( m_bBufOverrun || m_iBufLeft!=0 || m_pCur-m_pBuf!=iReqSize )
		SPH_RET(-1);

	// all fine
	SPH_RET ( iReqSize );
}

//////////////////////////////////////////////////////////////////////////////
// SPHINX HANDLER
//////////////////////////////////////////////////////////////////////////////

static const char * ha_sphinx_exts[] = { NullS };


#if MYSQL_VERSION_ID<50100
ha_sphinx::ha_sphinx ( TABLE_ARG * table )
	: handler ( &sphinx_hton, table )
#else
ha_sphinx::ha_sphinx ( handlerton * hton, TABLE_ARG * table )
	: handler ( hton, table )
#endif
	, m_pShare ( NULL )
	, m_iMatchesTotal ( 0 )
	, m_iCurrentPos ( 0 )
	, m_pCurrentKey ( NULL )
	, m_iCurrentKeyLen ( 0 )
	, m_pResponse ( NULL )
	, m_pResponseEnd ( NULL )
	, m_pCur ( NULL )
	, m_bUnpackError ( false )
	, m_iFields ( 0 )
	, m_dFields ( NULL )
	, m_iAttrs ( 0 )
	, m_dAttrs ( NULL )
	, m_bId64 ( 0 )
	, m_dUnboundFields ( NULL )
{
	SPH_ENTER_METHOD();
	SPH_VOID_RET();
}

ha_sphinx::~ha_sphinx()
{
  SafeDeleteArray ( m_dAttrs );
  SafeDeleteArray ( m_dUnboundFields );
  if ( m_dFields )
  {
    for (uint32 i=0; i< m_iFields; i++ )
      SafeDeleteArray ( m_dFields[i] );
    delete [] m_dFields;
  }
}


// If frm_error() is called then we will use this to to find out what file extentions
// exist for the storage engine. This is also used by the default rename_table and
// delete_table method in handler.cc.
const char ** ha_sphinx::bas_ext() const
{
	return ha_sphinx_exts;
}


// Used for opening tables. The name will be the name of the file.
// A table is opened when it needs to be opened. For instance
// when a request comes in for a select on the table (tables are not
// open and closed for each request, they are cached).
//
// Called from handler.cc by handler::ha_open(). The server opens all tables by
// calling ha_open() which then calls the handler specific open().
int ha_sphinx::open ( const char * name, int, uint )
{
	SPH_ENTER_METHOD();
	m_pShare = get_share ( name, table );
	if ( !m_pShare )
		SPH_RET(1);

	thr_lock_data_init ( &m_pShare->m_tLock, &m_tLock, NULL );

	#if MYSQL_VERSION_ID>50100
	void **tmp= thd_ha_data(table->in_use, ht);
        if (*tmp)
        {
          CSphSEThreadData* pTls = (CSphSEThreadData*) *tmp;
          SafeDelete(pTls);
          *tmp= NULL;
        }
	#else
	table->in_use->ha_data [ sphinx_hton.slot ] = NULL;
	#endif

	SPH_RET(0);
}


int ha_sphinx::Connect ( const char * sHost, ushort uPort )
{
	struct sockaddr_in sin;
#ifndef __WIN__
	struct sockaddr_un saun;
#endif

	int iDomain = 0;
	int iSockaddrSize = 0;
	struct sockaddr * pSockaddr = NULL;

	in_addr_t ip_addr;

	if ( uPort )
	{
		iDomain = AF_INET;
		iSockaddrSize = sizeof(sin);
		pSockaddr = (struct sockaddr *) &sin;

		memset ( &sin, 0, sizeof(sin) );
		sin.sin_family = AF_INET;
		sin.sin_port = htons(uPort);

		// prepare host address
		if ( (int)( ip_addr = inet_addr(sHost) )!=(int)INADDR_NONE )
		{
			memcpy ( &sin.sin_addr, &ip_addr, sizeof(ip_addr) );
		} else
		{
			int tmp_errno;
			bool bError = false;

#if MYSQL_VERSION_ID>=50515
			struct addrinfo *hp = NULL;
			tmp_errno = getaddrinfo ( sHost, NULL, NULL, &hp );
			if ( !tmp_errno || !hp || !hp->ai_addr )
			{
				bError = true;
				if ( hp )
					freeaddrinfo ( hp );
			}
#else
			struct hostent tmp_hostent, *hp;
			char buff2 [ GETHOSTBYNAME_BUFF_SIZE ];
			hp = my_gethostbyname_r ( sHost, &tmp_hostent, buff2, sizeof(buff2), &tmp_errno );
			if ( !hp )
			{
				my_gethostbyname_r_free();
				bError = true;
			}
#endif

			if ( bError )
			{
				char sError[256];
				my_snprintf ( sError, sizeof(sError), "failed to resolve searchd host (name=%s)", sHost );

				my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), sError );
				SPH_RET(-1);
			}

#if MYSQL_VERSION_ID>=50515
			memcpy ( &sin.sin_addr, hp->ai_addr, Min ( sizeof(sin.sin_addr), (size_t)hp->ai_addrlen ) );
			freeaddrinfo ( hp );
#else
			memcpy ( &sin.sin_addr, hp->h_addr, Min ( sizeof(sin.sin_addr), (size_t)hp->h_length ) );
			my_gethostbyname_r_free();
#endif
		}
	} else
	{
#ifndef __WIN__
		iDomain = AF_UNIX;
		iSockaddrSize = sizeof(saun);
		pSockaddr = (struct sockaddr *) &saun;

		memset ( &saun, 0, sizeof(saun) );
		saun.sun_family = AF_UNIX;
		strncpy ( saun.sun_path, sHost, sizeof(saun.sun_path)-1 );
#else
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), "UNIX sockets are not supported on Windows" );
		SPH_RET(-1);
#endif
	}

	char sError[512];
	int iSocket = socket ( iDomain, SOCK_STREAM, 0 );

	if ( iSocket<0 )
	{
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), "failed to create client socket" );
		SPH_RET(-1);
	}

	if ( connect ( iSocket, pSockaddr, iSockaddrSize )<0 )
	{
		sphSockClose ( iSocket );
		my_snprintf ( sError, sizeof(sError), "failed to connect to searchd (host=%s, errno=%d, port=%d)",
			sHost, errno, (int)uPort );
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), sError );
		SPH_RET(-1);
	}

	return iSocket;
}


int ha_sphinx::ConnectAPI ( const char * sQueryHost, int iQueryPort )
{
	SPH_ENTER_METHOD();

	const char * sHost = ( sQueryHost && *sQueryHost ) ? sQueryHost : m_pShare->m_sHost;
	ushort uPort = iQueryPort ? (ushort)iQueryPort : m_pShare->m_iPort;

	int iSocket = Connect ( sHost, uPort );
	if ( iSocket<0 )
		SPH_RET ( iSocket );

	char sError[512];

	int version;
	if ( ::recv ( iSocket, (char *)&version, sizeof(version), 0 )!=sizeof(version) )
	{
		sphSockClose ( iSocket );
		my_snprintf ( sError, sizeof(sError), "failed to receive searchd version (host=%s, port=%d)",
			sHost, (int)uPort );
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), sError );
		SPH_RET(-1);
	}

	uint uClientVersion = htonl ( SPHINX_SEARCHD_PROTO );
	if ( ::send ( iSocket, (char*)&uClientVersion, sizeof(uClientVersion), 0 )!=sizeof(uClientVersion) )
	{
		sphSockClose ( iSocket );
		my_snprintf ( sError, sizeof(sError), "failed to send client version (host=%s, port=%d)",
			sHost, (int)uPort );
		my_error ( ER_CONNECT_TO_FOREIGN_DATA_SOURCE, MYF(0), sError );
		SPH_RET(-1);
	}

	SPH_RET ( iSocket );
}


// Closes a table. We call the free_share() function to free any resources
// that we have allocated in the "shared" structure.
//
// Called from sql_base.cc, sql_select.cc, and table.cc.
// In sql_select.cc it is only used to close up temporary tables or during
// the process where a temporary table is converted over to being a
// myisam table.
// For sql_base.cc look at close_data_tables().
int ha_sphinx::close()
{
	SPH_ENTER_METHOD();
	SPH_RET ( free_share ( m_pShare ) );
}


int ha_sphinx::HandleMysqlError ( MYSQL * pConn, int iErrCode )
{
	CSphSEThreadData * pTls = GetTls ();
	if ( pTls )
	{
		strncpy ( pTls->m_tStats.m_sLastMessage, mysql_error ( pConn ), sizeof ( pTls->m_tStats.m_sLastMessage ) );
		pTls->m_tStats.m_bLastError = true;
	}

	mysql_close ( pConn );

	my_error ( iErrCode, MYF(0), pTls->m_tStats.m_sLastMessage );
	return -1;
}


int ha_sphinx::extra ( enum ha_extra_function op )
{
	CSphSEThreadData * pTls = GetTls();
	if ( pTls )
	{
		if ( op==HA_EXTRA_WRITE_CAN_REPLACE )
			pTls->m_bReplace = true;
		else if ( op==HA_EXTRA_WRITE_CANNOT_REPLACE )
			pTls->m_bReplace = false;
	}
	return 0;
}


int ha_sphinx::write_row ( byte * )
{
	SPH_ENTER_METHOD();
	if ( !m_pShare || !m_pShare->m_bSphinxQL )
		SPH_RET ( HA_ERR_WRONG_COMMAND );

	// SphinxQL inserts only, pretty much similar to abandoned federated
	char sQueryBuf[1024];
	char sValueBuf[1024];

	String sQuery ( sQueryBuf, sizeof(sQueryBuf), &my_charset_bin );
	String sValue ( sValueBuf, sizeof(sQueryBuf), &my_charset_bin );
	sQuery.length ( 0 );
	sValue.length ( 0 );

	CSphSEThreadData * pTls = GetTls ();
	sQuery.append ( pTls && pTls->m_bReplace ? "REPLACE INTO " : "INSERT INTO " );
	sQuery.append ( m_pShare->m_sIndex );
	sQuery.append ( " (" );

	for ( Field ** ppField = table->field; *ppField; ppField++ )
	{
		sQuery.append ( (*ppField)->field_name );
		if ( ppField[1] )
			sQuery.append ( ", " );
	}
	sQuery.append ( ") VALUES (" );

	for ( Field ** ppField = table->field; *ppField; ppField++ )
	{
		if ( (*ppField)->is_null() )
		{
			sQuery.append ( "''" );

		} else
		{
			if ( (*ppField)->type()==MYSQL_TYPE_TIMESTAMP )
			{
				Item_field * pWrap = new Item_field ( *ppField ); // autofreed by query arena, I assume
				Item_func_unix_timestamp * pConv = new Item_func_unix_timestamp ( pWrap );
				pConv->quick_fix_field();
				unsigned int uTs = (unsigned int) pConv->val_int();

				snprintf ( sValueBuf, sizeof(sValueBuf), "'%u'", uTs );
				sQuery.append ( sValueBuf );

			} else
			{
				(*ppField)->val_str ( &sValue );
				sQuery.append ( "'" );
				sValue.print ( &sQuery );
				sQuery.append ( "'" );
				sValue.length(0);
			}
		}

		if ( ppField[1] )
			sQuery.append ( ", " );
	}
	sQuery.append ( ")" );

	// FIXME? pretty inefficient to reconnect every time under high load,
	// but this was intentionally written for a low load scenario..
	MYSQL * pConn = mysql_init ( NULL );
	if ( !pConn )
		SPH_RET ( ER_OUT_OF_RESOURCES );

	unsigned int uTimeout = 1;
	mysql_options ( pConn, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&uTimeout );

	if ( !mysql_real_connect ( pConn, m_pShare->m_sHost, "root", "", "", m_pShare->m_iPort, m_pShare->m_sSocket, 0 ) )
		SPH_RET ( HandleMysqlError ( pConn, ER_CONNECT_TO_FOREIGN_DATA_SOURCE ) );

	if ( mysql_real_query ( pConn, sQuery.ptr(), sQuery.length() ) )
		SPH_RET ( HandleMysqlError ( pConn, ER_QUERY_ON_FOREIGN_DATA_SOURCE ) );

	// all ok!
	mysql_close ( pConn );
	SPH_RET(0);
}


static inline bool IsIntegerFieldType ( enum_field_types eType )
{
	return eType==MYSQL_TYPE_LONG || eType==MYSQL_TYPE_LONGLONG;
}


static inline bool IsIDField ( Field * pField )
{
	enum_field_types eType = pField->type();

	if ( eType==MYSQL_TYPE_LONGLONG )
		return true;

	if ( eType==MYSQL_TYPE_LONG && ((Field_num*)pField)->unsigned_flag )
		return true;

	return false;
}


int ha_sphinx::delete_row ( const byte * )
{
	SPH_ENTER_METHOD();
	if ( !m_pShare || !m_pShare->m_bSphinxQL )
		SPH_RET ( HA_ERR_WRONG_COMMAND );

	char sQueryBuf[1024];
	String sQuery ( sQueryBuf, sizeof(sQueryBuf), &my_charset_bin );
	sQuery.length ( 0 );

	sQuery.append ( "DELETE FROM " );
	sQuery.append ( m_pShare->m_sIndex );
	sQuery.append ( " WHERE id=" );

	char sValue[32];
	snprintf ( sValue, sizeof(sValue), "%lld", table->field[0]->val_int() );
	sQuery.append ( sValue );

	// FIXME? pretty inefficient to reconnect every time under high load,
	// but this was intentionally written for a low load scenario..
	MYSQL * pConn = mysql_init ( NULL );
	if ( !pConn )
		SPH_RET ( ER_OUT_OF_RESOURCES );

	unsigned int uTimeout = 1;
	mysql_options ( pConn, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&uTimeout );

	if ( !mysql_real_connect ( pConn, m_pShare->m_sHost, "root", "", "", m_pShare->m_iPort, m_pShare->m_sSocket, 0 ) )
		SPH_RET ( HandleMysqlError ( pConn, ER_CONNECT_TO_FOREIGN_DATA_SOURCE ) );

	if ( mysql_real_query ( pConn, sQuery.ptr(), sQuery.length() ) )
		SPH_RET ( HandleMysqlError ( pConn, ER_QUERY_ON_FOREIGN_DATA_SOURCE ) );

	// all ok!
	mysql_close ( pConn );
	SPH_RET(0);
}


int ha_sphinx::update_row ( const byte *, byte * )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_WRONG_COMMAND );
}


// keynr is key (index) number
// sorted is 1 if result MUST be sorted according to index
int ha_sphinx::index_init ( uint keynr, bool )
{
	SPH_ENTER_METHOD();
	active_index = keynr;

	CSphSEThreadData * pTls = GetTls();
	if ( pTls )
		pTls->m_bCondDone = false;

	SPH_RET(0);
}


int ha_sphinx::index_end()
{
	SPH_ENTER_METHOD();
	SPH_RET(0);
}


bool ha_sphinx::CheckResponcePtr ( int iLen )
{
	if ( m_pCur+iLen>m_pResponseEnd )
	{
		m_pCur = m_pResponseEnd;
		m_bUnpackError = true;
		return false;
	}

	return true;
}


uint32 ha_sphinx::UnpackDword ()
{
	if ( !CheckResponcePtr ( sizeof(uint32) ) ) // NOLINT
	{
		return 0;
	}

	uint32 uRes = ntohl ( sphUnalignedRead ( *(uint32*)m_pCur ) );
	m_pCur += sizeof(uint32); // NOLINT
	return uRes;
}


char * ha_sphinx::UnpackString ()
{
	uint32 iLen = UnpackDword ();
	if ( !iLen )
		return NULL;

	if ( !CheckResponcePtr ( iLen ) )
	{
		return NULL;
	}

	char * sRes = new char [ 1+iLen ];
	memcpy ( sRes, m_pCur, iLen );
	sRes[iLen] = '\0';
	m_pCur += iLen;
	return sRes;
}


static inline const char * FixNull ( const char * s )
{
	return s ? s : "(null)";
}


bool ha_sphinx::UnpackSchema ()
{
	SPH_ENTER_METHOD();

	// cleanup
	if ( m_dFields )
		for ( int i=0; i<(int)m_iFields; i++ )
			SafeDeleteArray ( m_dFields[i] );
	SafeDeleteArray ( m_dFields );

	// unpack network packet
	uint32 uStatus = UnpackDword ();
	char * sMessage = NULL;

	if ( uStatus!=SEARCHD_OK )
	{
		sMessage = UnpackString ();
		CSphSEThreadData * pTls = GetTls ();
		if ( pTls )
		{
			strncpy ( pTls->m_tStats.m_sLastMessage, sMessage, sizeof(pTls->m_tStats.m_sLastMessage) );
			pTls->m_tStats.m_bLastError = ( uStatus==SEARCHD_ERROR );
		}

		if ( uStatus==SEARCHD_ERROR )
		{
			char sError[1024];
			my_snprintf ( sError, sizeof(sError), "searchd error: %s", sMessage );
			my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), sError );
			SafeDeleteArray ( sMessage );
			SPH_RET ( false );
		}
	}

	m_iFields = UnpackDword ();
	m_dFields = new char * [ m_iFields ];
	if ( !m_dFields )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: UnpackSchema() failed (fields alloc error)" );
		SPH_RET(false);
	}

	for ( uint32 i=0; i<m_iFields; i++ )
		m_dFields[i] = UnpackString ();

	SafeDeleteArray ( m_dAttrs );
	m_iAttrs = UnpackDword ();
	m_dAttrs = new CSphSEAttr [ m_iAttrs ];
	if ( !m_dAttrs )
	{
		for ( int i=0; i<(int)m_iFields; i++ )
			SafeDeleteArray ( m_dFields[i] );
		SafeDeleteArray ( m_dFields );
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: UnpackSchema() failed (attrs alloc error)" );
		SPH_RET(false);
	}

	for ( uint32 i=0; i<m_iAttrs; i++ )
	{
		m_dAttrs[i].m_sName = UnpackString ();
		m_dAttrs[i].m_uType = UnpackDword ();
		if ( m_bUnpackError ) // m_sName may be null
			break;

		m_dAttrs[i].m_iField = -1;
		for ( int j=SPHINXSE_SYSTEM_COLUMNS; j<m_pShare->m_iTableFields; j++ )
		{
			const char * sTableField = m_pShare->m_sTableField[j];
			const char * sAttrField = m_dAttrs[i].m_sName;
			if ( m_dAttrs[i].m_sName[0]=='@' )
			{
				const char * sAtPrefix = "_sph_";
				if ( strncmp ( sTableField, sAtPrefix, strlen(sAtPrefix) ) )
					continue;
				sTableField += strlen(sAtPrefix);
				sAttrField++;
			}

			if ( !strcasecmp ( sAttrField, sTableField ) )
			{
				// we're almost good, but
				// let's enforce that timestamp columns can only receive timestamp attributes
				if ( m_pShare->m_eTableFieldType[j]!=MYSQL_TYPE_TIMESTAMP || m_dAttrs[i].m_uType==SPH_ATTR_TIMESTAMP )
					m_dAttrs[i].m_iField = j;
				break;
			}
		}
	}

	m_iMatchesTotal = UnpackDword ();

	m_bId64 = UnpackDword ();
	if ( m_bId64 && m_pShare->m_eTableFieldType[0]!=MYSQL_TYPE_LONGLONG )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: 1st column must be bigint to accept 64-bit DOCIDs" );
		SPH_RET(false);
	}

	// network packet unpacked; build unbound fields map
	SafeDeleteArray ( m_dUnboundFields );
	m_dUnboundFields = new int [ m_pShare->m_iTableFields ];

	for ( int i=0; i<m_pShare->m_iTableFields; i++ )
	{
		if ( i<SPHINXSE_SYSTEM_COLUMNS )
			m_dUnboundFields[i] = SPH_ATTR_NONE;

		else if ( m_pShare->m_eTableFieldType[i]==MYSQL_TYPE_TIMESTAMP )
			m_dUnboundFields[i] = SPH_ATTR_TIMESTAMP;

		else
			m_dUnboundFields[i] = SPH_ATTR_INTEGER;
	}

	for ( uint32 i=0; i<m_iAttrs; i++ )
		if ( m_dAttrs[i].m_iField>=0 )
			m_dUnboundFields [ m_dAttrs[i].m_iField ] = SPH_ATTR_NONE;

	if ( m_bUnpackError )
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: UnpackSchema() failed (unpack error)" );

	SPH_RET ( !m_bUnpackError );
}


bool ha_sphinx::UnpackStats ( CSphSEStats * pStats )
{
	assert ( pStats );

	char * pCurSave = m_pCur;
	for ( uint i=0; i<m_iMatchesTotal && m_pCur<m_pResponseEnd-sizeof(uint32); i++ ) // NOLINT
	{
		m_pCur += m_bId64 ? 12 : 8; // skip id+weight
		for ( uint32 i=0; i<m_iAttrs && m_pCur<m_pResponseEnd-sizeof(uint32); i++ ) // NOLINT
		{
			if ( m_dAttrs[i].m_uType==SPH_ATTR_UINT32SET || m_dAttrs[i].m_uType==SPH_ATTR_UINT64SET )
			{
				// skip MVA list
				uint32 uCount = UnpackDword ();
				m_pCur += uCount*4;
			} else if ( m_dAttrs[i].m_uType==SPH_ATTR_STRING )
			{
				uint32 iLen = UnpackDword();
				m_pCur += iLen;
			} else // skip normal value
				m_pCur += m_dAttrs[i].m_uType==SPH_ATTR_BIGINT ? 8 : 4;
		}
	}

	pStats->m_iMatchesTotal = UnpackDword ();
	pStats->m_iMatchesFound = UnpackDword ();
	pStats->m_iQueryMsec = UnpackDword ();
	pStats->m_iWords = UnpackDword ();

	if ( m_bUnpackError )
		return false;

	if ( pStats->m_iWords<0 || pStats->m_iWords>=SPHINXSE_MAX_KEYWORDSTATS )
		return false;

	SafeDeleteArray ( pStats->m_dWords );
	pStats->m_dWords = new CSphSEWordStats [ pStats->m_iWords ];
	if ( !pStats->m_dWords )
		return false;

	for ( int i=0; i<pStats->m_iWords; i++ )
	{
		CSphSEWordStats & tWord = pStats->m_dWords[i];
		tWord.m_sWord = UnpackString ();
		tWord.m_iDocs = UnpackDword ();
		tWord.m_iHits = UnpackDword ();
	}

	if ( m_bUnpackError )
		return false;

	m_pCur = pCurSave;
	return true;
}


/// condition pushdown implementation, to properly intercept WHERE clauses on my columns
const COND * ha_sphinx::cond_push ( const COND * cond )
{
	// catch the simplest case: query_column="some text"
	for ( ;; )
	{
		if ( cond->type()!=COND::FUNC_ITEM )
			break;

		Item_func * condf = (Item_func *)cond;
		if ( condf->functype()!=Item_func::EQ_FUNC || condf->argument_count()!=2 )
			break;

		// get my tls
		CSphSEThreadData * pTls = GetTls ();
		if ( !pTls )
			break;

		Item ** args = condf->arguments();
		if ( !m_pShare->m_bSphinxQL )
		{
			// on non-QL tables, intercept query=value condition for SELECT
			if (!( args[0]->type()==COND::FIELD_ITEM && args[1]->type()==COND::STRING_ITEM ))
				break;

			Item_field * pField = (Item_field *) args[0];
			if ( pField->field->field_index!=2 ) // FIXME! magic key index
				break;

			// copy the query, and let know that we intercepted this condition
			Item_string * pString = (Item_string *) args[1];
			pTls->m_bQuery = true;
			strncpy ( pTls->m_sQuery, pString->str_value.c_ptr(), sizeof(pTls->m_sQuery) );
			pTls->m_sQuery[sizeof(pTls->m_sQuery)-1] = '\0';
			pTls->m_pQueryCharset = pString->str_value.charset();

		} else
		{
			if (!( args[0]->type()==COND::FIELD_ITEM && args[1]->type()==COND::INT_ITEM ))
				break;

			// on QL tables, intercept id=value condition for DELETE
			Item_field * pField = (Item_field *) args[0];
			if ( pField->field->field_index!=0 ) // FIXME! magic key index
				break;

			Item_int * pVal = (Item_int *) args[1];
			pTls->m_iCondId = pVal->val_int();
			pTls->m_bCondId = true;
		}

		// we intercepted this condition
		return NULL;
	}

	// don't change anything
	return cond;
}


/// condition popup
void ha_sphinx::cond_pop ()
{
	CSphSEThreadData * pTls = GetTls ();
	if ( pTls )
		pTls->m_bQuery = false;
}


/// get TLS (maybe allocate it, too)
CSphSEThreadData * ha_sphinx::GetTls()
{
	// where do we store that pointer in today's version?
	CSphSEThreadData ** ppTls;
#if MYSQL_VERSION_ID>50100
	ppTls = (CSphSEThreadData**) thd_ha_data ( table->in_use, ht );
#else
	ppTls = (CSphSEThreadData**) &current_thd->ha_data[sphinx_hton.slot];
#endif // >50100

	// allocate if needed
	if ( !*ppTls )
		*ppTls = new CSphSEThreadData ();

	// errors will be handled by caller
	return *ppTls;
}


// Positions an index cursor to the index specified in the handle. Fetches the
// row if available. If the key value is null, begin at the first key of the
// index.
int ha_sphinx::index_read ( byte * buf, const byte * key, uint key_len, enum ha_rkey_function )
{
	SPH_ENTER_METHOD();
	char sError[256];

	// set new data for thd->ha_data, it is used in show_status
	CSphSEThreadData * pTls = GetTls();
	if ( !pTls )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: TLS malloc() failed" );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}
	pTls->m_tStats.Reset ();

	// sphinxql table, just return the key once
	if ( m_pShare->m_bSphinxQL )
	{
		// over and out
		if ( pTls->m_bCondDone )
			SPH_RET ( HA_ERR_END_OF_FILE );

		// return a value from pushdown, if any
		if ( pTls->m_bCondId )
		{
			table->field[0]->store ( pTls->m_iCondId, 1 );
			pTls->m_bCondDone = true;
			SPH_RET(0);
		}

		// return a value from key
		longlong iRef = 0;
		if ( key_len==4 )
			iRef = uint4korr ( key );
		else if ( key_len==8 )
			iRef = uint8korr ( key );
		else
		{
			my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: unexpected key length" );
			SPH_RET ( HA_ERR_END_OF_FILE );
		}

		table->field[0]->store ( iRef, 1 );
		pTls->m_bCondDone = true;
		SPH_RET(0);
	}

	// parse query
	if ( pTls->m_bQuery )
	{
		// we have a query from condition pushdown
		m_pCurrentKey = (const byte *) pTls->m_sQuery;
		m_iCurrentKeyLen = strlen(pTls->m_sQuery);
	} else
	{
		// just use the key (might be truncated)
		m_pCurrentKey = key+HA_KEY_BLOB_LENGTH;
		m_iCurrentKeyLen = uint2korr(key); // or maybe key_len?
		pTls->m_pQueryCharset = m_pShare ? m_pShare->m_pTableQueryCharset : NULL;
	}

	CSphSEQuery q ( (const char*)m_pCurrentKey, m_iCurrentKeyLen, m_pShare->m_sIndex );
	if ( !q.Parse () )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), q.m_sParseError );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	// do connect
	int iSocket = ConnectAPI ( q.m_sHost, q.m_iPort );
	if ( iSocket<0 )
		SPH_RET ( HA_ERR_END_OF_FILE );

	// my buffer
	char * pBuffer; // will be free by CSphSEQuery dtor; do NOT free manually
	int iReqLen = q.BuildRequest ( &pBuffer );

	if ( iReqLen<=0 )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: q.BuildRequest() failed" );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	// send request
	::send ( iSocket, pBuffer, iReqLen, 0 );

	// receive reply
	char sHeader[8];
	int iGot = ::recv ( iSocket, sHeader, sizeof(sHeader), RECV_FLAGS );
	if ( iGot!=sizeof(sHeader) )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "failed to receive response header (searchd went away?)" );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	short int uRespStatus = ntohs ( sphUnalignedRead ( *(short int*)( &sHeader[0] ) ) );
	short int uRespVersion = ntohs ( sphUnalignedRead ( *(short int*)( &sHeader[2] ) ) );
	uint uRespLength = ntohl ( sphUnalignedRead ( *(uint *)( &sHeader[4] ) ) );
	SPH_DEBUG ( "got response header (status=%d version=%d length=%d)",
		uRespStatus, uRespVersion, uRespLength );

	SafeDeleteArray ( m_pResponse );
	if ( uRespLength<=SPHINXSE_MAX_ALLOC )
		m_pResponse = new char [ uRespLength+1 ];

	if ( !m_pResponse )
	{
		my_snprintf ( sError, sizeof(sError), "bad searchd response length (length=%u)", uRespLength );
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), sError );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	int iRecvLength = 0;
	while ( iRecvLength<(int)uRespLength )
	{
		int iRecv = ::recv ( iSocket, m_pResponse+iRecvLength, uRespLength-iRecvLength, RECV_FLAGS );
		if ( iRecv<0 )
			break;
		iRecvLength += iRecv;
	}

	::closesocket ( iSocket );
	iSocket = -1;

	if ( iRecvLength!=(int)uRespLength )
	{
		my_snprintf ( sError, sizeof(sError), "net read error (expected=%d, got=%d)", uRespLength, iRecvLength );
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), sError );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	// we'll have a message, at least
	pTls->m_bStats = true;

	// parse reply
	m_iCurrentPos = 0;
	m_pCur = m_pResponse;
	m_pResponseEnd = m_pResponse + uRespLength;
	m_bUnpackError = false;

	if ( uRespStatus!=SEARCHD_OK )
	{
		char * sMessage = UnpackString ();
		if ( !sMessage )
		{
			my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "no valid response from searchd (status=%d, resplen=%d)",
				uRespStatus, uRespLength );
			SPH_RET ( HA_ERR_END_OF_FILE );
		}

		strncpy ( pTls->m_tStats.m_sLastMessage, sMessage, sizeof(pTls->m_tStats.m_sLastMessage) );
		SafeDeleteArray ( sMessage );

		if ( uRespStatus!=SEARCHD_WARNING )
		{
			my_snprintf ( sError, sizeof(sError), "searchd error: %s", pTls->m_tStats.m_sLastMessage );
			my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), sError );

			pTls->m_tStats.m_bLastError = true;
			SPH_RET ( HA_ERR_END_OF_FILE );
		}
	}

	if ( !UnpackSchema () )
		SPH_RET ( HA_ERR_END_OF_FILE );

	if ( !UnpackStats ( &pTls->m_tStats ) )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: UnpackStats() failed" );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	SPH_RET ( get_rec ( buf, key, key_len ) );
}


// Positions an index cursor to the index specified in key. Fetches the
// row if any. This is only used to read whole keys.
int ha_sphinx::index_read_idx ( byte *, uint, const byte *, uint, enum ha_rkey_function )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_WRONG_COMMAND );
}


// Used to read forward through the index.
int ha_sphinx::index_next ( byte * buf )
{
	SPH_ENTER_METHOD();
	SPH_RET ( get_rec ( buf, m_pCurrentKey, m_iCurrentKeyLen ) );
}


int ha_sphinx::index_next_same ( byte * buf, const byte * key, uint keylen )
{
	SPH_ENTER_METHOD();
	SPH_RET ( get_rec ( buf, key, keylen ) );
}


int ha_sphinx::get_rec ( byte * buf, const byte *, uint )
{
	SPH_ENTER_METHOD();

	if ( m_iCurrentPos>=m_iMatchesTotal )
	{
		SafeDeleteArray ( m_pResponse );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	#if MYSQL_VERSION_ID>50100
	my_bitmap_map * org_bitmap = dbug_tmp_use_all_columns ( table, table->write_set );
	#endif
	Field ** field = table->field;

	// unpack and return the match
	longlong uMatchID = UnpackDword ();
	if ( m_bId64 )
		uMatchID = ( uMatchID<<32 ) + UnpackDword();
	uint32 uMatchWeight = UnpackDword ();

	field[0]->store ( uMatchID, 1 );
	field[1]->store ( uMatchWeight, 1 );
	field[2]->store ( (const char*)m_pCurrentKey, m_iCurrentKeyLen, &my_charset_bin );

	for ( uint32 i=0; i<m_iAttrs; i++ )
	{
		longlong iValue64 = 0;
		uint32 uValue = UnpackDword ();
		if ( m_dAttrs[i].m_uType==SPH_ATTR_BIGINT )
			iValue64 = ( (longlong)uValue<<32 ) | UnpackDword();
		if ( m_dAttrs[i].m_iField<0 )
		{
			// skip MVA or String
			if ( m_dAttrs[i].m_uType==SPH_ATTR_UINT32SET || m_dAttrs[i].m_uType==SPH_ATTR_UINT64SET )
			{
				for ( ; uValue>0 && !m_bUnpackError; uValue-- )
					UnpackDword();
			} else if ( m_dAttrs[i].m_uType==SPH_ATTR_STRING && CheckResponcePtr ( uValue ) )
			{
				m_pCur += uValue;
			}
			continue;
		}

		Field * af = field [ m_dAttrs[i].m_iField ];
		switch ( m_dAttrs[i].m_uType )
		{
			case SPH_ATTR_INTEGER:
			case SPH_ATTR_ORDINAL:
			case SPH_ATTR_BOOL:
				af->store ( uValue, 1 );
				break;

			case SPH_ATTR_FLOAT:
				af->store ( sphDW2F(uValue) );
				break;

			case SPH_ATTR_TIMESTAMP:
				if ( af->type()==MYSQL_TYPE_TIMESTAMP )
					longstore ( af->ptr, uValue ); // because store() does not accept timestamps
				else
					af->store ( uValue, 1 );
				break;

			case SPH_ATTR_BIGINT:
				af->store ( iValue64, 0 );
				break;

			case SPH_ATTR_STRING:
				if ( !uValue )
					af->store ( "", 0, &my_charset_bin );
				else if ( CheckResponcePtr ( uValue ) )
				{
					af->store ( m_pCur, uValue, &my_charset_bin );
					m_pCur += uValue;
				}
				break;

			case SPH_ATTR_UINT64SET:
			case SPH_ATTR_UINT32SET :
				if ( uValue<=0 )
				{
					// shortcut, empty MVA set
					af->store ( "", 0, &my_charset_bin );

				} else
				{
					// convert MVA set to comma-separated string
					char sBuf[1024]; // FIXME! magic size
					char * pCur = sBuf;

					if ( m_dAttrs[i].m_uType==SPH_ATTR_UINT32SET )
					{
						for ( ; uValue>0 && !m_bUnpackError; uValue-- )
						{
							uint32 uEntry = UnpackDword ();
							if ( pCur < sBuf+sizeof(sBuf)-16 ) // 10 chars per 32bit value plus some safety bytes
							{
								snprintf ( pCur, sBuf+sizeof(sBuf)-pCur, "%u", uEntry );
								while ( *pCur ) pCur++;
								if ( uValue>1 )
									*pCur++ = ','; // non-trailing commas
							}
						}
					} else
					{
						for ( ; uValue>0 && !m_bUnpackError; uValue-=2 )
						{
							uint32 uEntryLo = UnpackDword ();
							uint32 uEntryHi = UnpackDword();
							if ( pCur < sBuf+sizeof(sBuf)-24 ) // 20 chars per 64bit value plus some safety bytes
							{
								snprintf ( pCur, sBuf+sizeof(sBuf)-pCur, "%u%u", uEntryHi, uEntryLo );
								while ( *pCur ) pCur++;
								if ( uValue>2 )
									*pCur++ = ','; // non-trailing commas
							}
						}
					}

					af->store ( sBuf, pCur-sBuf, &my_charset_bin );
				}
				break;

			default:
				my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: unhandled attr type" );
				SafeDeleteArray ( m_pResponse );
				SPH_RET ( HA_ERR_END_OF_FILE );
		}
	}

	if ( m_bUnpackError )
	{
		my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0), "INTERNAL ERROR: response unpacker failed" );
		SafeDeleteArray ( m_pResponse );
		SPH_RET ( HA_ERR_END_OF_FILE );
	}

	// zero out unmapped fields
	for ( int i=SPHINXSE_SYSTEM_COLUMNS; i<(int)table->s->fields; i++ )
		if ( m_dUnboundFields[i]!=SPH_ATTR_NONE )
			switch ( m_dUnboundFields[i] )
	{
		case SPH_ATTR_INTEGER:		table->field[i]->store ( 0, 1 ); break;
		case SPH_ATTR_TIMESTAMP:	longstore ( table->field[i]->ptr, 0 ); break;
		default:
			my_error ( ER_QUERY_ON_FOREIGN_DATA_SOURCE, MYF(0),
				"INTERNAL ERROR: unhandled unbound field type %d", m_dUnboundFields[i] );
			SafeDeleteArray ( m_pResponse );
			SPH_RET ( HA_ERR_END_OF_FILE );
	}

	memset ( buf, 0, table->s->null_bytes );
	m_iCurrentPos++;

	#if MYSQL_VERSION_ID > 50100
	dbug_tmp_restore_column_map ( table->write_set, org_bitmap );
	#endif

	SPH_RET(0);
}


// Used to read backwards through the index.
int ha_sphinx::index_prev ( byte * )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_WRONG_COMMAND );
}


// index_first() asks for the first key in the index.
//
// Called from opt_range.cc, opt_sum.cc, sql_handler.cc,
// and sql_select.cc.
int ha_sphinx::index_first ( byte * )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_END_OF_FILE );
}

// index_last() asks for the last key in the index.
//
// Called from opt_range.cc, opt_sum.cc, sql_handler.cc,
// and sql_select.cc.
int ha_sphinx::index_last ( byte * )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_WRONG_COMMAND );
}


int ha_sphinx::rnd_init ( bool )
{
	SPH_ENTER_METHOD();
	SPH_RET(0);
}


int ha_sphinx::rnd_end()
{
	SPH_ENTER_METHOD();
	SPH_RET(0);
}


int ha_sphinx::rnd_next ( byte * )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_END_OF_FILE );
}


void ha_sphinx::position ( const byte * )
{
	SPH_ENTER_METHOD();
	SPH_VOID_RET();
}


// This is like rnd_next, but you are given a position to use
// to determine the row. The position will be of the type that you stored in
// ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
// or position you saved when position() was called.
// Called from filesort.cc records.cc sql_insert.cc sql_select.cc sql_update.cc.
int ha_sphinx::rnd_pos ( byte *, byte * )
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_WRONG_COMMAND );
}


#if MYSQL_VERSION_ID>=50030
int ha_sphinx::info ( uint )
#else
void ha_sphinx::info ( uint )
#endif
{
	SPH_ENTER_METHOD();

	if ( table->s->keys>0 )
		table->key_info[0].rec_per_key[0] = 1;

	#if MYSQL_VERSION_ID>50100
	stats.records = 20;
	#else
	records = 20;
	#endif

#if MYSQL_VERSION_ID>=50030
	SPH_RET(0);
#else
	SPH_VOID_RET();
#endif
}


int ha_sphinx::reset ()
{
	SPH_ENTER_METHOD();
	CSphSEThreadData * pTls = GetTls ();
	if ( pTls )
		pTls->m_bQuery = false;
	SPH_RET(0);
}


int ha_sphinx::delete_all_rows()
{
	SPH_ENTER_METHOD();
	SPH_RET ( HA_ERR_WRONG_COMMAND );
}


// First you should go read the section "locking functions for mysql" in
// lock.cc to understand this.
// This create a lock on the table. If you are implementing a storage engine
// that can handle transacations look at ha_berkely.cc to see how you will
// want to go about doing this. Otherwise you should consider calling flock()
// here.
//
// Called from lock.cc by lock_external() and unlock_external(). Also called
// from sql_table.cc by copy_data_between_tables().
int ha_sphinx::external_lock ( THD *, int )
{
	SPH_ENTER_METHOD();
	SPH_RET(0);
}


THR_LOCK_DATA ** ha_sphinx::store_lock ( THD *, THR_LOCK_DATA ** to,
	enum thr_lock_type lock_type )
{
	SPH_ENTER_METHOD();

	if ( lock_type!=TL_IGNORE && m_tLock.type==TL_UNLOCK )
		m_tLock.type = lock_type;

	*to++ = &m_tLock;
	SPH_RET(to);
}


int ha_sphinx::delete_table ( const char * )
{
	SPH_ENTER_METHOD();
	SPH_RET(0);
}


// Renames a table from one name to another from alter table call.
//
// If you do not implement this, the default rename_table() is called from
// handler.cc and it will delete all files with the file extentions returned
// by bas_ext().
//
// Called from sql_table.cc by mysql_rename_table().
int ha_sphinx::rename_table ( const char *, const char * )
{
	SPH_ENTER_METHOD();
	SPH_RET(0);
}


// Given a starting key, and an ending key estimate the number of rows that
// will exist between the two. end_key may be empty which in case determine
// if start_key matches any rows.
//
// Called from opt_range.cc by check_quick_keys().
ha_rows ha_sphinx::records_in_range ( uint, key_range *, key_range * )
{
	SPH_ENTER_METHOD();
	SPH_RET(3); // low number to force index usage
}


// create() is called to create a database. The variable name will have the name
// of the table. When create() is called you do not need to worry about opening
// the table. Also, the FRM file will have already been created so adjusting
// create_info will not do you any good. You can overwrite the frm file at this
// point if you wish to change the table definition, but there are no methods
// currently provided for doing that.
//
// Called from handle.cc by ha_create_table().
int ha_sphinx::create ( const char * name, TABLE * table, HA_CREATE_INFO * )
{
	SPH_ENTER_METHOD();
	char sError[256];

	CSphSEShare tInfo;
	if ( !ParseUrl ( &tInfo, table, true ) )
		SPH_RET(-1);

	// check SphinxAPI table
	for ( ; !tInfo.m_bSphinxQL; )
	{
		// check system fields (count and types)
		if ( table->s->fields<SPHINXSE_SYSTEM_COLUMNS )
		{
			my_snprintf ( sError, sizeof(sError), "%s: there MUST be at least %d columns",
				name, SPHINXSE_SYSTEM_COLUMNS );
			break;
		}

		if ( !IsIDField ( table->field[0] ) )
		{
			my_snprintf ( sError, sizeof(sError), "%s: 1st column (docid) MUST be unsigned integer or bigint", name );
			break;
		}

		if ( !IsIntegerFieldType ( table->field[1]->type() ) )
		{
			my_snprintf ( sError, sizeof(sError), "%s: 2nd column (weight) MUST be integer or bigint", name );
			break;
		}

		enum_field_types f2 = table->field[2]->type();
		if ( f2!=MYSQL_TYPE_VARCHAR
			&& f2!=MYSQL_TYPE_BLOB && f2!=MYSQL_TYPE_MEDIUM_BLOB && f2!=MYSQL_TYPE_LONG_BLOB && f2!=MYSQL_TYPE_TINY_BLOB )
		{
			my_snprintf ( sError, sizeof(sError), "%s: 3rd column (search query) MUST be varchar or text", name );
			break;
		}

		// check attributes
		int i;
		for ( i=3; i<(int)table->s->fields; i++ )
		{
			enum_field_types eType = table->field[i]->type();
			if ( eType!=MYSQL_TYPE_TIMESTAMP && !IsIntegerFieldType(eType) && eType!=MYSQL_TYPE_VARCHAR && eType!=MYSQL_TYPE_FLOAT )
			{
				my_snprintf ( sError, sizeof(sError), "%s: %dth column (attribute %s) MUST be integer, bigint, timestamp, varchar, or float",
					name, i+1, table->field[i]->field_name );
				break;
			}
		}

		if ( i!=(int)table->s->fields )
			break;

		// check index
		if (
			table->s->keys!=1 ||
			table->key_info[0].key_parts!=1 ||
			strcasecmp ( table->key_info[0].key_part[0].field->field_name, table->field[2]->field_name ) )
		{
			my_snprintf ( sError, sizeof(sError), "%s: there must be an index on '%s' column",
				name, table->field[2]->field_name );
			break;
		}

		// all good
		sError[0] = '\0';
		break;
	}

	// check SphinxQL table
	for ( ; tInfo.m_bSphinxQL; )
	{
		sError[0] = '\0';

		// check that 1st column is id, is of int type, and has an index
		if ( strcmp ( table->field[0]->field_name, "id" ) )
		{
			my_snprintf ( sError, sizeof(sError), "%s: 1st column must be called 'id'", name );
			break;
		}

		if ( !IsIDField ( table->field[0] ) )
		{
			my_snprintf ( sError, sizeof(sError), "%s: 'id' column must be INT UNSIGNED or BIGINT", name );
			break;
		}

		// check index
		if (
			table->s->keys!=1 ||
			table->key_info[0].key_parts!=1 ||
			strcasecmp ( table->key_info[0].key_part[0].field->field_name, "id" ) )
		{
			my_snprintf ( sError, sizeof(sError), "%s: 'id' column must be indexed", name );
			break;
		}

		// check column types
		for ( int i=1; i<(int)table->s->fields; i++ )
		{
			enum_field_types eType = table->field[i]->type();
			if ( eType!=MYSQL_TYPE_TIMESTAMP && !IsIntegerFieldType(eType) && eType!=MYSQL_TYPE_VARCHAR && eType!=MYSQL_TYPE_FLOAT )
			{
				my_snprintf ( sError, sizeof(sError), "%s: column %d(%s) is of unsupported type (use int/bigint/timestamp/varchar/float)",
					name, i+1, table->field[i]->field_name );
				break;
			}
		}
		if ( sError[0] )
			break;

		// all good
		break;
	}

	// report and bail
	if ( sError[0] )
	{
		my_error ( ER_CANT_CREATE_TABLE, MYF(0), sError, -1 );
		SPH_RET(-1);
	}

	SPH_RET(0);
}

// show functions

#if MYSQL_VERSION_ID<50100
#define SHOW_VAR_FUNC_BUFF_SIZE 1024
#endif

CSphSEStats * sphinx_get_stats ( THD * thd, SHOW_VAR * out )
{
#if MYSQL_VERSION_ID>50100
	if ( sphinx_hton_ptr )
	{
		CSphSEThreadData *pTls = (CSphSEThreadData *) *thd_ha_data ( thd, sphinx_hton_ptr );

		if ( pTls && pTls->m_bStats )
			return &pTls->m_tStats;
	}
#else
	CSphSEThreadData *pTls = (CSphSEThreadData *) thd->ha_data[sphinx_hton.slot];
	if ( pTls && pTls->m_bStats )
		return &pTls->m_tStats;
#endif

	out->type = SHOW_CHAR;
	out->value = (char*) "";
	return 0;
}

int sphinx_showfunc_total ( THD * thd, SHOW_VAR * out, char * )
{
	CSphSEStats * pStats = sphinx_get_stats ( thd, out );
	if ( pStats )
	{
		out->type = SHOW_INT;
		out->value = (char *) &pStats->m_iMatchesTotal;
	}
	return 0;
}

int sphinx_showfunc_total_found ( THD * thd, SHOW_VAR * out, char * )
{
	CSphSEStats * pStats = sphinx_get_stats ( thd, out );
	if ( pStats )
	{
		out->type = SHOW_INT;
		out->value = (char *) &pStats->m_iMatchesFound;
	}
	return 0;
}

int sphinx_showfunc_time ( THD * thd, SHOW_VAR * out, char * )
{
	CSphSEStats * pStats = sphinx_get_stats ( thd, out );
	if ( pStats )
	{
		out->type = SHOW_INT;
		out->value = (char *) &pStats->m_iQueryMsec;
	}
	return 0;
}

int sphinx_showfunc_word_count ( THD * thd, SHOW_VAR * out, char * )
{
	CSphSEStats * pStats = sphinx_get_stats ( thd, out );
	if ( pStats )
	{
		out->type = SHOW_INT;
		out->value = (char *) &pStats->m_iWords;
	}
	return 0;
}

int sphinx_showfunc_words ( THD * thd, SHOW_VAR * out, char * sBuffer )
{
#if MYSQL_VERSION_ID>50100
	if ( sphinx_hton_ptr )
	{
		CSphSEThreadData * pTls = (CSphSEThreadData *) *thd_ha_data ( thd, sphinx_hton_ptr );
#else
	{
		CSphSEThreadData * pTls = (CSphSEThreadData *) thd->ha_data[sphinx_hton.slot];
#endif
		if ( pTls && pTls->m_bStats )
		{
			CSphSEStats * pStats = &pTls->m_tStats;
			if ( pStats && pStats->m_iWords )
			{
				uint uBuffLen = 0;

				out->type = SHOW_CHAR;
				out->value = sBuffer;

				// the following is partially based on code in sphinx_show_status()
				sBuffer[0] = 0;
				for ( int i=0; i<pStats->m_iWords; i++ )
				{
					CSphSEWordStats & tWord = pStats->m_dWords[i];
					uBuffLen = my_snprintf ( sBuffer, SHOW_VAR_FUNC_BUFF_SIZE, "%s%s:%d:%d ", sBuffer,
						tWord.m_sWord, tWord.m_iDocs, tWord.m_iHits );
				}

				if ( uBuffLen > 0 )
				{
					// trim last space
					sBuffer [ --uBuffLen ] = 0;

					if ( pTls->m_pQueryCharset )
					{
						// String::c_ptr() will nul-terminate the buffer.
						//
						// NOTE: It's not entirely clear whether this conversion is necessary at all.

						String sConvert;
						uint iErrors;
						sConvert.copy ( sBuffer, uBuffLen, pTls->m_pQueryCharset, system_charset_info, &iErrors );
						memcpy ( sBuffer, sConvert.c_ptr(), sConvert.length() + 1 );
					}
				}

				return 0;
			}
		}
	}

	out->type = SHOW_CHAR;
	out->value = (char*) "";
	return 0;
}

int sphinx_showfunc_error ( THD * thd, SHOW_VAR * out, char * )
{
	CSphSEStats * pStats = sphinx_get_stats ( thd, out );
	if ( pStats && pStats->m_bLastError )
	{
		out->type = SHOW_CHAR;
		out->value = pStats->m_sLastMessage;
	}
	return 0;
}

#if MYSQL_VERSION_ID>50100
struct st_mysql_storage_engine sphinx_storage_engine =
{
	MYSQL_HANDLERTON_INTERFACE_VERSION
};

struct st_mysql_show_var sphinx_status_vars[] =
{
	{"Sphinx_total",		(char *)sphinx_showfunc_total,			SHOW_FUNC},
	{"Sphinx_total_found",	(char *)sphinx_showfunc_total_found,	SHOW_FUNC},
	{"Sphinx_time",			(char *)sphinx_showfunc_time,			SHOW_FUNC},
	{"Sphinx_word_count",	(char *)sphinx_showfunc_word_count,		SHOW_FUNC},
	{"Sphinx_words",		(char *)sphinx_showfunc_words,			SHOW_FUNC},
	{"Sphinx_error",		(char *)sphinx_showfunc_error,			SHOW_FUNC},
	{0, 0, (enum_mysql_show_type)0}
};


maria_declare_plugin(sphinx)
{
	MYSQL_STORAGE_ENGINE_PLUGIN,
	&sphinx_storage_engine,
	sphinx_hton_name,
	"Sphinx developers",
	sphinx_hton_comment,
	PLUGIN_LICENSE_GPL,
	sphinx_init_func, // Plugin Init
	sphinx_done_func, // Plugin Deinit
	0x0200, // 2.0
	sphinx_status_vars,
	NULL,
        SPHINXSE_VERSION, // string version
	MariaDB_PLUGIN_MATURITY_EXPERIMENTAL
}
maria_declare_plugin_end;

#endif // >50100

//
// $Id: ha_sphinx.cc 3133 2012-03-01 13:47:52Z shodan $
//
