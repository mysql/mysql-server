#include "udfargument.h"

bool operator > ( const UdfArgumentString& a1, const UdfArgumentString& a2 )
{
	if (a1.null_)
	{
		return false;
	}

	if (a2.null_)
	{
		return true;
	}

	if (a1.value_ == NULL)
	{
		return false;
	}

	if (a2.value_ == NULL)
	{
		return true;
	}

	return strcmp(a1.value_,a2.value_) > 0;
}

bool operator > ( const UdfArgumentReal& a1, const UdfArgumentReal& a2 )
{
	if (a1.null_)
	{
		return false;
	}

	if (a2.null_)
	{
		return true;
	}

	return a1.value_ > a2.value_;
}

bool operator > ( const UdfArgumentInt& a1, const UdfArgumentInt& a2 )
{
	if (a1.null_)
	{
		return false;
	}

	if (a2.null_)
	{
		return true;
	}

	return a1.value_ > a2.value_;
}

bool operator > ( const UdfArgumentDecimal& a1, const UdfArgumentDecimal& a2 )
{
	if (a1.null_)
	{
		return false;
	}

	if (a2.null_)
	{
		return true;
	}

	if (a1.value_ == NULL)
	{
		return false;
	}

	if (a2.value_ == NULL)
	{
		return true;
	}

	return atof(a1.value_) > atof(a2.value_);
}
