/*
 * ANSI/POSIX
 */
/* Generate an overflow to create +Inf; the multiply shuts up gcc 1 */ 
#define HUGE_VAL        (1e250*1e250)           /* IEEE: positive infinity */

