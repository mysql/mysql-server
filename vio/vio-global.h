#include <global.h>

#if !defined(VIO_HAVE_OPENSSL) && defined(HAVE_OPENSSL)
#define	VIO_HAVE_OPENSSL	HAVE_OPENSSL
#endif	/* !defined(VIO_HAVE_OPENSSL) && defined(HAVE_OPENSSL) */

#include "viotypes.h"
#include "Vio.h"
#include "VioAcceptorFd.h"
#include "VioFd.h"
#include "VioPipe.h"
#include "VioSocket.h"
#ifdef VIO_HAVE_OPENSSL
#include "VioSSL.h"
#include "VioSSLFactoriesFd.h"
#endif /* VIO_HAVE_OPENSSL */


#if VIO_HAVE_NAMESPACES
#define	VIO_STD_NS	std
#define	VIO_STD_NS_USING using namespace std;
#define	VIO_NS		VirtualIO
#define	VIO_NS_BEGIN	namespace VIO_NS {
#define	VIO_NS_END	}
#define	VIO_NS_USING	using namespace VIO_NS;
#else
#define	VIO_STD_NS
#define	VIO_STD_NS_USING
#define	VIO_NS
#define	VIO_NS_BEGIN
#define	VIO_NS_END
#define	VIO_NS_USING
#endif
