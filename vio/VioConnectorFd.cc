
/*
 * Unneccessary virtual destructor.
 */

#include	"vio-global.h"
#include	<unistd.h>
#include	<fcntl.h>
#include	<assert.h>

#include	"viotypes.h"
#include	"Vio.h"
#include	"VioConnectorFd.h"

VIO_NS_BEGIN

VioConnectorFd::~VioConnectorFd()
{
}

VIO_NS_END

