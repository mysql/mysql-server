/*

 BerkeleyDB.xs -- Perl 5 interface to Berkeley DB version 2 & 3

 written by Paul Marquess <Paul.Marquess@btinternet.com>

 All comments/suggestions/problems are welcome

     Copyright (c) 1997-2001 Paul Marquess. All rights reserved.
     This program is free software; you can redistribute it and/or
     modify it under the same terms as Perl itself.

     Please refer to the COPYRIGHT section in

 Changes:
        0.01 -  First Alpha Release
        0.02 -

*/



#ifdef __cplusplus
extern "C" {
#endif
#define PERL_POLLUTE
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* Being the Berkeley DB we prefer the <sys/cdefs.h> (which will be
 * shortly #included by the <db.h>) __attribute__ to the possibly
 * already defined __attribute__, for example by GNUC or by Perl. */

#undef __attribute__

#ifndef PERL_VERSION
#    include "patchlevel.h"
#    define PERL_REVISION	5
#    define PERL_VERSION	PATCHLEVEL
#    define PERL_SUBVERSION	SUBVERSION
#endif

#if PERL_REVISION == 5 && (PERL_VERSION < 4 || (PERL_VERSION == 4 && PERL_SUBVERSION <= 75 ))

#    define PL_sv_undef		sv_undef
#    define PL_na		na
#    define PL_dirty		dirty

#endif

#include <db.h>

#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0)
#  define IS_DB_3_0
#endif

#if DB_VERSION_MAJOR >= 3
#  define AT_LEAST_DB_3
#endif

#if DB_VERSION_MAJOR > 3 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 1)
#  define AT_LEAST_DB_3_1
#endif

#if DB_VERSION_MAJOR > 3 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 2)
#  define AT_LEAST_DB_3_2
#endif

/* need to define DEFSV & SAVE_DEFSV for older version of Perl */
#ifndef DEFSV
#    define DEFSV GvSV(defgv)
#endif

#ifndef SAVE_DEFSV
#    define SAVE_DEFSV SAVESPTR(GvSV(defgv))
#endif

#ifndef pTHX
#    define pTHX
#    define pTHX_
#    define aTHX
#    define aTHX_
#endif

#ifndef dTHR
#    define dTHR
#endif

#ifndef newSVpvn
#    define newSVpvn(a,b)       newSVpv(a,b)
#endif

#ifdef __cplusplus
}
#endif

#define DBM_FILTERING
#define STRICT_CLOSE
/* #define ALLOW_RECNO_OFFSET */
/* #define TRACE */

#if DB_VERSION_MAJOR == 2 && ! defined(DB_LOCK_DEADLOCK)
#  define DB_LOCK_DEADLOCK	EAGAIN
#endif /* DB_VERSION_MAJOR == 2 */

#if DB_VERSION_MAJOR == 2
#  define DB_QUEUE		4
#endif /* DB_VERSION_MAJOR == 2 */

#ifdef AT_LEAST_DB_3_2
#    define DB_callback	DB * db,
#else
#    define DB_callback
#endif

#if DB_VERSION_MAJOR > 2
typedef struct {
        int              db_lorder;
        size_t           db_cachesize;
        size_t           db_pagesize;


        void *(*db_malloc) __P((size_t));
        int (*dup_compare)
            __P((DB_callback const DBT *, const DBT *));

        u_int32_t        bt_maxkey;
        u_int32_t        bt_minkey;
        int (*bt_compare)
            __P((DB_callback const DBT *, const DBT *));
        size_t (*bt_prefix)
            __P((DB_callback const DBT *, const DBT *));

        u_int32_t        h_ffactor;
        u_int32_t        h_nelem;
        u_int32_t      (*h_hash)
            __P((DB_callback const void *, u_int32_t));

        int              re_pad;
        int              re_delim;
        u_int32_t        re_len;
        char            *re_source;

#define DB_DELIMITER            0x0001
#define DB_FIXEDLEN             0x0008
#define DB_PAD                  0x0010
        u_int32_t        flags;
        u_int32_t        q_extentsize;
} DB_INFO ;

#endif /* DB_VERSION_MAJOR > 2 */

typedef struct {
	int		Status ;
	/* char		ErrBuff[1000] ; */
	SV *		ErrPrefix ;
	SV *		ErrHandle ;
	DB_ENV *	Env ;
	int		open_dbs ;
	int		TxnMgrStatus ;
	int		active ;
	bool		txn_enabled ;
	} BerkeleyDB_ENV_type ;


typedef struct {
        DBTYPE  	type ;
	bool		recno_or_queue ;
	char *		filename ;
	BerkeleyDB_ENV_type * parent_env ;
        DB *    	dbp ;
        SV *    	compare ;
        SV *    	dup_compare ;
        SV *    	prefix ;
        SV *   	 	hash ;
	int		Status ;
        DB_INFO *	info ;
        DBC *   	cursor ;
	DB_TXN *	txn ;
	int		open_cursors ;
	u_int32_t	partial ;
	u_int32_t	dlen ;
	u_int32_t	doff ;
	int		active ;
#ifdef ALLOW_RECNO_OFFSET
	int		array_base ;
#endif
#ifdef DBM_FILTERING
        SV *    filter_fetch_key ;
        SV *    filter_store_key ;
        SV *    filter_fetch_value ;
        SV *    filter_store_value ;
        int     filtering ;
#endif
        } BerkeleyDB_type;


typedef struct {
        DBTYPE  	type ;
	bool		recno_or_queue ;
	char *		filename ;
        DB *    	dbp ;
        SV *    	compare ;
        SV *    	dup_compare ;
        SV *    	prefix ;
        SV *   	 	hash ;
	int		Status ;
        DB_INFO *	info ;
        DBC *   	cursor ;
	DB_TXN *	txn ;
	BerkeleyDB_type *		parent_db ;
	u_int32_t	partial ;
	u_int32_t	dlen ;
	u_int32_t	doff ;
	int		active ;
#ifdef ALLOW_RECNO_OFFSET
	int		array_base ;
#endif
#ifdef DBM_FILTERING
        SV *    filter_fetch_key ;
        SV *    filter_store_key ;
        SV *    filter_fetch_value ;
        SV *    filter_store_value ;
        int     filtering ;
#endif
        } BerkeleyDB_Cursor_type;

typedef struct {
	BerkeleyDB_ENV_type *	env ;
	} BerkeleyDB_TxnMgr_type ;

#if 1
typedef struct {
	int		Status ;
	DB_TXN *	txn ;
	int		active ;
	} BerkeleyDB_Txn_type ;
#else
typedef DB_TXN                BerkeleyDB_Txn_type ;
#endif

typedef BerkeleyDB_ENV_type *	BerkeleyDB__Env ;
typedef BerkeleyDB_ENV_type *	BerkeleyDB__Env__Raw ;
typedef BerkeleyDB_ENV_type *	BerkeleyDB__Env__Inner ;
typedef BerkeleyDB_type * 	BerkeleyDB ;
typedef void * 			BerkeleyDB__Raw ;
typedef BerkeleyDB_type *	BerkeleyDB__Common ;
typedef BerkeleyDB_type *	BerkeleyDB__Common__Raw ;
typedef BerkeleyDB_type *	BerkeleyDB__Common__Inner ;
typedef BerkeleyDB_type * 	BerkeleyDB__Hash ;
typedef BerkeleyDB_type * 	BerkeleyDB__Hash__Raw ;
typedef BerkeleyDB_type * 	BerkeleyDB__Btree ;
typedef BerkeleyDB_type * 	BerkeleyDB__Btree__Raw ;
typedef BerkeleyDB_type * 	BerkeleyDB__Recno ;
typedef BerkeleyDB_type * 	BerkeleyDB__Recno__Raw ;
typedef BerkeleyDB_type * 	BerkeleyDB__Queue ;
typedef BerkeleyDB_type * 	BerkeleyDB__Queue__Raw ;
typedef BerkeleyDB_Cursor_type   	BerkeleyDB__Cursor_type ;
typedef BerkeleyDB_Cursor_type * 	BerkeleyDB__Cursor ;
typedef BerkeleyDB_Cursor_type * 	BerkeleyDB__Cursor__Raw ;
typedef BerkeleyDB_TxnMgr_type * BerkeleyDB__TxnMgr ;
typedef BerkeleyDB_TxnMgr_type * BerkeleyDB__TxnMgr__Raw ;
typedef BerkeleyDB_TxnMgr_type * BerkeleyDB__TxnMgr__Inner ;
typedef BerkeleyDB_Txn_type *	BerkeleyDB__Txn ;
typedef BerkeleyDB_Txn_type *	BerkeleyDB__Txn__Raw ;
typedef BerkeleyDB_Txn_type *	BerkeleyDB__Txn__Inner ;
#if 0
typedef DB_LOG *      		BerkeleyDB__Log ;
typedef DB_LOCKTAB *  		BerkeleyDB__Lock ;
#endif
typedef DBT 			DBTKEY ;
typedef DBT 			DBT_OPT ;
typedef DBT 			DBT_B ;
typedef DBT 			DBTKEY_B ;
typedef DBT 			DBTVALUE ;
typedef void *	      		PV_or_NULL ;
typedef PerlIO *      		IO_or_NULL ;
typedef int			DualType ;

static void
hash_delete(char * hash, IV key);

#ifdef TRACE
#  define Trace(x)	printf x
#else
#  define Trace(x)
#endif

#ifdef ALLOW_RECNO_OFFSET
#  define RECNO_BASE	db->array_base
#else
#  define RECNO_BASE	1
#endif

#if DB_VERSION_MAJOR == 2
#  define flagSet_DB2(i, f) i |= f
#else
#  define flagSet_DB2(i, f)
#endif

#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 5
#  define flagSet(bitmask)        (flags & (bitmask))
#else
#  define flagSet(bitmask)	((flags & DB_OPFLAGS_MASK) == (bitmask))
#endif

#ifdef DBM_FILTERING
#define ckFilter(arg,type,name)                                 \
        if (db->type) {                                         \
            SV * save_defsv ;                                   \
            /* printf("filtering %s\n", name) ;*/               \
            if (db->filtering)                                  \
                softCrash("recursion detected in %s", name) ;   \
            db->filtering = TRUE ;                              \
            save_defsv = newSVsv(DEFSV) ;                       \
            sv_setsv(DEFSV, arg) ;                              \
            PUSHMARK(sp) ;                                      \
            (void) perl_call_sv(db->type, G_DISCARD|G_NOARGS);  \
            sv_setsv(arg, DEFSV) ;                              \
            sv_setsv(DEFSV, save_defsv) ;                       \
            SvREFCNT_dec(save_defsv) ;                          \
            db->filtering = FALSE ;                             \
            /*printf("end of filtering %s\n", name) ;*/         \
        }
#else
#define ckFilter(type, sv, name)
#endif

#define ERR_BUFF "BerkeleyDB::Error"

#define ZMALLOC(to, typ) ((to = (typ *)safemalloc(sizeof(typ))), \
				Zero(to,1,typ))

#define DBT_clear(x)	Zero(&x, 1, DBT) ;

#if 1
#define getInnerObject(x) SvIV(*av_fetch((AV*)SvRV(x), 0, FALSE))
#else
#define getInnerObject(x) SvIV((SV*)SvRV(sv))
#endif

#define my_sv_setpvn(sv, d, s) (s ? sv_setpvn(sv, d, s) : sv_setpv(sv, "") )

#define SetValue_iv(i, k) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = SvIV(sv)
#define SetValue_io(i, k) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = IoOFP(sv_2io(sv))
#define SetValue_sv(i, k) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = sv
#define SetValue_pv(i, k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = (t)SvPV(sv,PL_na)
#define SetValue_pvx(i, k, t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = (t)SvPVX(sv)
#define SetValue_ov(i,k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) {\
				IV tmp = getInnerObject(sv) ;	\
				i = (t) tmp ;			\
			  }

#define SetValue_ovx(i,k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) {\
				HV * hv = (HV *)GetInternalObject(sv);		\
				SV ** svp = hv_fetch(hv, "db", 2, FALSE);\
				IV tmp = SvIV(*svp);			\
				i = (t) tmp ;				\
			  }

#define SetValue_ovX(i,k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) {\
				IV tmp = SvIV(GetInternalObject(sv));\
				i = (t) tmp ;				\
			  }

#define LastDBerror DB_RUNRECOVERY

#define setDUALerrno(var, err)					\
		sv_setnv(var, (double)err) ;			\
		sv_setpv(var, ((err) ? db_strerror(err) : "")) ;\
		SvNOK_on(var);

#define OutputValue(arg, name)                                  \
        { if (RETVAL == 0) {                                    \
              my_sv_setpvn(arg, name.data, name.size) ;         \
              ckFilter(arg, filter_fetch_value,"filter_fetch_value") ;            \
          }                                                     \
        }

#define OutputValue_B(arg, name)                                  \
        { if (RETVAL == 0) {                                    \
		if (db->type == DB_BTREE && 			\
			flagSet(DB_GET_RECNO)){			\
                    sv_setiv(arg, (I32)(*(I32*)name.data) - RECNO_BASE); \
                }                                               \
                else {                                          \
                    my_sv_setpvn(arg, name.data, name.size) ;   \
                }                                               \
                ckFilter(arg, filter_fetch_value, "filter_fetch_value");          \
          }                                                     \
        }

#define OutputKey(arg, name)                                    \
        { if (RETVAL == 0) 					\
          {                                                     \
                if (!db->recno_or_queue) {                     	\
                    my_sv_setpvn(arg, name.data, name.size);    \
                }                                               \
                else                                            \
                    sv_setiv(arg, (I32)*(I32*)name.data - RECNO_BASE);   \
                ckFilter(arg, filter_fetch_key, "filter_fetch_key") ;            \
          }                                                     \
        }

#define OutputKey_B(arg, name)                                  \
        { if (RETVAL == 0) 					\
          {                                                     \
                if (db->recno_or_queue ||			\
			(db->type == DB_BTREE && 		\
			    flagSet(DB_GET_RECNO))){		\
                    sv_setiv(arg, (I32)(*(I32*)name.data) - RECNO_BASE); \
                }                                               \
                else {                                          \
                    my_sv_setpvn(arg, name.data, name.size);    \
                }                                               \
                ckFilter(arg, filter_fetch_key, "filter_fetch_key") ;            \
          }                                                     \
        }

#define SetPartial(data,db) 					\
	data.flags = db->partial ;				\
	data.dlen  = db->dlen ;					\
	data.doff  = db->doff ;

#define ckActive(active, type) 					\
    {								\
	if (!active)						\
	    softCrash("%s is already closed", type) ;		\
    }

#define ckActive_Environment(a)	ckActive(a, "Environment")
#define ckActive_TxnMgr(a)	ckActive(a, "Transaction Manager")
#define ckActive_Transaction(a) ckActive(a, "Transaction")
#define ckActive_Database(a) 	ckActive(a, "Database")
#define ckActive_Cursor(a) 	ckActive(a, "Cursor")

/* Internal Global Data */
static db_recno_t Value ;
static db_recno_t zero = 0 ;
static BerkeleyDB	CurrentDB ;
static DBTKEY	empty ;
static char	ErrBuff[1000] ;

static char *
my_strdup(const char *s)
{
    if (s == NULL)
        return NULL ;

    {
        MEM_SIZE l = strlen(s);
        char *s1 = (char *)safemalloc(l);

        Copy(s, s1, (MEM_SIZE)l, char);
        return s1;
    }
}

#if DB_VERSION_MAJOR == 2
static char *
db_strerror(int err)
{
    if (err == 0)
        return "" ;

    if (err > 0)
        return Strerror(err) ;

    switch (err) {
	case DB_INCOMPLETE:
		return ("DB_INCOMPLETE: Sync was unable to complete");
	case DB_KEYEMPTY:
		return ("DB_KEYEMPTY: Non-existent key/data pair");
	case DB_KEYEXIST:
		return ("DB_KEYEXIST: Key/data pair already exists");
	case DB_LOCK_DEADLOCK:
		return (
		    "DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock");
	case DB_LOCK_NOTGRANTED:
		return ("DB_LOCK_NOTGRANTED: Lock not granted");
	case DB_LOCK_NOTHELD:
		return ("DB_LOCK_NOTHELD: Lock not held by locker");
	case DB_NOTFOUND:
		return ("DB_NOTFOUND: No matching key/data pair found");
	case DB_RUNRECOVERY:
		return ("DB_RUNRECOVERY: Fatal error, run database recovery");
	default:
		return "Unknown Error" ;

    }
}
#endif 	/* DB_VERSION_MAJOR == 2 */

static char *
my_db_strerror(int err)
{
    static char buffer[1000] ;
    SV * sv = perl_get_sv(ERR_BUFF, FALSE) ;
    sprintf(buffer, "%d: %s", err, db_strerror(err)) ;
    if (err && sv) {
        strcat(buffer, ", ") ;
	strcat(buffer, SvPVX(sv)) ;
    }
    return buffer;
}

static void
close_everything(void)
{
    dTHR;
    Trace(("close_everything\n")) ;
    /* Abort All Transactions */
    {
	BerkeleyDB__Txn__Raw 	tid ;
	HE * he ;
	I32 len ;
	HV * hv = perl_get_hv("BerkeleyDB::Term::Txn", TRUE);
	I32 ret = hv_iterinit(hv) ;
	int  all = 0 ;
	int  closed = 0 ;
	Trace(("BerkeleyDB::Term::close_all_txns dirty=%d\n", PL_dirty)) ;
	while ( he = hv_iternext(hv) ) {
	    tid = * (BerkeleyDB__Txn__Raw *) (IV) hv_iterkey(he, &len) ;
	    Trace(("  Aborting Transaction [%d] in [%d] Active [%d]\n", tid->txn, tid, tid->active));
	    if (tid->active) {
	        txn_abort(tid->txn);
		++ closed ;
	    }
	    tid->active = FALSE ;
	    ++ all ;
	}
	Trace(("End of BerkeleyDB::Term::close_all_txns aborted %d of %d transactios\n",closed, all)) ;
    }

    /* Close All Cursors */
    {
	BerkeleyDB__Cursor db ;
	HE * he ;
	I32 len ;
	HV * hv = perl_get_hv("BerkeleyDB::Term::Cursor", TRUE);
	I32 ret = hv_iterinit(hv) ;
	int  all = 0 ;
	int  closed = 0 ;
	Trace(("BerkeleyDB::Term::close_all_cursors \n")) ;
	while ( he = hv_iternext(hv) ) {
	    db = * (BerkeleyDB__Cursor*) (IV) hv_iterkey(he, &len) ;
	    Trace(("  Closing Cursor [%d] in [%d] Active [%d]\n", db->cursor, db, db->active));
	    if (db->active) {
    	        ((db->cursor)->c_close)(db->cursor) ;
		++ closed ;
	    }
	    db->active = FALSE ;
	    ++ all ;
	}
	Trace(("End of BerkeleyDB::Term::close_all_cursors closed %d of %d cursors\n",closed, all)) ;
    }

    /* Close All Databases */
    {
	BerkeleyDB db ;
	HE * he ;
	I32 len ;
	HV * hv = perl_get_hv("BerkeleyDB::Term::Db", TRUE);
	I32 ret = hv_iterinit(hv) ;
	int  all = 0 ;
	int  closed = 0 ;
	Trace(("BerkeleyDB::Term::close_all_dbs\n" )) ;
	while ( he = hv_iternext(hv) ) {
	    db = * (BerkeleyDB*) (IV) hv_iterkey(he, &len) ;
	    Trace(("  Closing Database [%d] in [%d] Active [%d]\n", db->dbp, db, db->active));
	    if (db->active) {
	        (db->dbp->close)(db->dbp, 0) ;
		++ closed ;
	    }
	    db->active = FALSE ;
	    ++ all ;
	}
	Trace(("End of BerkeleyDB::Term::close_all_dbs closed %d of %d dbs\n",closed, all)) ;
    }

    /* Close All Environments */
    {
	BerkeleyDB__Env env ;
	HE * he ;
	I32 len ;
	HV * hv = perl_get_hv("BerkeleyDB::Term::Env", TRUE);
	I32 ret = hv_iterinit(hv) ;
	int  all = 0 ;
	int  closed = 0 ;
	Trace(("BerkeleyDB::Term::close_all_envs\n")) ;
	while ( he = hv_iternext(hv) ) {
	    env = * (BerkeleyDB__Env*) (IV) hv_iterkey(he, &len) ;
	    Trace(("  Closing Environment [%d] in [%d] Active [%d]\n", env->Env, env, env->active));
	    if (env->active) {
#if DB_VERSION_MAJOR == 2
                db_appexit(env->Env) ;
#else
	        (env->Env->close)(env->Env, 0) ;
#endif
		++ closed ;
	    }
	    env->active = FALSE ;
	    ++ all ;
	}
	Trace(("End of BerkeleyDB::Term::close_all_envs closed %d of %d dbs\n",closed, all)) ;
    }

    Trace(("end close_everything\n")) ;

}

static void
destroyDB(BerkeleyDB db)
{
    dTHR;
    if (! PL_dirty && db->active) {
      	-- db->open_cursors ;
	((db->dbp)->close)(db->dbp, 0) ;
    }
    if (db->hash)
       	  SvREFCNT_dec(db->hash) ;
    if (db->compare)
       	  SvREFCNT_dec(db->compare) ;
    if (db->dup_compare)
       	  SvREFCNT_dec(db->dup_compare) ;
    if (db->prefix)
       	  SvREFCNT_dec(db->prefix) ;
#ifdef DBM_FILTERING
    if (db->filter_fetch_key)
          SvREFCNT_dec(db->filter_fetch_key) ;
    if (db->filter_store_key)
          SvREFCNT_dec(db->filter_store_key) ;
    if (db->filter_fetch_value)
          SvREFCNT_dec(db->filter_fetch_value) ;
    if (db->filter_store_value)
          SvREFCNT_dec(db->filter_store_value) ;
#endif
    hash_delete("BerkeleyDB::Term::Db", (IV)db) ;
    if (db->filename)
             Safefree(db->filename) ;
    Safefree(db) ;
}

static void
softCrash(const char *pat, ...)
{
    char buffer1 [500] ;
    char buffer2 [500] ;
    va_list args;
    va_start(args, pat);

    Trace(("softCrash: %s\n", pat)) ;

#define ABORT_PREFIX "BerkeleyDB Aborting: "

    /* buffer = (char*) safemalloc(strlen(pat) + strlen(ABORT_PREFIX) + 1) ; */
    strcpy(buffer1, ABORT_PREFIX) ;
    strcat(buffer1, pat) ;

    vsprintf(buffer2, buffer1, args) ;

    croak(buffer2);

    /* NOTREACHED */
    va_end(args);
}


static I32
GetArrayLength(BerkeleyDB db)
{
    DBT		key ;
    DBT		value ;
    int		RETVAL = 0 ;
    DBC *   	cursor ;

    DBT_clear(key) ;
    DBT_clear(value) ;
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
    if ( ((db->dbp)->cursor)(db->dbp, db->txn, &cursor) == 0 )
#else
    if ( ((db->dbp)->cursor)(db->dbp, db->txn, &cursor, 0) == 0 )
#endif
    {
        RETVAL = cursor->c_get(cursor, &key, &value, DB_LAST) ;
        if (RETVAL == 0)
            RETVAL = *(I32 *)key.data ;
        else /* No key means empty file */
            RETVAL = 0 ;
        cursor->c_close(cursor) ;
    }

    Trace(("GetArrayLength got %d\n", RETVAL)) ;
    return ((I32)RETVAL) ;
}

#if 0

#define GetRecnoKey(db, value)  _GetRecnoKey(db, value)

static db_recno_t
_GetRecnoKey(BerkeleyDB db, I32 value)
{
    Trace(("GetRecnoKey start value = %d\n", value)) ;
    if (db->recno_or_queue && value < 0) {
	/* Get the length of the array */
	I32 length = GetArrayLength(db) ;

	/* check for attempt to write before start of array */
	if (length + value + RECNO_BASE <= 0)
	    softCrash("Modification of non-creatable array value attempted, subscript %ld", (long)value) ;

	value = length + value + RECNO_BASE ;
    }
    else
        ++ value ;

    Trace(("GetRecnoKey end value = %d\n", value)) ;

    return value ;
}

#else /* ! 0 */

#if 0
#ifdef ALLOW_RECNO_OFFSET
#define GetRecnoKey(db, value) _GetRecnoKey(db, value)

static db_recno_t
_GetRecnoKey(BerkeleyDB db, I32 value)
{
    if (value + RECNO_BASE < 1)
	softCrash("key value %d < base (%d)", (value), RECNO_BASE?0:1) ;
    return value + RECNO_BASE ;
}

#else
#endif /* ALLOW_RECNO_OFFSET */
#endif /* 0 */

#define GetRecnoKey(db, value) ((value) + RECNO_BASE )

#endif /* 0 */

static SV *
GetInternalObject(SV * sv)
{
    SV * info = (SV*) NULL ;
    SV * s ;
    MAGIC * mg ;

    Trace(("in GetInternalObject %d\n", sv)) ;
    if (sv == NULL || !SvROK(sv))
        return NULL ;

    s = SvRV(sv) ;
    if (SvMAGICAL(s))
    {
        if (SvTYPE(s) == SVt_PVHV || SvTYPE(s) == SVt_PVAV)
            mg = mg_find(s, 'P') ;
        else
            mg = mg_find(s, 'q') ;

	 /* all this testing is probably overkill, but till I know more
	    about global destruction it stays.
	 */
        /* if (mg && mg->mg_obj && SvRV(mg->mg_obj) && SvPVX(SvRV(mg->mg_obj))) */
        if (mg && mg->mg_obj && SvRV(mg->mg_obj) )
            info = SvRV(mg->mg_obj) ;
	else
	    info = s ;
    }

    Trace(("end of GetInternalObject %d\n", info)) ;
    return info ;
}

static int
btree_compare(DB_callback const DBT * key1, const DBT * key2 )
{
    dSP ;
    void * data1, * data2 ;
    int retval ;
    int count ;

    data1 = key1->data ;
    data2 = key2->data ;

#ifndef newSVpvn
    /* As newSVpv will assume that the data pointer is a null terminated C
       string if the size parameter is 0, make sure that data points to an
       empty string if the length is 0
    */
    if (key1->size == 0)
        data1 = "" ;
    if (key2->size == 0)
        data2 = "" ;
#endif

    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;
    EXTEND(SP,2) ;
    PUSHs(sv_2mortal(newSVpvn(data1,key1->size)));
    PUSHs(sv_2mortal(newSVpvn(data2,key2->size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->compare, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("in btree_compare - expected 1 return value from compare sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    return (retval) ;

}

static int
dup_compare(DB_callback const DBT * key1, const DBT * key2 )
{
    dSP ;
    void * data1, * data2 ;
    int retval ;
    int count ;

    Trace(("In dup_compare \n")) ;
    if (!CurrentDB)
	softCrash("Internal Error - No CurrentDB in dup_compare") ;
    if (CurrentDB->dup_compare == NULL)
        softCrash("in dup_compare: no callback specified for database '%s'", CurrentDB->filename) ;

    data1 = key1->data ;
    data2 = key2->data ;

#ifndef newSVpvn
    /* As newSVpv will assume that the data pointer is a null terminated C
       string if the size parameter is 0, make sure that data points to an
       empty string if the length is 0
    */
    if (key1->size == 0)
        data1 = "" ;
    if (key2->size == 0)
        data2 = "" ;
#endif

    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;
    EXTEND(SP,2) ;
    PUSHs(sv_2mortal(newSVpvn(data1,key1->size)));
    PUSHs(sv_2mortal(newSVpvn(data2,key2->size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->dup_compare, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("dup_compare: expected 1 return value from compare sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    return (retval) ;

}

static size_t
btree_prefix(DB_callback const DBT * key1, const DBT * key2 )
{
    dSP ;
    void * data1, * data2 ;
    int retval ;
    int count ;

    data1 = key1->data ;
    data2 = key2->data ;

#ifndef newSVpvn
    /* As newSVpv will assume that the data pointer is a null terminated C
       string if the size parameter is 0, make sure that data points to an
       empty string if the length is 0
    */
    if (key1->size == 0)
        data1 = "" ;
    if (key2->size == 0)
        data2 = "" ;
#endif

    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;
    EXTEND(SP,2) ;
    PUSHs(sv_2mortal(newSVpvn(data1,key1->size)));
    PUSHs(sv_2mortal(newSVpvn(data2,key2->size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->prefix, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("btree_prefix: expected 1 return value from prefix sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;

    return (retval) ;
}

static u_int32_t
hash_cb(DB_callback const void * data, u_int32_t size)
{
    dSP ;
    int retval ;
    int count ;

#ifndef newSVpvn
    if (size == 0)
        data = "" ;
#endif

    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;

    XPUSHs(sv_2mortal(newSVpvn((char*)data,size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->hash, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("hash_cb: expected 1 return value from hash sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;

    return (retval) ;
}

static void
db_errcall_cb(const char * db_errpfx, char * buffer)
{
#if 0

    if (db_errpfx == NULL)
	db_errpfx = "" ;
    if (buffer == NULL )
	buffer = "" ;
    ErrBuff[0] = '\0';
    if (strlen(db_errpfx) + strlen(buffer) + 3 <= 1000) {
	if (*db_errpfx != '\0') {
	    strcat(ErrBuff, db_errpfx) ;
	    strcat(ErrBuff, ": ") ;
	}
	strcat(ErrBuff, buffer) ;
    }

#endif

    SV * sv = perl_get_sv(ERR_BUFF, FALSE) ;
    if (sv) {
        if (db_errpfx)
	    sv_setpvf(sv, "%s: %s", db_errpfx, buffer) ;
        else
            sv_setpv(sv, buffer) ;
    }
}

static SV *
readHash(HV * hash, char * key)
{
    SV **       svp;
    svp = hv_fetch(hash, key, strlen(key), FALSE);
    if (svp && SvOK(*svp))
        return *svp ;
    return NULL ;
}

static void
hash_delete(char * hash, IV key)
{
    HV * hv = perl_get_hv(hash, TRUE);
    (void) hv_delete(hv, (char*)&key, sizeof(key), G_DISCARD);
}

static void
hash_store_iv(char * hash, IV key, IV value)
{
    HV * hv = perl_get_hv(hash, TRUE);
    SV ** ret = hv_store(hv, (char*)&key, sizeof(key), newSViv(value), 0);
    /* printf("hv_store returned %d\n", ret) ; */
}

static void
hv_store_iv(HV * hash, char * key, IV value)
{
    hv_store(hash, key, strlen(key), newSViv(value), 0);
}

static BerkeleyDB
my_db_open(
		BerkeleyDB	db ,
		SV * 		ref,
		SV *		ref_dbenv ,
		BerkeleyDB__Env	dbenv ,
		const char *	file,
		const char *	subname,
		DBTYPE		type,
		int		flags,
		int		mode,
		DB_INFO * 	info
	)
{
    DB_ENV *	env    = NULL ;
    BerkeleyDB 	RETVAL = NULL ;
    DB *	dbp ;
    int		Status ;

    Trace(("_db_open(dbenv[%lu] ref_dbenv [%lu] file[%s] subname [%s] type[%d] flags[%d] mode[%d]\n",
		dbenv, ref_dbenv, file, subname, type, flags, mode)) ;

    CurrentDB = db ;
    if (dbenv)
	env = dbenv->Env ;

#if DB_VERSION_MAJOR == 2
    if (subname)
        softCrash("Subname needs Berkeley DB 3 or better") ;
#endif

#if DB_VERSION_MAJOR > 2
    Status = db_create(&dbp, env, 0) ;
    Trace(("db_create returned %s\n", my_db_strerror(Status))) ;
    if (Status)
        return RETVAL ;

    if (info->re_source) {
        Status = dbp->set_re_source(dbp, info->re_source) ;
	Trace(("set_re_source [%s] returned %s\n",
		info->re_source, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->db_cachesize) {
        Status = dbp->set_cachesize(dbp, 0, info->db_cachesize, 0) ;
	Trace(("set_cachesize [%d] returned %s\n",
		info->db_cachesize, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->db_lorder) {
        Status = dbp->set_lorder(dbp, info->db_lorder) ;
	Trace(("set_lorder [%d] returned %s\n",
		info->db_lorder, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->db_pagesize) {
        Status = dbp->set_pagesize(dbp, info->db_pagesize) ;
	Trace(("set_pagesize [%d] returned %s\n",
		info->db_pagesize, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->h_ffactor) {
        Status = dbp->set_h_ffactor(dbp, info->h_ffactor) ;
	Trace(("set_h_ffactor [%d] returned %s\n",
		info->h_ffactor, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->h_nelem) {
        Status = dbp->set_h_nelem(dbp, info->h_nelem) ;
	Trace(("set_h_nelem [%d] returned %s\n",
		info->h_nelem, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->bt_minkey) {
        Status = dbp->set_bt_minkey(dbp, info->bt_minkey) ;
	Trace(("set_bt_minkey [%d] returned %s\n",
		info->bt_minkey, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->bt_compare) {
        Status = dbp->set_bt_compare(dbp, info->bt_compare) ;
	Trace(("set_bt_compare [%d] returned %s\n",
		info->bt_compare, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->h_hash) {
        Status = dbp->set_h_hash(dbp, info->h_hash) ;
	Trace(("set_h_hash [%d] returned %s\n",
		info->h_hash, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->dup_compare) {
        Status = dbp->set_dup_compare(dbp, info->dup_compare) ;
	Trace(("set_dup_compare [%d] returned %s\n",
		info->dup_compare, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->bt_prefix) {
        Status = dbp->set_bt_prefix(dbp, info->bt_prefix) ;
	Trace(("set_bt_prefix [%d] returned %s\n",
		info->bt_prefix, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->re_len) {
        Status = dbp->set_re_len(dbp, info->re_len) ;
	Trace(("set_re_len [%d] returned %s\n",
		info->re_len, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->re_delim) {
        Status = dbp->set_re_delim(dbp, info->re_delim) ;
	Trace(("set_re_delim [%d] returned %s\n",
		info->re_delim, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->re_pad) {
        Status = dbp->set_re_pad(dbp, info->re_pad) ;
	Trace(("set_re_pad [%d] returned %s\n",
		info->re_pad, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->flags) {
        Status = dbp->set_flags(dbp, info->flags) ;
	Trace(("set_flags [%d] returned %s\n",
		info->flags, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
    }

    if (info->q_extentsize) {
#ifdef AT_LEAST_DB_3_2
        Status = dbp->set_q_extentsize(dbp, info->q_extentsize) ;
	Trace(("set_flags [%d] returned %s\n",
		info->flags, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
#else
        softCrash("-ExtentSize needs at least Berkeley DB 3.2.x") ;
#endif
    }

    if ((Status = (dbp->open)(dbp, file, subname, type, flags, mode)) == 0) {
#else /* DB_VERSION_MAJOR == 2 */
    if ((Status = db_open(file, type, flags, mode, env, info, &dbp)) == 0) {
#endif /* DB_VERSION_MAJOR == 2 */

	Trace(("db_opened\n"));
	RETVAL = db ;
	RETVAL->dbp  = dbp ;
#if DB_VERSION_MAJOR == 2
    	RETVAL->type = dbp->type ;
#else /* DB_VERSION_MAJOR > 2 */
    	RETVAL->type = dbp->get_type(dbp) ;
#endif /* DB_VERSION_MAJOR > 2 */
    	RETVAL->recno_or_queue = (RETVAL->type == DB_RECNO ||
	                          RETVAL->type == DB_QUEUE) ;
	RETVAL->filename = my_strdup(file) ;
	RETVAL->Status = Status ;
	RETVAL->active = TRUE ;
	hash_store_iv("BerkeleyDB::Term::Db", (IV)RETVAL, 1) ;
	Trace(("  storing %d %d in BerkeleyDB::Term::Db\n", RETVAL, dbp)) ;
	if (dbenv) {
	    RETVAL->parent_env = dbenv ;
	    dbenv->Status = Status ;
	    ++ dbenv->open_dbs ;
	}
    }
    else {
#if DB_VERSION_MAJOR > 2
	(dbp->close)(dbp, 0) ;
#endif
	destroyDB(db) ;
        Trace(("db open returned %s\n", my_db_strerror(Status))) ;
    }

    return RETVAL ;
}

static double
constant(char * name, int arg)
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	break;
    case 'C':
	break;
    case 'D':
        if (strEQ(name, "DB_AFTER"))
#ifdef DB_AFTER
            return DB_AFTER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_APPEND"))
#ifdef DB_APPEND
            return DB_APPEND;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_ARCH_ABS"))
#ifdef DB_ARCH_ABS
            return DB_ARCH_ABS;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_ARCH_DATA"))
#ifdef DB_ARCH_DATA
            return DB_ARCH_DATA;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_ARCH_LOG"))
#ifdef DB_ARCH_LOG
            return DB_ARCH_LOG;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_BEFORE"))
#ifdef DB_BEFORE
            return DB_BEFORE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_BTREE"))
            return DB_BTREE;
        if (strEQ(name, "DB_BTREEMAGIC"))
#ifdef DB_BTREEMAGIC
            return DB_BTREEMAGIC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_BTREEOLDVER"))
#ifdef DB_BTREEOLDVER
            return DB_BTREEOLDVER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_BTREEVERSION"))
#ifdef DB_BTREEVERSION
            return DB_BTREEVERSION;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_CHECKPOINT"))
#ifdef DB_CHECKPOINT
            return DB_CHECKPOINT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_CONSUME"))
#ifdef DB_CONSUME
            return DB_CONSUME;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_CREATE"))
#ifdef DB_CREATE
            return DB_CREATE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_CURLSN"))
#ifdef DB_CURLSN
            return DB_CURLSN;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_CURRENT"))
#ifdef DB_CURRENT
            return DB_CURRENT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DBT_MALLOC"))
#ifdef DB_DBT_MALLOC
            return DB_DBT_MALLOC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DBT_PARTIAL"))
#ifdef DB_DBT_PARTIAL
            return DB_DBT_PARTIAL;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DBT_USERMEM"))
#ifdef DB_DBT_USERMEM
            return DB_DBT_USERMEM;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DELETED"))
#ifdef DB_DELETED
            return DB_DELETED;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DELIMITER"))
#ifdef DB_DELIMITER
            return DB_DELIMITER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DUP"))
#ifdef DB_DUP
            return DB_DUP;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_DUPSORT"))
#ifdef DB_DUPSORT
            return DB_DUPSORT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_ENV_APPINIT"))
#ifdef DB_ENV_APPINIT
            return DB_ENV_APPINIT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_ENV_STANDALONE"))
#ifdef DB_ENV_STANDALONE
            return DB_ENV_STANDALONE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_ENV_THREAD"))
#ifdef DB_ENV_THREAD
            return DB_ENV_THREAD;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_EXCL"))
#ifdef DB_EXCL
            return DB_EXCL;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_FILE_ID_LEN"))
#ifdef DB_FILE_ID_LEN
            return DB_FILE_ID_LEN;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_FIRST"))
#ifdef DB_FIRST
            return DB_FIRST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_FIXEDLEN"))
#ifdef DB_FIXEDLEN
            return DB_FIXEDLEN;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_FLUSH"))
#ifdef DB_FLUSH
            return DB_FLUSH;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_FORCE"))
#ifdef DB_FORCE
            return DB_FORCE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_GET_BOTH"))
#ifdef DB_GET_BOTH
            return DB_GET_BOTH;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_GET_RECNO"))
#ifdef DB_GET_RECNO
            return DB_GET_RECNO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_HASH"))
            return DB_HASH;
        if (strEQ(name, "DB_HASHMAGIC"))
#ifdef DB_HASHMAGIC
            return DB_HASHMAGIC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_HASHOLDVER"))
#ifdef DB_HASHOLDVER
            return DB_HASHOLDVER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_HASHVERSION"))
#ifdef DB_HASHVERSION
            return DB_HASHVERSION;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_INCOMPLETE"))
#ifdef DB_INCOMPLETE
            return DB_INCOMPLETE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_INIT_CDB"))
#ifdef DB_INIT_CDB
            return DB_INIT_CDB;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_INIT_LOCK"))
#ifdef DB_INIT_LOCK
            return DB_INIT_LOCK;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_INIT_LOG"))
#ifdef DB_INIT_LOG
            return DB_INIT_LOG;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_INIT_MPOOL"))
#ifdef DB_INIT_MPOOL
            return DB_INIT_MPOOL;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_INIT_TXN"))
#ifdef DB_INIT_TXN
            return DB_INIT_TXN;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_JOIN_ITEM"))
#ifdef DB_JOIN_ITEM
            return DB_JOIN_ITEM;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_KEYEMPTY"))
#ifdef DB_KEYEMPTY
            return DB_KEYEMPTY;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_KEYEXIST"))
#ifdef DB_KEYEXIST
            return DB_KEYEXIST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_KEYFIRST"))
#ifdef DB_KEYFIRST
            return DB_KEYFIRST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_KEYLAST"))
#ifdef DB_KEYLAST
            return DB_KEYLAST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LAST"))
#ifdef DB_LAST
            return DB_LAST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCKMAGIC"))
#ifdef DB_LOCKMAGIC
            return DB_LOCKMAGIC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCKVERSION"))
#ifdef DB_LOCKVERSION
            return DB_LOCKVERSION;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_CONFLICT"))
#ifdef DB_LOCK_CONFLICT
            return DB_LOCK_CONFLICT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_DEADLOCK"))
#ifdef DB_LOCK_DEADLOCK
            return DB_LOCK_DEADLOCK;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_DEFAULT"))
#ifdef DB_LOCK_DEFAULT
            return DB_LOCK_DEFAULT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_GET"))
            return DB_LOCK_GET;
        if (strEQ(name, "DB_LOCK_NORUN"))
#ifdef DB_LOCK_NORUN
            return DB_LOCK_NORUN;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_NOTGRANTED"))
#ifdef DB_LOCK_NOTGRANTED
            return DB_LOCK_NOTGRANTED;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_NOTHELD"))
#ifdef DB_LOCK_NOTHELD
            return DB_LOCK_NOTHELD;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_NOWAIT"))
#ifdef DB_LOCK_NOWAIT
            return DB_LOCK_NOWAIT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_OLDEST"))
#ifdef DB_LOCK_OLDEST
            return DB_LOCK_OLDEST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_RANDOM"))
#ifdef DB_LOCK_RANDOM
            return DB_LOCK_RANDOM;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_RIW_N"))
#ifdef DB_LOCK_RIW_N
            return DB_LOCK_RIW_N;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_RW_N"))
#ifdef DB_LOCK_RW_N
            return DB_LOCK_RW_N;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOCK_YOUNGEST"))
#ifdef DB_LOCK_YOUNGEST
            return DB_LOCK_YOUNGEST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOGMAGIC"))
#ifdef DB_LOGMAGIC
            return DB_LOGMAGIC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_LOGOLDVER"))
#ifdef DB_LOGOLDVER
            return DB_LOGOLDVER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MAX_PAGES"))
#ifdef DB_MAX_PAGES
            return DB_MAX_PAGES;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MAX_RECORDS"))
#ifdef DB_MAX_RECORDS
            return DB_MAX_RECORDS;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_CLEAN"))
#ifdef DB_MPOOL_CLEAN
            return DB_MPOOL_CLEAN;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_CREATE"))
#ifdef DB_MPOOL_CREATE
            return DB_MPOOL_CREATE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_DIRTY"))
#ifdef DB_MPOOL_DIRTY
            return DB_MPOOL_DIRTY;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_DISCARD"))
#ifdef DB_MPOOL_DISCARD
            return DB_MPOOL_DISCARD;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_LAST"))
#ifdef DB_MPOOL_LAST
            return DB_MPOOL_LAST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_NEW"))
#ifdef DB_MPOOL_NEW
            return DB_MPOOL_NEW;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MPOOL_PRIVATE"))
#ifdef DB_MPOOL_PRIVATE
            return DB_MPOOL_PRIVATE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MUTEXDEBUG"))
#ifdef DB_MUTEXDEBUG
            return DB_MUTEXDEBUG;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_MUTEXLOCKS"))
#ifdef DB_MUTEXLOCKS
            return DB_MUTEXLOCKS;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NEEDSPLIT"))
#ifdef DB_NEEDSPLIT
            return DB_NEEDSPLIT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NEXT"))
#ifdef DB_NEXT
            return DB_NEXT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NEXT_DUP"))
#ifdef DB_NEXT_DUP
            return DB_NEXT_DUP;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NOMMAP"))
#ifdef DB_NOMMAP
            return DB_NOMMAP;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NOOVERWRITE"))
#ifdef DB_NOOVERWRITE
            return DB_NOOVERWRITE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NOSYNC"))
#ifdef DB_NOSYNC
            return DB_NOSYNC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_NOTFOUND"))
#ifdef DB_NOTFOUND
            return DB_NOTFOUND;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_PAD"))
#ifdef DB_PAD
            return DB_PAD;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_PAGEYIELD"))
#ifdef DB_PAGEYIELD
            return DB_PAGEYIELD;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_POSITION"))
#ifdef DB_POSITION
            return DB_POSITION;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_PREV"))
#ifdef DB_PREV
            return DB_PREV;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_PRIVATE"))
#ifdef DB_PRIVATE
            return DB_PRIVATE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_QUEUE"))
            return DB_QUEUE;
        if (strEQ(name, "DB_RDONLY"))
#ifdef DB_RDONLY
            return DB_RDONLY;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RECNO"))
            return DB_RECNO;
        if (strEQ(name, "DB_RECNUM"))
#ifdef DB_RECNUM
            return DB_RECNUM;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RECORDCOUNT"))
#ifdef DB_RECORDCOUNT
            return DB_RECORDCOUNT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RECOVER"))
#ifdef DB_RECOVER
            return DB_RECOVER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RECOVER_FATAL"))
#ifdef DB_RECOVER_FATAL
            return DB_RECOVER_FATAL;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_REGISTERED"))
#ifdef DB_REGISTERED
            return DB_REGISTERED;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RENUMBER"))
#ifdef DB_RENUMBER
            return DB_RENUMBER;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RMW"))
#ifdef DB_RMW
            return DB_RMW;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_RUNRECOVERY"))
#ifdef DB_RUNRECOVERY
            return DB_RUNRECOVERY;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_SEQUENTIAL"))
#ifdef DB_SEQUENTIAL
            return DB_SEQUENTIAL;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_SET"))
#ifdef DB_SET
            return DB_SET;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_SET_RANGE"))
#ifdef DB_SET_RANGE
            return DB_SET_RANGE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_SET_RECNO"))
#ifdef DB_SET_RECNO
            return DB_SET_RECNO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_SNAPSHOT"))
#ifdef DB_SNAPSHOT
            return DB_SNAPSHOT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_SWAPBYTES"))
#ifdef DB_SWAPBYTES
            return DB_SWAPBYTES;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TEMPORARY"))
#ifdef DB_TEMPORARY
            return DB_TEMPORARY;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_THREAD"))
#ifdef DB_THREAD
            return DB_THREAD;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TRUNCATE"))
#ifdef DB_TRUNCATE
            return DB_TRUNCATE;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXNMAGIC"))
#ifdef DB_TXNMAGIC
            return DB_TXNMAGIC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXNVERSION"))
#ifdef DB_TXNVERSION
            return DB_TXNVERSION;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_BACKWARD_ROLL"))
            return DB_TXN_BACKWARD_ROLL;
        if (strEQ(name, "DB_TXN_CKP"))
#ifdef DB_TXN_CKP
            return DB_TXN_CKP;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_FORWARD_ROLL"))
            return DB_TXN_FORWARD_ROLL;
        if (strEQ(name, "DB_TXN_LOCK_2PL"))
#ifdef DB_TXN_LOCK_2PL
            return DB_TXN_LOCK_2PL;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOCK_MASK"))
#ifdef DB_TXN_LOCK_MASK
            return DB_TXN_LOCK_MASK;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOCK_OPTIMIST"))
#ifdef DB_TXN_LOCK_OPTIMIST
            return DB_TXN_LOCK_OPTIMIST;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOCK_OPTIMISTIC"))
#ifdef DB_TXN_LOCK_OPTIMISTIC
            return DB_TXN_LOCK_OPTIMISTIC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOG_MASK"))
#ifdef DB_TXN_LOG_MASK
            return DB_TXN_LOG_MASK;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOG_REDO"))
#ifdef DB_TXN_LOG_REDO
            return DB_TXN_LOG_REDO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOG_UNDO"))
#ifdef DB_TXN_LOG_UNDO
            return DB_TXN_LOG_UNDO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_LOG_UNDOREDO"))
#ifdef DB_TXN_LOG_UNDOREDO
            return DB_TXN_LOG_UNDOREDO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_NOSYNC"))
#ifdef DB_TXN_NOSYNC
            return DB_TXN_NOSYNC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_NOWAIT"))
#ifdef DB_TXN_NOWAIT
            return DB_TXN_NOWAIT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_OPENFILES"))
            return DB_TXN_OPENFILES;
        if (strEQ(name, "DB_TXN_REDO"))
#ifdef DB_TXN_REDO
            return DB_TXN_REDO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_SYNC"))
#ifdef DB_TXN_SYNC
            return DB_TXN_SYNC;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_TXN_UNDO"))
#ifdef DB_TXN_UNDO
            return DB_TXN_UNDO;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_UNKNOWN"))
            return DB_UNKNOWN;
        if (strEQ(name, "DB_USE_ENVIRON"))
#ifdef DB_USE_ENVIRON
            return DB_USE_ENVIRON;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_USE_ENVIRON_ROOT"))
#ifdef DB_USE_ENVIRON_ROOT
            return DB_USE_ENVIRON_ROOT;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_VERSION_MAJOR"))
#ifdef DB_VERSION_MAJOR
            return DB_VERSION_MAJOR;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_VERSION_MINOR"))
#ifdef DB_VERSION_MINOR
            return DB_VERSION_MINOR;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_VERSION_PATCH"))
#ifdef DB_VERSION_PATCH
            return DB_VERSION_PATCH;
#else
            goto not_there;
#endif
        if (strEQ(name, "DB_WRITECURSOR"))
#ifdef DB_WRITECURSOR
            return DB_WRITECURSOR;
#else
            goto not_there;
#endif
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    case 'a':
	break;
    case 'b':
	break;
    case 'c':
	break;
    case 'd':
	break;
    case 'e':
	break;
    case 'f':
	break;
    case 'g':
	break;
    case 'h':
	break;
    case 'i':
	break;
    case 'j':
	break;
    case 'k':
	break;
    case 'l':
	break;
    case 'm':
	break;
    case 'n':
	break;
    case 'o':
	break;
    case 'p':
	break;
    case 'q':
	break;
    case 'r':
	break;
    case 's':
	break;
    case 't':
	break;
    case 'u':
	break;
    case 'v':
	break;
    case 'w':
	break;
    case 'x':
	break;
    case 'y':
	break;
    case 'z':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}


MODULE = BerkeleyDB		PACKAGE = BerkeleyDB	PREFIX = env_

char *
DB_VERSION_STRING()
	CODE:
	  RETVAL = DB_VERSION_STRING ;
	OUTPUT:
	  RETVAL


double
constant(name,arg)
	char *		name
	int		arg

#define env_db_version(maj, min, patch) 	db_version(&maj, &min, &patch)
char *
env_db_version(maj, min, patch)
	int  maj
	int  min
	int  patch
	OUTPUT:
	  RETVAL
	  maj
	  min
	  patch

int
db_value_set(value, which)
	int value
	int which
        NOT_IMPLEMENTED_YET


DualType
_db_remove(ref)
	SV * 		ref
	CODE:
	{
#if DB_VERSION_MAJOR == 2
	    softCrash("BerkeleyDB::db_remove needs Berkeley DB 3.x or better") ;
#else
	    HV *		hash ;
    	    DB *		dbp ;
	    SV * 		sv ;
	    const char *	db ;
	    const char *	subdb 	= NULL ;
	    BerkeleyDB__Env	env 	= NULL ;
    	    DB_ENV *		dbenv   = NULL ;
	    u_int32_t		flags	= 0 ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(db,    "Filename", char *) ;
	    SetValue_pv(subdb, "Subname", char *) ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_ov(env, "Env", BerkeleyDB__Env) ;
    	    if (env)
		dbenv = env->Env ;
            RETVAL = db_create(&dbp, dbenv, 0) ;
	    if (RETVAL == 0) {
	        RETVAL = dbp->remove(dbp, db, subdb, flags) ;
	    }
#endif
	}
	OUTPUT:
	    RETVAL

MODULE = BerkeleyDB::Env		PACKAGE = BerkeleyDB::Env PREFIX = env_


BerkeleyDB::Env::Raw
_db_appinit(self, ref)
	char *		self
	SV * 		ref
	CODE:
	{
	    HV *	hash ;
	    SV *	sv ;
	    char *	home = NULL ;
	    char * 	server = NULL ;
	    char **	config = NULL ;
	    int		flags = 0 ;
	    int		cachesize = 0 ;
	    int		lk_detect = 0 ;
	    int		mode = 0 ;
	    SV *	errprefix = NULL;
	    DB_ENV *	env ;
	    int status ;

	    Trace(("in _db_appinit [%s] %d\n", self, ref)) ;
	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(home,      "Home", char *) ;
	    SetValue_pv(config,    "Config", char **) ;
	    SetValue_sv(errprefix, "ErrPrefix") ;
	    SetValue_iv(flags,     "Flags") ;
	    SetValue_pv(server,    "Server", char *) ;
	    SetValue_iv(cachesize, "Cachesize") ;
	    SetValue_iv(lk_detect, "LockDetect") ;
#ifndef AT_LEAST_DB_3_1
	    if (server)
	        softCrash("-Server needs Berkeley DB 3.1 or better") ;
#endif /* ! AT_LEAST_DB_3_1 */
	    Trace(("_db_appinit(config=[%d], home=[%s],errprefix=[%s],flags=[%d]\n",
			config, home, errprefix, flags)) ;
#ifdef TRACE
	    if (config) {
	       int i ;
	      for (i = 0 ; i < 10 ; ++ i) {
		if (config[i] == NULL) {
		    printf("    End\n") ;
		    break ;
		}
	        printf("    config = [%s]\n", config[i]) ;
	      }
	    }
#endif /* TRACE */
	    ZMALLOC(RETVAL, BerkeleyDB_ENV_type) ;
	    if (flags & DB_INIT_TXN)
	        RETVAL->txn_enabled = TRUE ;
#if DB_VERSION_MAJOR == 2
	  ZMALLOC(RETVAL->Env, DB_ENV) ;
	  env = RETVAL->Env ;
	  {
	    /* Take a copy of the error prefix */
	    if (errprefix) {
	        Trace(("copying errprefix\n" )) ;
		RETVAL->ErrPrefix = newSVsv(errprefix) ;
		SvPOK_only(RETVAL->ErrPrefix) ;
	    }
	    if (RETVAL->ErrPrefix)
	        RETVAL->Env->db_errpfx = SvPVX(RETVAL->ErrPrefix) ;

	    if ((sv = readHash(hash, "ErrFile")) && sv != &PL_sv_undef) {
		env->db_errfile = IoOFP(sv_2io(sv)) ;
		RETVAL->ErrHandle = newRV(sv) ;
	    }
	    /* SetValue_io(RETVAL->Env.db_errfile, "ErrFile") ; */
	    SetValue_iv(env->db_verbose, "Verbose") ;
	    /* env->db_errbuf = RETVAL->ErrBuff ; */
	    env->db_errcall = db_errcall_cb ;
	    RETVAL->active = TRUE ;
	    status = db_appinit(home, config, env, flags) ;
	    Trace(("  status = %d env %d Env %d\n", status, RETVAL, env)) ;
	    if (status == 0)
	        hash_store_iv("BerkeleyDB::Term::Env", (IV)RETVAL, 1) ;
	    else {
                if (RETVAL->ErrHandle)
                    SvREFCNT_dec(RETVAL->ErrHandle) ;
                if (RETVAL->ErrPrefix)
                    SvREFCNT_dec(RETVAL->ErrPrefix) ;
                Safefree(RETVAL->Env) ;
                Safefree(RETVAL) ;
		RETVAL = NULL ;
	    }
	  }
#else /* DB_VERSION_MAJOR > 2 */
#ifndef AT_LEAST_DB_3_1
#    define DB_CLIENT	0
#endif
	  status = db_env_create(&RETVAL->Env, server ? DB_CLIENT : 0) ;
	  Trace(("db_env_create flags = %d returned %s\n", flags,
	  					my_db_strerror(status))) ;
	  env = RETVAL->Env ;
	  if (status == 0 && cachesize) {
	      status = env->set_cachesize(env, 0, cachesize, 0) ;
	      Trace(("set_cachesize [%d] returned %s\n",
			cachesize, my_db_strerror(status)));
	  }

	  if (status == 0 && lk_detect) {
	      status = env->set_lk_detect(env, lk_detect) ;
	      Trace(("set_lk_detect [%d] returned %s\n",
	              lk_detect, my_db_strerror(status)));
	  }
#ifdef AT_LEAST_DB_3_1
	  /* set the server */
	  if (server && status == 0)
	  {
	      status = env->set_server(env, server, 0, 0, 0);
	      Trace(("ENV->set_server server = %s returned %s\n", server,
	  					my_db_strerror(status))) ;
	  }
#endif
	  if (status == 0)
	  {
	    /* Take a copy of the error prefix */
	    if (errprefix) {
	        Trace(("copying errprefix\n" )) ;
		RETVAL->ErrPrefix = newSVsv(errprefix) ;
		SvPOK_only(RETVAL->ErrPrefix) ;
	    }
	    if (RETVAL->ErrPrefix)
	        env->set_errpfx(env, SvPVX(RETVAL->ErrPrefix)) ;

	    if ((sv = readHash(hash, "ErrFile")) && sv != &PL_sv_undef) {
		env->set_errfile(env, IoOFP(sv_2io(sv))) ;
		RETVAL->ErrHandle = newRV(sv) ;
	    }
	    /* SetValue_iv(RETVAL->Env.db_verbose, "Verbose") ; */ /* TODO */
	    SetValue_iv(mode, "Mode") ;
	    /* RETVAL->Env.db_errbuf = RETVAL->ErrBuff ; */
	    env->set_errcall(env, db_errcall_cb) ;
	    RETVAL->active = TRUE ;
#ifdef IS_DB_3_0
	    status = (env->open)(env, home, config, flags, mode) ;
#else /* > 3.0 */
	    status = (env->open)(env, home, flags, mode) ;
#endif
	    Trace(("ENV->open returned %s\n", my_db_strerror(status))) ;
	  }

	  if (status == 0)
	      hash_store_iv("BerkeleyDB::Term::Env", (IV)RETVAL, 1) ;
	  else {
	      (env->close)(env, 0) ;
              if (RETVAL->ErrHandle)
                  SvREFCNT_dec(RETVAL->ErrHandle) ;
              if (RETVAL->ErrPrefix)
                  SvREFCNT_dec(RETVAL->ErrPrefix) ;
              Safefree(RETVAL) ;
	      RETVAL = NULL ;
	  }
#endif /* DB_VERSION_MAJOR > 2 */
	}
	OUTPUT:
	    RETVAL

BerkeleyDB::Txn::Raw
_txn_begin(env, pid=NULL, flags=0)
	BerkeleyDB::Env		env
	BerkeleyDB::Txn		pid
	u_int32_t		flags
	CODE:
	{
	    DB_TXN *txn ;
	    DB_TXN *p_id = NULL ;
	    Trace(("txn_begin pid %d, flags %d\n", pid, flags)) ;
#if DB_VERSION_MAJOR == 2
	    if (env->Env->tx_info == NULL)
		softCrash("Transaction Manager not enabled") ;
#endif
	    if (!env->txn_enabled)
		softCrash("Transaction Manager not enabled") ;
	    if (pid)
		p_id = pid->txn ;
	    env->TxnMgrStatus =
#if DB_VERSION_MAJOR == 2
	    	txn_begin(env->Env->tx_info, p_id, &txn) ;
#else
	    	txn_begin(env->Env, p_id, &txn, flags) ;
#endif
	    if (env->TxnMgrStatus == 0) {
	      ZMALLOC(RETVAL, BerkeleyDB_Txn_type) ;
	      RETVAL->txn  = txn ;
	      RETVAL->active = TRUE ;
	      Trace(("_txn_begin created txn [%d] in [%d]\n", txn, RETVAL));
	      hash_store_iv("BerkeleyDB::Term::Txn", (IV)RETVAL, 1) ;
	    }
	    else
		RETVAL = NULL ;
	}
	OUTPUT:
	    RETVAL


#if DB_VERSION_MAJOR == 2
#  define env_txn_checkpoint(e,k,m) txn_checkpoint(e->Env->tx_info, k, m)
#else /* DB 3.0 or better */
#  ifdef AT_LEAST_DB_3_1
#    define env_txn_checkpoint(e,k,m) txn_checkpoint(e->Env, k, m, 0)
#  else
#    define env_txn_checkpoint(e,k,m) txn_checkpoint(e->Env, k, m)
#  endif
#endif
DualType
env_txn_checkpoint(env, kbyte, min)
	BerkeleyDB::Env		env
	long			kbyte
	long			min

HV *
txn_stat(env)
	BerkeleyDB::Env		env
	HV *			RETVAL = NULL ;
	CODE:
	{
	    DB_TXN_STAT *	stat ;
#if DB_VERSION_MAJOR == 2
	    if(txn_stat(env->Env->tx_info, &stat, safemalloc) == 0) {
#else
	    if(txn_stat(env->Env, &stat, safemalloc) == 0) {
#endif
	    	RETVAL = (HV*)sv_2mortal((SV*)newHV()) ;
		hv_store_iv(RETVAL, "st_time_ckp", stat->st_time_ckp) ;
		hv_store_iv(RETVAL, "st_last_txnid", stat->st_last_txnid) ;
		hv_store_iv(RETVAL, "st_maxtxns", stat->st_maxtxns) ;
		hv_store_iv(RETVAL, "st_naborts", stat->st_naborts) ;
		hv_store_iv(RETVAL, "st_nbegins", stat->st_nbegins) ;
		hv_store_iv(RETVAL, "st_ncommits", stat->st_ncommits) ;
		hv_store_iv(RETVAL, "st_nactive", stat->st_nactive) ;
#if DB_VERSION_MAJOR > 2
		hv_store_iv(RETVAL, "st_maxnactive", stat->st_maxnactive) ;
		hv_store_iv(RETVAL, "st_regsize", stat->st_regsize) ;
		hv_store_iv(RETVAL, "st_region_wait", stat->st_region_wait) ;
		hv_store_iv(RETVAL, "st_region_nowait", stat->st_region_nowait) ;
#endif
		safefree(stat) ;
	    }
	}
	OUTPUT:
	    RETVAL

#define EnDis(x)	((x) ? "Enabled" : "Disabled")
void
printEnv(env)
        BerkeleyDB::Env  env
	INIT:
	    ckActive_Environment(env->active) ;
	CODE:
#if 0
	  printf("env             [0x%X]\n", env) ;
	  printf("  ErrPrefix     [%s]\n", env->ErrPrefix
				           ? SvPVX(env->ErrPrefix) : 0) ;
	  printf("  DB_ENV\n") ;
	  printf("    db_lorder   [%d]\n", env->Env.db_lorder) ;
	  printf("    db_home     [%s]\n", env->Env.db_home) ;
	  printf("    db_data_dir [%s]\n", env->Env.db_data_dir) ;
	  printf("    db_log_dir  [%s]\n", env->Env.db_log_dir) ;
	  printf("    db_tmp_dir  [%s]\n", env->Env.db_tmp_dir) ;
	  printf("    lk_info     [%s]\n", EnDis(env->Env.lk_info)) ;
	  printf("    lk_max      [%d]\n", env->Env.lk_max) ;
	  printf("    lg_info     [%s]\n", EnDis(env->Env.lg_info)) ;
	  printf("    lg_max      [%d]\n", env->Env.lg_max) ;
	  printf("    mp_info     [%s]\n", EnDis(env->Env.mp_info)) ;
	  printf("    mp_size     [%d]\n", env->Env.mp_size) ;
	  printf("    tx_info     [%s]\n", EnDis(env->Env.tx_info)) ;
	  printf("    tx_max      [%d]\n", env->Env.tx_max) ;
	  printf("    flags       [%d]\n", env->Env.flags) ;
	  printf("\n") ;
#endif

SV *
errPrefix(env, prefix)
        BerkeleyDB::Env  env
	SV * 		 prefix
	INIT:
	    ckActive_Environment(env->active) ;
	CODE:
	  if (env->ErrPrefix) {
	      RETVAL = newSVsv(env->ErrPrefix) ;
              SvPOK_only(RETVAL) ;
	      sv_setsv(env->ErrPrefix, prefix) ;
	  }
	  else {
	      RETVAL = NULL ;
	      env->ErrPrefix = newSVsv(prefix) ;
	  }
	  SvPOK_only(env->ErrPrefix) ;
#if DB_VERSION_MAJOR == 2
	  env->Env->db_errpfx = SvPVX(env->ErrPrefix) ;
#else
	  env->Env->set_errpfx(env->Env, SvPVX(env->ErrPrefix)) ;
#endif
	OUTPUT:
	  RETVAL

DualType
status(env)
        BerkeleyDB::Env 	env
	CODE:
	    RETVAL =  env->Status ;
	OUTPUT:
	    RETVAL

DualType
db_appexit(env)
        BerkeleyDB::Env 	env
	INIT:
	    ckActive_Environment(env->active) ;
	CODE:
#ifdef STRICT_CLOSE
	    if (env->open_dbs)
		softCrash("attempted to close an environment with %d open database(s)",
			env->open_dbs) ;
#endif /* STRICT_CLOSE */
#if DB_VERSION_MAJOR == 2
	    RETVAL = db_appexit(env->Env) ;
#else
	    RETVAL = (env->Env->close)(env->Env, 0) ;
#endif
	    env->active = FALSE ;
	    hash_delete("BerkeleyDB::Term::Env", (IV)env) ;
	OUTPUT:
	    RETVAL


void
_DESTROY(env)
        BerkeleyDB::Env  env
	int RETVAL = 0 ;
	CODE:
	  Trace(("In BerkeleyDB::Env::DESTROY\n"));
	  Trace(("    env %ld Env %ld dirty %d\n", env, &env->Env, PL_dirty)) ;
	  if (env->active)
#if DB_VERSION_MAJOR == 2
              db_appexit(env->Env) ;
#else
	      (env->Env->close)(env->Env, 0) ;
#endif
          if (env->ErrHandle)
              SvREFCNT_dec(env->ErrHandle) ;
          if (env->ErrPrefix)
              SvREFCNT_dec(env->ErrPrefix) ;
#if DB_VERSION_MAJOR == 2
          Safefree(env->Env) ;
#endif
          Safefree(env) ;
	  hash_delete("BerkeleyDB::Term::Env", (IV)env) ;
	  Trace(("End of BerkeleyDB::Env::DESTROY %d\n", RETVAL)) ;

BerkeleyDB::TxnMgr::Raw
_TxnMgr(env)
        BerkeleyDB::Env  env
	INIT:
	    ckActive_Environment(env->active) ;
	    if (!env->txn_enabled)
		softCrash("Transaction Manager not enabled") ;
	CODE:
	    ZMALLOC(RETVAL, BerkeleyDB_TxnMgr_type) ;
	    RETVAL->env  = env ;
	    /* hash_store_iv("BerkeleyDB::Term::TxnMgr", (IV)txn, 1) ; */
	OUTPUT:
	    RETVAL

int
set_data_dir(env, dir)
        BerkeleyDB::Env  env
	char *		 dir
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3_1
	    softCrash("$env->set_data_dir needs Berkeley DB 3.1 or better") ;
#else
	    RETVAL = env->Status = env->Env->set_data_dir(env->Env, dir);
#endif
	OUTPUT:
	    RETVAL

int
set_lg_dir(env, dir)
        BerkeleyDB::Env  env
	char *		 dir
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3_1
	    softCrash("$env->set_lg_dir needs Berkeley DB 3.1 or better") ;
#else
	    RETVAL = env->Status = env->Env->set_lg_dir(env->Env, dir);
#endif
	OUTPUT:
	    RETVAL

int
set_tmp_dir(env, dir)
        BerkeleyDB::Env  env
	char *		 dir
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3_1
	    softCrash("$env->set_tmp_dir needs Berkeley DB 3.1 or better") ;
#else
	    RETVAL = env->Status = env->Env->set_tmp_dir(env->Env, dir);
#endif
	OUTPUT:
	    RETVAL

int
set_mutexlocks(env, do_lock)
        BerkeleyDB::Env  env
	int 		 do_lock
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3
	    softCrash("$env->set_setmutexlocks needs Berkeley DB 3.0 or better") ;
#else
#if defined(IS_DB_3_0) || defined(AT_LEAST_DB_3_2)
	    RETVAL = env->Status = env->Env->set_mutexlocks(env->Env, do_lock);
#else /* DB 3.1 */
	    RETVAL = env->Status = db_env_set_mutexlocks(do_lock);
#endif
#endif
	OUTPUT:
	    RETVAL

MODULE = BerkeleyDB::Term		PACKAGE = BerkeleyDB::Term

void
close_everything()

#define safeCroak(string)	softCrash(string)
void
safeCroak(string)
	char * string

MODULE = BerkeleyDB::Hash	PACKAGE = BerkeleyDB::Hash	PREFIX = hash_

BerkeleyDB::Hash::Raw
_db_open_hash(self, ref)
	char *		self
	SV * 		ref
	CODE:
	{
	    HV *		hash ;
	    SV * 		sv ;
	    DB_INFO 		info ;
	    BerkeleyDB__Env	dbenv = NULL;
	    SV *		ref_dbenv = NULL;
	    const char *	file = NULL ;
	    const char *	subname = NULL ;
	    int			flags = 0 ;
	    int			mode = 0 ;
    	    BerkeleyDB 		db ;

    	    Trace(("_db_open_hash start\n")) ;
	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Filename", char *) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.h_ffactor, "Ffactor") ;
	    SetValue_iv(info.h_nelem, "Nelem") ;
	    SetValue_iv(info.flags, "Property") ;
	    ZMALLOC(db, BerkeleyDB_type) ;
	    if ((sv = readHash(hash, "Hash")) && sv != &PL_sv_undef) {
		info.h_hash = hash_cb ;
		db->hash = newSVsv(sv) ;
	    }
	    /* DB_DUPSORT was introduced in DB 2.5.9 */
	    if ((sv = readHash(hash, "DupCompare")) && sv != &PL_sv_undef) {
#ifdef DB_DUPSORT
		info.dup_compare = dup_compare ;
		db->dup_compare = newSVsv(sv) ;
		info.flags |= DB_DUP|DB_DUPSORT ;
#else
	        croak("DupCompare needs Berkeley DB 2.5.9 or later") ;
#endif
	    }
	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, file, subname, DB_HASH, flags, mode, &info) ;
    	    Trace(("_db_open_hash end\n")) ;
	}
	OUTPUT:
	    RETVAL


HV *
db_stat(db, flags=0)
	BerkeleyDB::Common	db
	int			flags
	HV *			RETVAL = NULL ;
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	{
#if DB_VERSION_MAJOR == 2
	    softCrash("$db->db_stat for a Hash needs Berkeley DB 3.x or better") ;
#else
	    DB_HASH_STAT *	stat ;
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, safemalloc, flags) ;
	    if (db->Status == 0) {
	    	RETVAL = (HV*)sv_2mortal((SV*)newHV()) ;
		hv_store_iv(RETVAL, "hash_magic", stat->hash_magic) ;
		hv_store_iv(RETVAL, "hash_version", stat->hash_version);
		hv_store_iv(RETVAL, "hash_pagesize", stat->hash_pagesize);
#ifdef AT_LEAST_DB_3_1
		hv_store_iv(RETVAL, "hash_nkeys", stat->hash_nkeys);
		hv_store_iv(RETVAL, "hash_ndata", stat->hash_ndata);
#else
		hv_store_iv(RETVAL, "hash_nrecs", stat->hash_nrecs);
#endif
		hv_store_iv(RETVAL, "hash_nelem", stat->hash_nelem);
		hv_store_iv(RETVAL, "hash_ffactor", stat->hash_ffactor);
		hv_store_iv(RETVAL, "hash_buckets", stat->hash_buckets);
		hv_store_iv(RETVAL, "hash_free", stat->hash_free);
		hv_store_iv(RETVAL, "hash_bfree", stat->hash_bfree);
		hv_store_iv(RETVAL, "hash_bigpages", stat->hash_bigpages);
		hv_store_iv(RETVAL, "hash_big_bfree", stat->hash_big_bfree);
		hv_store_iv(RETVAL, "hash_overflows", stat->hash_overflows);
		hv_store_iv(RETVAL, "hash_ovfl_free", stat->hash_ovfl_free);
		hv_store_iv(RETVAL, "hash_dup", stat->hash_dup);
		hv_store_iv(RETVAL, "hash_dup_free", stat->hash_dup_free);
#if DB_VERSION_MAJOR >= 3
		hv_store_iv(RETVAL, "hash_metaflags", stat->hash_metaflags);
#endif
		safefree(stat) ;
	    }
#endif
	}
	OUTPUT:
	    RETVAL


MODULE = BerkeleyDB::Unknown	PACKAGE = BerkeleyDB::Unknown	PREFIX = hash_

void
_db_open_unknown(ref)
	SV * 		ref
	PPCODE:
	{
	    HV *		hash ;
	    SV * 		sv ;
	    DB_INFO 		info ;
	    BerkeleyDB__Env	dbenv = NULL;
	    SV *		ref_dbenv = NULL;
	    const char *	file = NULL ;
	    const char *	subname = NULL ;
	    int			flags = 0 ;
	    int			mode = 0 ;
    	    BerkeleyDB 		db ;
	    BerkeleyDB		RETVAL ;
	    static char * 		Names[] = {"", "Btree", "Hash", "Recno"} ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Filename", char *) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.h_ffactor, "Ffactor") ;
	    SetValue_iv(info.h_nelem, "Nelem") ;
	    SetValue_iv(info.flags, "Property") ;
	    ZMALLOC(db, BerkeleyDB_type) ;

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, file, subname, DB_UNKNOWN, flags, mode, &info) ;
	    XPUSHs(sv_2mortal(newSViv((IV)RETVAL)));
	    if (RETVAL)
	        XPUSHs(sv_2mortal(newSVpv(Names[RETVAL->type], 0))) ;
	    else
	        XPUSHs(sv_2mortal(newSViv((IV)NULL)));
	}



MODULE = BerkeleyDB::Btree	PACKAGE = BerkeleyDB::Btree	PREFIX = btree_

BerkeleyDB::Btree::Raw
_db_open_btree(self, ref)
	char *		self
	SV * 		ref
	CODE:
	{
	    HV *		hash ;
	    SV * 		sv ;
	    DB_INFO 		info ;
	    BerkeleyDB__Env	dbenv = NULL;
	    SV *		ref_dbenv = NULL;
	    const char *	file = NULL ;
	    const char *	subname = NULL ;
	    int			flags = 0 ;
	    int			mode = 0 ;
    	    BerkeleyDB  	db ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Filename", char*) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.bt_minkey, "Minkey") ;
	    SetValue_iv(info.flags, "Property") ;
	    ZMALLOC(db, BerkeleyDB_type) ;
	    if ((sv = readHash(hash, "Compare")) && sv != &PL_sv_undef) {
		info.bt_compare = btree_compare ;
		db->compare = newSVsv(sv) ;
	    }
	    /* DB_DUPSORT was introduced in DB 2.5.9 */
	    if ((sv = readHash(hash, "DupCompare")) && sv != &PL_sv_undef) {
#ifdef DB_DUPSORT
		info.dup_compare = dup_compare ;
		db->dup_compare = newSVsv(sv) ;
		info.flags |= DB_DUP|DB_DUPSORT ;
#else
	        softCrash("DupCompare needs Berkeley DB 2.5.9 or later") ;
#endif
	    }
	    if ((sv = readHash(hash, "Prefix")) && sv != &PL_sv_undef) {
		info.bt_prefix = btree_prefix ;
		db->prefix = newSVsv(sv) ;
	    }

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, file, subname, DB_BTREE, flags, mode, &info) ;
	}
	OUTPUT:
	    RETVAL


HV *
db_stat(db, flags=0)
	BerkeleyDB::Common	db
	int			flags
	HV *			RETVAL = NULL ;
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	{
	    DB_BTREE_STAT *	stat ;
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, safemalloc, flags) ;
	    if (db->Status == 0) {
	    	RETVAL = (HV*)sv_2mortal((SV*)newHV()) ;
		hv_store_iv(RETVAL, "bt_magic", stat->bt_magic);
		hv_store_iv(RETVAL, "bt_version", stat->bt_version);
#if DB_VERSION_MAJOR > 2
		hv_store_iv(RETVAL, "bt_metaflags", stat->bt_metaflags) ;
		hv_store_iv(RETVAL, "bt_flags", stat->bt_metaflags) ;
#else
		hv_store_iv(RETVAL, "bt_flags", stat->bt_flags) ;
#endif
		hv_store_iv(RETVAL, "bt_maxkey", stat->bt_maxkey) ;
		hv_store_iv(RETVAL, "bt_minkey", stat->bt_minkey);
		hv_store_iv(RETVAL, "bt_re_len", stat->bt_re_len);
		hv_store_iv(RETVAL, "bt_re_pad", stat->bt_re_pad);
		hv_store_iv(RETVAL, "bt_pagesize", stat->bt_pagesize);
		hv_store_iv(RETVAL, "bt_levels", stat->bt_levels);
#ifdef AT_LEAST_DB_3_1
		hv_store_iv(RETVAL, "bt_nkeys", stat->bt_nkeys);
		hv_store_iv(RETVAL, "bt_ndata", stat->bt_ndata);
#else
		hv_store_iv(RETVAL, "bt_nrecs", stat->bt_nrecs);
#endif
		hv_store_iv(RETVAL, "bt_int_pg", stat->bt_int_pg);
		hv_store_iv(RETVAL, "bt_leaf_pg", stat->bt_leaf_pg);
		hv_store_iv(RETVAL, "bt_dup_pg", stat->bt_dup_pg);
		hv_store_iv(RETVAL, "bt_over_pg", stat->bt_over_pg);
		hv_store_iv(RETVAL, "bt_free", stat->bt_free);
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 5
		hv_store_iv(RETVAL, "bt_freed", stat->bt_freed);
		hv_store_iv(RETVAL, "bt_pfxsaved", stat->bt_pfxsaved);
		hv_store_iv(RETVAL, "bt_split", stat->bt_split);
		hv_store_iv(RETVAL, "bt_rootsplit", stat->bt_rootsplit);
		hv_store_iv(RETVAL, "bt_fastsplit", stat->bt_fastsplit);
		hv_store_iv(RETVAL, "bt_added", stat->bt_added);
		hv_store_iv(RETVAL, "bt_deleted", stat->bt_deleted);
		hv_store_iv(RETVAL, "bt_get", stat->bt_get);
		hv_store_iv(RETVAL, "bt_cache_hit", stat->bt_cache_hit);
		hv_store_iv(RETVAL, "bt_cache_miss", stat->bt_cache_miss);
#endif
		hv_store_iv(RETVAL, "bt_int_pgfree", stat->bt_int_pgfree);
		hv_store_iv(RETVAL, "bt_leaf_pgfree", stat->bt_leaf_pgfree);
		hv_store_iv(RETVAL, "bt_dup_pgfree", stat->bt_dup_pgfree);
		hv_store_iv(RETVAL, "bt_over_pgfree", stat->bt_over_pgfree);
		safefree(stat) ;
	    }
	}
	OUTPUT:
	    RETVAL


MODULE = BerkeleyDB::Recno	PACKAGE = BerkeleyDB::Recno	PREFIX = recno_

BerkeleyDB::Recno::Raw
_db_open_recno(self, ref)
	char *		self
	SV * 		ref
	CODE:
	{
	    HV *		hash ;
	    SV * 		sv ;
	    DB_INFO 		info ;
	    BerkeleyDB__Env	dbenv = NULL;
	    SV *		ref_dbenv = NULL;
	    const char *	file = NULL ;
	    const char *	subname = NULL ;
	    int			flags = 0 ;
	    int			mode = 0 ;
    	    BerkeleyDB 		db ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Fname", char*) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.bt_minkey, "Minkey") ;

	    SetValue_iv(info.flags, "Property") ;
	    SetValue_pv(info.re_source, "Source", char*) ;
	    if ((sv = readHash(hash, "Len")) && sv != &PL_sv_undef) {
		info.re_len = SvIV(sv) ; ;
		flagSet_DB2(info.flags, DB_FIXEDLEN) ;
	    }
	    if ((sv = readHash(hash, "Delim")) && sv != &PL_sv_undef) {
		info.re_delim = SvPOK(sv) ? *SvPV(sv,PL_na) : SvIV(sv) ; ;
		flagSet_DB2(info.flags, DB_DELIMITER) ;
	    }
	    if ((sv = readHash(hash, "Pad")) && sv != &PL_sv_undef) {
		info.re_pad = (u_int32_t)SvPOK(sv) ? *SvPV(sv,PL_na) : SvIV(sv) ; ;
		flagSet_DB2(info.flags, DB_PAD) ;
	    }
	    ZMALLOC(db, BerkeleyDB_type) ;
#ifdef ALLOW_RECNO_OFFSET
	    SetValue_iv(db->array_base, "ArrayBase") ;
	    db->array_base = (db->array_base == 0 ? 1 : 0) ;
#endif /* ALLOW_RECNO_OFFSET */

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, file, subname, DB_RECNO, flags, mode, &info) ;
	}
	OUTPUT:
	    RETVAL


MODULE = BerkeleyDB::Queue	PACKAGE = BerkeleyDB::Queue	PREFIX = recno_

BerkeleyDB::Queue::Raw
_db_open_queue(self, ref)
	char *		self
	SV * 		ref
	CODE:
	{
#ifndef AT_LEAST_DB_3
            softCrash("BerkeleyDB::Queue needs Berkeley DB 3.0.x or better");
#else
	    HV *		hash ;
	    SV * 		sv ;
	    DB_INFO 		info ;
	    BerkeleyDB__Env	dbenv = NULL;
	    SV *		ref_dbenv = NULL;
	    const char *	file = NULL ;
	    const char *	subname = NULL ;
	    int			flags = 0 ;
	    int			mode = 0 ;
    	    BerkeleyDB 		db ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Fname", char*) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.bt_minkey, "Minkey") ;
    	    SetValue_iv(info.q_extentsize, "ExtentSize") ;


	    SetValue_iv(info.flags, "Property") ;
	    if ((sv = readHash(hash, "Len")) && sv != &PL_sv_undef) {
		info.re_len = SvIV(sv) ; ;
		flagSet_DB2(info.flags, DB_PAD) ;
	    }
	    if ((sv = readHash(hash, "Pad")) && sv != &PL_sv_undef) {
		info.re_pad = (u_int32_t)SvPOK(sv) ? *SvPV(sv,PL_na) : SvIV(sv) ; ;
		flagSet_DB2(info.flags, DB_PAD) ;
	    }
	    ZMALLOC(db, BerkeleyDB_type) ;
#ifdef ALLOW_RECNO_OFFSET
	    SetValue_iv(db->array_base, "ArrayBase") ;
	    db->array_base = (db->array_base == 0 ? 1 : 0) ;
#endif /* ALLOW_RECNO_OFFSET */

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, file, subname, DB_QUEUE, flags, mode, &info) ;
#endif
	}
	OUTPUT:
	    RETVAL

HV *
db_stat(db, flags=0)
	BerkeleyDB::Common	db
	int			flags
	HV *			RETVAL = NULL ;
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	{
#if DB_VERSION_MAJOR == 2
	    softCrash("$db->db_stat for a Queue needs Berkeley DB 3.x or better") ;
#else /* Berkeley DB 3, or better */
	    DB_QUEUE_STAT *	stat ;
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, safemalloc, flags) ;
	    if (db->Status == 0) {
	    	RETVAL = (HV*)sv_2mortal((SV*)newHV()) ;
		hv_store_iv(RETVAL, "qs_magic", stat->qs_magic) ;
		hv_store_iv(RETVAL, "qs_version", stat->qs_version);
#ifdef AT_LEAST_DB_3_1
		hv_store_iv(RETVAL, "qs_nkeys", stat->qs_nkeys);
		hv_store_iv(RETVAL, "qs_ndata", stat->qs_ndata);
#else
		hv_store_iv(RETVAL, "qs_nrecs", stat->qs_nrecs);
#endif
		hv_store_iv(RETVAL, "qs_pages", stat->qs_pages);
		hv_store_iv(RETVAL, "qs_pagesize", stat->qs_pagesize);
		hv_store_iv(RETVAL, "qs_pgfree", stat->qs_pgfree);
		hv_store_iv(RETVAL, "qs_re_len", stat->qs_re_len);
		hv_store_iv(RETVAL, "qs_re_pad", stat->qs_re_pad);
#ifdef AT_LEAST_DB_3_2
#else
		hv_store_iv(RETVAL, "qs_start", stat->qs_start);
#endif
		hv_store_iv(RETVAL, "qs_first_recno", stat->qs_first_recno);
		hv_store_iv(RETVAL, "qs_cur_recno", stat->qs_cur_recno);
#if DB_VERSION_MAJOR >= 3
		hv_store_iv(RETVAL, "qs_metaflags", stat->qs_metaflags);
#endif
		safefree(stat) ;
	    }
#endif
	}
	OUTPUT:
	    RETVAL


MODULE = BerkeleyDB::Common  PACKAGE = BerkeleyDB::Common	PREFIX = dab_


DualType
db_close(db,flags=0)
        BerkeleyDB::Common 	db
	int 			flags
	INIT:
	    ckActive_Database(db->active) ;
	    CurrentDB = db ;
	CODE:
	    Trace(("BerkeleyDB::Common::db_close %d\n", db));
#ifdef STRICT_CLOSE
	    if (db->txn)
		softCrash("attempted to close a database while a transaction was still open") ;
	    if (db->open_cursors)
		softCrash("attempted to close a database with %d open cursor(s)",
				db->open_cursors) ;
#endif /* STRICT_CLOSE */
	    RETVAL =  db->Status = ((db->dbp)->close)(db->dbp, flags) ;
	    if (db->parent_env && db->parent_env->open_dbs)
		-- db->parent_env->open_dbs ;
	    db->active = FALSE ;
	    hash_delete("BerkeleyDB::Term::Db", (IV)db) ;
	    -- db->open_cursors ;
	    Trace(("end of BerkeleyDB::Common::db_close\n"));
	OUTPUT:
	    RETVAL

void
dab__DESTROY(db)
	BerkeleyDB::Common	db
	CODE:
	  CurrentDB = db ;
	  Trace(("In BerkeleyDB::Common::_DESTROY db %d dirty=%d\n", db, PL_dirty)) ;
	  destroyDB(db) ;
	  Trace(("End of BerkeleyDB::Common::DESTROY \n")) ;

#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6
#define db_cursor(db, txn, cur,flags)  ((db->dbp)->cursor)(db->dbp, txn, cur)
#else
#define db_cursor(db, txn, cur,flags)  ((db->dbp)->cursor)(db->dbp, txn, cur,flags)
#endif
BerkeleyDB::Cursor::Raw
_db_cursor(db, flags=0)
        BerkeleyDB::Common 	db
	u_int32_t		flags
        BerkeleyDB::Cursor 	RETVAL = NULL ;
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
	{
	  DBC *		cursor ;
	  CurrentDB = db ;
	  if ((db->Status = db_cursor(db, db->txn, &cursor, flags)) == 0){
	      ZMALLOC(RETVAL, BerkeleyDB__Cursor_type) ;
	      db->open_cursors ++ ;
	      RETVAL->parent_db  = db ;
	      RETVAL->cursor  = cursor ;
	      RETVAL->dbp     = db->dbp ;
              RETVAL->type    = db->type ;
              RETVAL->recno_or_queue    = db->recno_or_queue ;
              RETVAL->filename    = my_strdup(db->filename) ;
              RETVAL->compare = db->compare ;
              RETVAL->dup_compare = db->dup_compare ;
              RETVAL->prefix  = db->prefix ;
              RETVAL->hash    = db->hash ;
	      RETVAL->partial = db->partial ;
	      RETVAL->doff    = db->doff ;
	      RETVAL->dlen    = db->dlen ;
	      RETVAL->active  = TRUE ;
#ifdef ALLOW_RECNO_OFFSET
	      RETVAL->array_base  = db->array_base ;
#endif /* ALLOW_RECNO_OFFSET */
#ifdef DBM_FILTERING
	      RETVAL->filtering   = FALSE ;
	      RETVAL->filter_fetch_key    = db->filter_fetch_key ;
	      RETVAL->filter_store_key    = db->filter_store_key ;
	      RETVAL->filter_fetch_value  = db->filter_fetch_value ;
	      RETVAL->filter_store_value  = db->filter_store_value ;
#endif
              /* RETVAL->info ; */
	      hash_store_iv("BerkeleyDB::Term::Cursor", (IV)RETVAL, 1) ;
	  }
	}
	OUTPUT:
	  RETVAL

BerkeleyDB::Cursor::Raw
_db_join(db, cursors, flags=0)
        BerkeleyDB::Common 	db
	AV *			cursors
	u_int32_t		flags
        BerkeleyDB::Cursor 	RETVAL = NULL ;
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
	{
#if DB_VERSION_MAJOR == 2 && (DB_VERSION_MINOR < 5 || (DB_VERSION_MINOR == 5 && DB_VERSION_PATCH < 2))
	    softCrash("join needs Berkeley DB 2.5.2 or later") ;
#else /* Berkeley DB >= 2.5.2 */
	  DBC *		join_cursor ;
	  DBC **	cursor_list ;
	  I32		count = av_len(cursors) + 1 ;
	  int		i ;
	  CurrentDB = db ;
	  if (count < 1 )
	      softCrash("db_join: No cursors in parameter list") ;
	  cursor_list = (DBC **)safemalloc(sizeof(DBC*) * (count + 1));
	  for (i = 0 ; i < count ; ++i) {
	      SV * obj = (SV*) * av_fetch(cursors, i, FALSE) ;
	      BerkeleyDB__Cursor cur = (BerkeleyDB__Cursor) getInnerObject(obj) ;
	      cursor_list[i] = cur->cursor ;
	  }
	  cursor_list[i] = NULL ;
#if DB_VERSION_MAJOR == 2
	  if ((db->Status = ((db->dbp)->join)(db->dbp, cursor_list, flags, &join_cursor)) == 0){
#else
	  if ((db->Status = ((db->dbp)->join)(db->dbp, cursor_list, &join_cursor, flags)) == 0){
#endif
	      ZMALLOC(RETVAL, BerkeleyDB__Cursor_type) ;
	      db->open_cursors ++ ;
	      RETVAL->parent_db  = db ;
	      RETVAL->cursor  = join_cursor ;
	      RETVAL->dbp     = db->dbp ;
              RETVAL->type    = db->type ;
              RETVAL->filename    = my_strdup(db->filename) ;
              RETVAL->compare = db->compare ;
              RETVAL->dup_compare = db->dup_compare ;
              RETVAL->prefix  = db->prefix ;
              RETVAL->hash    = db->hash ;
	      RETVAL->partial = db->partial ;
	      RETVAL->doff    = db->doff ;
	      RETVAL->dlen    = db->dlen ;
	      RETVAL->active  = TRUE ;
#ifdef ALLOW_RECNO_OFFSET
	      RETVAL->array_base  = db->array_base ;
#endif /* ALLOW_RECNO_OFFSET */
#ifdef DBM_FILTERING
	      RETVAL->filtering   = FALSE ;
	      RETVAL->filter_fetch_key    = db->filter_fetch_key ;
	      RETVAL->filter_store_key    = db->filter_store_key ;
	      RETVAL->filter_fetch_value  = db->filter_fetch_value ;
	      RETVAL->filter_store_value  = db->filter_store_value ;
#endif
              /* RETVAL->info ; */
	      hash_store_iv("BerkeleyDB::Term::Cursor", (IV)RETVAL, 1) ;
	  }
	  safefree(cursor_list) ;
#endif /* Berkeley DB >= 2.5.2 */
	}
	OUTPUT:
	  RETVAL

int
ArrayOffset(db)
        BerkeleyDB::Common 	db
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
#ifdef ALLOW_RECNO_OFFSET
	    RETVAL = db->array_base ? 0 : 1 ;
#else
	    RETVAL = 0 ;
#endif /* ALLOW_RECNO_OFFSET */
	OUTPUT:
	    RETVAL

int
type(db)
        BerkeleyDB::Common 	db
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
	    RETVAL = db->type ;
	OUTPUT:
	    RETVAL

int
byteswapped(db)
        BerkeleyDB::Common 	db
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
#if DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 5
	    softCrash("byteswapped needs Berkeley DB 2.5 or later") ;
#else
#if DB_VERSION_MAJOR == 2
	    RETVAL = db->dbp->byteswapped ;
#else
	    RETVAL = db->dbp->get_byteswapped(db->dbp) ;
#endif
#endif
	OUTPUT:
	    RETVAL

DualType
status(db)
        BerkeleyDB::Common 	db
	CODE:
	    RETVAL =  db->Status ;
	OUTPUT:
	    RETVAL

#ifdef DBM_FILTERING

#define setFilter(ftype)				\
	{						\
	    if (db->ftype)				\
	        RETVAL = sv_mortalcopy(db->ftype) ;	\
	    ST(0) = RETVAL ;				\
	    if (db->ftype && (code == &PL_sv_undef)) {	\
                SvREFCNT_dec(db->ftype) ;		\
	        db->ftype = NULL ;			\
	    }						\
	    else if (code) {				\
	        if (db->ftype)				\
	            sv_setsv(db->ftype, code) ;		\
	        else					\
	            db->ftype = newSVsv(code) ;		\
	    }	    					\
	}


SV *
filter_fetch_key(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_key) ;

SV *
filter_store_key(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_key) ;

SV *
filter_fetch_value(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_fetch_value) ;

SV *
filter_store_value(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    setFilter(filter_store_value) ;

#endif /* DBM_FILTERING */

void
partial_set(db, offset, length)
        BerkeleyDB::Common 	db
	u_int32_t		offset
	u_int32_t		length
	INIT:
	    ckActive_Database(db->active) ;
	PPCODE:
	    if (GIMME == G_ARRAY) {
		XPUSHs(sv_2mortal(newSViv(db->partial == DB_DBT_PARTIAL))) ;
		XPUSHs(sv_2mortal(newSViv(db->doff))) ;
		XPUSHs(sv_2mortal(newSViv(db->dlen))) ;
	    }
	    db->partial = DB_DBT_PARTIAL ;
	    db->doff    = offset ;
	    db->dlen    = length ;


void
partial_clear(db)
        BerkeleyDB::Common 	db
	INIT:
	    ckActive_Database(db->active) ;
	PPCODE:
	    if (GIMME == G_ARRAY) {
		XPUSHs(sv_2mortal(newSViv(db->partial == DB_DBT_PARTIAL))) ;
		XPUSHs(sv_2mortal(newSViv(db->doff))) ;
		XPUSHs(sv_2mortal(newSViv(db->dlen))) ;
	    }
	    db->partial =
	    db->doff    =
	    db->dlen    = 0 ;


#define db_del(db, key, flags)  \
	(db->Status = ((db->dbp)->del)(db->dbp, db->txn, &key, flags))
DualType
db_del(db, key, flags=0)
	BerkeleyDB::Common	db
	DBTKEY		key
	u_int		flags
	INIT:
	    ckActive_Database(db->active) ;
	    CurrentDB = db ;


#define db_get(db, key, data, flags)   \
	(db->Status = ((db->dbp)->get)(db->dbp, db->txn, &key, &data, flags))
DualType
db_get(db, key, data, flags=0)
	BerkeleyDB::Common	db
	u_int		flags
	DBTKEY_B	key
	DBT_OPT		data
	INIT:
	  ckActive_Database(db->active) ;
	  CurrentDB = db ;
	  SetPartial(data,db) ;
	OUTPUT:
	  key	if (flagSet(DB_SET_RECNO)) OutputValue(ST(1), key) ;
	  data

#define db_put(db,key,data,flag)	\
		(db->Status = (db->dbp->put)(db->dbp,db->txn,&key,&data,flag))
DualType
db_put(db, key, data, flags=0)
	BerkeleyDB::Common	db
	DBTKEY			key
	DBT			data
	u_int			flags
	INIT:
	  ckActive_Database(db->active) ;
	  CurrentDB = db ;
	  /* SetPartial(data,db) ; */
	OUTPUT:
	  key	if (flagSet(DB_APPEND)) OutputKey(ST(1), key) ;

#define db_key_range(db, key, range, flags)   \
	(db->Status = ((db->dbp)->key_range)(db->dbp, db->txn, &key, &range, flags))
DualType
db_key_range(db, key, less, equal, greater, flags=0)
	BerkeleyDB::Common	db
	DBTKEY_B	key
	double          less = NO_INIT
	double          equal = NO_INIT
	double          greater = NO_INIT
	u_int32_t	flags
	CODE:
	{
#ifndef AT_LEAST_DB_3_1
          softCrash("key_range needs Berkeley DB 3.1.x or later") ;
#else
          DB_KEY_RANGE range ;
          range.less = range.equal = range.greater = 0.0 ;
	  ckActive_Database(db->active) ;
	  CurrentDB = db ;
	  RETVAL = db_key_range(db, key, range, flags);
	  if (RETVAL == 0) {
	        less = range.less ;
	        equal = range.equal;
	        greater = range.greater;
	  }
#endif
	}
	OUTPUT:
	  RETVAL
	  less
	  equal
	  greater


#define db_fd(d, x)	(db->Status = (db->dbp->fd)(db->dbp, &x))
DualType
db_fd(db)
	BerkeleyDB::Common	db
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	  CurrentDB = db ;
	  db_fd(db, RETVAL) ;
	OUTPUT:
	  RETVAL


#define db_sync(db, fl)	(db->Status = (db->dbp->sync)(db->dbp, fl))
DualType
db_sync(db, flags=0)
	BerkeleyDB::Common	db
	u_int			flags
	INIT:
	  ckActive_Database(db->active) ;
	  CurrentDB = db ;

void
_Txn(db, txn=NULL)
        BerkeleyDB::Common      db
        BerkeleyDB::Txn         txn
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	   if (txn) {
	       Trace(("_Txn(%d in %d) active [%d]\n", txn->txn, txn, txn->active));
	       ckActive_Transaction(txn->active) ;
	       db->txn = txn->txn ;
	   }
	   else {
	       Trace(("_Txn(undef) \n"));
	       db->txn = NULL ;
	   }




MODULE = BerkeleyDB::Cursor              PACKAGE = BerkeleyDB::Cursor	PREFIX = cu_

BerkeleyDB::Cursor::Raw
_c_dup(db, flags=0)
    	BerkeleyDB::Cursor	db
	u_int32_t		flags
        BerkeleyDB::Cursor 	RETVAL = NULL ;
	INIT:
	    CurrentDB = db->parent_db ;
	    ckActive_Database(db->active) ;
	CODE:
	{
#ifndef AT_LEAST_DB_3
          softCrash("c_dup needs at least Berkeley DB 3.0.x");
#else
	  DBC *		newcursor ;
	  db->Status = ((db->cursor)->c_dup)(db->cursor, &newcursor, flags) ;
	  if (db->Status == 0){
	      ZMALLOC(RETVAL, BerkeleyDB__Cursor_type) ;
	      db->parent_db->open_cursors ++ ;
	      RETVAL->parent_db  = db->parent_db ;
	      RETVAL->cursor  = newcursor ;
	      RETVAL->dbp     = db->dbp ;
              RETVAL->type    = db->type ;
              RETVAL->recno_or_queue    = db->recno_or_queue ;
              RETVAL->filename    = my_strdup(db->filename) ;
              RETVAL->compare = db->compare ;
              RETVAL->dup_compare = db->dup_compare ;
              RETVAL->prefix  = db->prefix ;
              RETVAL->hash    = db->hash ;
	      RETVAL->partial = db->partial ;
	      RETVAL->doff    = db->doff ;
	      RETVAL->dlen    = db->dlen ;
	      RETVAL->active  = TRUE ;
#ifdef ALLOW_RECNO_OFFSET
	      RETVAL->array_base  = db->array_base ;
#endif /* ALLOW_RECNO_OFFSET */
#ifdef DBM_FILTERING
	      RETVAL->filtering   = FALSE ;
	      RETVAL->filter_fetch_key    = db->filter_fetch_key ;
	      RETVAL->filter_store_key    = db->filter_store_key ;
	      RETVAL->filter_fetch_value  = db->filter_fetch_value ;
	      RETVAL->filter_store_value  = db->filter_store_value ;
#endif /* DBM_FILTERING */
              /* RETVAL->info ; */
	      hash_store_iv("BerkeleyDB::Term::Cursor", (IV)RETVAL, 1) ;
	  }
#endif	
	}
	OUTPUT:
	  RETVAL

DualType
_c_close(db)
    BerkeleyDB::Cursor	db
	INIT:
	  CurrentDB = db->parent_db ;
	  ckActive_Cursor(db->active) ;
	  hash_delete("BerkeleyDB::Term::Cursor", (IV)db) ;
	CODE:
	  RETVAL =  db->Status =
    	          ((db->cursor)->c_close)(db->cursor) ;
	  db->active = FALSE ;
	  if (db->parent_db->open_cursors)
	      -- db->parent_db->open_cursors ;
	OUTPUT:
	  RETVAL

void
_DESTROY(db)
    BerkeleyDB::Cursor	db
	CODE:
	  CurrentDB = db->parent_db ;
	  Trace(("In BerkeleyDB::Cursor::_DESTROY db %d dirty=%d active=%d\n", db, PL_dirty, db->active));
	  hash_delete("BerkeleyDB::Term::Cursor", (IV)db) ;
	  if (db->active)
    	      ((db->cursor)->c_close)(db->cursor) ;
	  if (db->parent_db->open_cursors)
	      -- db->parent_db->open_cursors ;
          Safefree(db->filename) ;
          Safefree(db) ;
	  Trace(("End of BerkeleyDB::Cursor::_DESTROY\n")) ;

DualType
status(db)
        BerkeleyDB::Cursor 	db
	CODE:
	    RETVAL =  db->Status ;
	OUTPUT:
	    RETVAL


#define cu_c_del(c,f)	(c->Status = ((c->cursor)->c_del)(c->cursor,f))
DualType
cu_c_del(db, flags=0)
    BerkeleyDB::Cursor	db
    int			flags
	INIT:
	  CurrentDB = db->parent_db ;
	  ckActive_Cursor(db->active) ;
	OUTPUT:
	  RETVAL


#define cu_c_get(c,k,d,f) (c->Status = (c->cursor->c_get)(c->cursor,&k,&d,f))
DualType
cu_c_get(db, key, data, flags=0)
    BerkeleyDB::Cursor	db
    int			flags
    DBTKEY_B		key
    DBT_B		data
	INIT:
	  Trace(("c_get db [%d] flags [%d]\n", db, flags)) ;
	  CurrentDB = db->parent_db ;
	  ckActive_Cursor(db->active) ;
	  SetPartial(data,db) ;
	  Trace(("c_get end\n")) ;
	OUTPUT:
	  RETVAL
	  key
	  data		if (! flagSet(DB_JOIN_ITEM)) OutputValue_B(ST(2), data) ;


#define cu_c_put(c,k,d,f)  (c->Status = (c->cursor->c_put)(c->cursor,&k,&d,f))
DualType
cu_c_put(db, key, data, flags=0)
    BerkeleyDB::Cursor	db
    DBTKEY		key
    DBT			data
    int			flags
	INIT:
	  CurrentDB = db->parent_db ;
	  ckActive_Cursor(db->active) ;
	  /* SetPartial(data,db) ; */
	OUTPUT:
	  RETVAL

#define cu_c_count(c,p,f) (c->Status = (c->cursor->c_count)(c->cursor,&p,f))
DualType
cu_c_count(db, count, flags=0)
    BerkeleyDB::Cursor	db
    u_int32_t           count = NO_INIT
    int			flags
	CODE:
#ifndef AT_LEAST_DB_3_1
          softCrash("c_count needs at least Berkeley DB 3.1.x");
#else
	  Trace(("c_get count [%d] flags [%d]\n", db, flags)) ;
	  CurrentDB = db->parent_db ;
	  ckActive_Cursor(db->active) ;
	  RETVAL = cu_c_count(db, count, flags) ;
	  Trace(("    c_count got %d duplicates\n", count)) ;
#endif
	OUTPUT:
	  RETVAL
	  count

MODULE = BerkeleyDB::TxnMgr           PACKAGE = BerkeleyDB::TxnMgr	PREFIX = xx_

BerkeleyDB::Txn::Raw
_txn_begin(txnmgr, pid=NULL, flags=0)
	BerkeleyDB::TxnMgr	txnmgr
	BerkeleyDB::Txn		pid
	u_int32_t		flags
	CODE:
	{
	    DB_TXN *txn ;
	    DB_TXN *p_id = NULL ;
#if DB_VERSION_MAJOR == 2
	    if (txnmgr->env->Env->tx_info == NULL)
		softCrash("Transaction Manager not enabled") ;
#endif
	    if (pid)
		p_id = pid->txn ;
	    txnmgr->env->TxnMgrStatus =
#if DB_VERSION_MAJOR == 2
	    	txn_begin(txnmgr->env->Env->tx_info, p_id, &txn) ;
#else
	    	txn_begin(txnmgr->env->Env, p_id, &txn, flags) ;
#endif
	    if (txnmgr->env->TxnMgrStatus == 0) {
	      ZMALLOC(RETVAL, BerkeleyDB_Txn_type) ;
	      RETVAL->txn  = txn ;
	      RETVAL->active = TRUE ;
	      Trace(("_txn_begin created txn [%d] in [%d]\n", txn, RETVAL));
	      hash_store_iv("BerkeleyDB::Term::Txn", (IV)RETVAL, 1) ;
	    }
	    else
		RETVAL = NULL ;
	}
	OUTPUT:
	    RETVAL


DualType
status(mgr)
        BerkeleyDB::TxnMgr 	mgr
	CODE:
	    RETVAL =  mgr->env->TxnMgrStatus ;
	OUTPUT:
	    RETVAL


void
_DESTROY(mgr)
    BerkeleyDB::TxnMgr	mgr
	CODE:
	  Trace(("In BerkeleyDB::TxnMgr::DESTROY dirty=%d\n", PL_dirty)) ;
          Safefree(mgr) ;
	  Trace(("End of BerkeleyDB::TxnMgr::DESTROY\n")) ;

DualType
txn_close(txnp)
	BerkeleyDB::TxnMgr	txnp
        NOT_IMPLEMENTED_YET


#if DB_VERSION_MAJOR == 2
#  define xx_txn_checkpoint(t,k,m) txn_checkpoint(t->env->Env->tx_info, k, m)
#else
#  ifdef AT_LEAST_DB_3_1
#    define xx_txn_checkpoint(t,k,m) txn_checkpoint(t->env->Env, k, m, 0)
#  else
#    define xx_txn_checkpoint(t,k,m) txn_checkpoint(t->env->Env, k, m)
#  endif
#endif
DualType
xx_txn_checkpoint(txnp, kbyte, min)
	BerkeleyDB::TxnMgr	txnp
	long			kbyte
	long			min

HV *
txn_stat(txnp)
	BerkeleyDB::TxnMgr	txnp
	HV *			RETVAL = NULL ;
	CODE:
	{
	    DB_TXN_STAT *	stat ;
#if DB_VERSION_MAJOR == 2
	    if(txn_stat(txnp->env->Env->tx_info, &stat, safemalloc) == 0) {
#else
	    if(txn_stat(txnp->env->Env, &stat, safemalloc) == 0) {
#endif
	    	RETVAL = (HV*)sv_2mortal((SV*)newHV()) ;
		hv_store_iv(RETVAL, "st_time_ckp", stat->st_time_ckp) ;
		hv_store_iv(RETVAL, "st_last_txnid", stat->st_last_txnid) ;
		hv_store_iv(RETVAL, "st_maxtxns", stat->st_maxtxns) ;
		hv_store_iv(RETVAL, "st_naborts", stat->st_naborts) ;
		hv_store_iv(RETVAL, "st_nbegins", stat->st_nbegins) ;
		hv_store_iv(RETVAL, "st_ncommits", stat->st_ncommits) ;
		hv_store_iv(RETVAL, "st_nactive", stat->st_nactive) ;
#if DB_VERSION_MAJOR > 2
		hv_store_iv(RETVAL, "st_maxnactive", stat->st_maxnactive) ;
		hv_store_iv(RETVAL, "st_regsize", stat->st_regsize) ;
		hv_store_iv(RETVAL, "st_region_wait", stat->st_region_wait) ;
		hv_store_iv(RETVAL, "st_region_nowait", stat->st_region_nowait) ;
#endif
		safefree(stat) ;
	    }
	}
	OUTPUT:
	    RETVAL


BerkeleyDB::TxnMgr
txn_open(dir, flags, mode, dbenv)
    const char *	dir
    int 		flags
    int 		mode
    BerkeleyDB::Env 	dbenv
        NOT_IMPLEMENTED_YET


MODULE = BerkeleyDB::Txn              PACKAGE = BerkeleyDB::Txn		PREFIX = xx_

DualType
status(tid)
        BerkeleyDB::Txn 	tid
	CODE:
	    RETVAL =  tid->Status ;
	OUTPUT:
	    RETVAL

int
_DESTROY(tid)
    BerkeleyDB::Txn	tid
	CODE:
	  Trace(("In BerkeleyDB::Txn::_DESTROY txn [%d] active [%d] dirty=%d\n", tid->txn, tid->active, PL_dirty)) ;
	  if (tid->active)
	    txn_abort(tid->txn) ;
          RETVAL = (int)tid ;
	  hash_delete("BerkeleyDB::Term::Txn", (IV)tid) ;
          Safefree(tid) ;
	  Trace(("End of BerkeleyDB::Txn::DESTROY\n")) ;
	OUTPUT:
	  RETVAL

#define xx_txn_unlink(d,f,e)	txn_unlink(d,f,&(e->Env))
DualType
xx_txn_unlink(dir, force, dbenv)
    const char *	dir
    int 		force
    BerkeleyDB::Env 	dbenv
        NOT_IMPLEMENTED_YET

#define xx_txn_prepare(t) (t->Status = txn_prepare(t->txn))
DualType
xx_txn_prepare(tid)
	BerkeleyDB::Txn	tid
	INIT:
	    ckActive_Transaction(tid->active) ;

#if DB_VERSION_MAJOR == 2
#  define _txn_commit(t,flags) (t->Status = txn_commit(t->txn))
#else
#  define _txn_commit(t, flags) (t->Status = txn_commit(t->txn, flags))
#endif
DualType
_txn_commit(tid, flags=0)
	BerkeleyDB::Txn	tid
	u_int32_t	flags
	INIT:
	    ckActive_Transaction(tid->active) ;
	    hash_delete("BerkeleyDB::Term::Txn", (IV)tid) ;
	    tid->active = FALSE ;

#define _txn_abort(t) (t->Status = txn_abort(t->txn))
DualType
_txn_abort(tid)
	BerkeleyDB::Txn	tid
	INIT:
	    ckActive_Transaction(tid->active) ;
	    hash_delete("BerkeleyDB::Term::Txn", (IV)tid) ;
	    tid->active = FALSE ;

#define xx_txn_id(t) txn_id(t->txn)
u_int32_t
xx_txn_id(tid)
	BerkeleyDB::Txn	tid

MODULE = BerkeleyDB::_tiedHash        PACKAGE = BerkeleyDB::_tiedHash

int
FIRSTKEY(db)
        BerkeleyDB::Common         db
        CODE:
        {
            DBTKEY      key ;
            DBT         value ;
	    DBC *	cursor ;

	    /*
		TODO!
		set partial value to 0 - to eliminate the retrieval of
		the value need to store any existing partial settings &
		restore at the end.

	     */
            CurrentDB = db ;
	    DBT_clear(key) ;
	    DBT_clear(value) ;
	    /* If necessary create a cursor for FIRSTKEY/NEXTKEY use */
	    if (!db->cursor &&
		(db->Status = db_cursor(db, db->txn, &cursor, 0)) == 0 )
	            db->cursor  = cursor ;

	    if (db->cursor)
	        RETVAL = (db->Status) =
		    ((db->cursor)->c_get)(db->cursor, &key, &value, DB_FIRST);
	    else
		RETVAL = db->Status ;
	    /* check for end of cursor */
	    if (RETVAL == DB_NOTFOUND) {
	      ((db->cursor)->c_close)(db->cursor) ;
	      db->cursor = NULL ;
	    }
            ST(0) = sv_newmortal();
	    OutputKey(ST(0), key)
        }



int
NEXTKEY(db, key)
        BerkeleyDB::Common  db
        DBTKEY              key
        CODE:
        {
            DBT         value ;

            CurrentDB = db ;
	    DBT_clear(value) ;
	    key.flags = 0 ;
	    RETVAL = (db->Status) =
		((db->cursor)->c_get)(db->cursor, &key, &value, DB_NEXT);

	    /* check for end of cursor */
	    if (RETVAL == DB_NOTFOUND) {
	      ((db->cursor)->c_close)(db->cursor) ;
	      db->cursor = NULL ;
	    }
            ST(0) = sv_newmortal();
	    OutputKey(ST(0), key)
        }

MODULE = BerkeleyDB::_tiedArray        PACKAGE = BerkeleyDB::_tiedArray

I32
FETCHSIZE(db)
        BerkeleyDB::Common         db
        CODE:
            CurrentDB = db ;
            RETVAL = GetArrayLength(db) ;
        OUTPUT:
            RETVAL


MODULE = BerkeleyDB        PACKAGE = BerkeleyDB

BOOT:
  {
    SV * sv_err = perl_get_sv(ERR_BUFF, GV_ADD|GV_ADDMULTI) ;
    SV * version_sv = perl_get_sv("BerkeleyDB::db_version", GV_ADD|GV_ADDMULTI) ;
    SV * ver_sv = perl_get_sv("BerkeleyDB::db_ver", GV_ADD|GV_ADDMULTI) ;
    int Major, Minor, Patch ;
    (void)db_version(&Major, &Minor, &Patch) ;
    /* Check that the versions of db.h and libdb.a are the same */
    if (Major != DB_VERSION_MAJOR || Minor != DB_VERSION_MINOR
                || Patch != DB_VERSION_PATCH)
        croak("\nBerkeleyDB needs compatible versions of libdb & db.h\n\tyou have db.h version %d.%d.%d and libdb version %d.%d.%d\n",
                DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH,
                Major, Minor, Patch) ;

    if (Major < 2 || (Major == 2 && Minor < 6))
    {
        croak("BerkeleyDB needs Berkeley DB 2.6 or greater. This is %d.%d.%d\n",
		Major, Minor, Patch) ;
    }
    sv_setpvf(version_sv, "%d.%d", Major, Minor) ;
    sv_setpvf(ver_sv, "%d.%03d%03d", Major, Minor, Patch) ;
    sv_setpv(sv_err, "");

    DBT_clear(empty) ;
    empty.data  = &zero ;
    empty.size  =  sizeof(db_recno_t) ;
    empty.flags = 0 ;

  }

