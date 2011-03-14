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

#include <my_global.h>
#include <my_stacktrace.h>

#ifndef __WIN__
#include <signal.h>
#include <my_pthread.h>
#include <m_string.h>
#ifdef HAVE_STACKTRACE
#include <unistd.h>
#include <strings.h>

#ifdef __linux__
#include <ctype.h>          /* isprint */
#include <sys/syscall.h>    /* SYS_gettid */
#endif

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#define PTR_SANE(p) ((p) && (char*)(p) >= heap_start && (char*)(p) <= heap_end)

static char *heap_start;

#ifdef HAVE_BSS_START
extern char *__bss_start;
#endif

void my_init_stacktrace()
{
#ifdef HAVE_BSS_START
  heap_start = (char*) &__bss_start;
#endif
}

#ifdef __linux__

static void print_buffer(char *buffer, size_t count)
{
  for (; count && *buffer; --count)
  {
    int c= (int) *buffer++;
    fputc(isprint(c) ? c : ' ', stderr);
  }
}

/**
  Access the pages of this process through /proc/self/task/<tid>/mem
  in order to safely print the contents of a memory address range.

  @param  addr      The address at the start of the memory region.
  @param  max_len   The length of the memory region.

  @return Zero on success.
*/
static int safe_print_str(const char *addr, int max_len)
{
  int fd;
  pid_t tid;
  off_t offset;
  ssize_t nbytes= 0;
  size_t total, count;
  char buf[256];

  tid= (pid_t) syscall(SYS_gettid);

  sprintf(buf, "/proc/self/task/%d/mem", tid);

  if ((fd= open(buf, O_RDONLY)) < 0)
    return -1;

  /* Ensure that off_t can hold a pointer. */
  compile_time_assert(sizeof(off_t) >= sizeof(intptr));

  total= max_len;
  offset= (intptr) addr;

  /* Read up to the maximum number of bytes. */
  while (total)
  {
    count= min(sizeof(buf), total);

    if ((nbytes= pread(fd, buf, count, offset)) < 0)
    {
      /* Just in case... */
      if (errno == EINTR)
        continue;
      else
        break;
    }

    /* Advance offset into memory. */
    total-= nbytes;
    offset+= nbytes;
    addr+= nbytes;

    /* Output the printable characters. */
    print_buffer(buf, nbytes);

    /* Break if less than requested... */
    if ((count - nbytes))
      break;
  }

  /* Output a new line if something was printed. */
  if (total != (size_t) max_len)
    fputc('\n', stderr);

  if (nbytes == -1)
    fprintf(stderr, "Can't read from address %p: %m.\n", addr);

  close(fd);

  return 0;
}

#endif

void my_safe_print_str(const char* val, int max_len)
{
  char *heap_end;

#ifdef __linux__
  if (!safe_print_str(val, max_len))
    return;
#endif

  heap_end= (char*) sbrk(0);

  if (!PTR_SANE(val))
  {
    fprintf(stderr, "is an invalid pointer\n");
    return;
  }

  for (; max_len && PTR_SANE(val) && *val; --max_len)
    fputc(*val++, stderr);
  fputc('\n', stderr);
}

#if defined(HAVE_PRINTSTACK)

/* Use Solaris' symbolic stack trace routine. */
#include <ucontext.h>

void my_print_stacktrace(uchar* stack_bottom __attribute__((unused)), 
                         ulong thread_stack __attribute__((unused)))
{
  if (printstack(fileno(stderr)) == -1)
    fprintf(stderr, "Error when traversing the stack, stack appears corrupt.\n");
  else
    fprintf(stderr,
            "Please read "
            "http://dev.mysql.com/doc/refman/5.1/en/resolve-stack-dump.html\n"
            "and follow instructions on how to resolve the stack trace.\n"
            "Resolved stack trace is much more helpful in diagnosing the\n"
            "problem, so please do resolve it\n");
}

#elif HAVE_BACKTRACE && (HAVE_BACKTRACE_SYMBOLS || HAVE_BACKTRACE_SYMBOLS_FD)

#if BACKTRACE_DEMANGLE

char __attribute__ ((weak)) *
my_demangle(const char *mangled_name __attribute__((unused)),
            int *status __attribute__((unused)))
{
  return NULL;
}

static void my_demangle_symbols(char **addrs, int n)
{
  int status, i;
  char *begin, *end, *demangled;

  for (i= 0; i < n; i++)
  {
    demangled= NULL;
    begin= strchr(addrs[i], '(');
    end= begin ? strchr(begin, '+') : NULL;

    if (begin && end)
    {
      *begin++= *end++= '\0';
      demangled= my_demangle(begin, &status);
      if (!demangled || status)
      {
        demangled= NULL;
        begin[-1]= '(';
        end[-1]= '+';
      }
    }

    if (demangled)
      fprintf(stderr, "%s(%s+%s\n", addrs[i], demangled, end);
    else
      fprintf(stderr, "%s\n", addrs[i]);
  }
}

#endif /* BACKTRACE_DEMANGLE */

void my_print_stacktrace(uchar* stack_bottom, ulong thread_stack)
{
  void *addrs[128];
  char **strings= NULL;
  int n = backtrace(addrs, array_elements(addrs));
  fprintf(stderr, "stack_bottom = %p thread_stack 0x%lx\n",
          stack_bottom, thread_stack);
#if BACKTRACE_DEMANGLE
  if ((strings= backtrace_symbols(addrs, n)))
  {
    my_demangle_symbols(strings, n);
    free(strings);
  }
#endif
#if HAVE_BACKTRACE_SYMBOLS_FD
  if (!strings)
  {
    backtrace_symbols_fd(addrs, n, fileno(stderr));
  }
#endif
}

#elif defined(TARGET_OS_LINUX)

#ifdef __i386__
#define SIGRETURN_FRAME_OFFSET 17
#endif

#ifdef __x86_64__
#define SIGRETURN_FRAME_OFFSET 23
#endif

#if defined(__alpha__) && defined(__GNUC__)
/*
  The only way to backtrace without a symbol table on alpha
  is to find stq fp,N(sp), and the first byte
  of the instruction opcode will give us the value of N. From this
  we can find where the old value of fp is stored
*/

#define MAX_INSTR_IN_FUNC  10000

inline uchar** find_prev_fp(uint32* pc, uchar** fp)
{
  int i;
  for (i = 0; i < MAX_INSTR_IN_FUNC; ++i,--pc)
  {
    uchar* p = (uchar*)pc;
    if (p[2] == 222 &&  p[3] == 35)
    {
      return (uchar**)((uchar*)fp - *(short int*)p);
    }
  }
  return 0;
}

inline uint32* find_prev_pc(uint32* pc, uchar** fp)
{
  int i;
  for (i = 0; i < MAX_INSTR_IN_FUNC; ++i,--pc)
  {
    char* p = (char*)pc;
    if (p[1] == 0 && p[2] == 94 &&  p[3] == -73)
    {
      uint32* prev_pc = (uint32*)*((fp+p[0]/sizeof(fp)));
      return prev_pc;
    }
  }
  return 0;
}
#endif /* defined(__alpha__) && defined(__GNUC__) */

void my_print_stacktrace(uchar* stack_bottom, ulong thread_stack)
{
  uchar** fp;
  uint frame_count = 0, sigreturn_frame_count;
#if defined(__alpha__) && defined(__GNUC__)
  uint32* pc;
#endif
  LINT_INIT(fp);


#ifdef __i386__
  __asm __volatile__ ("movl %%ebp,%0"
		      :"=r"(fp)
		      :"r"(fp));
#endif
#ifdef __x86_64__
  __asm __volatile__ ("movq %%rbp,%0"
		      :"=r"(fp)
		      :"r"(fp));
#endif
#if defined(__alpha__) && defined(__GNUC__) 
  __asm __volatile__ ("mov $30,%0"
		      :"=r"(fp)
		      :"r"(fp));
#endif
  if (!fp)
  {
    fprintf(stderr, "frame pointer is NULL, did you compile with\n\
-fomit-frame-pointer? Aborting backtrace!\n");
    return;
  }

  if (!stack_bottom || (uchar*) stack_bottom > (uchar*) &fp)
  {
    ulong tmp= min(0x10000,thread_stack);
    /* Assume that the stack starts at the previous even 65K */
    stack_bottom= (uchar*) (((ulong) &fp + tmp) &
			  ~(ulong) 0xFFFF);
    fprintf(stderr, "Cannot determine thread, fp=%p, backtrace may not be correct.\n", fp);
  }
  if (fp > (uchar**) stack_bottom ||
      fp < (uchar**) stack_bottom - thread_stack)
  {
    fprintf(stderr, "Bogus stack limit or frame pointer,\
 fp=%p, stack_bottom=%p, thread_stack=%ld, aborting backtrace.\n",
	    fp, stack_bottom, thread_stack);
    return;
  }

  fprintf(stderr, "Stack range sanity check OK, backtrace follows:\n");
#if defined(__alpha__) && defined(__GNUC__)
  fprintf(stderr, "Warning: Alpha stacks are difficult -\
 will be taking some wild guesses, stack trace may be incorrect or \
 terminate abruptly\n");
  /* On Alpha, we need to get pc */
  __asm __volatile__ ("bsr %0, do_next; do_next: "
		      :"=r"(pc)
		      :"r"(pc));
#endif  /* __alpha__ */

  /* We are 1 frame above signal frame with NPTL and 2 frames above with LT */
  sigreturn_frame_count = thd_lib_detected == THD_LIB_LT ? 2 : 1;

  while (fp < (uchar**) stack_bottom)
  {
#if defined(__i386__) || defined(__x86_64__)
    uchar** new_fp = (uchar**)*fp;
    fprintf(stderr, "%p\n", frame_count == sigreturn_frame_count ?
	    *(fp + SIGRETURN_FRAME_OFFSET) : *(fp + 1));
#endif /* defined(__386__)  || defined(__x86_64__) */

#if defined(__alpha__) && defined(__GNUC__)
    uchar** new_fp = find_prev_fp(pc, fp);
    if (frame_count == sigreturn_frame_count - 1)
    {
      new_fp += 90;
    }

    if (fp && pc)
    {
      pc = find_prev_pc(pc, fp);
      if (pc)
	fprintf(stderr, "%p\n", pc);
      else
      {
	fprintf(stderr, "Not smart enough to deal with the rest\
 of this stack\n");
	goto end;
      }
    }
    else
    {
      fprintf(stderr, "Not smart enough to deal with the rest of this stack\n");
      goto end;
    }
#endif /* defined(__alpha__) && defined(__GNUC__) */
    if (new_fp <= fp )
    {
      fprintf(stderr, "New value of fp=%p failed sanity check,\
 terminating stack trace!\n", new_fp);
      goto end;
    }
    fp = new_fp;
    ++frame_count;
  }

  fprintf(stderr, "Stack trace seems successful - bottom reached\n");

end:
  fprintf(stderr,
          "Please read http://dev.mysql.com/doc/refman/5.1/en/resolve-stack-dump.html\n"
          "and follow instructions on how to resolve the stack trace.\n"
          "Resolved stack trace is much more helpful in diagnosing the\n"
          "problem, so please do resolve it\n");
}
#endif /* TARGET_OS_LINUX */
#endif /* HAVE_STACKTRACE */

/* Produce a core for the thread */
void my_write_core(int sig)
{
#ifdef HAVE_gcov
  extern void __gcov_flush(void);
#endif
  signal(sig, SIG_DFL);
#ifdef HAVE_gcov
  /*
    For GCOV build, crashing will prevent the writing of code coverage
    information from this process, causing gcov output to be incomplete.
    So we force the writing of coverage information here before terminating.
  */
  __gcov_flush();
#endif
  pthread_kill(pthread_self(), sig);
#if defined(P_MYID) && !defined(SCO)
  /* On Solaris, the above kill is not enough */
  sigsend(P_PID,P_MYID,sig);
#endif
}

#else /* __WIN__*/

#include <dbghelp.h>
#include <tlhelp32.h>
#if _MSC_VER
#pragma comment(lib, "dbghelp")
#endif

static EXCEPTION_POINTERS *exception_ptrs;

#define MODULE64_SIZE_WINXP 576
#define STACKWALK_MAX_FRAMES 64

void my_init_stacktrace()
{
}


void my_set_exception_pointers(EXCEPTION_POINTERS *ep)
{
  exception_ptrs = ep;
}

/*
  Appends directory to symbol path.
*/
static void add_to_symbol_path(char *path, size_t path_buffer_size, 
  char *dir, size_t dir_buffer_size)
{
  strcat_s(dir, dir_buffer_size, ";");
  if (!strstr(path, dir))
  {
    strcat_s(path, path_buffer_size, dir);
  }
}

/*
  Get symbol path - semicolon-separated list of directories to search for debug
  symbols. We expect PDB in the same directory as corresponding exe or dll,
  so the path is build from directories of the loaded modules. If environment
  variable _NT_SYMBOL_PATH is set, it's value appended to the symbol search path
*/
static void get_symbol_path(char *path, size_t size)
{ 
  HANDLE hSnap; 
  char *envvar;
  char *p;
#ifndef DBUG_OFF
  static char pdb_debug_dir[MAX_PATH + 7];
#endif

  path[0]= '\0';

#ifndef DBUG_OFF
  /* 
    Add "debug" subdirectory of the application directory, sometimes PDB will 
    placed here by installation.
  */
  GetModuleFileName(NULL, pdb_debug_dir, MAX_PATH);
  p= strrchr(pdb_debug_dir, '\\');
  if(p)
  { 
    *p= 0;
    strcat_s(pdb_debug_dir, sizeof(pdb_debug_dir), "\\debug;");
    add_to_symbol_path(path, size, pdb_debug_dir, sizeof(pdb_debug_dir));
  }
#endif

  /*
    Enumerate all modules, and add their directories to the path.
    Avoid duplicate entries.
  */
  hSnap= CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  if (hSnap != INVALID_HANDLE_VALUE)
  {
    BOOL ret;
    MODULEENTRY32 mod;
    mod.dwSize= sizeof(MODULEENTRY32);
    for (ret= Module32First(hSnap, &mod); ret; ret= Module32Next(hSnap, &mod))
    {
      char *module_dir= mod.szExePath;
      p= strrchr(module_dir,'\\');
      if (!p)
      {
        /*
          Path separator was not found. Not known to happen, if ever happens,
          will indicate current directory.
        */
        module_dir[0]= '.';
        module_dir[1]= '\0';
      }
      else
      {
        *p= '\0';
      }
      add_to_symbol_path(path, size, module_dir,sizeof(mod.szExePath));
    }
    CloseHandle(hSnap);
  }

  
  /* Add _NT_SYMBOL_PATH, if present. */
  envvar= getenv("_NT_SYMBOL_PATH");
  if(envvar)
  {
    strcat_s(path, size, envvar);
  }
}

#define MAX_SYMBOL_PATH 32768

/* Platform SDK in VS2003 does not have definition for SYMOPT_NO_PROMPTS*/
#ifndef SYMOPT_NO_PROMPTS
#define SYMOPT_NO_PROMPTS 0
#endif

void my_print_stacktrace(uchar* unused1, ulong unused2)
{
  HANDLE  hProcess= GetCurrentProcess();
  HANDLE  hThread= GetCurrentThread();
  static  IMAGEHLP_MODULE64 module= {sizeof(module)};
  static  IMAGEHLP_SYMBOL64_PACKAGE package;
  DWORD64 addr;
  DWORD   machine;
  int     i;
  CONTEXT context;
  STACKFRAME64 frame={0};
  static char symbol_path[MAX_SYMBOL_PATH];

  if(!exception_ptrs)
    return;

  /* Copy context, as stackwalking on original will unwind the stack */
  context = *(exception_ptrs->ContextRecord);
  /*Initialize symbols.*/
  SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_NO_PROMPTS|SYMOPT_DEFERRED_LOADS|SYMOPT_DEBUG);
  get_symbol_path(symbol_path, sizeof(symbol_path));
  SymInitialize(hProcess, symbol_path, TRUE);

  /*Prepare stackframe for the first StackWalk64 call*/
  frame.AddrFrame.Mode= frame.AddrPC.Mode= frame.AddrStack.Mode= AddrModeFlat;
#if (defined _M_IX86)
  machine= IMAGE_FILE_MACHINE_I386;
  frame.AddrFrame.Offset= context.Ebp;
  frame.AddrPC.Offset=    context.Eip;
  frame.AddrStack.Offset= context.Esp;
#elif (defined _M_X64)
  machine = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrFrame.Offset= context.Rbp;
  frame.AddrPC.Offset=    context.Rip;
  frame.AddrStack.Offset= context.Rsp;
#else
  /*There is currently no need to support IA64*/
#pragma error ("unsupported architecture")
#endif

  package.sym.SizeOfStruct= sizeof(package.sym);
  package.sym.MaxNameLength= sizeof(package.name);

  /*Walk the stack, output useful information*/ 
  for(i= 0; i< STACKWALK_MAX_FRAMES;i++)
  {
    DWORD64 function_offset= 0;
    DWORD line_offset= 0;
    IMAGEHLP_LINE64 line= {sizeof(line)};
    BOOL have_module= FALSE;
    BOOL have_symbol= FALSE;
    BOOL have_source= FALSE;

    if(!StackWalk64(machine, hProcess, hThread, &frame, &context, 0, 0, 0 ,0))
      break;
    addr= frame.AddrPC.Offset;

    have_module= SymGetModuleInfo64(hProcess,addr,&module);
#ifdef _M_IX86
    if(!have_module)
    {
      /*
        ModuleInfo structure has been "compatibly" extended in releases after XP,
        and its size was increased. To make XP dbghelp.dll function
        happy, pretend passing the old structure.
      */
      module.SizeOfStruct= MODULE64_SIZE_WINXP;
      have_module= SymGetModuleInfo64(hProcess, addr, &module);
    }
#endif

    have_symbol= SymGetSymFromAddr64(hProcess, addr, &function_offset,
      &(package.sym));
    have_source= SymGetLineFromAddr64(hProcess, addr, &line_offset, &line);

    fprintf(stderr, "%p    ", addr);
    if(have_module)
    {
      char *base_image_name= strrchr(module.ImageName, '\\');
      if(base_image_name)
        base_image_name++;
      else
        base_image_name= module.ImageName;
      fprintf(stderr, "%s!", base_image_name);
    }
    if(have_symbol)
      fprintf(stderr, "%s()", package.sym.Name);
    else if(have_module)
      fprintf(stderr, "???");

    if(have_source)
    {
      char *base_file_name= strrchr(line.FileName, '\\');
      if(base_file_name)
        base_file_name++;
      else
        base_file_name= line.FileName;
      fprintf(stderr,"[%s:%u]", base_file_name, line.LineNumber);
    }
    fprintf(stderr, "\n");
  }
  fflush(stderr);
}


/*
  Write dump. The dump is created in current directory,
  file name is constructed from executable name plus
  ".dmp" extension
*/
void my_write_core(int unused)
{
  char path[MAX_PATH];
  char dump_fname[MAX_PATH]= "core.dmp";
  MINIDUMP_EXCEPTION_INFORMATION info;
  HANDLE hFile;

  if(!exception_ptrs)
    return;

  info.ExceptionPointers= exception_ptrs;
  info.ClientPointers= FALSE;
  info.ThreadId= GetCurrentThreadId();

  if(GetModuleFileName(NULL, path, sizeof(path)))
  {
    _splitpath(path, NULL, NULL,dump_fname,NULL);
    strncat(dump_fname, ".dmp", sizeof(dump_fname));
  }

  hFile= CreateFile(dump_fname, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL, 0);
  if(hFile)
  {
    /* Create minidump */
    if(MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
      hFile, MiniDumpNormal, &info, 0, 0))
    {
      fprintf(stderr, "Minidump written to %s\n",
        _fullpath(path, dump_fname, sizeof(path)) ? path : dump_fname);
    }
    else
    {
      fprintf(stderr,"MiniDumpWriteDump() failed, last error %u\n",
        GetLastError());
    }
    CloseHandle(hFile);
  }
  else
  {
    fprintf(stderr, "CreateFile(%s) failed, last error %u\n", dump_fname,
      GetLastError());
  }
  fflush(stderr);
}


void my_safe_print_str(const char *val, int len)
{
  __try
  {
    fprintf(stderr,"=%.*s\n", len, val);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    fprintf(stderr,"is an invalid string pointer\n");
  }
}
#endif /*__WIN__*/
