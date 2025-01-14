#ifndef _spw_api_assert_h
#define _spw_api_assert_h

#include <stdlib.h>

#include "my_compiler.h"


/*
** Generic Macro
*/

inline void spwAssertionFailure (const char* expr, const char* filename, int lineno)
{
	const char* text = "assertion failed: %s, in file %s, line %d\n";
	printf(text, expr, filename, lineno);
#ifdef _WIN32
	DebugBreak();
#endif
	abort();
}

#define _SPW_ASSERT(a) do { if ((a) == 0) spwAssertionFailure (#a, __FILE__, __LINE__); } while (0)

void PRINT_DBUG(const char* format, ...) MY_ATTRIBUTE((format(printf, 1, 2)));
void PRINT_INFO(const char* format, ...) MY_ATTRIBUTE((format(printf, 1, 2)));
void PRINT_WARN(const char* format, ...) MY_ATTRIBUTE((format(printf, 1, 2)));
void PRINT_ERR(const char* format, ...) MY_ATTRIBUTE((format(printf, 1, 2)));

inline void PRINT_WARN(const char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stdout, "warn: ");
	vfprintf(stdout, format, args);
	fprintf(stdout, "\n");
	va_end(args);
}

inline void PRINT_ERR(const char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stdout, "error: ");
	vfprintf(stdout, format, args);
	fprintf(stdout, "\n");
	va_end(args);
}

#ifdef NDEBUG

#define PRINT_DBUG(...)
#define PRINT_INFO(...)
#define SPW_ASSERT(a)
#define SPW_dbgASSERT(a)
#define SPW_relASSERT(a) _SPW_ASSERT(a)

#else

inline void PRINT_DBUG(const char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stdout, "debug: ");
	vfprintf(stdout, format, args);
	fprintf(stdout, "\n");
	va_end(args);
}

inline void PRINT_INFO(const char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stdout, "info: ");
	vfprintf(stdout, format, args);
	fprintf(stdout, "\n");
	va_end(args);
}

#define SPW_ASSERT(a) _SPW_ASSERT(a)
#define SPW_dbgASSERT(a) _SPW_ASSERT(a)
#define SPW_relASSERT(a) _SPW_ASSERT(a)

#endif


#endif		// _spw_api_assert_h
