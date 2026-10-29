/* Copyright (c) 2001, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/stacktrace.cc
*/

#include "my_config.h"

#include <fcntl.h>
#include <sys/types.h>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#if defined(__linux__) || defined(__sun) || defined(__FreeBSD__)
#include <sys/syscall.h>
#ifdef HAVE_EXT_BACKTRACE
#include <backtrace/stacktrace.hpp>
#endif
#endif
#include <ctime>

#include <algorithm>
#include <cinttypes>
#include <iterator>

#include "my_inttypes.h"
#include "my_stacktrace.h"
#include "template_utils.h"  // IWYU pragma: keep

#ifdef HAVE_STACKTRACE

/* Output format of my_safe_puts_stderr(), see my_stacktrace.h. */
bool my_safe_puts_tagged_lines = false;

/*
  Number of bytes of the string read, and in the tagged-lines format emitted
  per line, by my_safe_puts_stderr(). Together with the label, offset and
  separator prefix and the trailing newline, a tagged line stays well below
  the 512-byte buffer used by the my_safe_* formatters and below PIPE_BUF,
  so each line can be emitted with a single write(2) that is not
  interleaved with output from other threads.
*/
static constexpr size_t MY_SAFE_PUTS_CHUNK = 132;

/*
  Single-line format: number of bytes of the string read, and emitted with
  one write, at a time. The single-line format does not split the string
  into lines, so it is read and written in runs of this size rather than
  in MY_SAFE_PUTS_CHUNK sized chunks, to keep the number of writes for a
  long statement low. The buffer holding a run is on the stack; no memory
  is allocated.
*/
static constexpr size_t MY_SAFE_PUTS_RUN = 4096;

/* Number of bytes of the string to read at a time in the selected format. */
static inline size_t my_safe_puts_step() {
  return my_safe_puts_tagged_lines ? MY_SAFE_PUTS_CHUNK : MY_SAFE_PUTS_RUN;
}

/*
  Number of bytes of a chunk (tagged lines) or run (single line) of len
  bytes to print, so that no multi-byte UTF-8 character is split between
  two lines or two runs. Each line is imported on its own when the error
  log is read back, e.g. into performance_schema.error_log, and a line
  starting with the continuation bytes of a character would not be
  well-formed there. Likewise, each run is checked on its own by
  my_safe_puts_filter(), so a character cut by the end of a run would be
  replaced by spaces.

  If the last 1-3 bytes of the chunk are a lead byte (0xC2-0xDF, 0xE0-0xEF
  or 0xF0-0xF4) followed by fewer continuation bytes (0x80-0xBF) than it
  needs, the chunk is ended before the lead byte; the caller continues the
  next chunk at the lead byte, so the offset of every line stays the exact
  byte offset of its first byte. 0xC0, 0xC1 and 0xF5-0xFF are not lead
  bytes, as in utf8mb4, and never end a chunk early. Other invalid
  sequences do not affect where the chunk ends: a lead byte 0xC2-0xF4
  followed only by continuation bytes is moved to the next line, even if
  the sequence then turns out to be invalid, and my_safe_puts_filter()
  replaces it there like anywhere else. A chunk is never shortened to zero
  bytes.
  Plain byte arithmetic, safe to use in a signal handler.

  Only applies when more of the string follows the chunk (more is true);
  the end of the string, e.g. cut by max_len, is kept, and a sequence cut
  there is replaced by my_safe_puts_filter().
*/
static inline size_t my_safe_puts_chunk_len(const char *chunk, size_t len,
                                            bool more) {
  if (!more) return len;
  for (size_t back = 1; back <= 3 && back <= len; ++back) {
    const unsigned char c = static_cast<unsigned char>(chunk[len - back]);
    /* Continuation byte: keep looking for the lead byte. */
    if ((c & 0xc0) == 0x80) continue;
    size_t need = 1;
    if (c >= 0xc2 && c <= 0xdf)
      need = 2;
    else if (c >= 0xe0 && c <= 0xef)
      need = 3;
    else if (c >= 0xf0 && c <= 0xf4)
      need = 4;
    /* Incomplete character, and not at the start of the chunk. */
    if (need > back && back < len) return len - back;
    return len;
  }
  return len;
}

/*
  Tagged-lines format: diagnostic emitted as the payload of a line when (the
  rest of) the string cannot be read; the offset of that line tells how far
  the dump got.
*/
static const char MY_SAFE_PUTS_INVALID[] = "<is an invalid pointer>";

/*
  Emit one chunk of the string in the selected format, see
  my_safe_puts_stderr(). Defined next to the other my_safe_* formatters at
  the bottom of this file, as it is shared by the Linux /proc path, the
  direct-memory fallback and the Windows variant.
*/
static void my_safe_puts_line(const char *label, size_t offset,
                              const char *chunk, size_t len);
static void my_safe_puts_chunk(const char *label, size_t offset, char *chunk,
                               size_t len);

#endif /* HAVE_STACKTRACE */

#ifndef _WIN32
#include <csignal>

#include "my_thread.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STACKTRACE

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>  // IWYU pragma: keep
#endif

#ifdef __FreeBSD__
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#ifdef __linux__
/* __bss_start doesn't seem to work on FreeBSD and doesn't exist on OSX/Solaris.
 */
static const char *heap_start;
extern char *__bss_start;
#endif /* __linux */

static inline bool ptr_sane(const char *p [[maybe_unused]],
                            const char *heap_end [[maybe_unused]]) {
#ifdef __linux__
  return p && p >= heap_start && p <= heap_end;
#else
  return true;
#endif
}

void my_init_stacktrace() {
#ifdef __linux__
  heap_start = (char *)&__bss_start;
#endif /* __linux__ */
}

#ifdef __linux__

/**
  Access the pages of this process through /proc/self/task/\<tid\>/mem
  in order to safely print the contents of a memory address range.

  The range is read one chunk at a time, see my_safe_puts_step(), and each
  chunk is printed with my_safe_puts_chunk(). Printing stops at the first
  NUL byte or after max_len bytes. If (part of) the range is not readable,
  whatever could be read is printed, followed by a diagnostic, see
  my_safe_puts_stderr().

  @param  label     Text identifying the string, e.g. "Query".
  @param  addr      The address at the start of the memory region.
  @param  max_len   The length of the memory region.
  @param  buf       Buffer owned by the caller, used for the /proc path name
                    and for the chunks read. Passed in rather than declared
                    here so that only one such buffer is on the stack of the
                    signal handler.
  @param  buf_size  Size of buf, at least my_safe_puts_step().

  @return Zero on success, -1 if /proc could not be opened and the caller
          has to fall back to accessing the memory directly.
*/
static int safe_print_str(const char *label, const char *addr, int max_len,
                          char *buf, size_t buf_size) {
  int fd;
  pid_t tid;
  off_t offset;
  ssize_t nbytes = 0;
  size_t total, count;
  const size_t step = std::min(my_safe_puts_step(), buf_size);
  bool failed = false;

  tid = (pid_t)syscall(SYS_gettid);

  my_safe_snprintf(buf, buf_size, "/proc/self/task/%d/mem", (int)tid);

  if ((fd = open(buf, O_RDONLY)) < 0) return -1;

  static_assert(sizeof(off_t) >= sizeof(intptr),
                "off_t needs to be able to hold a pointer.");

  total = max_len;
  offset = (intptr)addr;

  /* Tagged lines: a zero-length string still yields one (empty) line. */
  if (total == 0 && my_safe_puts_tagged_lines)
    my_safe_puts_line(label, 0, buf, 0);

  /* Read up to the maximum number of bytes, one chunk at a time. */
  while (total) {
    count = std::min(step, total);

    if ((nbytes = pread(fd, buf, count, offset)) < 0) {
      /* Just in case... */
      if (errno == EINTR) continue;
      failed = true;
      break;
    }

    /* Stop at the first NUL byte, like the direct memory access path does. */
    const char *nul = static_cast<const char *>(memchr(buf, 0, nbytes));
    /*
      End the line (tagged lines) or run (single line) before a character
      cut by the end of a full chunk that more of the string follows; the
      next read starts at that character.
    */
    const size_t len = my_safe_puts_chunk_len(
        buf, nul ? (size_t)(nul - buf) : (size_t)nbytes,
        nul == nullptr && (size_t)nbytes == count && total > count);

    /* Output the printable characters. */
    my_safe_puts_chunk(label, max_len - total, buf, len);

    /* Advance offset into memory, past the bytes printed if not at a NUL. */
    const size_t advance = nul ? (size_t)nbytes : len;
    total -= advance;
    offset += advance;

    if (nul != nullptr) break;

    /* Break if less than requested... */
    if ((count - nbytes)) {
      failed = true;
      break;
    }
  }

  const size_t done = max_len - total;
  if (my_safe_puts_tagged_lines) {
    if (failed)
      my_safe_puts_line(label, done, MY_SAFE_PUTS_INVALID,
                        sizeof(MY_SAFE_PUTS_INVALID) - 1);
  } else {
    /* Output a new line if something was printed. */
    if (done != 0) my_safe_printf_stderr("%s", "\n");
    /*
      Report an unreadable address the way the single-line format always
      has. It used to read 256 bytes at a time, and as a read of /proc
      returns the readable part of a range, only a read starting at an
      unreadable byte failed and was reported, i.e. when reading failed at
      an offset that is a multiple of 256. Keep that exact output.
    */
    if (failed && done % 256 == 0)
      my_safe_printf_stderr("Can't read from address %p\n", addr + done);
  }

  close(fd);

  return 0;
}

#endif /* __linux __ */

void my_safe_puts_stderr(const char *label, const char *val, size_t max_len) {
  const char *heap_end = nullptr;
  /*
    The one buffer holding a chunk of the string, shared with
    safe_print_str() on Linux, so that only one MY_SAFE_PUTS_RUN sized buffer
    is on the stack of the signal handler.
  */
  char buf[MY_SAFE_PUTS_RUN];

  /* Single line: the label is printed once, followed by the string. */
  if (!my_safe_puts_tagged_lines) my_safe_printf_stderr("%s: ", label);

#ifdef __linux__
  if (!safe_print_str(label, val, max_len, buf, sizeof buf)) return;

  /* Only needed by the linux version of ptr_sane() */
  heap_end = static_cast<const char *>(sbrk(0));
#endif

  if (!ptr_sane(val, heap_end)) {
    if (my_safe_puts_tagged_lines)
      my_safe_puts_line(label, 0, MY_SAFE_PUTS_INVALID,
                        sizeof(MY_SAFE_PUTS_INVALID) - 1);
    else
      my_safe_printf_stderr("%s", "is an invalid pointer\n");
    return;
  }

  /*
    Copy the string chunk by chunk into a local buffer, checking every byte
    with ptr_sane() before touching it, and print each chunk with
    my_safe_puts_chunk(). Stops at the first NUL byte or after max_len bytes.
  */
  const size_t step = my_safe_puts_step();
  size_t offset = 0;
  for (;;) {
    size_t len = 0;
    bool nul = false, failed = false;
    while (len < step && offset + len < max_len) {
      const char *p = val + offset + len;
      if (!ptr_sane(p, heap_end)) {
        failed = true;
        break;
      }
      if (*p == '\0') {
        nul = true;
        break;
      }
      buf[len++] = *p;
    }

    /* Keep characters whole, see my_safe_puts_chunk_len(). */
    len = my_safe_puts_chunk_len(buf, len,
                                 !nul && !failed && offset + len < max_len);
    my_safe_puts_chunk(label, offset, buf, len);
    offset += len;

    if (failed) {
      if (my_safe_puts_tagged_lines)
        my_safe_puts_line(label, offset, MY_SAFE_PUTS_INVALID,
                          sizeof(MY_SAFE_PUTS_INVALID) - 1);
      break;
    }
    if (nul || offset >= max_len) break;
  }

  if (!my_safe_puts_tagged_lines) my_safe_printf_stderr("%s", "\n");
}

#ifdef HAVE_EXT_BACKTRACE
static size_t my_write_stderr(const std::string_view &sv) {
  return my_write_stderr(std::data(sv), std::size(sv));
}

void my_print_stacktrace(const uchar *stack_bottom, ulong thread_stack) {
  my_safe_printf_stderr("stack_bottom = %p thread_stack 0x%lx\n", stack_bottom,
                        thread_stack);

  struct cookie_t {
    int index;
  };
  auto print_callback = [](void *cookie, uintptr_t pc, const char *filename,
                           int lineno, const char *function) {
    auto idx = static_cast<cookie_t *>(cookie)->index++;
    my_safe_printf_stderr(" #%d 0x%lx ", idx, (unsigned long)pc);
    my_write_stderr(function == nullptr ? "<unknown>" : function);
    if (filename != nullptr) {
      std::string_view filename_sv{filename};
      constexpr std::string_view parent_dir{"../"};
      if (auto pos = filename_sv.rfind(parent_dir);
          pos != std::string_view::npos) {
        filename_sv.remove_prefix(pos + std::size(parent_dir));
      }
      constexpr std::string_view mysql_dir{"mysql/"};
      if (auto pos = filename_sv.find(mysql_dir);
          pos != std::string_view::npos) {
        filename_sv.remove_prefix(pos + std::size(mysql_dir));
      }
      my_write_stderr(" at ");
      my_write_stderr(filename_sv);
      my_safe_printf_stderr(":%d", lineno);
    }
    my_write_stderr("\n");

    return 0;
  };
  auto error_callback = [](void *, const char *msg, int errnum) {
    my_safe_printf_stderr("libbacktrace: %s", msg);
    if (errnum > 0) my_safe_printf_stderr(": %d", errnum);
    my_safe_printf_stderr("\n");
  };
  cookie_t cookie{.index = 0};
  stacktrace::full(1, print_callback, error_callback, &cookie);
}
#elif defined(HAVE_BACKTRACE)

#ifdef HAVE_ABI_CXA_DEMANGLE

#include <cxxabi.h>

static char *my_demangle(const char *mangled_name, int *status) {
  return abi::__cxa_demangle(mangled_name, nullptr, nullptr, status);
}

static bool my_demangle_symbol(char *line) {
  char *demangled = nullptr;
#ifdef __APPLE__  // OS X formatting of stacktraces is different from Linux
  char *begin = strstr(line, "_Z");
  char *end = begin ? strchr(begin, ' ') : nullptr;

  if (begin && end) {
    begin[-1] = '\0';
    *end = '\0';
    int status;
    demangled = my_demangle(begin, &status);
    if (!demangled || status) {
      demangled = nullptr;
      begin[-1] = '_';
      *end = ' ';
    }
  }
  if (demangled) my_safe_printf_stderr("%s %s %s\n", line, demangled, end + 1);
#else             // !__APPLE__
  char *begin = strchr(line, '(');
  char *end = begin ? strchr(begin, '+') : nullptr;

  if (begin && end) {
    *begin++ = *end++ = '\0';
    int status;
    demangled = my_demangle(begin, &status);
    if (!demangled || status) {
      demangled = nullptr;
      begin[-1] = '(';
      end[-1] = '+';
    }
  }
  if (demangled) my_safe_printf_stderr("%s(%s+%s\n", line, demangled, end);
#endif
  bool ret = (demangled == nullptr);
  std::free(demangled);
  return (ret);
}

// If it does not start with "_Z" it is a C function, and demangling fails.
// Print the original line, with modifications done by my_demangle_symbol().
static void my_demangle_symbols(char **addrs, int n) {
  for (int i = 0; i < n; i++) {
    if (my_demangle_symbol(addrs[i]))  // demangling failed
      my_safe_printf_stderr("%s\n", addrs[i]);
  }
}

#endif /* HAVE_ABI_CXA_DEMANGLE */

void my_print_stacktrace(const uchar *stack_bottom, ulong thread_stack) {
#if defined(__FreeBSD__)
  static char procname_buffer[2048];
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  unw_word_t offp;
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_proc_name(&cursor, procname_buffer, sizeof(procname_buffer), &offp);
    int status;
    char *demangled = my_demangle(procname_buffer, &status);
    my_safe_printf_stderr("[0x%lx] %s+0x%lx\n", ip,
                          demangled ? demangled : procname_buffer, offp);
    if (demangled) std::free(demangled);
  }
#endif
  void *addrs[128];
  char **strings = nullptr;
  int n = backtrace(addrs, array_elements(addrs));
  my_safe_printf_stderr("stack_bottom = %p thread_stack 0x%lx\n", stack_bottom,
                        thread_stack);
#ifdef HAVE_ABI_CXA_DEMANGLE
  if ((strings = backtrace_symbols(addrs, n))) {
    my_demangle_symbols(strings, n);
    std::free(strings);
  }
#endif
  if (!strings) {
    backtrace_symbols_fd(addrs, n, fileno(stderr));
  }
}
#endif /* HAVE_EXT_BACKTRACE || HAVE_BACKTRACE */
#endif /* HAVE_STACKTRACE */

/* Produce a core for the thread */
void my_write_core(int sig) {
  signal(sig, SIG_DFL);
  pthread_kill(my_thread_self(), sig);
#if defined(P_MYID)
  /* On Solaris, the above kill is not enough */
  sigsend(P_PID, P_MYID, sig);
#endif
}

#else /* _WIN32*/

#include <dbghelp.h>
#include <tlhelp32.h>
#if _MSC_VER
#pragma comment(lib, "dbghelp")
#endif

static thread_local EXCEPTION_POINTERS *exception_ptrs;

#define MODULE64_SIZE_WINXP 576
#define STACKWALK_MAX_FRAMES 64

void my_init_stacktrace() {}

void my_set_exception_pointers(EXCEPTION_POINTERS *ep) { exception_ptrs = ep; }

/*
  Appends directory to symbol path.
*/
static void add_to_symbol_path(char *path, size_t path_buffer_size, char *dir,
                               size_t dir_buffer_size) {
  strcat_s(dir, dir_buffer_size, ";");
  if (!strstr(path, dir)) {
    strcat_s(path, path_buffer_size, dir);
  }
}

/*
  Get symbol path - semicolon-separated list of directories to search for debug
  symbols. We expect PDB in the same directory as corresponding exe or dll,
  so the path is build from directories of the loaded modules. If environment
  variable _NT_SYMBOL_PATH is set, it's value appended to the symbol search path
*/
static void get_symbol_path(char *path, size_t size) {
  HANDLE hSnap;
  char *envvar;
  char *p;
#ifndef NDEBUG
  static char pdb_debug_dir[MAX_PATH + 7];
#endif

  path[0] = '\0';

#ifndef NDEBUG
  /*
    Add "debug" subdirectory of the application directory, sometimes PDB will
    placed here by installation.
  */
  GetModuleFileName(nullptr, pdb_debug_dir, MAX_PATH);
  p = strrchr(pdb_debug_dir, '\\');
  if (p) {
    *p = 0;
    strcat_s(pdb_debug_dir, sizeof(pdb_debug_dir), "\\debug;");
    add_to_symbol_path(path, size, pdb_debug_dir, sizeof(pdb_debug_dir));
  }
#endif

  /*
    Enumerate all modules, and add their directories to the path.
    Avoid duplicate entries.
  */
  hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  if (hSnap != INVALID_HANDLE_VALUE) {
    BOOL ret;
    MODULEENTRY32 mod;
    mod.dwSize = sizeof(MODULEENTRY32);
    for (ret = Module32First(hSnap, &mod); ret;
         ret = Module32Next(hSnap, &mod)) {
      char *module_dir = mod.szExePath;
      p = strrchr(module_dir, '\\');
      if (!p) {
        /*
          Path separator was not found. Not known to happen, if ever happens,
          will indicate current directory.
        */
        module_dir[0] = '.';
        module_dir[1] = '\0';
      } else {
        *p = '\0';
      }
      add_to_symbol_path(path, size, module_dir, sizeof(mod.szExePath));
    }
    CloseHandle(hSnap);
  }

  /* Add _NT_SYMBOL_PATH, if present. */
  envvar = getenv("_NT_SYMBOL_PATH");
  if (envvar) {
    strcat_s(path, size, envvar);
  }
}

#define MAX_SYMBOL_PATH 32768

/* Platform SDK in VS2003 does not have definition for SYMOPT_NO_PROMPTS*/
#ifndef SYMOPT_NO_PROMPTS
#define SYMOPT_NO_PROMPTS 0
#endif

void my_print_stacktrace(const uchar * /* stack_bottom */,
                         ulong /* thread_stack */) {
  HANDLE hProcess = GetCurrentProcess();
  HANDLE hThread = GetCurrentThread();
  static IMAGEHLP_MODULE64 module{};
  module.SizeOfStruct = sizeof(module);

  static IMAGEHLP_SYMBOL64_PACKAGE package;
  DWORD64 addr;
  DWORD machine;
  int i;
  CONTEXT context;
  STACKFRAME64 frame{};
  static char symbol_path[MAX_SYMBOL_PATH];

  if (exception_ptrs) {
    /* Copy context, as stackwalking on original will unwind the stack */
    context = *(exception_ptrs->ContextRecord);
  } else {
    /*
      We are to print stack outside the signal handler, let's just capture the
      current context.
    */
    RtlCaptureContext(&context);
  }

  /*Initialize symbols.*/
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_NO_PROMPTS | SYMOPT_DEFERRED_LOADS |
                SYMOPT_DEBUG);
  get_symbol_path(symbol_path, sizeof(symbol_path));
  SymInitialize(hProcess, symbol_path, true);

  /*Prepare stackframe for the first StackWalk64 call*/
  frame.AddrFrame.Mode = frame.AddrPC.Mode = frame.AddrStack.Mode =
      AddrModeFlat;
#if (defined _M_IX86)
  machine = IMAGE_FILE_MACHINE_I386;
  frame.AddrFrame.Offset = context.Ebp;
  frame.AddrPC.Offset = context.Eip;
  frame.AddrStack.Offset = context.Esp;
#elif (defined _M_X64)
  machine = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrFrame.Offset = context.Rbp;
  frame.AddrPC.Offset = context.Rip;
  frame.AddrStack.Offset = context.Rsp;
#else
  /*There is currently no need to support IA64*/
  /* Warning C4068: unknown pragma 'error' */
#pragma error("unsupported architecture")
#endif

  package.sym.SizeOfStruct = sizeof(package.sym);
  package.sym.MaxNameLength = sizeof(package.name);

  /*Walk the stack, output useful information*/
  for (i = 0; i < STACKWALK_MAX_FRAMES; i++) {
    DWORD64 function_offset = 0;
    DWORD line_offset = 0;
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);

    BOOL have_module = false;
    BOOL have_symbol = false;
    BOOL have_source = false;

    if (!StackWalk64(machine, hProcess, hThread, &frame, &context, nullptr,
                     nullptr, nullptr, nullptr))
      break;
    addr = frame.AddrPC.Offset;

    have_module = SymGetModuleInfo64(hProcess, addr, &module);
    have_symbol =
        SymGetSymFromAddr64(hProcess, addr, &function_offset, &(package.sym));
    have_source = SymGetLineFromAddr64(hProcess, addr, &line_offset, &line);

    my_safe_printf_stderr("%llx    ", addr);
    if (have_module) {
      char *base_image_name = strrchr(module.ImageName, '\\');
      if (base_image_name)
        base_image_name++;
      else
        base_image_name = module.ImageName;
      my_safe_printf_stderr("%s!", base_image_name);
    }
    if (have_symbol)
      my_safe_printf_stderr("%s()", package.sym.Name);

    else if (have_module)
      my_safe_printf_stderr("%s", "???");

    if (have_source) {
      char *base_file_name = strrchr(line.FileName, '\\');
      if (base_file_name)
        base_file_name++;
      else
        base_file_name = line.FileName;
      my_safe_printf_stderr("[%s:%lu]", base_file_name, line.LineNumber);
    }
    my_safe_printf_stderr("%s", "\n");
  }
}

/*
  Write dump. The dump is created in current directory,
  file name is constructed from executable name plus
  ".dmp" extension
*/
void my_write_core(int /* sig */) {
  char path[MAX_PATH];
  // See comment below for clarification about size of dump_fname
  char dump_fname[MAX_PATH + 1 + 10 + 4 + 1] = "core.dmp";

  if (!exception_ptrs) return;

  if (GetModuleFileName(nullptr, path, sizeof(path))) {
    char module_name[MAX_PATH];
    _splitpath(path, nullptr, nullptr, module_name, nullptr);
    // max length of a value being placed to dump_fname is
    // MAX_PATH + 1 byte for '.' + up to 10 bytes for string
    // representation of DWORD value + 4 bytes for .dmp suffix +
    // 1 byte for termitated \0. Such size of output buffer guarantees
    // that there is enough space to place a result of string formatting
    // performed by snprintf().
    snprintf(dump_fname, sizeof(dump_fname), "%s.%lu.dmp", module_name,
             GetCurrentProcessId());
  }
  my_create_minidump(dump_fname, nullptr, 0);
}

/** Create a minidump.
  @param name    path of minidump file.
  @param process HANDLE to process. (0 for own process).
  @param pid     Process id.
*/

void my_create_minidump(const char *name, HANDLE process, DWORD pid) {
  char path[MAX_PATH];
  MINIDUMP_EXCEPTION_INFORMATION info;
  PMINIDUMP_EXCEPTION_INFORMATION info_ptr = nullptr;
  HANDLE hFile;

  if (process == nullptr) {
    /* Does not need to CloseHandle() for the below. */
    process = GetCurrentProcess();
    pid = GetCurrentProcessId();
    info.ExceptionPointers = exception_ptrs;
    info.ClientPointers = false;
    info.ThreadId = GetCurrentThreadId();
    info_ptr = &info;
  }

  hFile = CreateFile(name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile) {
    const MINIDUMP_TYPE mdt =
        (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo |
                        MiniDumpWithProcessThreadData);
    /* Create minidump, use info only if same process. */
    if (MiniDumpWriteDump(process, pid, hFile, mdt, info_ptr, nullptr,
                          nullptr)) {
      my_safe_printf_stderr("Minidump written to %s\n",
                            _fullpath(path, name, sizeof(path)) ? path : name);
    } else {
      my_safe_printf_stderr("MiniDumpWriteDump() failed, last error %lu\n",
                            GetLastError());
    }
    CloseHandle(hFile);
  } else {
    my_safe_printf_stderr("CreateFile(%s) failed, last error %lu\n", name,
                          GetLastError());
  }
}

void my_safe_puts_stderr(const char *label, const char *val, size_t max_len) {
  /*
    Same chunked reading as the Unix version, see my_safe_puts_chunk().
    Reading an unmapped address raises an access violation, which is caught
    by the __except handler below; whatever was copied into buf before the
    fault is still printed, followed by a diagnostic.
  */
  char buf[MY_SAFE_PUTS_RUN];
  const size_t step = my_safe_puts_step();
  /*
    volatile: modified inside __try and read in the __except handler.
    Plain assignments only: ++ and compound assignment on volatile are
    deprecated since C++20 (-Wdeprecated-volatile).
  */
  volatile size_t offset = 0;
  volatile size_t len = 0;

  /* Single line: the label is printed once, followed by the string. */
  if (!my_safe_puts_tagged_lines) my_safe_printf_stderr("%s: ", label);

  __try {
    for (;;) {
      bool nul = false;
      len = 0;
      while (len < step && offset + len < max_len) {
        const char c = val[offset + len];
        if (c == '\0') {
          nul = true;
          break;
        }
        buf[len] = c;
        len = len + 1;
      }

      /* Keep characters whole, see my_safe_puts_chunk_len(). */
      len = my_safe_puts_chunk_len(buf, len, !nul && offset + len < max_len);
      my_safe_puts_chunk(label, offset, buf, len);
      offset = offset + len;
      len = 0;

      if (nul || offset >= max_len) break;
    }
    if (!my_safe_puts_tagged_lines) my_safe_printf_stderr("%s", "\n");
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    /* Flush the bytes read before the fault, then report the bad address. */
    if (len > 0) {
      my_safe_puts_chunk(label, offset, buf, len);
      offset = offset + len;
    }
    if (my_safe_puts_tagged_lines)
      my_safe_puts_line(label, offset, MY_SAFE_PUTS_INVALID,
                        sizeof(MY_SAFE_PUTS_INVALID) - 1);
    else
      my_safe_printf_stderr("%s", "is an invalid string pointer\n");
  }
}
#endif /* _WIN32 */

#ifdef _WIN32
size_t my_write_stderr(const void *buf, size_t count) {
  DWORD bytes_written;
  SetFilePointer(GetStdHandle(STD_ERROR_HANDLE), 0, nullptr, FILE_END);
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, (DWORD)count, &bytes_written,
            nullptr);
  return bytes_written;
}
#else
size_t my_write_stderr(const void *buf, size_t count) {
  return (size_t)write(STDERR_FILENO, buf, count);
}
#endif

static const char digits[] = "0123456789abcdef";

char *my_safe_utoa(int base, ulonglong val, char *buf) {
  *buf-- = 0;
  do {
    *buf-- = digits[val % base];
  } while ((val /= base) != 0);
  return buf + 1;
}

char *my_safe_itoa(int base, longlong val, char *buf) {
  char *orig_buf = buf;
  const bool is_neg = (val < 0);
  *buf-- = 0;

  if (is_neg) val = -val;
  if (is_neg && base == 16) {
    int ix;
    val -= 1;
    for (ix = 0; ix < 16; ++ix) buf[-ix] = '0';
  }

  do {
    *buf-- = digits[val % base];
  } while ((val /= base) != 0);

  if (is_neg && base == 10) *buf-- = '-';

  if (is_neg && base == 16) {
    int ix;
    buf = orig_buf - 1;
    for (ix = 0; ix < 16; ++ix, --buf) {
      switch (*buf) {
        case '0':
          *buf = 'f';
          break;
        case '1':
          *buf = 'e';
          break;
        case '2':
          *buf = 'd';
          break;
        case '3':
          *buf = 'c';
          break;
        case '4':
          *buf = 'b';
          break;
        case '5':
          *buf = 'a';
          break;
        case '6':
          *buf = '9';
          break;
        case '7':
          *buf = '8';
          break;
        case '8':
          *buf = '7';
          break;
        case '9':
          *buf = '6';
          break;
        case 'a':
          *buf = '5';
          break;
        case 'b':
          *buf = '4';
          break;
        case 'c':
          *buf = '3';
          break;
        case 'd':
          *buf = '2';
          break;
        case 'e':
          *buf = '1';
          break;
        case 'f':
          *buf = '0';
          break;
      }
    }
  }
  return buf + 1;
}

static const char *check_longlong(const char *fmt, bool *have_longlong) {
  *have_longlong = false;
  if (*fmt == 'l') {
    fmt++;
    if (*fmt != 'l')
      *have_longlong = (sizeof(long) == sizeof(longlong));
    else {
      fmt++;
      *have_longlong = true;
    }
  }
  return fmt;
}

static size_t my_safe_vsnprintf(char *to, size_t size, const char *format,
                                va_list ap) {
  char *start = to;
  char *end = start + size - 1;
  for (; *format; ++format) {
    bool have_longlong = false;
    if (*format != '%') {
      if (to == end) /* end of buffer */
        break;
      *to++ = *format; /* copy ordinary char */
      continue;
    }
    ++format; /* skip '%' */

    format = check_longlong(format, &have_longlong);

    switch (*format) {
      case 'd':
      case 'i':
      case 'u':
      case 'x':
      case 'p': {
        longlong ival = 0;
        ulonglong uval = 0;
        if (*format == 'p')
          have_longlong = (sizeof(void *) == sizeof(longlong));
        if (have_longlong) {
          if (*format == 'u')
            uval = va_arg(ap, ulonglong);
          else
            ival = va_arg(ap, longlong);
        } else {
          if (*format == 'u')
            uval = va_arg(ap, unsigned int);
          else
            ival = va_arg(ap, int);
        }

        {
          char buff[22];
          const int base = (*format == 'x' || *format == 'p') ? 16 : 10;
          char *val_as_str =
              (*format == 'u')
                  ? my_safe_utoa(base, uval, &buff[sizeof(buff) - 1])
                  : my_safe_itoa(base, ival, &buff[sizeof(buff) - 1]);

          /*
            Strip off "ffffffff" if we have 'x' format without 'll'
            Similarly for 'p' format on 32bit systems.
          */
          if (base == 16 && !have_longlong && ival < 0) val_as_str += 8;

          while (*val_as_str && to < end) *to++ = *val_as_str++;
          continue;
        }
      }
      case 's': {
        const char *val = va_arg(ap, char *);
        if (!val) val = "(null)";
        while (*val && to < end) *to++ = *val++;
        continue;
      }
    }
  }
  *to = 0;
  return to - start;
}

size_t my_safe_snprintf(char *to, size_t n, const char *fmt, ...) {
  size_t result;
  va_list args;
  va_start(args, fmt);
  result = my_safe_vsnprintf(to, n, fmt, args);
  va_end(args);
  return result;
}

size_t my_safe_printf_stderr(const char *fmt, ...) {
  char to[512];
  size_t result;
  va_list args;
  va_start(args, fmt);
  result = my_safe_vsnprintf(to, sizeof(to), fmt, args);
  va_end(args);
  my_write_stderr(to, result);
  return result;
}

#ifdef HAVE_STACKTRACE
/*
  Make the len bytes at s well-formed utf8mb4, in place: keep every byte
  that is part of a well-formed sequence, and replace every other byte by
  one ' ', so offsets and lengths are unchanged. Control bytes (0x00-0x1f,
  0x7f) are replaced as well, so the output contains no ASCII line break
  and no C0/DEL control byte.

  Well-formed is what my_charset_utf8mb4 accepts (my_mb_wc_utf8_prototype()
  in strings/mb_wc.h): 0x00-0x7f, of which the control bytes are replaced
  anyway; a lead byte 0xc2-0xdf followed by one continuation byte
  (0x80-0xbf); 0xe0-0xef followed by two, the first of them 0xa0-0xbf after
  0xe0 (no overlong form) and 0x80-0x9f after 0xed (no surrogate);
  0xf0-0xf4 followed by three, the first of them 0x90-0xbf after 0xf0 (no
  overlong form) and 0x80-0x8f after 0xf4 (nothing above U+10FFFF). A
  sequence cut by the end of s is not well-formed: callers end a chunk
  before a character when more of the string follows, see
  my_safe_puts_chunk_len(), so only the end of the string can cut one.
  When the bytes after a lead byte do not complete a sequence, only the
  lead byte is replaced and checking resumes at the next byte.

  Plain byte arithmetic on the caller's buffer: no locale, no library
  call, safe to use in a signal handler. iscntrl()/isprint() are neither
  async-signal-safe nor locale-independent.
*/
static void my_safe_puts_filter(char *s, size_t len) {
  for (size_t i = 0; i < len;) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
      if (c < 0x20 || c == 0x7f) s[i] = ' ';
      ++i;
      continue;
    }
    size_t need = 0;
    /* Range of the byte following the lead byte. */
    unsigned char lo = 0x80, hi = 0xbf;
    if (c >= 0xc2 && c <= 0xdf) {
      need = 2;
    } else if (c >= 0xe0 && c <= 0xef) {
      need = 3;
      if (c == 0xe0) lo = 0xa0;
      if (c == 0xed) hi = 0x9f;
    } else if (c >= 0xf0 && c <= 0xf4) {
      need = 4;
      if (c == 0xf0) lo = 0x90;
      if (c == 0xf4) hi = 0x8f;
    }
    /* need == 0: a continuation byte, 0xc0, 0xc1 or 0xf5-0xff. */
    bool valid = need != 0 && need <= len - i;
    for (size_t k = 1; valid && k < need; ++k) {
      const unsigned char n = static_cast<unsigned char>(s[i + k]);
      valid = k == 1 ? (n >= lo && n <= hi) : (n & 0xc0) == 0x80;
    }
    if (valid) {
      i += need;
    } else {
      s[i] = ' ';
      ++i;
    }
  }
}

/**
  Tagged-lines format: emit a single, self-describing line consisting of
  the label, " +", the decimal offset, ": ", the chunk and a newline,
  e.g. "Query (7f3a1c00b010) +132: ...", using one write(2).

  The payload is well-formed UTF-8: bytes that are not part of a
  well-formed sequence and control bytes are replaced by one ' ' each, see
  my_safe_puts_filter(), so the line contains no embedded ASCII line break
  and no C0/DEL control byte. A valid multi-byte UTF-8 character is kept
  and never split between two lines, see my_safe_puts_chunk_len().

  @param label   Text identifying the string, printed verbatim, e.g.
                 "Query (7f3a1c00b010)".
  @param offset  Byte offset of the chunk within the string.
  @param chunk   The bytes to print. Must already be known to be readable.
  @param len     Number of bytes in chunk, normally at most
                 MY_SAFE_PUTS_CHUNK.
*/
static void my_safe_puts_line(const char *label, size_t offset,
                              const char *chunk, size_t len) {
  char line[256];
  /* my_safe_vsnprintf has no %zu; callers bound offset far below 4 GiB. */
  size_t pos = my_safe_snprintf(line, sizeof(line), "%s +%lu: ", label,
                                static_cast<unsigned long>(offset));
  /* Never overflow the line buffer; always leave room for the newline. */
  len = std::min(len, sizeof(line) - 1 - pos);
  for (size_t i = 0; i < len; ++i) line[pos + i] = chunk[i];
  my_safe_puts_filter(line + pos, len);
  pos += len;
  line[pos++] = '\n';
  my_write_stderr(line, pos);
}

/**
  Print one chunk of the string in the format selected by
  my_safe_puts_tagged_lines: as one tagged line, see my_safe_puts_line(),
  or, in the single-line format, as the bare bytes with no prefix and no
  newline, so that consecutive chunks form one line. Either way the bytes
  are made well-formed UTF-8, see my_safe_puts_filter(), and the chunk
  takes one write(2).

  @param label   Text identifying the string, only used for tagged lines.
  @param offset  Byte offset of the chunk within the string.
  @param chunk   The bytes to print, a copy owned by the caller, which is
                 modified in place in the single-line format. Must already
                 be known to be readable.
  @param len     Number of bytes in chunk.
*/
static void my_safe_puts_chunk(const char *label, size_t offset, char *chunk,
                               size_t len) {
  if (my_safe_puts_tagged_lines) {
    my_safe_puts_line(label, offset, chunk, len);
    return;
  }
  if (len == 0) return;
  my_safe_puts_filter(chunk, len);
  my_write_stderr(chunk, len);
}
#endif /* HAVE_STACKTRACE */

void my_safe_print_system_time() {
  char hrs_buf[3] = "00";
  char mins_buf[3] = "00";
  char secs_buf[3] = "00";
  const int base = 10;
#ifdef _WIN32
  SYSTEMTIME utc_time;
  long hrs, mins, secs;
  GetSystemTime(&utc_time);
  hrs = utc_time.wHour;
  mins = utc_time.wMinute;
  secs = utc_time.wSecond;
#else
  /* Using time() instead of my_time() to avoid looping */
  const time_t curr_time = time(nullptr);
  /* Calculate time of day */
  const long tmins = curr_time / 60;
  const long thrs = tmins / 60;
  const long hrs = thrs % 24;
  const long mins = tmins % 60;
  const long secs = curr_time % 60;
#endif

  my_safe_itoa(base, hrs, &hrs_buf[2]);
  my_safe_itoa(base, mins, &mins_buf[2]);
  my_safe_itoa(base, secs, &secs_buf[2]);

  my_safe_printf_stderr("---------- %s:%s:%s UTC - ", hrs_buf, mins_buf,
                        secs_buf);
}
