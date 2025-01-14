/*
	Miscellaneous types.
*/

#ifndef _spw_api_misc_h_
#define _spw_api_misc_h_

#include "atomic.h"
//#include "serial.h"
//#include <ctype.h>
#include "api_assert.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AutoPtr
//////////////////////////////////////////////////////////////////////////////////////////////////////

// This  class  holds  a  pointer  and  automatically releases it when  it goes
// out-of-scope. Similar to std::auto_ptr, without the evil owner bit.
template<typename T, bool ARRAY = false> class AutoPtr {
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
		SPW_ASSERT(ptr_ != 0);
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
		SPW_ASSERT(ptr_ != 0);
		return *ptr_;
	}
    T* operator->() {
		SPW_ASSERT(ptr_ != 0);
		return ptr_;
	}
    T& operator[](const uint32_t index) {
		SPW_ASSERT(ptr_ != 0);
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
		SPW_ASSERT(ptr_ != 0);
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
		SPW_ASSERT(count_.refs () == 0);
	}

	RefCounted& operator = (const RefCounted&) = default;

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
		SPW_ASSERT(ptr_ != 0);
		return *ptr_;
	}
    T* operator->() const {
		SPW_ASSERT(ptr_ != 0);
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


}

#endif /* #ifndef _spw_api_misc_h_ */
