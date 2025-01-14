/*
	IP address.
*/

#ifndef _functions_ipaddress_h_
#define _functions_ipaddress_h_


#include <cstddef>
#include <cstdint>

namespace IvFunctions {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IpAddress
//////////////////////////////////////////////////////////////////////////////////////////////////////

class IpAddress {
private:

	uint8_t* bytes_;
	uint32_t length_;

private:

	static int parseInt(const char* buffer, uint32_t length);
	static int parseHex(const char* buffer, uint32_t length);

public:

	IpAddress() : bytes_(0), length_(0) {
	}

	IpAddress(const uint8_t* bytes, uint32_t length) : bytes_(const_cast<uint8_t*>(bytes)), length_(length) {
	}

	bool isValid() const {
		return bytes_ != 0 && (length_ == 4 || length_ == 16);
	}

	bool isV4() const {
		if (length_ == 4) {
			return true;
		} else {
			for (int i = 0; i < 12; ++i) {
				if (bytes_[i] != 0) {
					return false;
				}
			}
			return true;
		}
	}

	bool isPrivate() const {
		if (isV4()) {
			const uint32_t check = length_ == 4 ? ((bytes_[0] << 8) | bytes_[1]) : ((bytes_[12] << 8) | bytes_[13]);
			return (check & 0xff00) == 0xa00 || check == 0xc0a8 || (check >> 4) == 0xac1;
		} else {
			return bytes_[0] == static_cast<uint8_t>(0xfd);
		}
	}

	uint32_t print(char* buffer) const;

	bool parse(const char* buffer, uint32_t length);

	bool applyMask(const IpAddress& mask);

	void makeMask(int bits);
};

}

#endif /* #ifndef _functions_ipaddress_h_ */
