//
// $Id: ha_sphinx.h 1428 2008-09-05 18:06:30Z xale $
//

#ifdef USE_PRAGMA_INTERFACE
#pragma interface // gcc class implementation
#endif


#if MYSQL_VERSION_ID>50100
#define TABLE_ARG	st_table_share
#else
#define TABLE_ARG	st_table
#endif


#if MYSQL_VERSION_ID>=50120
typedef uchar byte;
#endif


/// forward decls
class THD;
struct CSphReqQuery;
struct CSphSEShare;
struct CSphSEAttr;
struct CSphSEStats;
struct CSphSEThreadData;

/// Sphinx SE handler class
class ha_sphinx : public handler
{
protected:
	THR_LOCK_DATA	m_tLock;				///< MySQL lock

	CSphSEShare *	m_pShare;				///< shared lock info

	uint			m_iMatchesTotal;
	uint			m_iCurrentPos;
	const byte *	m_pCurrentKey;
	uint			m_iCurrentKeyLen;

	char *			m_pResponse;			///< searchd response storage
	char *			m_pResponseEnd;			///< searchd response storage end (points to wilderness!)
	char *			m_pCur;					///< current position into response
	bool			m_bUnpackError;			///< any errors while unpacking response

public:
#if MYSQL_VERSION_ID<50100
					ha_sphinx ( TABLE_ARG * table_arg );
#else
					ha_sphinx ( handlerton * hton, TABLE_ARG * table_arg );
#endif
					~ha_sphinx () {}

	const char *	table_type () const		{ return "SPHINX"; }	///< SE name for display purposes
	const char *	index_type ( uint )		{ return "HASH"; }		///< index type name for display purposes
	const char **	bas_ext () const;								///< my file extensions

	#if MYSQL_VERSION_ID>50100
	ulonglong		table_flags () const	{ return HA_CAN_INDEX_BLOBS; }			///< bitmap of implemented flags (see handler.h for more info)
	#else
	ulong			table_flags () const	{ return HA_CAN_INDEX_BLOBS; }			///< bitmap of implemented flags (see handler.h for more info)
	#endif

	ulong			index_flags ( uint, uint, bool ) const	{ return 0; }	///< bitmap of flags that says how SE implements indexes
	uint			max_supported_record_length () const	{ return HA_MAX_REC_LENGTH; }
	uint			max_supported_keys () const				{ return 1; }
	uint			max_supported_key_parts () const		{ return 1; }
	uint			max_supported_key_length () const		{ return MAX_KEY_LENGTH; }
	uint			max_supported_key_part_length () const	{ return MAX_KEY_LENGTH; }

	#if MYSQL_VERSION_ID>50100
	virtual double	scan_time ()	{ return (double)( stats.records+stats.deleted )/20.0 + 10; }	///< called in test_quick_select to determine if indexes should be used
	#else
	virtual double	scan_time ()	{ return (double)( records+deleted )/20.0 + 10; }				///< called in test_quick_select to determine if indexes should be used
	#endif

	virtual double	read_time(uint index, uint ranges, ha_rows rows)
        { return (double)rows/20.0 + 1; }					///< index read time estimate

public:
	int				open ( const char * name, int mode, uint test_if_locked );
	int				close ();

	int				write_row ( uchar * buf );
	int				update_row ( const uchar * old_data, uchar * new_data );
	int				delete_row ( const uchar * buf );

	int				index_init ( uint keynr, bool sorted ); // 5.1.x
	int				index_init ( uint keynr ) { return index_init ( keynr, false ); } // 5.0.x

	int				index_end (); 
	int				index_read ( byte * buf, const byte * key, uint key_len, enum ha_rkey_function find_flag );
	int				index_read_idx ( byte * buf, uint idx, const byte * key, uint key_len, enum ha_rkey_function find_flag );
	int				index_next ( byte * buf );
	int				index_next_same ( byte * buf, const byte * key, uint keylen );
	int				index_prev ( byte * buf );
	int				index_first ( byte * buf );
	int				index_last ( byte * buf );

	int				get_rec ( byte * buf, const byte * key, uint keylen );

	int				rnd_init ( bool scan );
	int				rnd_end ();
	int				rnd_next ( byte * buf );
	int				rnd_pos ( byte * buf, byte * pos );
	void			position ( const byte * record );

#if MYSQL_VERSION_ID>=50030
	int				info ( uint );
#else
	void			info ( uint );
#endif

	int				reset();
	int				external_lock ( THD * thd, int lock_type );
	int				delete_all_rows ();
	ha_rows			records_in_range ( uint inx, key_range * min_key, key_range * max_key );

	int				delete_table ( const char * from );
	int				rename_table ( const char * from, const char * to );
	int				create ( const char * name, TABLE * form, HA_CREATE_INFO * create_info );

	THR_LOCK_DATA **store_lock ( THD * thd, THR_LOCK_DATA ** to, enum thr_lock_type lock_type );

public:
	virtual const COND *	cond_push ( const COND *cond );
	virtual void			cond_pop ();

private:
	uint32			m_iFields;
	char **			m_dFields;

	uint32			m_iAttrs;
	CSphSEAttr *	m_dAttrs;
	int				m_bId64;

	int *			m_dUnboundFields;

private:
	int				ConnectToSearchd ( const char * sQueryHost, int iQueryPort );

	uint32			UnpackDword ();
	char *			UnpackString ();
	bool			UnpackSchema ();
	bool			UnpackStats ( CSphSEStats * pStats );

	CSphSEThreadData *	GetTls ();
};


#if MYSQL_VERSION_ID < 50100
bool sphinx_show_status ( THD * thd );
#endif

//
// $Id: ha_sphinx.h 1428 2008-09-05 18:06:30Z xale $
//
