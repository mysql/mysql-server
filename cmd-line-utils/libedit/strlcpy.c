#include <string.h>

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size)
{
	if(size) {
		strncpy(dst, src, size-1);
		dst[size-1] = '\0';
	} else {
		dst[0] = '\0';
	}
	return strlen(src);
}

size_t strlcat(char *dst, const char *src, size_t size)
{
	int dl = strlen(dst);
	int sz = size-dl-1;
	
	if(sz >= 0) {
		strncat(dst, src, sz);
		dst[sz] = '\0';
	}

	return dl+strlen(src);
}

#endif
