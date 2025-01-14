#ifndef _my_exception_h_
#define _my_exception_h_

#include "my_compiler.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MyException
//////////////////////////////////////////////////////////////////////////////////////////////////////

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


class MyException {
private:

	int		errcode_;
	char	buffer_[1024];

public:

	MyException() : errcode_(0) { 
		buffer_[0] = '\0';
	}
	MyException(const char* text, int errcode = -1);

	MyException(const MyException&) = default;

	static MyException create(const bool addError, const char* format, ...) MY_ATTRIBUTE((format(printf, 2, 3)));
	static MyException create(const bool addError, int errcode, const char* format, ...)  MY_ATTRIBUTE((format(printf, 3, 4)));

	const MyException& operator = (const MyException&);
	const char* getText() const {
		return buffer_;
	}
	int getErrcode() const { return errcode_; }
	void set_err_code(unsigned int errcode) {
		errcode_ = errcode;
	}
	void toLog() const;
};

#endif /* #ifndef _my_exception_h_ */
