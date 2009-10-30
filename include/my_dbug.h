/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _dbug_h
#define _dbug_h

#ifdef  __cplusplus
extern "C" {
#endif
#if !defined(DBUG_OFF) && !defined(_lint)

struct _db_stack_frame_ {
  const char *func;      /* function name of the previous stack frame       */
  const char *file;      /* filename of the function of previous frame      */
  uint level;            /* this nesting level, highest bit enables tracing */
  struct _db_stack_frame_ *prev; /* pointer to the previous frame */
};

struct  _db_code_state_;
extern  my_bool _dbug_on_;
extern  my_bool _db_keyword_(struct _db_code_state_ *, const char *, int);
extern  int _db_explain_(struct _db_code_state_ *cs, char *buf, size_t len);
extern  int _db_explain_init_(char *buf, size_t len);
extern	int _db_is_pushed_(void);
extern  void _db_setjmp_(void);
extern  void _db_longjmp_(void);
extern  void _db_process_(const char *name);
extern  void _db_push_(const char *control);
extern  void _db_pop_(void);
extern  void _db_set_(const char *control);
extern  void _db_set_init_(const char *control);
extern void _db_enter_(const char *_func_, const char *_file_, uint _line_,
                       struct _db_stack_frame_ *_stack_frame_);
extern  void _db_return_(uint _line_, struct _db_stack_frame_ *_stack_frame_);
extern  void _db_pargs_(uint _line_,const char *keyword);
extern  void _db_doprnt_ _VARARGS((const char *format,...))
  ATTRIBUTE_FORMAT(printf, 1, 2);
extern  void _db_dump_(uint _line_,const char *keyword,
                       const unsigned char *memory, size_t length);
extern  void _db_end_(void);
extern  void _db_lock_file_(void);
extern  void _db_unlock_file_(void);
extern  FILE *_db_fp_(void);
extern  void _db_flush_();
extern  const char* _db_get_func_(void);

#define DBUG_ENTER(a) struct _db_stack_frame_ _db_stack_frame_; \
        _db_enter_ (a,__FILE__,__LINE__,&_db_stack_frame_)
#define DBUG_LEAVE _db_return_ (__LINE__, &_db_stack_frame_)
#define DBUG_RETURN(a1) do {DBUG_LEAVE; return(a1);} while(0)
#define DBUG_VOID_RETURN do {DBUG_LEAVE; return;} while(0)
#define DBUG_EXECUTE(keyword,a1) \
        do {if (_db_keyword_(0, (keyword), 0)) { a1 }} while(0)
#define DBUG_EXECUTE_IF(keyword,a1) \
        do {if (_db_keyword_(0, (keyword), 1)) { a1 }} while(0)
#define DBUG_EVALUATE(keyword,a1,a2) \
        (_db_keyword_(0,(keyword), 0) ? (a1) : (a2))
#define DBUG_EVALUATE_IF(keyword,a1,a2) \
        (_db_keyword_(0,(keyword), 1) ? (a1) : (a2))
#define DBUG_PRINT(keyword,arglist) \
        do {_db_pargs_(__LINE__,keyword); _db_doprnt_ arglist;} while(0)
#define DBUG_PUSH(a1) _db_push_ (a1)
#define DBUG_POP() _db_pop_ ()
#define DBUG_SET(a1) _db_set_ (a1)
#define DBUG_SET_INITIAL(a1) _db_set_init_ (a1)
#define DBUG_PROCESS(a1) _db_process_(a1)
#define DBUG_FILE _db_fp_()
#define DBUG_SETJMP(a1) (_db_setjmp_ (), setjmp (a1))
#define DBUG_LONGJMP(a1,a2) (_db_longjmp_ (), longjmp (a1, a2))
#define DBUG_DUMP(keyword,a1,a2) _db_dump_(__LINE__,keyword,a1,a2)
#define DBUG_END()  _db_end_ ()
#define DBUG_LOCK_FILE _db_lock_file_()
#define DBUG_UNLOCK_FILE _db_unlock_file_()
#define DBUG_ASSERT(A) assert(A)
#define DBUG_EXPLAIN(buf,len) _db_explain_(0, (buf),(len))
#define DBUG_EXPLAIN_INITIAL(buf,len) _db_explain_init_((buf),(len))
#define DEBUGGER_OFF                    do { _dbug_on_= 0; } while(0)
#define DEBUGGER_ON                     do { _dbug_on_= 1; } while(0)
#ifndef __WIN__
#define DBUG_ABORT()                    (_db_flush_(), abort())
#else
/*
  Avoid popup with abort/retry/ignore buttons. When BUG#31745 is fixed we can
  call abort() instead of _exit(3) (now it would cause a "test signal" popup).
*/
#include <crtdbg.h>
#define DBUG_ABORT() (_db_flush_(),\
                     (void)_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE),\
                     (void)_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR),\
                     _exit(3))
#endif
#define DBUG_CHECK_CRASH(func, op) \
  do { char _dbuf_[255]; strxnmov(_dbuf_, sizeof(_dbuf_)-1, (func), (op)); \
    DBUG_EXECUTE_IF(_dbuf_, DBUG_ABORT()); } while(0)
#define DBUG_CRASH_ENTER(func) \
  DBUG_ENTER(func); DBUG_CHECK_CRASH(func, "_crash_enter")
#define DBUG_CRASH_RETURN(val) \
  DBUG_CHECK_CRASH(_db_get_func_(), "_crash_return")
#define DBUG_CRASH_VOID_RETURN \
  DBUG_CHECK_CRASH (_db_get_func_(), "_crash_return")

#else                                           /* No debugger */

#define DBUG_ENTER(a1)
#define DBUG_LEAVE
#define DBUG_RETURN(a1)                 do { return(a1); } while(0)
#define DBUG_VOID_RETURN                do { return; } while(0)
#define DBUG_EXECUTE(keyword,a1)        do { } while(0)
#define DBUG_EXECUTE_IF(keyword,a1)     do { } while(0)
#define DBUG_EVALUATE(keyword,a1,a2) (a2)
#define DBUG_EVALUATE_IF(keyword,a1,a2) (a2)
#define DBUG_PRINT(keyword,arglist)     do { } while(0)
#define DBUG_PUSH(a1)                   do { } while(0)
#define DBUG_SET(a1)                    do { } while(0)
#define DBUG_SET_INITIAL(a1)            do { } while(0)
#define DBUG_POP()                      do { } while(0)
#define DBUG_PROCESS(a1)                do { } while(0)
#define DBUG_SETJMP(a1) setjmp(a1)
#define DBUG_LONGJMP(a1) longjmp(a1)
#define DBUG_DUMP(keyword,a1,a2)        do { } while(0)
#define DBUG_END()                      do { } while(0)
#define DBUG_ASSERT(A)                  do { } while(0)
#define DBUG_LOCK_FILE                  do { } while(0)
#define DBUG_FILE (stderr)
#define DBUG_UNLOCK_FILE                do { } while(0)
#define DBUG_EXPLAIN(buf,len)
#define DBUG_EXPLAIN_INITIAL(buf,len)
#define DEBUGGER_OFF                    do { } while(0)
#define DEBUGGER_ON                     do { } while(0)
#define DBUG_ABORT()                    abort()
#define DBUG_CRASH_ENTER(func)
#define DBUG_CRASH_RETURN(val)          do { return(val); } while(0)
#define DBUG_CRASH_VOID_RETURN          do { return; } while(0)

#endif
#ifdef  __cplusplus
}
#endif
#endif
