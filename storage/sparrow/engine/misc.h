/*
	Miscellaneous types.
*/

#ifndef _engine_misc_h_
#define _engine_misc_h_

//#include <sql_priv.h>
#include "sql/query_options.h"		// For mysqld options.
#include "atomic.h"
#include "serial.h"
#include <ctype.h>
#include "my_dbug.h"
#include "m_ctype.h"
#include "m_string.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CodeStateGuard and debug macros compatible with exceptions
//////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG
#define SPARROW_ENTER(a)
#define DBUG_LOCK
#else
class CodeStateGuard {
private:

	const char* file_;
	const uint line_;
	struct _db_stack_frame_ stackFrame_;
	bool valid_;

public:

	static Lock lock_;

public:

	CodeStateGuard(const char* func, const char* file, const uint line) : file_(file), line_(line), valid_(true) {
		_db_enter_(func, ::strlen(func), file, line, &stackFrame_);
	}

	void reset() {
		if (valid_) {
			valid_ = false;
			_db_return_(line_, &stackFrame_);
		}
	}

	~CodeStateGuard() {
		reset();
	}
};
#define SPARROW_ENTER(a) \
	CodeStateGuard _csGuard(a, __FILE__, __LINE__); \
	do { \
	} while(0)

// Macro to avoid overlapping log output.
#define DBUG_LOCK \
	Guard _debugGuard(CodeStateGuard::lock_); \
	do { \
	} while(0)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AutoPtr
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This  class  holds  a  pointer  and  automatically releases it when  it goes
// out-of-scope. Similar to std::auto_ptr, without the evil owner bit.
template<typename T, bool ARRAY = false>
class AutoPtr {
private:

    T*  ptr_;   // The pointer we encapsulate.

    AutoPtr(const AutoPtr&);
    AutoPtr& operator=(const AutoPtr&);

    void reset() {
		if (ptr_ != 0) {
			if (!ARRAY)	{
				delete ptr_;
			}
			else {
				delete [] ptr_;
			}
			ptr_ = 0;
		}
	}

public:
    // Construction / destruction
    AutoPtr() : ptr_ (0) {
	}
    explicit AutoPtr(T* ptr) : ptr_ (ptr) {
		assert(ptr_ != 0);
	}
    ~AutoPtr() {
		reset();
	}

    // Pointer-like operators
    //
    // NOTE: While it may be very tempting to write an automatic conversion operator
    // to T* here, it is usually considered an evil thing and may cause very subtle
    // glitches that can make your life truly miserable, so it's better to use get().
    AutoPtr& operator=(T* ptr) {
		reset();
		ptr_ = ptr;
		return *this;
	}

    bool operator==(const T* ptr) const {
	    return ptr_ == ptr;
	}
    bool operator!=(const T* ptr) const {
		return ptr_ != ptr;
	}
    T& operator*() {
		assert(ptr_ != 0);
		return *ptr_;
	}
    T* operator->() {
		assert(ptr_ != 0);
		return ptr_;
	}
    T& operator[](const uint32_t index) {
		assert(ptr_ != 0);
		return ptr_[index];
	}
    T* get() {
		return ptr_;
	}

	//
	// Releases  the   pointer  we're   holding,  meaning  the  the  caller becomes
	// responsible of it's deletion. The released pointer is returned.
	//
    T* release() {
		assert(ptr_ != 0);
		T* ptr = ptr_;
		ptr_   = 0;
		return ptr;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RefCounter
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Atomic reference counter.
class RefCounter {
public:

	RefCounter(const uint32_t n = 0): refs_ (n) { ; }

	void acquire() { Atomic::inc32(&refs_); }
	bool release() { return Atomic::dec32(&refs_) == 0; }

	uint32_t refs() const { return refs_; }

	// prefix
	uint32_t operator ++ () { return Atomic::inc32(&refs_); }
	uint32_t operator -- () { return Atomic::dec32(&refs_); }

	// postfix
	uint32_t operator ++ (int) { return Atomic::inc32(&refs_) - 1; }
	uint32_t operator -- (int) { return Atomic::dec32(&refs_) + 1; }

	// add/sub
	RefCounter& operator += (const int v) { Atomic::add32(&refs_, v); return *this; }
	RefCounter& operator -= (const int v) { Atomic::add32(&refs_, -v); return *this; }

	// reset/set
	void reset(const uint32_t n = 0) { refs_ = n; }
	RefCounter& operator = (const uint32_t n) { refs_ = n; return *this; }

private:

	volatile uint32_t refs_;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RefCounted
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Base class that provides a reference counting mechanism.
class RefCounted {
private:

    mutable RefCounter count_;  // The current reference count.

public:
    RefCounted() : count_ (0) {
	}

    // Copy constructor. Sets the count to 0.
    RefCounted(const RefCounted&) : count_ (0) {
	}

    // Destructor. Ensures nobody still holds a reference!
    virtual ~RefCounted() {
		assert(count_.refs () == 0);
	}

	void acquireRef() {
		++count_;
	}

	bool releaseRef() {
		return --count_ == 0;
	}

	void resetRef(uint32_t n) {
		count_.reset(n);
	}

	uint32_t refs() const {
		return count_.refs();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// RefPtr
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Smart  pointer class  that automatically  takes and  releases a reference on
// the object it holds. Usable with any kind of reference counted object.
//
// In order to be usable with this class, T must inherit from RefCounted.
template<typename T> class RefPtr {
private:

	T*      ptr_;   // The pointer we hold.

    // Takes a reference if we're holding a pointer.
    void connect() {
	    if (ptr_ != 0) {
			ptr_->acquireRef();
	    }
	}

	// Releases the reference on the object we may hold.
    void disconnect() {
		if (ptr_ != 0 && ptr_->releaseRef()) {
			delete ptr_;
		}
	}

public:

    RefPtr() : ptr_ (0) {
	}

    explicit RefPtr(T* ptr) : ptr_ (ptr) {
		connect ();
	}

    // Copy constructor.
    RefPtr(const RefPtr& ptr) : ptr_ (ptr.ptr_) {
		connect ();
	}

    // Copy constructor from other refptr types
    template<typename TT> RefPtr(const RefPtr<TT>& ptr) : ptr_ (ptr.get ()) {
		connect ();
	}

    // Destructor.
    ~RefPtr() {
		disconnect ();
	}

    // Returns the pointer we encapsulate.
    T* get() const {
		return ptr_;
	}
    T& operator*() const {
		assert(ptr_ != 0);
		return *ptr_;
	}
    T* operator->() const {
		assert(ptr_ != 0);
		return ptr_;
	}
    operator T*() const {
		return ptr_;
	}

    // Automatic conversion to other refptr types
    template<typename TT> operator RefPtr<TT>() const {
		return RefPtr<TT>(ptr_);
	}

    // Assignment operator.
    RefPtr& operator = (T* ptr) {
		if (ptr_ == ptr) {
			return *this;
		}
		disconnect ();
		ptr_ = ptr;
		connect ();
		return *this;
	}

    // Assignment operator.
    RefPtr& operator = (const RefPtr& ptr) {
		if (this == &ptr || ptr_ == ptr.ptr_) {
			return *this;
		}
		disconnect ();
		ptr_ = ptr.ptr_;
		connect ();
		return *this;
	}

    // Comparison operators.
    bool operator < (const RefPtr& ptr) const {
		if (ptr_ == 0) {
			return ptr.ptr_ != 0;
		}
		if (ptr.ptr_ == 0) {
			return false;
		}
		if (ptr_ == ptr.ptr_) {
			return false;
		}
		return *ptr_ < *ptr.ptr_;
	}
    bool operator == (const RefPtr& ptr) const {
		if (ptr_ == 0) {
			return ptr.ptr_ == 0;
		}
		if (ptr.ptr_ == 0) {
			return false;
		}
		if (ptr_ == ptr.ptr_) {
			return true;
		}
		return *ptr_ == *ptr.ptr_;
	}

	void reset() {
		ptr_ = 0;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Pair
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T, typename V> class Pair {
private:

	T t_;
	V v_;

public:

    Pair() {
	}

	Pair(const T& t, const V& v) : t_(t), v_(v) {
	}

	Pair& operator = (const Pair& right) {
		if (this != &right) {
			t_ = right.t_;
			v_ = right.v_;
		}
		return *this;
	}

	Pair(const Pair& right) {
		*this = right;
	}

	bool operator == (const Pair& right) const {
		if (this != &right) {
			return t_ == right.t_ && v_ == right.v_;
		} else {
			return true;
		}
	}

	bool operator < (const Pair& right) const {
		if (t_ < right.t_) {
			return true;
		} else if (right.t_ < t_) {
			return false;
		} else {
			return v_ < right.v_;
		}
	}

	const T& getFirst() const {
		return t_;
	}

	const V& getSecond() const {
		return v_;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Str
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Simple string class, not optimized for performance.
typedef Interval<uint64_t> TimePeriod;
class Str {
	friend ByteBuffer& operator >> (ByteBuffer& buffer, Str& s);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const Str& s);

private:

	static const char* empty_;
	const char* s_;
	uint32_t owned_:1;
	uint32_t length_:31;

	void clear() {
		if (owned_) {
			my_free(const_cast<char*>(s_));
		}
		s_ = 0;
	}

public:

	Str() {
		s_ = empty_;
		owned_ = false;
		length_ = 0;
	}

	explicit Str(const char* s, bool owned = true) {
		assert(s != 0);
		if (owned) {
			s_ = static_cast<const char*>(my_strdup(PSI_INSTRUMENT_ME, s, MYF(MY_FAE)));
			owned_ = true;
		} else {
			s_ = s;
			owned_ = false;
		}
		length_ = static_cast<uint32_t>(strlen(s_));
	}

	explicit Str(const char* s, int length) {
		if (length == 0) {
			s_ = empty_;
			owned_ = false;
			length_ = 0;
		} else {
			assert(s != 0);
            s_ = static_cast<const char *>(my_strndup(PSI_INSTRUMENT_ME, s, length, MYF(MY_FAE)));
			owned_ = true;
			length_ = static_cast<uint32_t>(strlen(s_));
		}
	}

	Str(const Str& s) {
		if (s.s_ == empty_) {
			s_ = empty_;
			owned_ = false;
		} else {
			s_ = static_cast<const char*>(my_strdup(PSI_INSTRUMENT_ME, s.s_, MYF(MY_FAE)));
			owned_ = true;
		}
		length_ = static_cast<uint32_t>(strlen(s_));
	}

	Str& operator = (const Str& s) {
		if (this == &s) {
			return *this;
		}
		clear();
		if (s.s_ == empty_) {
			s_ = empty_;
			owned_ = false;
		} else {
			s_ = static_cast<char*>(my_strdup(PSI_INSTRUMENT_ME, s.s_, MYF(MY_FAE)));
			owned_ = true;
		}
		length_ = static_cast<uint32_t>(strlen(s_));
		return *this;
	}

	~Str() {
		clear();
	}

	int length() const {
		return static_cast<int>(length_);
	}

	bool isEmpty() const {
		return length() == 0;
	}

	const char* c_str() const {
		return s_;
	}

	bool isOwned() const {
		return owned_;
	}

	int compareTo(const Str& s, const bool caseInsensitive) const {
		if (caseInsensitive) {
			return my_strcasecmp(system_charset_info, s_, s.s_);
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
		char* ns = static_cast<char*>(my_malloc(PSI_INSTRUMENT_ME, l + sl + 1, MYF(MY_WME)));
		memcpy(ns, s_, l);
		memcpy(ns + l, s.s_, sl + 1);
		clear();
		s_ = ns;
		owned_ = true;
		length_ = static_cast<uint32_t>(strlen(s_));
		return *this;
	}

	uint32_t hash() const {
		uint32_t h = 1;
		int off = 0;
		for ( ; ; ) {
			// Hash is case insensitive.
			uint8_t v = static_cast<uint8_t>(tolower(s_[off++]));
			if (v == 0) {
				break;
			}
			h = 31 * h + v;
		}
        return h;
	}

	// Timestamp is in milliseconds.
	static Str fromTimestamp(const uint64_t timestamp);

	static Str fromTimePeriod(const TimePeriod& period);

	// Duration is in milliseconds.
	static Str fromDuration(const uint64_t duration);

	// Size is in bytes.
	static Str fromSize(const uint64_t size);
};

inline ByteBuffer& operator >> (ByteBuffer& buffer, Str& s) {
	int length;
	buffer >> length;
	s.clear();
	s.s_ = static_cast<char*>(my_malloc(PSI_INSTRUMENT_ME, length + 1, MYF(MY_WME)));
	s.owned_ = true;
	s.length_ = length;
	ByteBuffer contents(reinterpret_cast<const uint8_t*>(s.s_), length);
	buffer >> contents;
	const_cast<char*>(s.s_)[length] = 0;
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

#endif /* #ifndef _engine_misc_h_ */
