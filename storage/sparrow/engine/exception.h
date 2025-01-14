/*
	Sparrow exception.
*/

#ifndef _engine_exception_h_
#define _engine_exception_h_

#include "my_compiler.h"

namespace Sparrow {


//////////////////////////////////////////////////////////////////////////////////////////////////////
// SparrowException
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
// Disable warning regarding exception specification.
#pragma warning(disable:4290)
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

class SparrowException {
private:

	const bool logged_;
	unsigned int	err_code_;
	char buffer_[1024];

public:

	SparrowException(const char* text, const bool logged = true, unsigned int err_code=-1);
	static SparrowException create(const bool addError, const char* format, ...) MY_ATTRIBUTE((format(printf, 2, 3)));
	bool isLogged() const {
		return logged_;
	}
	const char* getText() const {
		return buffer_;
	}
	void toLog() const;
	unsigned int get_err_code() const {
		return err_code_;
	}
	void set_err_code(unsigned int err_code) {
		err_code_ = err_code;
	}
};

}

#endif /* #ifndef _engine_exception_h_ */
