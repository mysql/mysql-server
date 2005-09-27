/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"
#include "my_static.h"
#include "mysys_err.h"
#include <m_string.h>
#include <m_ctype.h>
#include <signal.h>
#ifdef VMS
#include <my_static.c>
#include <m_ctype.h>
#endif
#ifdef __WIN__
#ifdef _MSC_VER
#include <locale.h>
#include <crtdbg.h>
#endif
my_bool have_tcpip=0;
static void my_win_init(void);
static my_bool win32_have_tcpip(void);
static my_bool win32_init_tcp_ip();
#else
#define my_win_init()
#endif
#ifdef __NETWARE__
static void netware_init();
#else
#define netware_init()
#endif

my_bool my_init_done= 0;
uint	mysys_usage_id= 0;              /* Incremented for each my_init() */

static ulong atoi_octal(const char *str)
{
  long int tmp;
  while (*str && my_isspace(&my_charset_latin1, *str))
    str++;
  str2int(str,
	  (*str == '0' ? 8 : 10),       /* Octalt or decimalt */
	  0, INT_MAX, &tmp);
  return (ulong) tmp;
}


/*
  Init my_sys functions and my_sys variabels

  SYNOPSIS
    my_init()

  RETURN
    0  ok
    1  Couldn't initialize environment
*/

my_bool my_init(void)
{
  my_string str;
  if (my_init_done)
    return 0;
  my_init_done=1;
  mysys_usage_id++;
  my_umask= 0660;                       /* Default umask for new files */
  my_umask_dir= 0700;                   /* Default umask for new directories */
#if defined(THREAD) && defined(SAFE_MUTEX)
  safe_mutex_global_init();		/* Must be called early */
#endif
  netware_init();
#ifdef THREAD
#if defined(HAVE_PTHREAD_INIT)
  pthread_init();			/* Must be called before DBUG_ENTER */
#endif
  if (my_thread_global_init())
    return 1;
#if !defined( __WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigfillset(&my_signals);		/* signals blocked by mf_brkhant */
#endif
#endif /* THREAD */
#ifdef UNIXWARE_7
  (void) isatty(0);			/* Go around connect() bug in UW7 */
#endif
  {
    DBUG_ENTER("my_init");
    DBUG_PROCESS(my_progname ? my_progname : (char*) "unknown");
    if (!home_dir)
    {					/* Don't initialize twice */
      my_win_init();
      if ((home_dir=getenv("HOME")) != 0)
	home_dir=intern_filename(home_dir_buff,home_dir);
#ifndef VMS
      /* Default creation of new files */
      if ((str=getenv("UMASK")) != 0)
	my_umask=(int) (atoi_octal(str) | 0600);
	/* Default creation of new dir's */
      if ((str=getenv("UMASK_DIR")) != 0)
	my_umask_dir=(int) (atoi_octal(str) | 0700);
#endif
#ifdef VMS
      init_ctype();			/* Stupid linker don't link _ctype.c */
#endif
      DBUG_PRINT("exit",("home: '%s'",home_dir));
    }
#ifdef __WIN__
    win32_init_tcp_ip();
#endif
    DBUG_RETURN(0);
  }
} /* my_init */


	/* End my_sys */

void my_end(int infoflag)
{
  /*
    this code is suboptimal to workaround a bug in
    Sun CC: Sun C++ 5.6 2004/06/02 for x86, and should not be
    optimized until this compiler is not in use anymore
  */
  FILE *info_file= DBUG_FILE;
  my_bool print_info= (info_file != stderr);
  DBUG_ENTER("my_end");
  if (!info_file)
  {
    info_file= stderr;
    print_info= 0;
  }

  DBUG_PRINT("info",("Shutting down: print_info: %d", print_info));
  if ((infoflag & MY_CHECK_ERROR) || print_info)

  {					/* Test if some file is left open */
    if (my_file_opened | my_stream_opened)
    {
      sprintf(errbuff[0],EE(EE_OPEN_WARNING),my_file_opened,my_stream_opened);
      (void) my_message_no_curses(EE_OPEN_WARNING,errbuff[0],ME_BELL);
      DBUG_PRINT("error",("%s",errbuff[0]));
    }
  }
  my_once_free();

  if ((infoflag & MY_GIVE_INFO) || print_info)
  {
#ifdef HAVE_GETRUSAGE
    struct rusage rus;
#ifdef HAVE_purify
    /* Purify assumes that rus is uninitialized after getrusage call */
    bzero((char*) &rus, sizeof(rus));
#endif
    if (!getrusage(RUSAGE_SELF, &rus))
      fprintf(info_file,"\n\
User time %.2f, System time %.2f\n\
Maximum resident set size %ld, Integral resident set size %ld\n\
Non-physical pagefaults %ld, Physical pagefaults %ld, Swaps %ld\n\
Blocks in %ld out %ld, Messages in %ld out %ld, Signals %ld\n\
Voluntary context switches %ld, Involuntary context switches %ld\n",
	      (rus.ru_utime.tv_sec * SCALE_SEC +
	       rus.ru_utime.tv_usec / SCALE_USEC) / 100.0,
	      (rus.ru_stime.tv_sec * SCALE_SEC +
	       rus.ru_stime.tv_usec / SCALE_USEC) / 100.0,
	      rus.ru_maxrss, rus.ru_idrss,
	      rus.ru_minflt, rus.ru_majflt,
	      rus.ru_nswap, rus.ru_inblock, rus.ru_oublock,
	      rus.ru_msgsnd, rus.ru_msgrcv, rus.ru_nsignals,
	      rus.ru_nvcsw, rus.ru_nivcsw);
#endif
#if ( defined(MSDOS) || defined(__NETWARE__) ) && !defined(__WIN__)
    fprintf(info_file,"\nRun time: %.1f\n",(double) clock()/CLOCKS_PER_SEC);
#endif
#if defined(SAFEMALLOC)
    TERMINATE(stderr);		/* Give statistic on screen */
#elif defined(__WIN__) && defined(_MSC_VER)
   _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
   _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
   _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
   _CrtCheckMemory();
   _CrtDumpMemoryLeaks();
#endif
  }
#ifdef THREAD
  DBUG_POP();				/* Must be done before my_thread_end */
  my_thread_end();
  my_thread_global_end();
#if defined(SAFE_MUTEX)
  /*
    Check on destroying of mutexes. A few may be left that will get cleaned
    up by C++ destructors
  */
  safe_mutex_end(infoflag & MY_GIVE_INFO ? stderr : (FILE *) 0);
#endif /* defined(SAFE_MUTEX) */
#endif /* THREAD */

#ifdef __WIN__
  if (have_tcpip)
    WSACleanup();
#endif /* __WIN__ */
  my_init_done=0;
} /* my_end */


#ifdef __WIN__

/*
  This code is specially for running MySQL, but it should work in
  other cases too.

  Inizializzazione delle variabili d'ambiente per Win a 32 bit.

  Vengono inserite nelle variabili d'ambiente (utilizzando cosi'
  le funzioni getenv e putenv) i valori presenti nelle chiavi
  del file di registro:

  HKEY_LOCAL_MACHINE\software\MySQL

  Se la kiave non esiste nonn inserisce nessun valore
*/

/* Crea la stringa d'ambiente */

void setEnvString(char *ret, const char *name, const char *value)
{
  DBUG_ENTER("setEnvString");
  strxmov(ret, name,"=",value,NullS);
  DBUG_VOID_RETURN ;
}

static void my_win_init(void)
{
  HKEY	hSoftMysql ;
  DWORD dimName = 256 ;
  DWORD dimData = 1024 ;
  DWORD dimNameValueBuffer = 256 ;
  DWORD dimDataValueBuffer = 1024 ;
  DWORD indexValue = 0 ;
  long	retCodeEnumValue ;
  char	NameValueBuffer[256] ;
  char	DataValueBuffer[1024] ;
  char	EnvString[1271] ;
  const char *targetKey = "Software\\MySQL" ;
  DBUG_ENTER("my_win_init");

  setlocale(LC_CTYPE, "");             /* To get right sortorder */

#if defined(_MSC_VER) && (_MSC_VER < 1300)
  /* 
    Clear the OS system variable TZ and avoid the 100% CPU usage
    Only for old versions of Visual C++
  */
  _putenv( "TZ=" ); 
#endif  
  _tzset();

  /* apre la chiave HKEY_LOCAL_MACHINES\software\MySQL */
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,(LPCTSTR)targetKey,0,
		   KEY_READ,&hSoftMysql) != ERROR_SUCCESS)
    DBUG_VOID_RETURN;

  /*
  ** Ne legge i valori e li inserisce  nell'ambiente
  ** suppone che tutti i valori letti siano di tipo stringa + '\0'
  ** Legge il valore con indice 0 e lo scarta
  */
  retCodeEnumValue = RegEnumValue(hSoftMysql, indexValue++,
				  (LPTSTR)NameValueBuffer, &dimNameValueBuffer,
				  NULL, NULL, (LPBYTE)DataValueBuffer,
				  &dimDataValueBuffer) ;

  while (retCodeEnumValue != ERROR_NO_MORE_ITEMS)
  {
    char *my_env;
    /* Crea la stringa d'ambiente */
    setEnvString(EnvString, NameValueBuffer, DataValueBuffer) ;

    /* Inserisce i dati come variabili d'ambiente */
    my_env=strdup(EnvString);  /* variable for putenv must be allocated ! */
    putenv(my_env) ;

    dimNameValueBuffer = dimName ;
    dimDataValueBuffer = dimData ;

    retCodeEnumValue = RegEnumValue(hSoftMysql, indexValue++,
				    NameValueBuffer, &dimNameValueBuffer,
				    NULL, NULL, (LPBYTE)DataValueBuffer,
				    &dimDataValueBuffer) ;
  }

  /* chiude la chiave */
  RegCloseKey(hSoftMysql) ;
  DBUG_VOID_RETURN ;
}


/*------------------------------------------------------------------
  Name: CheckForTcpip| Desc: checks if tcpip has been installed on system
  According to Microsoft Developers documentation the first registry
  entry should be enough to check if TCP/IP is installed, but as expected
  this doesn't work on all Win32 machines :(
------------------------------------------------------------------*/

#define TCPIPKEY  "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define WINSOCK2KEY "SYSTEM\\CurrentControlSet\\Services\\Winsock2\\Parameters"
#define WINSOCKKEY  "SYSTEM\\CurrentControlSet\\Services\\Winsock\\Parameters"

static my_bool win32_have_tcpip(void)
{
  HKEY hTcpipRegKey;
  if (RegOpenKeyEx ( HKEY_LOCAL_MACHINE, TCPIPKEY, 0, KEY_READ,
		      &hTcpipRegKey) != ERROR_SUCCESS)
  {
    if (RegOpenKeyEx ( HKEY_LOCAL_MACHINE, WINSOCK2KEY, 0, KEY_READ,
		      &hTcpipRegKey) != ERROR_SUCCESS)
    {
      if (RegOpenKeyEx ( HKEY_LOCAL_MACHINE, WINSOCKKEY, 0, KEY_READ,
			 &hTcpipRegKey) != ERROR_SUCCESS)
	if (!getenv("HAVE_TCPIP") || have_tcpip)	/* Provide a workaround */
	  return (FALSE);
    }
  }
  RegCloseKey ( hTcpipRegKey);
  return (TRUE);
}


static my_bool win32_init_tcp_ip()
{
  if (win32_have_tcpip())
  {
    WORD wVersionRequested = MAKEWORD( 2, 0 );
    WSADATA wsaData;
 	/* Be a good citizen: maybe another lib has already initialised
 		sockets, so dont clobber them unless necessary */
    if (WSAStartup( wVersionRequested, &wsaData ))
    {
      /* Load failed, maybe because of previously loaded
	 incompatible version; try again */
      WSACleanup( );
      if (!WSAStartup( wVersionRequested, &wsaData ))
	have_tcpip=1;
    }
    else
    {
      if (wsaData.wVersion != wVersionRequested)
      {
	/* Version is no good, try again */
	WSACleanup( );
	if (!WSAStartup( wVersionRequested, &wsaData ))
	  have_tcpip=1;
      }
      else
	have_tcpip=1;
    }
  }
  return(0);
}
#endif /* __WIN__ */


#ifdef __NETWARE__
/*
  Basic initialisation for netware
*/

static void netware_init()
{
  char cwd[PATH_MAX], *name;

  DBUG_ENTER("netware_init");

  /* init only if we are not a client library */
  if (my_progname)
  {
#if SUPPORTED_BY_LIBC   /* Removed until supported in Libc */
    struct termios tp;
    /* Disable control characters */
    tcgetattr(STDIN_FILENO, &tp);
    tp.c_cc[VINTR] = _POSIX_VDISABLE;
    tp.c_cc[VEOF] = _POSIX_VDISABLE;
    tp.c_cc[VSUSP] = _POSIX_VDISABLE;
    tcsetattr(STDIN_FILENO, TCSANOW, &tp);
#endif /* SUPPORTED_BY_LIBC */

    /* With stdout redirection */
    if (!isatty(STDOUT_FILENO))
    {
      setscreenmode(SCR_AUTOCLOSE_ON_EXIT);      /* auto close the screen */
    }
    else
    {
      setscreenmode(SCR_NO_MODE);		/* keep the screen up */
    }

    /* Parse program name and change to base format */
    name= my_progname;
    for (; *name; name++)
    {
      if (*name == '\\')
      {
        *name = '/';
      }
      else
      {
        *name = tolower(*name);
      }
    }
  }

  DBUG_VOID_RETURN;
}
#endif /* __NETWARE__ */
