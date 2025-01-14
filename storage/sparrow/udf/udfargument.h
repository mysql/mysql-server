#ifndef _udfargument_h_
#define _udfargument_h_

#include "my_sys.h"
#include <new>
#include <string.h>
//#include <my_global.h>
//#include "mysql_com.h"
#include "mysql/udf_registration_types.h"

////////////////////////////////////////////////////////////////////////////////
// UdfArgument
////////////////////////////////////////////////////////////////////////////////

template<typename T>
class UdfArgument
{
public:

	// virtual destructor
	virtual ~UdfArgument();

	// return value
	const T& getValue() const;

	// return type
	virtual Item_result getType() const = 0;

	// return whether the argument is null or not
	bool isNull() const;

protected:

	// constructor
	UdfArgument();

	// set value
	virtual void setValue(char* arg, unsigned long length) = 0;

	// the maximum reached value of the determiner
	T value_;

	// null flag
	bool null_;
};

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentString wraps char*
////////////////////////////////////////////////////////////////////////////////

class UdfArgumentString : public UdfArgument<char*>
{

friend bool operator > ( const UdfArgumentString& a1, const UdfArgumentString& a2 );

public:
	
	// constructor
	UdfArgumentString();
	UdfArgumentString(char* arg, unsigned long length);

	// virtual destructor
	virtual ~UdfArgumentString();

	// assignment
	UdfArgumentString& operator = (const UdfArgumentString& src);

	// set value
	virtual void setValue(char* arg, unsigned long length);

	// return type of determiner
	virtual Item_result getType() const;
};

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentReal wraps double
////////////////////////////////////////////////////////////////////////////////

class UdfArgumentReal : public UdfArgument<double>
{

friend bool operator > ( const UdfArgumentReal& a1, const UdfArgumentReal& a2 );

public:

	// constructor
	UdfArgumentReal();
	UdfArgumentReal(char* arg, unsigned long length);

	// virtual destructor
	virtual ~UdfArgumentReal();

	// assignment
	UdfArgumentReal& operator = (const UdfArgumentReal& src);

	// set value
	virtual void setValue(char* arg, unsigned long length);

	// return type of determiner
	Item_result getType() const;
};

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentInt wraps long long
////////////////////////////////////////////////////////////////////////////////

class UdfArgumentInt : public UdfArgument<longlong>
{

friend bool operator > ( const UdfArgumentInt& a1, const UdfArgumentInt& a2 );

public:

	// constructor
	UdfArgumentInt();
	UdfArgumentInt(char* arg, unsigned long length);

	// virtual destructor
	virtual ~UdfArgumentInt();

	// assignment
	UdfArgumentInt& operator = (const UdfArgumentInt& src);

	// set value
	virtual void setValue(char* arg, unsigned long length);

	// return type of determiner
	Item_result getType() const;
};

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentDecimal wraps char*
////////////////////////////////////////////////////////////////////////////////

class UdfArgumentDecimal : public UdfArgumentString
{

friend bool operator > ( const UdfArgumentDecimal& a1, const UdfArgumentDecimal& a2 );

public:
	
	// constructor
	UdfArgumentDecimal();
	UdfArgumentDecimal(char* arg, unsigned long length);

	// virtual destructor
	virtual ~UdfArgumentDecimal();

	// assignment
	UdfArgumentDecimal& operator = (const UdfArgumentDecimal& src);

	// set value
	virtual void setValue(char* arg, unsigned long length);

	// return type of determiner
	Item_result getType() const;
};




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// UdfArgument
////////////////////////////////////////////////////////////////////////////////

// constructor
template<typename T> 
inline UdfArgument<T>::UdfArgument()
{
	null_ = true;
}

// destructor
template<typename T> 
inline UdfArgument<T>::~UdfArgument()
{
}

// return value
template<typename T> 
inline const T& UdfArgument<T>::getValue() const
{
	return value_;
}

// return whether the argument is null or not
template<typename T> 
inline bool UdfArgument<T>::isNull() const
{
	return null_;
}

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentString wraps char*
////////////////////////////////////////////////////////////////////////////////

// constructor
inline UdfArgumentString::UdfArgumentString()
{
	value_ = NULL;
}

inline UdfArgumentString::UdfArgumentString(char* arg, unsigned long length)
{
	value_ = NULL;
	setValue(arg, length);
}

// destructor
inline UdfArgumentString::~UdfArgumentString()
{
	if (value_ != NULL)
	{
		delete value_;
	}
}

// assignment
inline UdfArgumentString& UdfArgumentString::operator = (const UdfArgumentString& src)
{
	if (&src == this)
	{
		return *this;
	}

	setValue(src.value_, (src.value_==NULL)? 0:(unsigned long)strlen(src.value_));

	return *this;
}

// set value
inline void UdfArgumentString::setValue(char* arg, unsigned long length)
{
	if (arg == NULL)
	{
		null_ = true;
	}
	else
	{
		if (value_ != NULL)
		{
			delete value_;
			value_ = NULL;
		}

		value_ = new (std::nothrow) char[length+1];

		if (value_ != NULL)
		{
			value_[length] = 0x00;
			if (length > 0)
			{
				strncpy(value_,arg,length);
			}
		}

		null_ = (value_ == NULL);
	}
}

// return type
inline Item_result UdfArgumentString::getType() const
{
	return STRING_RESULT;
}

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentInt wraps long long
////////////////////////////////////////////////////////////////////////////////

// constructor
inline UdfArgumentInt::UdfArgumentInt()
{
}

inline UdfArgumentInt::UdfArgumentInt(char* arg, unsigned long length)
{
	setValue(arg, length);
}

// destructor
inline UdfArgumentInt::~UdfArgumentInt()
{
}

// assignment
inline UdfArgumentInt& UdfArgumentInt::operator = (const UdfArgumentInt& src)
{
	if (&src == this)
	{
		return *this;
	}

	value_ = src.value_;
	null_ = src.null_;

	return *this;
}

// set value
inline void UdfArgumentInt::setValue(char* arg, unsigned long length)
{
	if (arg == NULL)
	{
		null_ = true;
	}
	else
	{
		value_ = *((long long*) arg);
		null_ = false;
	}
}

// return type
inline Item_result UdfArgumentInt::getType() const
{
	return INT_RESULT;
}

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentReal wraps double
////////////////////////////////////////////////////////////////////////////////

// constructor
inline UdfArgumentReal::UdfArgumentReal()
{
}

inline UdfArgumentReal::UdfArgumentReal(char* arg, unsigned long length)
{
	setValue(arg, length);
}

// destructor
inline UdfArgumentReal::~UdfArgumentReal()
{
}

// assignment
inline UdfArgumentReal& UdfArgumentReal::operator = (const UdfArgumentReal& src)
{
	if (&src == this)
	{
		return *this;
	}

	value_ = src.value_;
	null_ = src.null_;

	return *this;
}

// set value
inline void UdfArgumentReal::setValue(char* arg, unsigned long length)
{
	if (arg == NULL)
	{
		null_ = true;
	}
	else
	{
		value_ = *((double*) arg);
		null_ = false;
	}
}

// return type
inline Item_result UdfArgumentReal::getType() const
{
	return REAL_RESULT;
}

////////////////////////////////////////////////////////////////////////////////
// UdfArgumentDecimal wraps char*
////////////////////////////////////////////////////////////////////////////////

// constructor
inline UdfArgumentDecimal::UdfArgumentDecimal() : UdfArgumentString()
{
}

inline UdfArgumentDecimal::UdfArgumentDecimal(char* arg, unsigned long length) : UdfArgumentString(arg, length)
{
}

// destructor
inline UdfArgumentDecimal::~UdfArgumentDecimal()
{
}

// assignment
inline UdfArgumentDecimal& UdfArgumentDecimal::operator = (const UdfArgumentDecimal& src)
{
	if (&src == this)
	{
		return *this;
	}

	setValue(src.value_, (src.value_==NULL)? 0:(unsigned long)strlen(src.value_));

	return *this;
}

// set value
inline void UdfArgumentDecimal::setValue(char* arg, unsigned long length)
{
	UdfArgumentString::setValue(arg, length);
}

// return type
inline Item_result UdfArgumentDecimal::getType() const
{
	return DECIMAL_RESULT;
}



#endif //#define _udfargument_h_
