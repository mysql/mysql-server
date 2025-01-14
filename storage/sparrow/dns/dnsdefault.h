/*
	Default DNS server.
*/

#ifndef _dns_default_h_
#define _dns_default_h_

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// DnsDefault
//////////////////////////////////////////////////////////////////////////////////////////////////////

class DnsDefault {
private:

	static char** servers_;
	static int n_;

private:

	static char** initialize();

public:

	static int getNumber() {
		return n_;
	}

	static const char* getServer(int i) {
		return servers_[i];
	}
};

}

#endif /* #ifndef _dns_default_h_ */
