/* ==== syscall.S ============================================================
 * Written 1996 by Stefan Grefen, grefen@convex.com
 */

#include <sys/syscall.h>

#define _CAT(a,b)a##b
#define CAT(a,b)_CAT(a,b)

#define MKNAME(a)CAT(a,SYSCALL_NAME)

      .CODE
machdep_error                                                 
      sub %r0,%r28,%r28                                       
      bv,n %r0(%r2)                                           

      .label MKNAME(machdep_sys_)
      .PROC           
      .CALLINFO       NO_CALLS,FRAME=0
        ldil -0x80000,%r1             
        ble 4(%sr7,%r1)               
        ldi MKNAME(SYS_),%r22 
        or,= %r0,%r22,%r0
        b,n machdep_error
        bv,n %r0(%r2) 
      .PROCEND        
      .EXPORT         MKNAME(machdep_sys_)
