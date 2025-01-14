#include "memalloc.h"

#include "include/global.h"
#include "api_assert.h"

namespace Sparrow
{
	void my_free(void *ptr)
	{
		free(ptr);
	}

	void *my_malloc(size_t size, myf my_flags)
	{
		void* point;

		/* Safety */
		if (!size)
			size=1;

		point= malloc(size);

		if (point == NULL)
		{
			if (my_flags & MY_FAE)
				exit(1);
		}
		else if (my_flags & MY_ZEROFILL)
			memset(point, 0, size);

		return point;
	}


	void *my_realloc(void *oldpoint, size_t size, myf my_flags)
	{
		void *point;

		SPW_ASSERT(size > 0);
		if (!oldpoint && (my_flags & MY_ALLOW_ZERO_PTR))
			return my_malloc(size, my_flags);

		if ((point= realloc(oldpoint, size)) == NULL)
		{
			if (my_flags & MY_FREE_ON_ERROR)
				my_free(oldpoint);
			if (my_flags & MY_HOLD_ON_ERROR)
				return oldpoint;
		}
		return point;
	}


	void *my_memdup(const void *from, size_t length, myf my_flags)
	{
		void *ptr;
		if ((ptr= my_malloc(length,my_flags)) != 0)
			memcpy(ptr, from, length);
		return ptr;
	}


	char *my_strdup(const char *from, myf my_flags)
	{
		char *ptr;
		size_t length= strlen(from)+1;
		if ((ptr= (char*) my_malloc(length, my_flags)))
			memcpy(ptr, from, length);
		return ptr;
	}


	char *my_strndup(const char *from, size_t length, myf my_flags)
	{
		char *ptr;
		if ((ptr= (char*) my_malloc(length+1, my_flags)))
		{
			memcpy(ptr, from, length);
			ptr[length]= 0;
		}
		return ptr;
	}

}