#ifndef _spw_api_str_h_
#define _spw_api_str_h_

#include "memalloc.h"
#include "m_string.h"

#include "serial.h"
#include <ctype.h>


namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Str
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Simple string class, not optimized for performance.
class Str {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, Str& s);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const Str& s);

private:

	static const char* empty_;
	const char* s_;
	uint32_t owned_:1;
	uint32_t length_:31;

	void empty() {
		s_ = empty_;
		owned_ = false;
		length_ = 0;
	}

	void clear() {
		if (owned_) {
			my_free(const_cast<char*>(s_));
		}
		s_ = NULL;
	}

public:

	Str() {
		empty();
	}

	explicit Str(const char* s, bool owned = true) {
		if ( s == NULL || s[0] == '\0' ) {
			empty();
		} else {
			if (owned) {
				s_ = static_cast<const char*>(my_strdup(s, MYF(MY_FAE)));
				owned_ = true;
			} else {
				s_ = s;
				owned_ = false;
			}
			length_ = static_cast<uint32_t>(strlen(s_));
		}
	}

	explicit Str(const char* s, int length) {
		if ( s == NULL || length == 0 ) {
			empty();
		} else {
			s_ = static_cast<const char*>(my_strndup(s, length, MYF(MY_FAE)));
			owned_ = true;
			length_ = static_cast<uint32_t>(strlen(s_));
		}
	}

	Str(const Str& s) {
		if ( s.length_ == 0 ) {
			empty();
		} else {
			s_ = static_cast<const char*>(my_strdup(s.s_, MYF(MY_FAE)));
			owned_ = true;
			length_ = static_cast<uint32_t>(strlen(s_));
		}
	}

	Str& operator = (const Str& s) {
		if (this == &s) {
			return *this;
		}
		clear();
		if ( s.length_ == 0 ) {
			empty();
		} else {
			s_ = static_cast<char*>(my_strdup(s.s_, MYF(MY_FAE)));
			owned_ = true;
			length_ = static_cast<uint32_t>(strlen(s_));
		}
		return *this;
	}

	Str& operator = (const char* s) {
		clear();
		if ( s == NULL || s[0] == '\0' ) {
			empty();
		} else {
			s_ = static_cast<const char*>(my_strdup(s, MYF(MY_FAE)));
			owned_ = true;
			length_ = static_cast<uint32_t>(strlen(s_));
		}
		return *this;
	}

	~Str() {
		clear();
	}

	int length() const {
		return static_cast<int>(length_);
	}

	const char* c_str() const {
		return s_;
	}

	bool isOwned() const {
		return owned_;
	}

	int compareTo(const Str& s, const bool caseInsensitive) const {
		if (caseInsensitive) {
			return native_strcasecmp(s_, s.s_);
		} else {
			return strcmp(s_, s.s_);
		}
	}

	bool startsWith(const Str& s, const bool caseInsensitive) const {
		if (caseInsensitive) {
			return native_strncasecmp(s_, s.s_, s.length()) == 0;
		} else {
			return strncmp(s_, s.s_, s.length()) == 0;
		}
	}

	bool operator == (const Str& s) const {
		return length() == s.length() && compareTo(s, false) == 0;
	}

	bool operator != (const Str& s) const {
		return !(*this == s);
	}

	bool operator < (const Str& s) const {
		return compareTo(s, false) < 0;
	}

	void toLower() {
		if (owned_) {
			for (int i = 0; i < length(); ++i) {
				const_cast<char*>(s_)[i] = tolower(s_[i]);
			}
		} else {
			Str s(*this);
			s.toLower();
			*this = s;
		}
	}

	Str& operator += (const Str& s) {
		if (s.length() == 0) {
			return *this;
		} else if (length() == 0) {
			*this = s;
			return *this;
		}
		int l = length();
		int sl = s.length();
		char* ns = static_cast<char*>(my_malloc(l + sl + 1, MYF(MY_FAE)));
		memcpy(ns, s_, l);
		memcpy(ns + l, s.s_, sl + 1);
		clear();
		s_ = ns;
		owned_ = true;
		length_ = static_cast<uint32_t>(strlen(s_));
		return *this;
	}

	Str& operator += (const char* s) {
		SPW_ASSERT(s != 0);
		if (strlen(s) == 0) {
			return *this;
		} else if (length() == 0) {
			*this = s;
			return *this;
		}
		int l = length();
		size_t sl = strlen(s);
		char* ns = static_cast<char*>(my_malloc(l + sl + 1, MYF(MY_FAE)));
		memcpy(ns, s_, l);
		memcpy(ns + l, s, sl + 1);
		clear();
		s_ = ns;
		owned_ = true;
		length_ = static_cast<uint32_t>(strlen(s_));
		return *this;
	}

	uint32_t hash() const {
		uint32_t h = 1;
		int off = 0;
		for (;;) {
			// Hash is case insensitive.
			const uint8_t v = static_cast<uint8_t>(tolower(s_[off]));
			if (v == 0) {
				break;
			}
			++off;
			h = 31 * h + v;
		}
        return h;
	}

	// Timestamp is in milliseconds.
	static Str fromTimestamp(const uint64_t timestamp) {
		const uint32_t milliseconds = timestamp % 1000;
		time_t tt = static_cast<time_t>(timestamp / 1000);
		struct tm *t;
		t = localtime(&tt);
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%04d/%02d/%02d %2d:%02d:%02d.%03u", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec, milliseconds);
		return Str(buffer);
	}

	// Duration is in milliseconds.
	static Str fromDuration(const uint64_t duration) {
		char buffer[128];
		const uint32_t milliseconds = static_cast<uint32_t>(duration % 1000);
		if (duration < 1000) {
			snprintf(buffer, sizeof(buffer), "%ums", milliseconds);
		} else if (duration < 60000) {
			if (milliseconds == 0) {
				snprintf(buffer, sizeof(buffer), "%us", static_cast<uint>(duration / 1000));
			} else {
				snprintf(buffer, sizeof(buffer), "%us%03ums", static_cast<uint>(duration / 1000), milliseconds);
			}
		} else if (duration < 3600000) {
			snprintf(buffer, sizeof(buffer), "%um", static_cast<uint>(duration / 60000));
		} else if (duration < 86400000) {
			const uint minutes =  static_cast<uint>((duration % 3600000) / 60000);
			if (minutes == 0) {
				snprintf(buffer, sizeof(buffer), "%uh", static_cast<uint>(duration / 3600000));
			} else {
				snprintf(buffer, sizeof(buffer), "%uh%um", static_cast<uint>(duration / 3600000), minutes);
			}
		} else {
			const uint hours = static_cast<uint>((duration % 86400000) / 3600000);
			if (hours == 0) {
				snprintf(buffer, sizeof(buffer), "%ud", static_cast<uint>(duration / 86400000));
			} else {
				snprintf(buffer, sizeof(buffer), "%ud%uh", static_cast<uint>(duration / 86400000), hours);
			}
		}
		return Str(buffer);
	}

	// Size is in bytes.
	static Str fromSize(const uint64_t size) {
		char buffer[128];
		if (size < static_cast<uint64_t>(1024)) {
			snprintf(buffer, sizeof(buffer), "%llu", static_cast<ulonglong>(size));
		} else if (size < static_cast<uint64_t>(1024) * 1024) {
			snprintf(buffer, sizeof(buffer), "%llu KB", static_cast<ulonglong>(size / 1024));
		} else if (size < static_cast<uint64_t>(1024) * 1024 * 1024) {
			snprintf(buffer, sizeof(buffer), "%llu MB", static_cast<ulonglong>(size / 1024 / 1024));
		} else if (size < static_cast<uint64_t>(1024) * 1024 * 1024 * 1024) {
			snprintf(buffer, sizeof(buffer), "%.1f GB", static_cast<double>(size) / 1024 / 1024 / 1024);
		} else {
			snprintf(buffer, sizeof(buffer), "%.1f TB", static_cast<double>(size) / 1024 / 1024 / 1024 / 1024);
		}
		return Str(buffer);
	}
};

inline ByteBuffer& operator >> (ByteBuffer& buffer, Str& s) {
	int length;
	buffer >> length;
	s.clear();
	s.s_ = static_cast<char*>(my_malloc(length + 1, MYF(MY_FAE)));
	s.owned_ = true;
	ByteBuffer contents(reinterpret_cast<const uint8_t*>(s.s_), length);
	buffer >> contents;
	const_cast<char*>(s.s_)[length] = 0;
	s.length_ = length;
	return buffer;
}

inline ByteBuffer& operator << (ByteBuffer& buffer, const Str& s) {
	int length = s.length();
	buffer << length << ByteBuffer(reinterpret_cast<const uint8_t*>(s.c_str()), length);
	return buffer;
}

inline static Str operator + (const Str& left, const Str& right) {
	Str s = left;
	s += right;
	return s;
}

}

#endif /* #ifndef _spw_api_str_h_ */
