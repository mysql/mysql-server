/^#include <netinet.in.h>/a\
\extern void __dbsrv_timeout();
/^	return;/i\
\	__dbsrv_timeout(0);
s/^main/void __dbsrv_main/
