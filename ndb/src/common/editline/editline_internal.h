/*  $Revision: 1.2 $
**
**  Internal header file for editline library.
*/

#include <ndb_global.h>

#if	defined(SYS_UNIX)
#include "unix.h"
#endif	/* defined(SYS_UNIX) */

#define MEM_INC		64
#define SCREEN_INC	256

/*
**  Variables and routines internal to this package.
*/
extern int	rl_eof;
extern int	rl_erase;
extern int	rl_intr;
extern int	rl_kill;
extern int	rl_quit;
#if	defined(DO_SIGTSTP)
extern int	rl_susp;
#endif	/* defined(DO_SIGTSTP) */
extern char	*rl_complete();
extern int	rl_list_possib();
extern void	rl_ttyset();
extern void	rl_add_slash();

