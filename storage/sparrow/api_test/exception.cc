#include "exception.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include "Windows.h"
#else
#include <errno.h>
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////
// MyException
//////////////////////////////////////////////////////////////////////////////////////////////////////

MyException spwerror;

MyException::MyException(const char* text, int errcode /* = -1 */) 
	: errcode_(errcode) {
	strncpy(buffer_, text, sizeof(buffer_) - 1);
	buffer_[sizeof(buffer_) - 1] = '\0';
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif 

// STATIC
MyException MyException::create( const bool addError, int errcode, const char* format, ... ) {
	char buffer[1024];
	va_list varargs;
	va_start(varargs, format);
	vsnprintf(buffer, sizeof(buffer), format, varargs);
	va_end(varargs);
	if (addError) {
		char error[1024];
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
		char result[1024];
		snprintf(result, sizeof(result), "%s (%s)", buffer, error);
		return MyException(result, errcode);
	} else {
		return MyException(buffer, errcode);
	}
}

// STATIC
MyException MyException::create(const bool addError, const char* format, ...) {
	char buffer[1024];
	va_list varargs;
	va_start(varargs, format);
	vsnprintf(buffer, sizeof(buffer), format, varargs);
	va_end(varargs);
	if (addError) {
		char error[1024];
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
		char result[1024];
		snprintf(result, sizeof(result), "%s (%s)", buffer, error);
		return MyException(result);
	} else {
		return MyException(buffer);
	}
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 


const MyException& MyException::operator = ( const MyException& excpt ) {
	errcode_	= excpt.errcode_;
	memcpy( buffer_, excpt.buffer_, sizeof(buffer_) );
	return *this;
}

void MyException::toLog() const {
	fprintf(stderr, "Sparrow Test App: %s", getText());
}
