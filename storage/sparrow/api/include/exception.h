/*
	Sparrow exception.
*/

#ifndef _sparrow_api_exception_h_
#define _sparrow_api_exception_h_

#include "global.h"

#ifndef _WIN32
#include <errno.h>
#endif
#include <cstdio>
#include <stdio.h>
#include <stdarg.h>


//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowException
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
// Disable warning regarding exception specification.
#pragma warning(disable:4290 4996)
#endif

/*
  Disable MY_ATTRIBUTE for Visual Studio.
*/
#ifndef MY_ATTRIBUTE
#if defined(__GNUC__) || defined(__clang__)
#define MY_ATTRIBUTE(A) __attribute__(A)
#else
#define MY_ATTRIBUTE(A)
#endif
#endif

#ifdef __GNUG__
	#if __GNUC__ >= 8 
		#define _THROW_(a)
	#else
		#define _THROW_(a)		throw(a)
	#endif
#elif defined(_MSC_VER)
	#if _MSC_VER >= 1800
		#define _THROW_(a)
	#else
		#define _THROW_(a)		throw(a)
	#endif
#else
	#define _THROW_(a)		throw(a)
#endif


#define SPW_EXCEPT_MAXLENGTH	2048

class SparrowException {
private:

	bool		logged_;
	int32_t		errcode_;
	char		buffer_[SPW_EXCEPT_MAXLENGTH];

public:

	SparrowException() : logged_(false), errcode_(SPW_API_FAILED) { 
		buffer_[0] = '\0';
	}
	SparrowException(const char* text, const bool logged = true, int32_t err_code = SPW_API_FAILED);
	static SparrowException create(const bool addError, int32_t err_code, const char* format, ...) MY_ATTRIBUTE((format(printf, 3, 4)));
	SparrowException& operator = (const SparrowException& right) = default;
	bool isLogged() const {
		return logged_;
	}
	const char* getText() const {
		return buffer_;
	}
	int32_t getErrcode() const { return errcode_; }
	void toLog() const;

};

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif 

inline SparrowException::SparrowException(const char* text, const bool logged /* = true */, int32_t err_code /* = SPW_API_FAILED */) 
: logged_(logged), errcode_(err_code) {
	strncpy(buffer_, text, sizeof(buffer_) - 1);
	buffer_[sizeof(buffer_) - 1] = '\0';
}

// STATIC
inline SparrowException SparrowException::create( const bool addError, int32_t err_code, const char* format, ... ) {
	char buffer[SPW_EXCEPT_MAXLENGTH];
	va_list varargs;
	va_start(varargs, format);
	vsnprintf(buffer, sizeof(buffer), format, varargs);
	va_end(varargs);
	if (addError) {
		char error[SPW_EXCEPT_MAXLENGTH];
#ifdef _WIN32
		LPSTR serror = error;
		if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), serror, sizeof(error), 0) == 0) {
				snprintf(error, sizeof(error), "error %d", GetLastError());
		} else {	// Windows adds a nasty new line char...
			size_t l = strlen(error) - 1;
			while (error[l] == '\n' || error[l] == '\r') {
				error[l--] = 0;
			}
		}
#else
		snprintf(error, sizeof(error), "%s", strerror(errno));
#endif
		char result[SPW_EXCEPT_MAXLENGTH];	
		snprintf(result, sizeof(result), "%s (%s)", buffer, error);
		return SparrowException(result, true, err_code);
	} else {
		return SparrowException(buffer, true, err_code);
	}
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

inline void SparrowException::toLog() const {
	if (logged_) {
		fprintf(stderr, "Sparrow: %s", getText());
	}
}

#endif /* #ifndef _sparrow_api_exception_h_ */
