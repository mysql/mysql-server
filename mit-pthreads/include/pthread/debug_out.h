/* debug_out.h - macros to use for debugging prints in places where calls
                 to printf() and gang are ill-advised. */

#ifdef PTHREAD_DEBUGGING
#define PTHREAD_DEBUG_WriteStr(S) (void)machdep_sys_write(2,S,strlen(S))
#define PTHREAD_DEBUG_WriteInt32Hex(X)				\
  { char _xbuf[8]; int _temp = (int)(X), _temp2;		\
    _temp2 = ((_temp>>28)&0xf);					\
    _xbuf[0] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = ((_temp>>24)&0xf);					\
    _xbuf[1] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = ((_temp>>20)&0xf);					\
    _xbuf[2] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = ((_temp>>16)&0xf);					\
    _xbuf[3] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = ((_temp>>12)&0xf);					\
    _xbuf[4] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = ((_temp>>8)&0xf);					\
    _xbuf[5] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = ((_temp>>4)&0xf);					\
    _xbuf[6] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    _temp2 = (_temp&0xf);					\
    _xbuf[7] = (_temp2<10)? (_temp2+'0'): ((_temp2-10)+'a');	\
    (void)machdep_sys_write(2,_xbuf,8);				\
  }
#ifdef __alpha
#define PTHREAD_DEBUG_WriteInt64Hex(X)				\
  { long _tempX = (long)(X),_tempY;				\
    _tempY=((_tempX>>32)&0xffffffff);				\
    PTHREAD_DEBUG_WriteInt32Hex(_tempY);			\
    _tempY=(_tempX&0xffffffff);					\
    PTHREAD_DEBUG_WriteInt32Hex(_tempY);			\
  }
#define PTHREAD_DEBUG_WritePointer(X) PTHREAD_DEBUG_WriteInt64Hex(X)
#else
#define PTHREAD_DEBUG_WriteInt64Hex(X) PTHREAD_DEBUG_WriteInt32Hex(X)
#define PTHREAD_DEBUG_WritePointer(X) PTHREAD_DEBUG_WriteInt32Hex(X)
#endif /* __alpha */
#else /* ! PTHREAD_DEBUGGING */
#define PTHREAD_DEBUG_WriteStr(S)
#define PTHREAD_DEBUG_WriteInt32Hex(X)
#define PTHREAD_DEBUG_WriteInt64HeX(X)
#define PTHREAD_DEBUG_WritePointer(X)
#endif /* PTHREAD_DEBUGGING */
