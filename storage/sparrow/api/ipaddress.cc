/*
	IP address.
*/

#include "ipaddress.h"

#include <assert.h>
#include <cstdio>
#include <cstring>
#include <cctype>

namespace IvFunctions {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IpAddress
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Prints an IP address. Returns the length of the output string, or 0 in case of error.
uint32_t IpAddress::print(char* buffer) const {
	if (!isValid()) {
		return 0;
	}
	if (isV4()) {
		const uint8_t* bytes = (length_ == 4 ? bytes_ : bytes_ + 12);
		return sprintf(buffer, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
	} else {
		bool ok = false;

		// Check for IPv6-compatible, IPv4-mapped, and IPv4-translated addresses.
		if (bytes_[0] == 0 && bytes_[1] == 0 && bytes_[2] == 0 && bytes_[3] == 0
			&& bytes_[4] == 0 && bytes_[5] == 0 && bytes_[6] == 0 && bytes_[7] == 0
			&& (bytes_[12] != 0 || bytes_[13] != 0)) {
			if (bytes_[8] == 0 && bytes_[9] == 0
				&& ((bytes_[10] == 0 && bytes_[11] == 0) || (bytes_[10] == 0xff && bytes_[11] == 0xff))) {
				// Compatible or mapped.
				sprintf(buffer, "::%s%u.%u.%u.%u", bytes_[10] == 0 ? "" : "ffff:",
					bytes_[12], bytes_[13], bytes_[14], bytes_[15]);
				ok = true;
			}
			else if (bytes_[8] == 0xff && bytes_[9] == 0xff && bytes_[10] == 0 && bytes_[11] == 0) {
				// Compatible or mapped.
				sprintf(buffer, "::ffff:0:%u.%u.%u.%u", bytes_[12], bytes_[13], bytes_[14], bytes_[15]);
				ok = true;
			}
		}
		if (!ok) {
			int	maxFirst = 0;
			int maxLast = 0;
			int curFirst = 0;
			int curLast = 0;
			for (int i = 0; i < 8; ++i) {
				if (bytes_[i * 2] == 0 && bytes_[i * 2 + 1] == 0) {
					// Extend current substring.
					curLast = i + 1;

					// Check if current is now largest.
					if (curLast - curFirst > maxLast - maxFirst) {
						maxFirst = curFirst;
						maxLast = curLast;
					}
				} else {
					// Start a new substring.
					curFirst = i + 1;
					curLast = i + 1;
				}
			}

			// Ignore a substring of length 1.
			if (maxLast - maxFirst <= 1) {
				maxFirst = maxLast = 0;
			}

			// Write colon-separated words.
			// A double-colon takes the place of the longest string of zeroes.
			// All zeroes is just "::".
			char* tmpBuffer = buffer;
			for (int i = 0; i < 8; ++i) {
				// Skip over string of zeroes.
				if (maxFirst <= i && i < maxLast) {
					tmpBuffer += sprintf(tmpBuffer, "::");
					i = maxLast - 1;
					continue;
				}

				// Need colon separator if not at beginning.
				if (i != 0 && i != maxLast) {
					*tmpBuffer++ = ':';
				}
				tmpBuffer += sprintf(tmpBuffer, "%x", (bytes_[i * 2] << 8) | bytes_[i * 2 + 1]);
			}
		}
		return static_cast<uint32_t>(strlen(buffer));
	}
}

// Parses an IP address. Returns true if OK.
bool IpAddress::parse(const char* buffer, uint32_t length) {
	
	// Consider the input string is not null-terminated.
	// Try IPv4 first.
	if (length >= 7 && length <= 15) {
		int n = 0;
		int byte = 0;
		bool ok = true;
		for (uint32_t i = 0; i < length; ++i) {
			char c = buffer[i];
			if (isdigit(c)) {
				n = n * 10 + static_cast<int>(c - '0');
				if (n > 255) {
					ok = false;
					break;
				}
			}
			if (c == '.' || i + 1 == length) {
				if (byte == 4) {
					ok = false;
					break;
				}
				bytes_[byte++] = n;
				n = 0;
			} else if (!isdigit(c)) {
				ok = false;
				break;
			}
		}
		if (ok && byte == 4) {
			length_ = 4;
			return true;
		}
	}

	// Not IPv4: try IPv6.
    enum
	{
		start,
		inNumber,
		afterDoubleColon
	} state = start;

	bool result = true;
    int number = -1;
    bool sawHex = false;
    int numColons = 0, numDots = 0;
    int sawDoubleColon = 0;
    int i = 0;
	uint32_t l = 0;
	while (l < length) {
		char c = buffer[l];
		switch (state) {
			case start:
				if (c == ':') {
					// This case only handles double-colon at the beginning.
					if (numDots > 0 || numColons > 0 || buffer[1] != ':') {
						goto finish;
					}
					sawDoubleColon = 1;
					numColons = 2;
					bytes_[i * 2] = 0;		// Pretend it was 0::
					bytes_[i * 2 + 1] = 0;
					i++;
					l++;
					state = afterDoubleColon;
					break;
				}
				[[fallthrough]];
			case afterDoubleColon:
				if (isdigit(c)) {
					sawHex = false;
					number = l;
					state = inNumber;
				} else if (isxdigit(c)) {
					if (numDots > 0) {
						goto finish;
					}
					sawHex = true;
					number = l;
					state = inNumber;
				} else {
					goto finish;
				}
				break;
			case inNumber:
				if (isdigit(c)) {
					// Remain in InNumber state.
				} else if (isxdigit(c)) {
					if (numDots > 0) {
						goto finish;
					}
					sawHex = true;
					// Remain in InNumber state.
				}
				else if (c == ':') {
					if (numDots > 0) {
						goto finish;
					}
					if (numColons > 6) {
						goto finish;
					}
					if (buffer[l + 1] == ':') {
						if (sawDoubleColon || numColons > 5) {
							goto finish;
						}
						sawDoubleColon = numColons + 1;
						numColons += 2;
						l++;
						state = afterDoubleColon;
					} else {
						numColons++;
						state = start;
					}
				}
				else if (c == '.') {
					if (sawHex || numDots > 2 || numColons > 6) {
						goto finish;
					}
					numDots++;
					state = start;
				} else {
					goto finish;
				}
				break;
		}
		// If we finished a number, parse it.
		if (state != inNumber && number != -1) {
			// Note either numDots > 0 or numColons > 0,
			// because something terminated the number.
			if (numDots == 0) {
				int n = parseHex(buffer + number, length - number);
				if (n == -1) {
					return false;
				}
				bytes_[i * 2] = (n >> 8) & 0xff;
				bytes_[i * 2 + 1] = n & 0xff;
				i++;
			} else {
				int n = parseInt(buffer + number, length - number);
				if (n == -1) {
					return false;
				}
				bytes_[2 * i + numDots-1] = static_cast<uint8_t>(n);
			}
		}
		l++;
	}

finish:

	// Check that we have a complete address.
	if (numDots == 0) {
	} else if (numDots == 3) {
		numColons++;
	} else {
		result = false;
	}
	if (result) {
		if (sawDoubleColon) {
		} else if (numColons == 7) {
		} else {
			result = false;
		}
		if (result) {
			// Parse the last number, if necessary.
			if (state == inNumber) {
				if (numDots == 0) {
					int n = parseHex(buffer + number, length - number);
					if (n == -1) {
						return false;
					}
					bytes_[i * 2] = (n >> 8) & 0xff;
					bytes_[i * 2 + 1] = n & 0xff;
				} else {
					int n = parseInt(buffer + number, length - number);
					if (n == -1) {
						return false;
					}
					bytes_[2 * i + numDots] = static_cast<uint8_t>(n);
				}
			} else if (state == afterDoubleColon) {
				bytes_[i * 2] = 0; // pretend it was ::0
				bytes_[i * 2 + 1] = 0;
			} else {
				result = false;
			}

			// Insert zeroes for the double-colon, if necessary.
			if (result && sawDoubleColon) {
				memmove(&bytes_[(sawDoubleColon + 8 - numColons) * 2],
					&bytes_[sawDoubleColon * 2], (numColons - sawDoubleColon) * 2);
				memset(&bytes_[sawDoubleColon * 2], 0, (8 - numColons) * 2);
			}
		}
	}
	return result;
}

// Helper methods to parse decimal (0..255) and hex numbers (0..ffff).
// They return -1 in case of error.
// STATIC
int IpAddress::parseInt(const char* buffer, uint32_t length) {
	int n = 0;
	bool hasDigit = false;
	for (uint32_t i = 0; i < length; ++i) {
		char c = buffer[i];
		if (isdigit(c)) {
			hasDigit = true;
			n = n * 10 + static_cast<int>(c - '0');
		} else {
			if (isspace(c)) {
				if (hasDigit) {
					break;
				}
			} else {
				break;
			}
		}
	}
	return (hasDigit && n <= 255) ? n : -1;
}

// STATIC
int IpAddress::parseHex(const char* buffer, uint32_t length) {
	int n = 0;
	bool hasDigit = false;
	for (uint32_t i = 0; i < length; ++i) {
		char c = buffer[i];
		if (isxdigit(c)) {
			hasDigit = true;
			int hex = c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c - '0';
			n = n * 16 + hex;
		} else {
			if (isspace(c)) {
				if (hasDigit) {
					break;
				}
			} else {
				break;
			}
		}
	}
	return (hasDigit && n <= 65535) ? n : -1;
}

bool IpAddress::applyMask(const IpAddress& mask) {
	if (isV4()) {
		if (!mask.isV4()) {
			return false;
		}
		uint8_t* bytes = (length_ == 4 ? bytes_ : bytes_ + 12);
		const uint8_t* mbytes = (mask.length_ == 4 ? mask.bytes_ : mask.bytes_ + 12);
		for (uint32_t i = 0; i < 4; ++i) {
			bytes[i] &= mbytes[i];
		}
		return true;
	} else {
		for (uint32_t i = 0; i < mask.length_; ++i) {
			bytes_[i] &= mask.bytes_[i];
		}
		return true;
	}
}

void IpAddress::makeMask(int bits) {
	memset(bytes_, 0, length_);
	int i = 0;
	while (bits > 0) {
		bytes_[i++] = bits >= 8 ? 0xff : (((1 << bits) - 1) << (7 - bits));
		bits -= 8;
	}
}

}
