/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_tcl_ext_h_
#define	_tcl_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
int bdb_HCommand __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
#if DB_DBM_HSEARCH != 0
int bdb_NdbmOpen __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DBM **));
#endif
#if DB_DBM_HSEARCH != 0
int bdb_DbmCommand
    __P((Tcl_Interp *, int, Tcl_Obj * CONST*, int, DBM *));
#endif
int ndbm_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
int bdb_RandCommand __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
int tcl_Mutex __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *,
   DBTCL_INFO *));
int db_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
int dbc_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
int env_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
int tcl_EnvRemove __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
     DB_ENV *, DBTCL_INFO *));
int tcl_EnvVerbose __P((Tcl_Interp *, DB_ENV *, Tcl_Obj *,
   Tcl_Obj *));
int tcl_EnvTest __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
DBTCL_INFO *_NewInfo __P((Tcl_Interp *,
   void *, char *, enum INFOTYPE));
void *_NameToPtr __P((CONST char *));
char *_PtrToName __P((CONST void *));
DBTCL_INFO *_PtrToInfo __P((CONST void *));
DBTCL_INFO *_NameToInfo __P((CONST char *));
void  _SetInfoData __P((DBTCL_INFO *, void *));
void  _DeleteInfo __P((DBTCL_INFO *));
int _SetListElem __P((Tcl_Interp *,
   Tcl_Obj *, void *, int, void *, int));
int _SetListElemInt __P((Tcl_Interp *, Tcl_Obj *, void *, int));
int _SetListRecnoElem __P((Tcl_Interp *, Tcl_Obj *,
    db_recno_t, u_char *, int));
int _GetGlobPrefix __P((char *, char **));
int _ReturnSetup __P((Tcl_Interp *, int, char *));
int _ErrorSetup __P((Tcl_Interp *, int, char *));
void _ErrorFunc __P((CONST char *, char *));
int _GetLsn __P((Tcl_Interp *, Tcl_Obj *, DB_LSN *));
void _debug_check  __P((void));
int tcl_LockDetect __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LockGet __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LockStat __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LockVec __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogArchive __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogCompare __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*));
int tcl_LogFile __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogFlush __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogGet __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogPut __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogRegister __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogStat __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_LogUnregister __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
void _MpInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));
int tcl_MpSync __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
int tcl_MpTrickle __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_Mp __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *, DBTCL_INFO *));
int tcl_MpStat __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
void _TxnInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));
int tcl_TxnCheckpoint __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int tcl_Txn __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *, DBTCL_INFO *));
int tcl_TxnStat __P((Tcl_Interp *, int,
   Tcl_Obj * CONST*, DB_ENV *));
int txn_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
#if defined(__cplusplus)
}
#endif
#endif /* _tcl_ext_h_ */
