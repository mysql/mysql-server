/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif
#include "vio-global.h"

VIO_NS_BEGIN

void
Vio::release()
{
	delete this;
}

Vio::~Vio()
{
}

VIO_NS_END
