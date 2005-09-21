/*

 BerkeleyDB.xs -- Perl 5 interface to Berkeley DB version 2, 3 &4

 written by Paul Marquess <pmqs@cpan.org>

 All comments/suggestions/problems are welcome

     Copyright (c) 1997-2004 Paul Marquess. All rights reserved.
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
#include "ppport.h"


/* XSUB.h defines a macro called abort 				*/
/* This clashes with the txn abort method in Berkeley DB 4.x	*/
/* This is a problem with ActivePerl (at least)			*/

#ifdef _WIN32
#  ifdef abort
#    undef abort
#  endif
#  ifdef fopen
#    undef fopen
#  endif
#  ifdef fclose
#    undef fclose
#  endif
#  ifdef rename
#    undef rename
#  endif
#  ifdef open
#    undef open
#  endif
#endif

/* Being the Berkeley DB we prefer the <sys/cdefs.h> (which will be
 * shortly #included by the <db.h>) __attribute__ to the possibly
 * already defined __attribute__, for example by GNUC or by Perl. */

#undef __attribute__

#ifdef USE_PERLIO
#    define GetFILEptr(sv) PerlIO_findFILE(IoIFP(sv_2io(sv)))
#else
#    define GetFILEptr(sv) IoIFP(sv_2io(sv))
#endif

#include <db.h>

/* Check the version of Berkeley DB */

#ifndef DB_VERSION_MAJOR
#ifdef HASHMAGIC
#error db.h is from Berkeley DB 1.x - need at least Berkeley DB 2.6.4
#else
#error db.h is not for Berkeley DB at all.
#endif
#endif

#if (DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR < 6) ||\
    (DB_VERSION_MAJOR == 2 && DB_VERSION_MINOR == 6 && DB_VERSION_PATCH < 4)
#  error db.h is from Berkeley DB 2.0-2.5 - need at least Berkeley DB 2.6.4
#endif


#if (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 0)
#  define IS_DB_3_0_x
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

#if DB_VERSION_MAJOR > 3 || \
    (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR > 2) ||\
    (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR == 2 && DB_VERSION_PATCH >= 6)
#  define AT_LEAST_DB_3_2_6
#endif

#if DB_VERSION_MAJOR > 3 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3)
#  define AT_LEAST_DB_3_3
#endif

#if DB_VERSION_MAJOR >= 4
#  define AT_LEAST_DB_4
#endif

#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
#  define AT_LEAST_DB_4_1
#endif

#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 2)
#  define AT_LEAST_DB_4_2
#endif

#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 3)
#  define AT_LEAST_DB_4_3
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

#if DB_VERSION_MAJOR == 2 
#  define BackRef	internal
#else
#  if DB_VERSION_MAJOR == 3 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 0)
#    define BackRef	cj_internal
#  else
#    define BackRef	api_internal
#  endif
#endif

#ifdef AT_LEAST_DB_3_2
#    define DB_callback	DB * db,
#    define getCurrentDB ((BerkeleyDB)db->BackRef) 
#    define saveCurrentDB(db) 
#else
#    define DB_callback
#    define getCurrentDB CurrentDB
#    define saveCurrentDB(db) CurrentDB = db
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
	bool		opened ;
	bool		cds_enabled;
	} BerkeleyDB_ENV_type ;


typedef struct {
        DBTYPE  	type ;
	bool		recno_or_queue ;
	char *		filename ;
	BerkeleyDB_ENV_type * parent_env ;
        DB *    	dbp ;
        SV *    	compare ;
        bool    	in_compare ;
        SV *    	dup_compare ;
        bool    	in_dup_compare ;
        SV *    	prefix ;
        bool    	in_prefix ;
        SV *   	 	hash ;
        bool    	in_hash ;
#ifdef AT_LEAST_DB_3_3
        SV *   	 	associated ;
	bool		secondary_db ;
#endif
	int		Status ;
        DB_INFO *	info ;
        DBC *   	cursor ;
	DB_TXN *	txn ;
	int		open_cursors ;
	u_int32_t	partial ;
	u_int32_t	dlen ;
	u_int32_t	doff ;
	int		active ;
	bool		cds_enabled;
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
#ifdef AT_LEAST_DB_3_3
        SV *   	 	associated ;
	bool		secondary_db ;
#endif
	int		Status ;
        DB_INFO *	info ;
        DBC *   	cursor ;
	DB_TXN *	txn ;
	BerkeleyDB_type *		parent_db ;
	u_int32_t	partial ;
	u_int32_t	dlen ;
	u_int32_t	doff ;
	int		active ;
	bool		cds_enabled;
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
hash_delete(char * hash, char * key);

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


#define ERR_BUFF "BerkeleyDB::Error"

#define ZMALLOC(to, typ) ((to = (typ *)safemalloc(sizeof(typ))), \
				Zero(to,1,typ))

#define DBT_clear(x)	Zero(&x, 1, DBT) ;

#if 1
#define getInnerObject(x) (*av_fetch((AV*)SvRV(x), 0, FALSE))
#else
#define getInnerObject(x) ((SV*)SvRV(sv))
#endif

#define my_sv_setpvn(sv, d, s) (s ? sv_setpvn(sv, d, s) : sv_setpv(sv, "") )

#define SetValue_iv(i, k) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = SvIV(sv)
#define SetValue_io(i, k) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = GetFILEptr(sv)
#define SetValue_sv(i, k) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = sv
#define SetValue_pv(i, k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = (t)SvPV(sv,PL_na)
#define SetValue_pvx(i, k, t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) \
				i = (t)SvPVX(sv)
#define SetValue_ov(i,k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) {\
				IV tmp = SvIV(getInnerObject(sv)) ;	\
				i = INT2PTR(t, tmp) ;			\
			  }

#define SetValue_ovx(i,k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) {\
				HV * hv = (HV *)GetInternalObject(sv);		\
				SV ** svp = hv_fetch(hv, "db", 2, FALSE);\
				IV tmp = SvIV(*svp);			\
				i = INT2PTR(t, tmp) ;				\
			  }

#define SetValue_ovX(i,k,t) if ((sv = readHash(hash, k)) && sv != &PL_sv_undef) {\
				IV tmp = SvIV(GetInternalObject(sv));\
				i = INT2PTR(t, tmp) ;				\
			  }

#define LastDBerror DB_RUNRECOVERY

#define setDUALerrno(var, err)					\
		sv_setnv(var, (double)err) ;			\
		sv_setpv(var, ((err) ? db_strerror(err) : "")) ;\
		SvNOK_on(var);

#define OutputValue(arg, name)                                  \
        { if (RETVAL == 0) {                                    \
              my_sv_setpvn(arg, name.data, name.size) ;         \
              DBM_ckFilter(arg, filter_fetch_value,"filter_fetch_value") ;            \
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
                DBM_ckFilter(arg, filter_fetch_value, "filter_fetch_value");          \
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
                DBM_ckFilter(arg, filter_fetch_key, "filter_fetch_key") ;            \
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
                DBM_ckFilter(arg, filter_fetch_key, "filter_fetch_key") ;            \
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

#define dieIfEnvOpened(e, m) if (e->opened) softCrash("Cannot call method BerkeleyDB::Env::%s after environment has been opened", m);	

#define isSTDOUT_ERR(f) ((f) == stdout || (f) == stderr)

/* Internal Global Data */
static db_recno_t Value ;
static db_recno_t zero = 0 ;
static BerkeleyDB	CurrentDB ;

static DBTKEY	empty ;
#if 0
static char	ErrBuff[1000] ;
#endif

#ifdef AT_LEAST_DB_3_3
#    if PERL_REVISION == 5 && PERL_VERSION <= 4

/* saferealloc in perl5.004 will croak if it is given a NULL pointer*/
void *
MyRealloc(void * ptr, size_t size)
{
    if (ptr == NULL ) 
        return safemalloc(size) ; 
    else
        return saferealloc(ptr, size) ;
}

#    else
#        define MyRealloc saferealloc
#    endif
#endif

static char *
my_strdup(const char *s)
{
    if (s == NULL)
        return NULL ;

    {
        MEM_SIZE l = strlen(s) + 1;
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

#ifdef TRACE
#if DB_VERSION_MAJOR > 2
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
#endif
#endif

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
	int  all = 0 ;
	int  closed = 0 ;
	(void)hv_iterinit(hv) ;
	Trace(("BerkeleyDB::Term::close_all_txns dirty=%d\n", PL_dirty)) ;
	while ( (he = hv_iternext(hv)) ) {
	    tid = * (BerkeleyDB__Txn__Raw *) hv_iterkey(he, &len) ;
	    Trace(("  Aborting Transaction [%d] in [%d] Active [%d]\n", tid->txn, tid, tid->active));
	    if (tid->active) {
#ifdef AT_LEAST_DB_4
	    tid->txn->abort(tid->txn) ;
#else
	        txn_abort(tid->txn);
#endif
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
	int  all = 0 ;
	int  closed = 0 ;
	(void) hv_iterinit(hv) ;
	Trace(("BerkeleyDB::Term::close_all_cursors \n")) ;
	while ( (he = hv_iternext(hv)) ) {
	    db = * (BerkeleyDB__Cursor*) hv_iterkey(he, &len) ;
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
	int  all = 0 ;
	int  closed = 0 ;
	(void)hv_iterinit(hv) ;
	Trace(("BerkeleyDB::Term::close_all_dbs\n" )) ;
	while ( (he = hv_iternext(hv)) ) {
	    db = * (BerkeleyDB*) hv_iterkey(he, &len) ;
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
	int  all = 0 ;
	int  closed = 0 ;
	(void)hv_iterinit(hv) ;
	Trace(("BerkeleyDB::Term::close_all_envs\n")) ;
	while ( (he = hv_iternext(hv)) ) {
	    env = * (BerkeleyDB__Env*) hv_iterkey(he, &len) ;
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
	if (db->parent_env && db->parent_env->open_dbs)
	    -- db->parent_env->open_dbs ;
      	-- db->open_cursors ;
	((db->dbp)->close)(db->dbp, 0) ;
    }
    if (db->hash)
       	  SvREFCNT_dec(db->hash) ;
    if (db->compare)
       	  SvREFCNT_dec(db->compare) ;
    if (db->dup_compare)
       	  SvREFCNT_dec(db->dup_compare) ;
#ifdef AT_LEAST_DB_3_3
    if (db->associated && !db->secondary_db)
       	  SvREFCNT_dec(db->associated) ;
#endif
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
    hash_delete("BerkeleyDB::Term::Db", (char *)db) ;
    if (db->filename)
             Safefree(db->filename) ;
    Safefree(db) ;
}

static int
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
    return 1 ;
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

#if 0
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
#endif

static int
btree_compare(DB_callback const DBT * key1, const DBT * key2 )
{
    dSP ;
    char * data1, * data2 ;
    int retval ;
    int count ;
    BerkeleyDB	keepDB = CurrentDB ;

    data1 = (char*) key1->data ;
    data2 = (char*) key2->data ;

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

    count = perl_call_sv(getCurrentDB->compare, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("in btree_compare - expected 1 return value from compare sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    CurrentDB = keepDB ;
    return (retval) ;

}

static int
dup_compare(DB_callback const DBT * key1, const DBT * key2 )
{
    dSP ;
    char * data1, * data2 ;
    int retval ;
    int count ;
    BerkeleyDB	keepDB = CurrentDB ;

    Trace(("In dup_compare \n")) ;
    if (!getCurrentDB)
	softCrash("Internal Error - No CurrentDB in dup_compare") ;
    if (getCurrentDB->dup_compare == NULL)


        softCrash("in dup_compare: no callback specified for database '%s'", getCurrentDB->filename) ;

    data1 = (char*) key1->data ;
    data2 = (char*) key2->data ;

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

    count = perl_call_sv(getCurrentDB->dup_compare, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("dup_compare: expected 1 return value from compare sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    CurrentDB = keepDB ;
    return (retval) ;

}

static size_t
btree_prefix(DB_callback const DBT * key1, const DBT * key2 )
{
    dSP ;
    char * data1, * data2 ;
    int retval ;
    int count ;
    BerkeleyDB	keepDB = CurrentDB ;

    data1 = (char*) key1->data ;
    data2 = (char*) key2->data ;

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

    count = perl_call_sv(getCurrentDB->prefix, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("btree_prefix: expected 1 return value from prefix sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    CurrentDB = keepDB ;

    return (retval) ;
}

static u_int32_t
hash_cb(DB_callback const void * data, u_int32_t size)
{
    dSP ;
    int retval ;
    int count ;
    BerkeleyDB	keepDB = CurrentDB ;

#ifndef newSVpvn
    if (size == 0)
        data = "" ;
#endif

    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;

    XPUSHs(sv_2mortal(newSVpvn((char*)data,size)));
    PUTBACK ;

    count = perl_call_sv(getCurrentDB->hash, G_SCALAR);

    SPAGAIN ;

    if (count != 1)
        softCrash ("hash_cb: expected 1 return value from hash sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    CurrentDB = keepDB ;

    return (retval) ;
}

#ifdef AT_LEAST_DB_3_3

static int
associate_cb(DB_callback const DBT * pkey, const DBT * pdata, DBT * skey)
{
    dSP ;
    char * pk_dat, * pd_dat ;
    /* char *sk_dat ; */
    int retval ;
    int count ;
    SV * skey_SV ;
    STRLEN skey_len;
    char * skey_ptr ;

    Trace(("In associate_cb \n")) ;
    if (getCurrentDB->associated == NULL){
        Trace(("No Callback registered\n")) ;
        return EINVAL ;
    }

    skey_SV = newSVpv("",0);


    pk_dat = (char*) pkey->data ;
    pd_dat = (char*) pdata->data ;

#ifndef newSVpvn
    /* As newSVpv will assume that the data pointer is a null terminated C
       string if the size parameter is 0, make sure that data points to an
       empty string if the length is 0
    */
    if (pkey->size == 0)
        pk_dat = "" ;
    if (pdata->size == 0)
        pd_dat = "" ;
#endif

    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;
    EXTEND(SP,2) ;
    PUSHs(sv_2mortal(newSVpvn(pk_dat,pkey->size)));
    PUSHs(sv_2mortal(newSVpvn(pd_dat,pdata->size)));
    PUSHs(sv_2mortal(skey_SV));
    PUTBACK ;

    Trace(("calling associated cb\n"));
    count = perl_call_sv(getCurrentDB->associated, G_SCALAR);
    Trace(("called associated cb\n"));

    SPAGAIN ;

    if (count != 1)
        softCrash ("associate: expected 1 return value from prefix sub, got %d", count) ;

    retval = POPi ;

    PUTBACK ;
    
    /* retrieve the secondary key */
    DBT_clear(*skey);
    skey_ptr = SvPV(skey_SV, skey_len);
    skey->flags = DB_DBT_APPMALLOC;
    /* skey->size = SvCUR(skey_SV); */
    /* skey->data = (char*)safemalloc(skey->size); */
    skey->size = skey_len;
    skey->data = (char*)safemalloc(skey_len);
    memcpy(skey->data, skey_ptr, skey_len);
    Trace(("key is %d -- %.*s\n", skey->size, skey->size, skey->data));

    FREETMPS ;
    LEAVE ;

    return (retval) ;
}

#endif /* AT_LEAST_DB_3_3 */

static void
#ifdef AT_LEAST_DB_4_3
db_errcall_cb(const DB_ENV* dbenv, const char * db_errpfx, const char * buffer)
#else
db_errcall_cb(const char * db_errpfx, char * buffer)
#endif
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
hash_delete(char * hash, char * key)
{
    HV * hv = perl_get_hv(hash, TRUE);
    (void) hv_delete(hv, (char*)&key, sizeof(key), G_DISCARD);
}

static void
hash_store_iv(char * hash, char * key, IV value)
{
    HV * hv = perl_get_hv(hash, TRUE);
    (void)hv_store(hv, (char*)&key, sizeof(key), newSViv(value), 0);
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
    	    	BerkeleyDB__Txn txn, 
		const char *	file,
		const char *	subname,
		DBTYPE		type,
		int		flags,
		int		mode,
		DB_INFO * 	info,
		char *		password,
		int		enc_flags
	)
{
    DB_ENV *	env    = NULL ;
    BerkeleyDB 	RETVAL = NULL ;
    DB *	dbp ;
    int		Status ;
    DB_TXN* 	txnid = NULL ;

    Trace(("_db_open(dbenv[%p] ref_dbenv [%p] file[%s] subname [%s] type[%d] flags[%d] mode[%d]\n",
		dbenv, ref_dbenv, file, subname, type, flags, mode)) ;

    CurrentDB = db ;
    
    if (dbenv)
	env = dbenv->Env ;

    if (txn)
        txnid = txn->txn;

    Trace(("_db_open(dbenv[%p] ref_dbenv [%p] txn [%p] file[%s] subname [%s] type[%d] flags[%d] mode[%d]\n",
		dbenv, ref_dbenv, txn, file, subname, type, flags, mode)) ;

#if DB_VERSION_MAJOR == 2
    if (subname)
        softCrash("Subname needs Berkeley DB 3 or better") ;
#endif

#ifndef AT_LEAST_DB_4_1
	    if (password)
	        softCrash("-Encrypt needs Berkeley DB 4.x or better") ;
#endif /* ! AT_LEAST_DB_4_1 */

#if DB_VERSION_MAJOR > 2
    Status = db_create(&dbp, env, 0) ;
    Trace(("db_create returned %s\n", my_db_strerror(Status))) ;
    if (Status)
        return RETVAL ;

#ifdef AT_LEAST_DB_3_2
	dbp->BackRef = db;
#endif

#ifdef AT_LEAST_DB_3_3
    if (! env) {
	dbp->set_alloc(dbp, safemalloc, MyRealloc, safefree) ;
	dbp->set_errcall(dbp, db_errcall_cb) ;
    }
#endif

#ifdef AT_LEAST_DB_4_1
    /* set encryption */
    if (password)
    {
        Status = dbp->set_encrypt(dbp, password, enc_flags);
        Trace(("DB->set_encrypt passwd = %s, flags %d returned %s\n", 
			      		password, enc_flags,
  					my_db_strerror(Status))) ;
         if (Status)
              return RETVAL ;
    }
#endif	  

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
	Trace(("set_bt_compare [%p] returned %s\n",
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
	Trace(("set_q_extentsize [%d] returned %s\n",
		info->q_extentsize, my_db_strerror(Status)));
        if (Status)
            return RETVAL ;
#else
        softCrash("-ExtentSize needs at least Berkeley DB 3.2.x") ;
#endif
    }


#ifdef AT_LEAST_DB_4_1
    if ((Status = (dbp->open)(dbp, txnid, file, subname, type, flags, mode)) == 0) {
#else
    if ((Status = (dbp->open)(dbp, file, subname, type, flags, mode)) == 0) {
#endif /* AT_LEAST_DB_4_1 */
#else /* DB_VERSION_MAJOR == 2 */
    if ((Status = db_open(file, type, flags, mode, env, info, &dbp)) == 0) {
#endif /* DB_VERSION_MAJOR == 2 */

	Trace(("db_opened ok\n"));
	RETVAL = db ;
	RETVAL->dbp  = dbp ;
	RETVAL->txn  = txnid ;
#if DB_VERSION_MAJOR == 2
    	RETVAL->type = dbp->type ;
#else /* DB_VERSION_MAJOR > 2 */
#ifdef AT_LEAST_DB_3_3
    	dbp->get_type(dbp, &RETVAL->type) ;
#else /* DB 3.0 -> 3.2 */
    	RETVAL->type = dbp->get_type(dbp) ;
#endif
#endif /* DB_VERSION_MAJOR > 2 */
    	RETVAL->recno_or_queue = (RETVAL->type == DB_RECNO ||
	                          RETVAL->type == DB_QUEUE) ;
	RETVAL->filename = my_strdup(file) ;
	RETVAL->Status = Status ;
	RETVAL->active = TRUE ;
	hash_store_iv("BerkeleyDB::Term::Db", (char *)RETVAL, 1) ;
	Trace(("  storing %p %p in BerkeleyDB::Term::Db\n", RETVAL, dbp)) ;
	if (dbenv) {
	    RETVAL->cds_enabled = dbenv->cds_enabled ;
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


#include "constants.h"

MODULE = BerkeleyDB		PACKAGE = BerkeleyDB	PREFIX = env_

INCLUDE: constants.xs

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
	    const char *	db = NULL ;
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

DualType
_db_verify(ref)
	SV * 		ref
	CODE:
	{
#ifndef AT_LEAST_DB_3_1
	    softCrash("BerkeleyDB::db_verify needs Berkeley DB 3.1.x or better") ;
#else
	    HV *		hash ;
    	    DB *		dbp ;
	    SV * 		sv ;
	    const char *	db = NULL ;
	    const char *	subdb 	= NULL ;
	    const char *	outfile	= NULL ;
	    FILE *		ofh = NULL;
	    BerkeleyDB__Env	env 	= NULL ;
    	    DB_ENV *		dbenv   = NULL ;
	    u_int32_t		flags	= 0 ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(db,    "Filename", char *) ;
	    SetValue_pv(subdb, "Subname", char *) ;
	    SetValue_pv(outfile, "Outfile", char *) ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_ov(env, "Env", BerkeleyDB__Env) ;
            RETVAL = 0;
            if (outfile){
	        ofh = fopen(outfile, "w");
                if (! ofh)
                    RETVAL = errno;
            }
            if (! RETVAL) {
    	        if (env)
		    dbenv = env->Env ;
                RETVAL = db_create(&dbp, dbenv, 0) ;
	        if (RETVAL == 0) {
	            RETVAL = dbp->verify(dbp, db, subdb, ofh, flags) ;
	        }
	        if (outfile) 
                    fclose(ofh);
            }
#endif
	}
	OUTPUT:
	    RETVAL

DualType
_db_rename(ref)
	SV * 		ref
	CODE:
	{
#ifndef AT_LEAST_DB_3_1
	    softCrash("BerkeleyDB::db_rename needs Berkeley DB 3.1.x or better") ;
#else
	    HV *		hash ;
    	    DB *		dbp ;
	    SV * 		sv ;
	    const char *	db = NULL ;
	    const char *	subdb 	= NULL ;
	    const char *	newname	= NULL ;
	    BerkeleyDB__Env	env 	= NULL ;
    	    DB_ENV *		dbenv   = NULL ;
	    u_int32_t		flags	= 0 ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(db,    "Filename", char *) ;
	    SetValue_pv(subdb, "Subname", char *) ;
	    SetValue_pv(newname, "Newname", char *) ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_ov(env, "Env", BerkeleyDB__Env) ;
    	    if (env)
		dbenv = env->Env ;
            RETVAL = db_create(&dbp, dbenv, 0) ;
	    if (RETVAL == 0) {
	        RETVAL = (dbp->rename)(dbp, db, subdb, newname, flags) ;
	    }
#endif
	}
	OUTPUT:
	    RETVAL

MODULE = BerkeleyDB::Env		PACKAGE = BerkeleyDB::Env PREFIX = env_

BerkeleyDB::Env::Raw
create(flags=0)
	u_int32_t flags
	CODE:
	{
#ifndef AT_LEAST_DB_4_1
	    softCrash("$env->create needs Berkeley DB 4.1 or better") ;
#else
	    DB_ENV *	env ;
	    int    status;
	    RETVAL = NULL;
	    Trace(("in BerkeleyDB::Env::create flags=%d\n",  flags)) ;
	    status = db_env_create(&env, flags) ;
	    Trace(("db_env_create returned %s\n", my_db_strerror(status))) ;
	    if (status == 0) {
	        ZMALLOC(RETVAL, BerkeleyDB_ENV_type) ;
		RETVAL->Env = env ;
	        RETVAL->active = TRUE ;
	        RETVAL->opened = FALSE;
	        env->set_alloc(env, safemalloc, MyRealloc, safefree) ;
	        env->set_errcall(env, db_errcall_cb) ;
	    }
#endif	    
	}
	OUTPUT:
	    RETVAL

int
open(env, db_home=NULL, flags=0, mode=0777)
	BerkeleyDB::Env env
	char * db_home
	u_int32_t flags
	int mode
    CODE:
#ifndef AT_LEAST_DB_4_1
	    softCrash("$env->create needs Berkeley DB 4.1 or better") ;
#else
        RETVAL = env->Env->open(env->Env, db_home, flags, mode);
	env->opened = TRUE;
#endif
    OUTPUT:
        RETVAL

bool
cds_enabled(env)
	BerkeleyDB::Env env
	CODE:
	    RETVAL = env->cds_enabled ;
	OUTPUT:
	    RETVAL


int
set_encrypt(env, passwd, flags)
	BerkeleyDB::Env env
	const char * passwd
	u_int32_t flags
    CODE:
#ifndef AT_LEAST_DB_4_1
	    softCrash("$env->set_encrypt needs Berkeley DB 4.1 or better") ;
#else
        dieIfEnvOpened(env, "set_encrypt");
        RETVAL = env->Env->set_encrypt(env->Env, passwd, flags);
	env->opened = TRUE;
#endif
    OUTPUT:
        RETVAL




BerkeleyDB::Env::Raw
_db_appinit(self, ref, errfile=NULL)
	char *		self
	SV * 		ref
	SV * 		errfile 
	CODE:
	{
	    HV *	hash ;
	    SV *	sv ;
	    char *	enc_passwd = NULL ;
	    int		enc_flags = 0 ;
	    char *	home = NULL ;
	    char * 	server = NULL ;
	    char **	config = NULL ;
	    int		flags = 0 ;
	    int		setflags = 0 ;
	    int		cachesize = 0 ;
	    int		lk_detect = 0 ;
	    long	shm_key = 0 ;
	    SV *	errprefix = NULL;
	    DB_ENV *	env ;
	    int status ;

	    Trace(("in _db_appinit [%s] %d\n", self, ref)) ;
	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(home,      "Home", char *) ;
	    SetValue_pv(enc_passwd,"Enc_Passwd", char *) ;
	    SetValue_iv(enc_flags, "Enc_Flags") ;
	    SetValue_pv(config,    "Config", char **) ;
	    SetValue_sv(errprefix, "ErrPrefix") ;
	    SetValue_iv(flags,     "Flags") ;
	    SetValue_iv(setflags,  "SetFlags") ;
	    SetValue_pv(server,    "Server", char *) ;
	    SetValue_iv(cachesize, "Cachesize") ;
	    SetValue_iv(lk_detect, "LockDetect") ;
	    SetValue_iv(shm_key,   "SharedMemKey") ;
#ifndef AT_LEAST_DB_3_2
	    if (setflags)
	        softCrash("-SetFlags needs Berkeley DB 3.x or better") ;
#endif /* ! AT_LEAST_DB_3 */
#ifndef AT_LEAST_DB_3_1
	    if (shm_key)
	        softCrash("-SharedMemKey needs Berkeley DB 3.1 or better") ;
	    if (server)
	        softCrash("-Server needs Berkeley DB 3.1 or better") ;
#endif /* ! AT_LEAST_DB_3_1 */
#ifndef AT_LEAST_DB_4_1
	    if (enc_passwd)
	        softCrash("-Encrypt needs Berkeley DB 4.x or better") ;
#endif /* ! AT_LEAST_DB_4_1 */
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

	    if (SvGMAGICAL(errfile))
		    mg_get(errfile);
	    if (SvOK(errfile)) {
	        FILE * ef = GetFILEptr(errfile) ;
	    	if (! ef)
		    croak("Cannot open file ErrFile", Strerror(errno));
		RETVAL->ErrHandle = newSVsv(errfile) ;
	    	env->db_errfile = ef;
	    }
	    SetValue_iv(env->db_verbose, "Verbose") ;
	    env->db_errcall = db_errcall_cb ;
	    RETVAL->active = TRUE ;
	    RETVAL->opened = TRUE;
	    RETVAL->cds_enabled = ((flags & DB_INIT_CDB) != 0 ? TRUE : FALSE) ;
	    status = db_appinit(home, config, env, flags) ;
	    printf("  status = %d errno %d \n", status, errno) ;
	    Trace(("  status = %d env %d Env %d\n", status, RETVAL, env)) ;
	    if (status == 0)
	        hash_store_iv("BerkeleyDB::Term::Env", (char *)RETVAL, 1) ;
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
#ifdef AT_LEAST_DB_4_2
#    define DB_CLIENT	DB_RPCCLIENT
#endif
	  status = db_env_create(&RETVAL->Env, server ? DB_CLIENT : 0) ;
	  Trace(("db_env_create flags = %d returned %s\n", flags,
	  					my_db_strerror(status))) ;
	  env = RETVAL->Env ;
#ifdef AT_LEAST_DB_3_3
	  env->set_alloc(env, safemalloc, MyRealloc, safefree) ;
#endif
#ifdef AT_LEAST_DB_3_1
	  if (status == 0 && shm_key) {
	      status = env->set_shm_key(env, shm_key) ;
	      Trace(("set_shm_key [%d] returned %s\n", shm_key,
			my_db_strerror(status)));
	  }
#endif	  
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
#ifdef AT_LEAST_DB_4_1
	  /* set encryption */
	  if (enc_passwd && status == 0)
	  {
	      status = env->set_encrypt(env, enc_passwd, enc_flags);
	      Trace(("ENV->set_encrypt passwd = %s, flags %d returned %s\n", 
				      		enc_passwd, enc_flags,
	  					my_db_strerror(status))) ;
	  }
#endif	  
#ifdef AT_LEAST_DB_4
	  /* set the server */
	  if (server && status == 0)
	  {
	      status = env->set_rpc_server(env, NULL, server, 0, 0, 0);
	      Trace(("ENV->set_rpc_server server = %s returned %s\n", server,
	  					my_db_strerror(status))) ;
	  }
#else
#  if defined(AT_LEAST_DB_3_1) && ! defined(AT_LEAST_DB_4)
	  /* set the server */
	  if (server && status == 0)
	  {
	      status = env->set_server(env, server, 0, 0, 0);
	      Trace(("ENV->set_server server = %s returned %s\n", server,
	  					my_db_strerror(status))) ;
	  }
#  endif
#endif
#ifdef AT_LEAST_DB_3_2
	  if (setflags && status == 0)
	  {
	      status = env->set_flags(env, setflags, 1);
	      Trace(("ENV->set_flags value = %d returned %s\n", setflags,
	  					my_db_strerror(status))) ;
	  }
#endif
	  if (status == 0)
	  {
	    int		mode = 0 ;
	    /* Take a copy of the error prefix */
	    if (errprefix) {
	        Trace(("copying errprefix\n" )) ;
		RETVAL->ErrPrefix = newSVsv(errprefix) ;
		SvPOK_only(RETVAL->ErrPrefix) ;
	    }
	    if (RETVAL->ErrPrefix)
	        env->set_errpfx(env, SvPVX(RETVAL->ErrPrefix)) ;

	    if (SvGMAGICAL(errfile))
		    mg_get(errfile);
	    if (SvOK(errfile)) {
	        FILE * ef = GetFILEptr(errfile);
	    	if (! ef)
		    croak("Cannot open file ErrFile", Strerror(errno));
		RETVAL->ErrHandle = newSVsv(errfile) ;
	    	env->set_errfile(env, ef) ;

	    }

	    SetValue_iv(mode, "Mode") ;
	    env->set_errcall(env, db_errcall_cb) ;
	    RETVAL->active = TRUE ;
	    RETVAL->cds_enabled = ((flags & DB_INIT_CDB) != 0 ? TRUE : FALSE) ; 
#ifdef IS_DB_3_0_x
	    status = (env->open)(env, home, config, flags, mode) ;
#else /* > 3.0 */
	    status = (env->open)(env, home, flags, mode) ;
#endif
	    Trace(("ENV->open(env=%s,home=%s,flags=%d,mode=%d)\n",env,home,flags,mode)) ;
	    Trace(("ENV->open returned %s\n", my_db_strerror(status))) ;
	  }

	  if (status == 0)
	      hash_store_iv("BerkeleyDB::Term::Env", (char *)RETVAL, 1) ;
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
	  {
	      SV * sv_err = perl_get_sv(ERR_BUFF, FALSE);
	      sv_setpv(sv_err, db_strerror(status));
	  }
	}
	OUTPUT:
	    RETVAL

DB_ENV*
DB_ENV(env)
	BerkeleyDB::Env		env
	CODE:
	    if (env->active)
	        RETVAL = env->Env ;
	    else
	        RETVAL = NULL;


void
log_archive(env, flags=0)
	u_int32_t		flags
	BerkeleyDB::Env		env
	PPCODE:
	{
	  char ** list;
	  char ** file;
	  AV    * av;
#ifndef AT_LEAST_DB_3
          softCrash("log_archive needs at least Berkeley DB 3.x.x");
#else
#  ifdef AT_LEAST_DB_4
	  env->Status = env->Env->log_archive(env->Env, &list, flags) ;
#  else
#    ifdef AT_LEAST_DB_3_3
	  env->Status = log_archive(env->Env, &list, flags) ;
#    else
	  env->Status = log_archive(env->Env, &list, flags, safemalloc) ;
#    endif
#  endif
	  if (env->Status == 0 && list != NULL)
          {
	      for (file = list; *file != NULL; ++file)
	      {
	        XPUSHs(sv_2mortal(newSVpv(*file, 0))) ;
	      }
	      safefree(list);
	  }
#endif
	}

BerkeleyDB::Txn::Raw
_txn_begin(env, pid=NULL, flags=0)
	u_int32_t		flags
	BerkeleyDB::Env		env
	BerkeleyDB::Txn		pid
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
#  ifdef AT_LEAST_DB_4
	    	env->Env->txn_begin(env->Env, p_id, &txn, flags) ;
#  else
	    	txn_begin(env->Env, p_id, &txn, flags) ;
#  endif
#endif
	    if (env->TxnMgrStatus == 0) {
	      ZMALLOC(RETVAL, BerkeleyDB_Txn_type) ;
	      RETVAL->txn  = txn ;
	      RETVAL->active = TRUE ;
	      Trace(("_txn_begin created txn [%p] in [%p]\n", txn, RETVAL));
	      hash_store_iv("BerkeleyDB::Term::Txn", (char *)RETVAL, 1) ;
	    }
	    else
		RETVAL = NULL ;
	}
	OUTPUT:
	    RETVAL


#if DB_VERSION_MAJOR == 2
#  define env_txn_checkpoint(e,k,m,f) txn_checkpoint(e->Env->tx_info, k, m)
#else /* DB 3.0 or better */
#  ifdef AT_LEAST_DB_4 
#    define env_txn_checkpoint(e,k,m,f) e->Env->txn_checkpoint(e->Env, k, m, f)
#  else
#    ifdef AT_LEAST_DB_3_1
#      define env_txn_checkpoint(e,k,m,f) txn_checkpoint(e->Env, k, m, 0)
#    else
#      define env_txn_checkpoint(e,k,m,f) txn_checkpoint(e->Env, k, m)
#    endif
#  endif
#endif
DualType
env_txn_checkpoint(env, kbyte, min, flags=0)
	BerkeleyDB::Env		env
	long			kbyte
	long			min
	u_int32_t		flags

HV *
txn_stat(env)
	BerkeleyDB::Env		env
	HV *			RETVAL = NULL ;
	CODE:
	{
	    DB_TXN_STAT *	stat ;
#ifdef AT_LEAST_DB_4
	    if(env->Env->txn_stat(env->Env, &stat, 0) == 0) {
#else
#  ifdef AT_LEAST_DB_3_3
	    if(txn_stat(env->Env, &stat) == 0) {
#  else
#    if DB_VERSION_MAJOR == 2
	    if(txn_stat(env->Env->tx_info, &stat, safemalloc) == 0) {
#    else
	    if(txn_stat(env->Env, &stat, safemalloc) == 0) {
#    endif
#  endif
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
	ALIAS:	close =1
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
	    hash_delete("BerkeleyDB::Term::Env", (char *)env) ;
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
	  hash_delete("BerkeleyDB::Term::Env", (char *)env) ;
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
	    /* hash_store_iv("BerkeleyDB::Term::TxnMgr", (char *)txn, 1) ; */
	OUTPUT:
	    RETVAL

int
get_shm_key(env, id)
        BerkeleyDB::Env  env
	long  		 id = NO_INIT
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_4_2
	    softCrash("$env->get_shm_key needs Berkeley DB 4.2 or better") ;
#else
	    RETVAL = env->Env->get_shm_key(env->Env, &id);
#endif	    
	OUTPUT:
	    RETVAL
	    id


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
set_lg_bsize(env, bsize)
        BerkeleyDB::Env  env
	u_int32_t	 bsize
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3
	    softCrash("$env->set_lg_bsize needs Berkeley DB 3.0.55 or better") ;
#else
	    RETVAL = env->Status = env->Env->set_lg_bsize(env->Env, bsize);
#endif
	OUTPUT:
	    RETVAL

int
set_lg_max(env, lg_max)
        BerkeleyDB::Env  env
	u_int32_t	 lg_max
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3
	    softCrash("$env->set_lg_max needs Berkeley DB 3.0.55 or better") ;
#else
	    RETVAL = env->Status = env->Env->set_lg_max(env->Env, lg_max);
#endif
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
            dieIfEnvOpened(env, "set_data_dir");
	    RETVAL = env->Status = env->Env->set_data_dir(env->Env, dir);
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
#  ifdef AT_LEAST_DB_4
	    RETVAL = env->Status = env->Env->set_flags(env->Env, DB_NOLOCKING, do_lock);
#  else
#    if defined(AT_LEAST_DB_3_2_6) || defined(IS_DB_3_0_x)
	    RETVAL = env->Status = env->Env->set_mutexlocks(env->Env, do_lock);
#    else /* DB 3.1 or 3.2.3 */
	    RETVAL = env->Status = db_env_set_mutexlocks(do_lock);
#    endif
#  endif
#endif
	OUTPUT:
	    RETVAL

int
set_verbose(env, which, onoff)
        BerkeleyDB::Env  env
	u_int32_t	 which
	int	 	 onoff
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3
	    softCrash("$env->set_verbose needs Berkeley DB 3.x or better") ;
#else
	    RETVAL = env->Status = env->Env->set_verbose(env->Env, which, onoff);
#endif
	OUTPUT:
	    RETVAL

int
set_flags(env, flags, onoff)
        BerkeleyDB::Env  env
	u_int32_t	 flags
	int	 	 onoff
	INIT:
	  ckActive_Database(env->active) ;
	CODE:
#ifndef AT_LEAST_DB_3_2
	    softCrash("$env->set_flags needs Berkeley DB 3.2.x or better") ;
#else
	    RETVAL = env->Status = env->Env->set_flags(env->Env, flags, onoff);
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
    	    BerkeleyDB__Txn 	txn = NULL ;
	    char *	enc_passwd = NULL ;
	    int		enc_flags = 0 ;

    	    Trace(("_db_open_hash start\n")) ;
	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Filename", char *) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(txn, "Txn", BerkeleyDB__Txn) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;
	    SetValue_pv(enc_passwd,"Enc_Passwd", char *) ;
	    SetValue_iv(enc_flags, "Enc_Flags") ;

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
	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, txn, file, subname, DB_HASH, flags, mode, &info, enc_passwd, enc_flags) ;
    	    Trace(("_db_open_hash end\n")) ;
	}
	OUTPUT:
	    RETVAL


HV *
db_stat(db, flags=0)
	int			flags
	BerkeleyDB::Common	db
	HV *			RETVAL = NULL ;
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	{
#if DB_VERSION_MAJOR == 2
	    softCrash("$db->db_stat for a Hash needs Berkeley DB 3.x or better") ;
#else
	    DB_HASH_STAT *	stat ;
#ifdef AT_LEAST_DB_4_3
	    db->Status = ((db->dbp)->stat)(db->dbp, db->txn, &stat, flags) ;
#else        
#ifdef AT_LEAST_DB_3_3
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, flags) ;
#else
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, safemalloc, flags) ;
#endif
#endif
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
#ifndef AT_LEAST_DB_3_1
		hv_store_iv(RETVAL, "hash_nelem", stat->hash_nelem);
#endif
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
    	    BerkeleyDB__Txn 	txn = NULL ;
	    static char * 		Names[] = {"", "Btree", "Hash", "Recno"} ;
	    char *	enc_passwd = NULL ;
	    int		enc_flags = 0 ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Filename", char *) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(txn, "Txn", BerkeleyDB__Txn) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;
	    SetValue_pv(enc_passwd,"Enc_Passwd", char *) ;
	    SetValue_iv(enc_flags, "Enc_Flags") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.h_ffactor, "Ffactor") ;
	    SetValue_iv(info.h_nelem, "Nelem") ;
	    SetValue_iv(info.flags, "Property") ;
	    ZMALLOC(db, BerkeleyDB_type) ;

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, txn, file, subname, DB_UNKNOWN, flags, mode, &info, enc_passwd, enc_flags) ;
	    XPUSHs(sv_2mortal(newSViv(PTR2IV(RETVAL))));
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
    	    BerkeleyDB__Txn 	txn = NULL ;
	    char *	enc_passwd = NULL ;
	    int		enc_flags = 0 ;

	    Trace(("In _db_open_btree\n"));
	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Filename", char*) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(txn, "Txn", BerkeleyDB__Txn) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;
	    SetValue_pv(enc_passwd,"Enc_Passwd", char *) ;
	    SetValue_iv(enc_flags, "Enc_Flags") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.bt_minkey, "Minkey") ;
	    SetValue_iv(info.flags, "Property") ;
	    ZMALLOC(db, BerkeleyDB_type) ;
	    if ((sv = readHash(hash, "Compare")) && sv != &PL_sv_undef) {
		Trace(("    Parsed Compare callback\n"));
		info.bt_compare = btree_compare ;
		db->compare = newSVsv(sv) ;
	    }
	    /* DB_DUPSORT was introduced in DB 2.5.9 */
	    if ((sv = readHash(hash, "DupCompare")) && sv != &PL_sv_undef) {
#ifdef DB_DUPSORT
		Trace(("    Parsed DupCompare callback\n"));
		info.dup_compare = dup_compare ;
		db->dup_compare = newSVsv(sv) ;
		info.flags |= DB_DUP|DB_DUPSORT ;
#else
	        softCrash("DupCompare needs Berkeley DB 2.5.9 or later") ;
#endif
	    }
	    if ((sv = readHash(hash, "Prefix")) && sv != &PL_sv_undef) {
		Trace(("    Parsed Prefix callback\n"));
		info.bt_prefix = btree_prefix ;
		db->prefix = newSVsv(sv) ;
	    }

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, txn, file, subname, DB_BTREE, flags, mode, &info, enc_passwd, enc_flags) ;
	}
	OUTPUT:
	    RETVAL


HV *
db_stat(db, flags=0)
	int			flags
	BerkeleyDB::Common	db
	HV *			RETVAL = NULL ;
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	{
	    DB_BTREE_STAT *	stat ;
#ifdef AT_LEAST_DB_4_3
	    db->Status = ((db->dbp)->stat)(db->dbp, db->txn, &stat, flags) ;
#else        
#ifdef AT_LEAST_DB_3_3
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, flags) ;
#else
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, safemalloc, flags) ;
#endif
#endif
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
    	    BerkeleyDB__Txn 	txn = NULL ;
	    char *	enc_passwd = NULL ;
	    int		enc_flags = 0 ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Fname", char*) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_ov(txn, "Txn", BerkeleyDB__Txn) ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;
	    SetValue_pv(enc_passwd,"Enc_Passwd", char *) ;
	    SetValue_iv(enc_flags, "Enc_Flags") ;

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

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, txn, file, subname, DB_RECNO, flags, mode, &info, enc_passwd, enc_flags) ;
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
    	    BerkeleyDB__Txn 	txn = NULL ;
	    char *	enc_passwd = NULL ;
	    int		enc_flags = 0 ;

	    hash = (HV*) SvRV(ref) ;
	    SetValue_pv(file, "Fname", char*) ;
	    SetValue_pv(subname, "Subname", char *) ;
	    SetValue_ov(dbenv, "Env", BerkeleyDB__Env) ;
	    ref_dbenv = sv ;
	    SetValue_ov(txn, "Txn", BerkeleyDB__Txn) ;
	    SetValue_iv(flags, "Flags") ;
	    SetValue_iv(mode, "Mode") ;
	    SetValue_pv(enc_passwd,"Enc_Passwd", char *) ;
	    SetValue_iv(enc_flags, "Enc_Flags") ;

       	    Zero(&info, 1, DB_INFO) ;
	    SetValue_iv(info.db_cachesize, "Cachesize") ;
	    SetValue_iv(info.db_lorder, "Lorder") ;
	    SetValue_iv(info.db_pagesize, "Pagesize") ;
	    SetValue_iv(info.bt_minkey, "Minkey") ;
    	    SetValue_iv(info.q_extentsize, "ExtentSize") ;


	    SetValue_iv(info.flags, "Property") ;
	    if ((sv = readHash(hash, "Len")) && sv != &PL_sv_undef) {
		info.re_len = SvIV(sv) ; ;
		flagSet_DB2(info.flags, DB_FIXEDLEN) ;
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

	    RETVAL = my_db_open(db, ref, ref_dbenv, dbenv, txn, file, subname, DB_QUEUE, flags, mode, &info, enc_passwd, enc_flags) ;
#endif
	}
	OUTPUT:
	    RETVAL

HV *
db_stat(db, flags=0)
	int			flags
	BerkeleyDB::Common	db
	HV *			RETVAL = NULL ;
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	{
#if DB_VERSION_MAJOR == 2
	    softCrash("$db->db_stat for a Queue needs Berkeley DB 3.x or better") ;
#else /* Berkeley DB 3, or better */
	    DB_QUEUE_STAT *	stat ;
#ifdef AT_LEAST_DB_4_3
	    db->Status = ((db->dbp)->stat)(db->dbp, db->txn, &stat, flags) ;
#else        
#ifdef AT_LEAST_DB_3_3
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, flags) ;
#else
	    db->Status = ((db->dbp)->stat)(db->dbp, &stat, safemalloc, flags) ;
#endif
#endif
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
	int 			flags
        BerkeleyDB::Common 	db
	INIT:
	    ckActive_Database(db->active) ;
	    saveCurrentDB(db) ;
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
	    hash_delete("BerkeleyDB::Term::Db", (char *)db) ;
	    -- db->open_cursors ;
	    Trace(("end of BerkeleyDB::Common::db_close\n"));
	OUTPUT:
	    RETVAL

void
dab__DESTROY(db)
	BerkeleyDB::Common	db
	CODE:
	  saveCurrentDB(db) ;
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
	u_int32_t		flags
        BerkeleyDB::Common 	db
        BerkeleyDB::Cursor 	RETVAL = NULL ;
	ALIAS: __db_write_cursor = 1
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
	{
	  DBC *	cursor ;
	  saveCurrentDB(db) ;
	  if (ix == 1 && db->cds_enabled) {
#ifdef AT_LEAST_DB_3
	      flags |= DB_WRITECURSOR;
#else	      
	      flags |= DB_RMW;
#endif	      
	  }
	  if ((db->Status = db_cursor(db, db->txn, &cursor, flags)) == 0){
	      ZMALLOC(RETVAL, BerkeleyDB__Cursor_type) ;
	      db->open_cursors ++ ;
	      RETVAL->parent_db  = db ;
	      RETVAL->cursor  = cursor ;
	      RETVAL->dbp     = db->dbp ;
	      RETVAL->txn     = db->txn ;
              RETVAL->type    = db->type ;
              RETVAL->recno_or_queue    = db->recno_or_queue ;
              RETVAL->cds_enabled    = db->cds_enabled ;
              RETVAL->filename    = my_strdup(db->filename) ;
              RETVAL->compare = db->compare ;
              RETVAL->dup_compare = db->dup_compare ;
#ifdef AT_LEAST_DB_3_3
              RETVAL->associated = db->associated ;
	      RETVAL->secondary_db  = db->secondary_db;
#endif
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
	      hash_store_iv("BerkeleyDB::Term::Cursor", (char *)RETVAL, 1) ;
	  }
	}
	OUTPUT:
	  RETVAL

BerkeleyDB::Cursor::Raw
_db_join(db, cursors, flags=0)
	u_int32_t		flags
        BerkeleyDB::Common 	db
	AV *			cursors
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
	  saveCurrentDB(db) ;
	  if (count < 1 )
	      softCrash("db_join: No cursors in parameter list") ;
	  cursor_list = (DBC **)safemalloc(sizeof(DBC*) * (count + 1));
	  for (i = 0 ; i < count ; ++i) {
	      SV * obj = (SV*) * av_fetch(cursors, i, FALSE) ;
	      IV tmp = SvIV(getInnerObject(obj)) ;
	      BerkeleyDB__Cursor cur = INT2PTR(BerkeleyDB__Cursor, tmp);
	      if (cur->dbp == db->dbp)
	          softCrash("attempted to do a self-join");
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
#ifdef AT_LEAST_DB_3_3
              RETVAL->associated = db->associated ;
	      RETVAL->secondary_db  = db->secondary_db;
#endif
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
	      hash_store_iv("BerkeleyDB::Term::Cursor", (char *)RETVAL, 1) ;
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


bool
cds_enabled(db)
        BerkeleyDB::Common 	db
	INIT:
	    ckActive_Database(db->active) ;
	CODE:
	    RETVAL = db->cds_enabled ;
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
#ifdef AT_LEAST_DB_3_3
	    db->dbp->get_byteswapped(db->dbp, &RETVAL) ;
#else
	    RETVAL = db->dbp->get_byteswapped(db->dbp) ;
#endif
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
	    DBM_setFilter(db->filter_fetch_key, code) ;

SV *
filter_store_key(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_store_key, code) ;

SV *
filter_fetch_value(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_fetch_value, code) ;

SV *
filter_store_value(db, code)
	BerkeleyDB::Common		db
	SV *		code
	SV *		RETVAL = &PL_sv_undef ;
	CODE:
	    DBM_setFilter(db->filter_store_value, code) ;

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
	u_int		flags
	BerkeleyDB::Common	db
	DBTKEY		key
	INIT:
	    Trace(("db_del db[%p] in [%p] txn[%p] key[%.*s] flags[%d]\n", db->dbp, db, db->txn, key.size, key.data, flags)) ;
	    ckActive_Database(db->active) ;
	    saveCurrentDB(db) ;


#ifdef AT_LEAST_DB_3
#  ifdef AT_LEAST_DB_3_2
#    define writeToKey() (flagSet(DB_CONSUME)||flagSet(DB_CONSUME_WAIT)||flagSet(DB_GET_BOTH)||flagSet(DB_SET_RECNO))
#  else
#    define writeToKey() (flagSet(DB_CONSUME)||flagSet(DB_GET_BOTH)||flagSet(DB_SET_RECNO))
#  endif
#else
#define writeToKey() (flagSet(DB_GET_BOTH)||flagSet(DB_SET_RECNO))
#endif
#define db_get(db, key, data, flags)   \
	(db->Status = ((db->dbp)->get)(db->dbp, db->txn, &key, &data, flags))
DualType
db_get(db, key, data, flags=0)
	u_int		flags
	BerkeleyDB::Common	db
	DBTKEY_B	key
	DBT_OPT		data
	CODE:
	  ckActive_Database(db->active) ;
	  saveCurrentDB(db) ;
	  SetPartial(data,db) ;
	  Trace(("db_get db[%p] in [%p] txn[%p] key [%.*s] flags[%d]\n", db->dbp, db, db->txn, key.size, key.data, flags)) ;
	  RETVAL = db_get(db, key, data, flags);
	  Trace(("  RETVAL %d\n", RETVAL));
	OUTPUT:
	  RETVAL
	  key	if (writeToKey()) OutputKey(ST(1), key) ;
	  data

#define db_pget(db, key, pkey, data, flags)   \
	(db->Status = ((db->dbp)->pget)(db->dbp, db->txn, &key, &pkey, &data, flags))
DualType
db_pget(db, key, pkey, data, flags=0)
	u_int		flags
	BerkeleyDB::Common	db
	DBTKEY_B	key
	DBTKEY_B	pkey = NO_INIT
	DBT_OPT		data
	CODE:
#ifndef AT_LEAST_DB_3_3
          softCrash("db_pget needs at least Berkeley DB 3.3");
#else
	  Trace(("db_pget db [%p] in [%p] txn [%p] flags [%d]\n", db->dbp, db, db->txn, flags)) ;
	  ckActive_Database(db->active) ;
	  saveCurrentDB(db) ;
	  SetPartial(data,db) ;
	  DBT_clear(pkey);
	  RETVAL = db_pget(db, key, pkey, data, flags);
	  Trace(("  RETVAL %d\n", RETVAL));
#endif
	OUTPUT:
	  RETVAL
	  key	if (writeToKey()) OutputKey(ST(1), key) ;
	  pkey
	  data

#define db_put(db,key,data,flag)	\
		(db->Status = (db->dbp->put)(db->dbp,db->txn,&key,&data,flag))
DualType
db_put(db, key, data, flags=0)
	u_int			flags
	BerkeleyDB::Common	db
	DBTKEY			key
	DBT			data
	CODE:
	  ckActive_Database(db->active) ;
	  saveCurrentDB(db) ;
	  /* SetPartial(data,db) ; */
	  Trace(("db_put db[%p] in [%p] txn[%p] key[%.*s] data [%.*s] flags[%d]\n", db->dbp, db, db->txn, key.size, key.data, data.size, data.data, flags)) ;
	  RETVAL = db_put(db, key, data, flags);
	  Trace(("  RETVAL %d\n", RETVAL));
	OUTPUT:
	  RETVAL
	  key	if (flagSet(DB_APPEND)) OutputKey(ST(1), key) ;

#define db_key_range(db, key, range, flags)   \
	(db->Status = ((db->dbp)->key_range)(db->dbp, db->txn, &key, &range, flags))
DualType
db_key_range(db, key, less, equal, greater, flags=0)
	u_int32_t	flags
	BerkeleyDB::Common	db
	DBTKEY_B	key
	double          less = 0.0 ;
	double          equal = 0.0 ;
	double          greater = 0.0 ;
	CODE:
	{
#ifndef AT_LEAST_DB_3_1
          softCrash("key_range needs Berkeley DB 3.1.x or later") ;
#else
          DB_KEY_RANGE range ;
          range.less = range.equal = range.greater = 0.0 ;
	  ckActive_Database(db->active) ;
	  saveCurrentDB(db) ;
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
int
db_fd(db)
	BerkeleyDB::Common	db
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	  saveCurrentDB(db) ;
	  db_fd(db, RETVAL) ;
	OUTPUT:
	  RETVAL


#define db_sync(db, fl)	(db->Status = (db->dbp->sync)(db->dbp, fl))
DualType
db_sync(db, flags=0)
	u_int			flags
	BerkeleyDB::Common	db
	INIT:
	  ckActive_Database(db->active) ;
	  saveCurrentDB(db) ;

void
_Txn(db, txn=NULL)
        BerkeleyDB::Common      db
        BerkeleyDB::Txn         txn
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
	   if (txn) {
	       Trace(("_Txn[%p] in[%p] active [%d]\n", txn->txn, txn, txn->active));
	       ckActive_Transaction(txn->active) ;
	       db->txn = txn->txn ;
	   }
	   else {
	       Trace(("_Txn[undef] \n"));
	       db->txn = NULL ;
	   }


#define db_truncate(db, countp, flags)  \
	(db->Status = ((db->dbp)->truncate)(db->dbp, db->txn, &countp, flags))
DualType
truncate(db, countp, flags=0)
	BerkeleyDB::Common	db
	u_int32_t		countp
	u_int32_t		flags
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
#ifndef AT_LEAST_DB_3_3
          softCrash("truncate needs Berkeley DB 3.3 or later") ;
#else
	  saveCurrentDB(db) ;
	  RETVAL = db_truncate(db, countp, flags);
#endif
	OUTPUT:
	  RETVAL
	  countp

#ifdef AT_LEAST_DB_4_1
#  define db_associate(db, sec, cb, flags)\
	(db->Status = ((db->dbp)->associate)(db->dbp, NULL, sec->dbp, &cb, flags))
#else
#  define db_associate(db, sec, cb, flags)\
	(db->Status = ((db->dbp)->associate)(db->dbp, sec->dbp, &cb, flags))
#endif
DualType
associate(db, secondary, callback, flags=0)
	BerkeleyDB::Common	db
	BerkeleyDB::Common	secondary
	SV*			callback
	u_int32_t		flags
	INIT:
	  ckActive_Database(db->active) ;
	CODE:
#ifndef AT_LEAST_DB_3_3
          softCrash("associate needs Berkeley DB 3.3 or later") ;
#else
	  saveCurrentDB(db) ;
	  /* db->associated = newSVsv(callback) ; */
	  secondary->associated = newSVsv(callback) ;
	  /* secondary->dbp->app_private = secondary->associated ; */
	  secondary->secondary_db = TRUE;
	  RETVAL = db_associate(db, secondary, associate_cb, flags);
#endif
	OUTPUT:
	  RETVAL


MODULE = BerkeleyDB::Cursor              PACKAGE = BerkeleyDB::Cursor	PREFIX = cu_

BerkeleyDB::Cursor::Raw
_c_dup(db, flags=0)
	u_int32_t		flags
    	BerkeleyDB::Cursor	db
        BerkeleyDB::Cursor 	RETVAL = NULL ;
	INIT:
	    saveCurrentDB(db->parent_db);
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
              RETVAL->cds_enabled    = db->cds_enabled ;
              RETVAL->filename    = my_strdup(db->filename) ;
              RETVAL->compare = db->compare ;
              RETVAL->dup_compare = db->dup_compare ;
#ifdef AT_LEAST_DB_3_3
              RETVAL->associated = db->associated ;
#endif
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
	      hash_store_iv("BerkeleyDB::Term::Cursor", (char *)RETVAL, 1) ;
	  }
#endif	
	}
	OUTPUT:
	  RETVAL

DualType
_c_close(db)
    BerkeleyDB::Cursor	db
	INIT:
	  saveCurrentDB(db->parent_db);
	  ckActive_Cursor(db->active) ;
	  hash_delete("BerkeleyDB::Term::Cursor", (char *)db) ;
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
	  saveCurrentDB(db->parent_db);
	  Trace(("In BerkeleyDB::Cursor::_DESTROY db %d dirty=%d active=%d\n", db, PL_dirty, db->active));
	  hash_delete("BerkeleyDB::Term::Cursor", (char *)db) ;
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
    int			flags
    BerkeleyDB::Cursor	db
	INIT:
	  saveCurrentDB(db->parent_db);
	  ckActive_Cursor(db->active) ;
	OUTPUT:
	  RETVAL


#define cu_c_get(c,k,d,f) (c->Status = (c->cursor->c_get)(c->cursor,&k,&d,f))
DualType
cu_c_get(db, key, data, flags=0)
    int			flags
    BerkeleyDB::Cursor	db
    DBTKEY_B		key 
    DBT_B		data 
	INIT:
	  Trace(("c_get db [%p] in [%p] flags [%d]\n", db->dbp, db, flags)) ;
	  saveCurrentDB(db->parent_db);
	  ckActive_Cursor(db->active) ;
	  /* DBT_clear(key); */
	  /* DBT_clear(data); */
	  SetPartial(data,db) ;
	  Trace(("c_get end\n")) ;
	OUTPUT:
	  RETVAL
	  key
	  data		if (! flagSet(DB_JOIN_ITEM)) OutputValue_B(ST(2), data) ;

#define cu_c_pget(c,k,p,d,f) (c->Status = (c->secondary_db ? (c->cursor->c_pget)(c->cursor,&k,&p,&d,f) : EINVAL))
DualType
cu_c_pget(db, key, pkey, data, flags=0)
    int			flags
    BerkeleyDB::Cursor	db
    DBTKEY_B		key
    DBTKEY_B		pkey = NO_INIT
    DBT_B		data
	CODE:
#ifndef AT_LEAST_DB_3_3
          softCrash("db_c_pget needs at least Berkeley DB 3.3");
#else
	  Trace(("c_pget db [%d] flags [%d]\n", db, flags)) ;
	  saveCurrentDB(db->parent_db);
	  ckActive_Cursor(db->active) ;
	  SetPartial(data,db) ;
	  DBT_clear(pkey);
	  RETVAL = cu_c_pget(db, key, pkey, data, flags);
	  Trace(("c_pget end\n")) ;
#endif
	OUTPUT:
	  RETVAL
	  key
	  pkey
	  data		



#define cu_c_put(c,k,d,f)  (c->Status = (c->cursor->c_put)(c->cursor,&k,&d,f))
DualType
cu_c_put(db, key, data, flags=0)
    int			flags
    BerkeleyDB::Cursor	db
    DBTKEY		key
    DBT			data
	INIT:
	  saveCurrentDB(db->parent_db);
	  ckActive_Cursor(db->active) ;
	  /* SetPartial(data,db) ; */
	OUTPUT:
	  RETVAL

#define cu_c_count(c,p,f) (c->Status = (c->cursor->c_count)(c->cursor,&p,f))
DualType
cu_c_count(db, count, flags=0)
    int			flags
    BerkeleyDB::Cursor	db
    u_int32_t           count = NO_INIT
	CODE:
#ifndef AT_LEAST_DB_3_1
          softCrash("c_count needs at least Berkeley DB 3.1.x");
#else
	  Trace(("c_get count [%d] flags [%d]\n", db, flags)) ;
	  saveCurrentDB(db->parent_db);
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
	u_int32_t		flags
	BerkeleyDB::TxnMgr	txnmgr
	BerkeleyDB::Txn		pid
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
#  ifdef AT_LEAST_DB_4
	    	txnmgr->env->Env->txn_begin(txnmgr->env->Env, p_id, &txn, flags) ;
#  else
	    	txn_begin(txnmgr->env->Env, p_id, &txn, flags) ;
#  endif
#endif
	    if (txnmgr->env->TxnMgrStatus == 0) {
	      ZMALLOC(RETVAL, BerkeleyDB_Txn_type) ;
	      RETVAL->txn  = txn ;
	      RETVAL->active = TRUE ;
	      Trace(("_txn_begin created txn [%d] in [%d]\n", txn, RETVAL));
	      hash_store_iv("BerkeleyDB::Term::Txn", (char *)RETVAL, 1) ;
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
#  define xx_txn_checkpoint(t,k,m,f) txn_checkpoint(t->env->Env->tx_info, k, m)
#else
#  ifdef AT_LEAST_DB_4 
#    define xx_txn_checkpoint(e,k,m,f) e->env->Env->txn_checkpoint(e->env->Env, k, m, f)
#  else
#    ifdef AT_LEAST_DB_3_1
#      define xx_txn_checkpoint(t,k,m,f) txn_checkpoint(t->env->Env, k, m, 0)
#    else
#      define xx_txn_checkpoint(t,k,m,f) txn_checkpoint(t->env->Env, k, m)
#    endif
#  endif
#endif
DualType
xx_txn_checkpoint(txnp, kbyte, min, flags=0)
	BerkeleyDB::TxnMgr	txnp
	long			kbyte
	long			min
	u_int32_t		flags

HV *
txn_stat(txnp)
	BerkeleyDB::TxnMgr	txnp
	HV *			RETVAL = NULL ;
	CODE:
	{
	    DB_TXN_STAT *	stat ;
#ifdef AT_LEAST_DB_4
	    if(txnp->env->Env->txn_stat(txnp->env->Env, &stat, 0) == 0) {
#else
#  ifdef AT_LEAST_DB_3_3
	    if(txn_stat(txnp->env->Env, &stat) == 0) {
#  else
#    if DB_VERSION_MAJOR == 2
	    if(txn_stat(txnp->env->Env->tx_info, &stat, safemalloc) == 0) {
#    else
	    if(txn_stat(txnp->env->Env, &stat, safemalloc) == 0) {
#    endif
#  endif
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
    int 		flags
    const char *	dir
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
#ifdef AT_LEAST_DB_4
	    tid->txn->abort(tid->txn) ;
#else
	    txn_abort(tid->txn) ;
#endif
          RETVAL = (int)tid ;
	  hash_delete("BerkeleyDB::Term::Txn", (char *)tid) ;
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

#ifdef AT_LEAST_DB_4
#  define xx_txn_prepare(t) (t->Status = t->txn->prepare(t->txn, 0))
#else
#  ifdef AT_LEAST_DB_3_3
#    define xx_txn_prepare(t) (t->Status = txn_prepare(t->txn, 0))
#  else
#    define xx_txn_prepare(t) (t->Status = txn_prepare(t->txn))
#  endif
#endif
DualType
xx_txn_prepare(tid)
	BerkeleyDB::Txn	tid
	INIT:
	    ckActive_Transaction(tid->active) ;

#ifdef AT_LEAST_DB_4
#  define _txn_commit(t,flags) (t->Status = t->txn->commit(t->txn, flags))
#else
#  if DB_VERSION_MAJOR == 2
#    define _txn_commit(t,flags) (t->Status = txn_commit(t->txn))
#  else
#    define _txn_commit(t, flags) (t->Status = txn_commit(t->txn, flags))
#  endif
#endif
DualType
_txn_commit(tid, flags=0)
	u_int32_t	flags
	BerkeleyDB::Txn	tid
	INIT:
	    ckActive_Transaction(tid->active) ;
	    hash_delete("BerkeleyDB::Term::Txn", (char *)tid) ;
	    tid->active = FALSE ;

#ifdef AT_LEAST_DB_4
#  define _txn_abort(t) (t->Status = t->txn->abort(t->txn))
#else
#  define _txn_abort(t) (t->Status = txn_abort(t->txn))
#endif
DualType
_txn_abort(tid)
	BerkeleyDB::Txn	tid
	INIT:
	    ckActive_Transaction(tid->active) ;
	    hash_delete("BerkeleyDB::Term::Txn", (char *)tid) ;
	    tid->active = FALSE ;

#ifdef AT_LEAST_DB_4
#  define _txn_discard(t,f) (t->Status = t->txn->discard(t->txn, f))
#else
#  ifdef AT_LEAST_DB_3_3_4
#    define _txn_discard(t,f) (t->Status = txn_discard(t->txn, f))
#  else
#    define _txn_discard(t,f) (int)softCrash("txn_discard needs Berkeley DB 3.3.4 or better") ;
#  endif
#endif
DualType
_txn_discard(tid, flags=0)
	BerkeleyDB::Txn	tid
	u_int32_t       flags
	INIT:
	    ckActive_Transaction(tid->active) ;
	    hash_delete("BerkeleyDB::Term::Txn", (char *)tid) ;
	    tid->active = FALSE ;

#ifdef AT_LEAST_DB_4
#  define xx_txn_id(t) t->txn->id(t->txn)
#else
#  define xx_txn_id(t) txn_id(t->txn)
#endif
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
            saveCurrentDB(db) ;
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
        DBTKEY              key = NO_INIT
        CODE:
        {
            DBT         value ;

            saveCurrentDB(db) ;
	    DBT_clear(key) ;
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
            saveCurrentDB(db) ;
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

