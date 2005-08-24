/* Divert all help information on NetWare to logger screen. */

#ifdef __NETWARE__
#define printf    consoleprintf
#define puts(s)   consoleprintf("%s\n",s)
#define fputs(s,f)   puts(s)
#define fputc(s,f)   consoleprintf("%c", s)
#define putchar(s)   consoleprintf("%c", s)
#endif
