/*
 Vector types.
 */

#ifndef _engine_vec_h_
#define _engine_vec_h_

#include <assert.h>

#include "list.h"

namespace Sparrow {

// Constant for "not found".
#ifndef SYS_NPOS
#define SYS_NPOS (~(static_cast<uint32_t>(0)))
#endif

//
// Default allocator for vectors
//
template<class T> class SYSallocator {
private:

	uint32_t capacity_;

public:

	SYSallocator() : capacity_(0) {
	}
	uint32_t getCount() const;
	void resetCount();
	T* build(uint32_t n);
	void destroy(T* p);
};

template<class T> inline uint32_t SYSallocator<T>::getCount() const {
	return capacity_;
}

template<class T> inline void SYSallocator<T>::resetCount() {
	capacity_ = 0;
}

template<class T> inline T* SYSallocator<T>::build(uint32_t n) {
	capacity_ = n;
	return new T[n];
}

template<class T> inline void SYSallocator<T>::destroy(T* p) {
	delete[] p;
}

//
// SYSarray: simple array
//
template<class T, class A = SYSallocator<T> > class SYSarray : public A {
public:

	SYSarray<T, A>(const uint32_t size = 0);
	SYSarray<T, A>(const uint32_t size, const T& init);
	SYSarray<T, A>(const SYSarray<T, A>& right);
	~SYSarray<T, A>();

	// accessors
	uint32_t length() const;
	const T& operator [](const uint32_t index) const;
	T& operator [](const uint32_t index);
	const T* data() const {
		return array_;
	}
	T* data() {
		return array_;
	}

	// operations
	void clear();
	void reshape(const uint32_t n, const bool doCopy = true);

	// operators
	SYSarray<T, A>& operator =(const SYSarray<T, A>& right);
	bool operator ==(const SYSarray<T, A>& right) const;

protected:

	void copy(T* destination, const T* source, uint32_t n);

protected:

	T* array_;
};

template<class T, class A> inline void SYSarray<T, A>::copy(T* destination, const T* source, uint32_t n) {
	assert(source != destination && n > 0);

	// in case arrays overlap
	if (destination < source) {
		while (n-- > 0) {
			*destination++ = *source++;
		}
	} else {
		destination += n;
		source += n;
		while (n-- > 0) {
			*--destination = *--source;
		}
	}
}

template<class T, class A> inline uint32_t SYSarray<T, A>::length() const {
	return (array_ == 0 ? 0 : this->getCount());
}

template<class T, class A> inline void SYSarray<T, A>::reshape(const uint32_t n, const bool doCopy /* = true */) {
	const uint32_t l = length();
	if (n == 0) {
		if (array_ != 0) {
			this->destroy(array_);
			this->resetCount();
		}
		array_ = 0;
	} else if (n != l) {
		T* newArray = this->build(n);
		if (array_ != 0) {
			if (doCopy && l > 0) {
				copy(newArray, array_, l > n ? n : l);
			}
			this->destroy(array_);
		}
		array_ = newArray;
	}
}

// constructors
template<class T, class A> inline SYSarray<T, A>::SYSarray(const uint32_t size /* = 0 */) : A() {
	array_ = 0;
	reshape(size, false);
}

template<class T, class A> inline SYSarray<T, A>::SYSarray(const uint32_t size, const T& init) : A() {
	array_ = 0;
	reshape(size, false);
	for (uint32_t i = 0; i < size; ++i) {
		array_[i] = init;
	}
}

template<class T, class A> inline const T& SYSarray<T, A>::operator [](const uint32_t index) const {
	assert(index < length());
	return array_[index];
}

template<class T, class A> inline T& SYSarray<T, A>::operator [](const uint32_t index) {
	assert(index < length());
	return array_[index];
}

template<class T, class A> inline void SYSarray<T, A>::clear() {
	if (array_ != 0) {
		this->destroy(array_);
		this->resetCount();
		array_ = 0;
	}
}

template<class T, class A> inline SYSarray<T, A>::~SYSarray() {
	clear();
}

template<class T, class A> inline SYSarray<T, A>& SYSarray<T, A>::operator =(const SYSarray<T, A>& right) {
	if (this == &right) {
		return *this;
	}
	const uint32_t l = right.length();
	reshape(l, false);
	if (l > 0) {
		copy(array_, right.array_, l);
	}
	return *this;
}

template<class T, class A> inline bool SYSarray<T, A>::operator ==(const SYSarray<T, A>& right) const {
	if (length() != right.length()) {
		return false;
	}
	for (uint32_t i = 0; i < length(); ++i) {
		if (!((*this)[i] == right[i])) {
			return false;
		}
	}
	return true;
}

// copy constructor
template<class T, class A> inline SYSarray<T, A>::SYSarray(const SYSarray<T, A>& right) : A() {
	array_ = 0;
	*this = right;
}

//
// SYSvector: simple vector
// note: SYSvector inherits from the allocator to perform empty base optimization (EBO)
//
template<class T, uint32_t G = 0, class A = SYSallocator<T> > class SYSvector: public A {
public:

	SYSvector<T, G, A>(const uint32_t size = 0);
	SYSvector<T, G, A>(const SYSvector<T, G, A>& right);
	~SYSvector<T, G, A>();

	// accessors
	uint32_t entries() const;
	bool isEmpty() const;
	uint32_t length() const;
	uint32_t capacity() const;
	const T& operator [](const uint32_t index) const;
	T& operator [](const uint32_t index);
	const T& first() const;
	T& first();
	const T& last() const;
	T& last();
	uint32_t index(const T& t) const;
	bool contains(const T& t) const;
	const T* data() const;

	// operations
	void insertAt(const uint32_t index, const T& t);
	void removeAt(const uint32_t index);
	void removeFirst();
	void removeLast();
	bool remove(const T& t);
	void append(const T& t);
	void append(const SYSvector<T, G, A>& right);
	void insert(const T& t);
	void resize(const uint32_t n, const bool canShrink = false, const bool doCopy = true);
	void reshape(const uint32_t n, const bool doCopy = true);
	void clear();
	void forceLength(const uint32_t length) {
		if (length <= capacity()) {
			n_ = length;
		}
	}
	bool contains(const SYSvector<T, G, A>& vect) const;
	bool containsTheSame(const SYSvector<T, G, A>& vect) const;

	// operators
	SYSvector<T, G, A>& operator =(const SYSvector<T, G, A>& right);
	bool operator ==(const SYSvector<T, G, A>& right) const;

protected:

	void copy(T* destination, const T* source, uint32_t n);

protected:

	T* array_;
	uint32_t n_;
};

// Returns true if this all the items from this vector also exist in the the vector passed as argument.
template<class T, uint32_t G, class A> inline bool SYSvector<T, G, A>::contains(const SYSvector<T, G, A>& vect) const {
	for (uint32_t i = 0; i < length(); ++i) {
		if (!(vect.contains((*this)[i]))) {
			return false;
		}
	}
	return true;
}

// Returns true if the two vectors contain the same items, independent of their position in the vector.
template<class T, uint32_t G, class A> inline bool SYSvector<T, G, A>::containsTheSame(const SYSvector<T, G, A>& vect) const {
	if (length() != vect.length()) {
		return false;
	}
	return contains(vect);
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::copy(T* destination, const T* source, uint32_t n) {
	assert(source != destination);

	// in case arrays overlap
	if (destination < source) {
		while (n-- > 0) {
			*destination++ = *source++;
		}
	} else {
		destination += n;
		source += n;
		while (n-- > 0) {
			*--destination = *--source;
		}
	}
}

template<class T, uint32_t G, class A> inline uint32_t SYSvector<T, G, A>::capacity() const {
	return (array_ == 0 ? 0 : this->getCount());
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::resize(const uint32_t n, bool canShrink /* = false */, bool doCopy /* = true */) {
	// cannot shrink under the number of elements, unless specified
	if (!canShrink && n < n_)
		return;
	if (n == 0) {
		if (array_ != 0) {
			this->destroy(array_);
			this->resetCount();
		}
		array_ = 0;
	} else if (n != capacity()) {
		T* newArray = this->build(n);
		if (array_ != 0) {
			if (doCopy && n_ > 0) {
				copy(newArray, array_, n_ > n ? n : n_);
			}
			this->destroy(array_);
		}
		array_ = newArray;
	}
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::reshape(const uint32_t n, const bool doCopy /* = true */) {
	// shrinking allowed
	resize(n, true, doCopy);
	n_ = n;
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::insertAt(const uint32_t index, const T& t) {
	assert(index <= n_);
	uint32_t size = capacity();
	T* dest = array_;
	assert(size >= n_);
	if (size == n_) {
		T* newArray = this->build(n_ + (G == 0 ? 1 : G));
		dest = newArray;
		if (index > 0) {
			copy(dest, array_, index);
		}
	}
	if (n_ > index) {
		copy(dest + index + 1, array_ + index, n_ - index);
	}
	dest[index] = t;
	n_++;
	if (dest != array_) {
		if (array_ != 0) {
			this->destroy(array_);
		}
		array_ = dest;
	}
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::append(const T& t) {
	this->insertAt(n_, t);
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::append(const SYSvector<T, G, A>& right) {
	resize(this->entries() + right.entries());
	for (uint i=0; i<right.entries(); ++i) {
		append(right[i]);
	}
}


template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::insert(const T& t) {
	this->insertAt(n_, t);
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::removeAt(const uint32_t index) {
	assert(index < n_);
	if (n_ == 1 && G == 0) {
		this->destroy(array_);
		this->resetCount();
		array_ = 0;
		n_ = 0;
	} else {
		T* dest = array_;
		if (G == 0) {
			T* newArray = this->build(n_ - 1);
			dest = newArray;
			if (index > 0) {
				copy(dest, array_, index);
			}
		}
		n_--;
		if (n_ > index) {
			copy(dest + index, array_ + index + 1, n_ - index);
		}
		if (dest != array_) {
			this->destroy(array_);
			array_ = dest;
		} else {
			array_[n_] = T();
		}
	}
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::removeFirst() {
	removeAt(0);
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::removeLast() {
	assert(n_ > 0);
	removeAt(n_ - 1);
}

// constructor
template<class T, uint32_t G, class A> inline SYSvector<T, G, A>::SYSvector(const uint32_t size /* = 0 */) {
	array_ = 0;
	n_ = 0;
	resize(size);
}

template<class T, uint32_t G, class A> inline uint32_t SYSvector<T, G, A>::entries() const {
	return n_;
}

template<class T, uint32_t G, class A> inline bool SYSvector<T, G, A>::isEmpty() const {
	return n_ == 0;
}

template<class T, uint32_t G, class A> inline uint32_t SYSvector<T, G, A>::length() const {
	return n_;
}

template<class T, uint32_t G, class A> inline const T& SYSvector<T, G, A>::operator [](const uint32_t index) const {
	assert(index < n_);
	return array_[index];
}

template<class T, uint32_t G, class A> inline T& SYSvector<T, G, A>::operator [](const uint32_t index) {
	assert(index < n_);
	return array_[index];
}

template<class T, uint32_t G, class A> inline void SYSvector<T, G, A>::clear() {
	if (array_ != 0) {
		this->destroy(array_);
		this->resetCount();
		array_ = 0;
		n_ = 0;
	}
}

template<class T, uint32_t G, class A> inline SYSvector<T, G, A>::~SYSvector() {
	clear();
}

template<class T, uint32_t G, class A> inline SYSvector<T, G, A>& SYSvector<T, G,	A>::operator =(const SYSvector<T, G, A>& right) {
	if (this == &right) {
		return *this;
	}
	clear();
	resize(right.capacity());
	const uint32_t l = right.length();
	if (l > 0) {
		copy(array_, right.array_, l);
	}
	n_ = l;
	return *this;
}

template<class T, uint32_t G, class A> inline bool SYSvector<T, G, A>::operator ==(const SYSvector<T, G, A>& right) const {
	if (length() != right.length()) {
		return false;
	}
	for (uint32_t i = 0; i < length(); ++i) {
		if (!((*this)[i] == right[i])) {
			return false;
		}
	}
	return true;
}

// copy constructor
template<class T, uint32_t G, class A> inline SYSvector<T, G, A>::SYSvector(const SYSvector<T, G, A>& right) : A() {
	array_ = 0;
	n_ = 0;
	*this = right;
}

template<class T, uint32_t G, class A> inline const T& SYSvector<T, G, A>::first() const {
	return (*this)[0];
}

template<class T, uint32_t G, class A> inline T& SYSvector<T, G, A>::first() {
	return (*this)[0];
}

template<class T, uint32_t G, class A> inline const T& SYSvector<T, G, A>::last() const {
	assert(n_ > 0);
	return (*this)[n_ - 1];
}

template<class T, uint32_t G, class A> inline T& SYSvector<T, G, A>::last() {
	assert(n_ > 0);
	return (*this)[n_ - 1];
}

template<class T, uint32_t G, class A> inline uint32_t SYSvector<T, G, A>::index(const T& t) const {
	for (uint32_t i = 0; i < n_; ++i) {
		if (array_[i] == t) {
			return i;
		}
	}
	return SYS_NPOS;
}

template<class T, uint32_t G, class A> inline bool SYSvector<T, G, A>::remove(const T& t) {
	const uint32_t i = index(t);
	const bool found = (i != SYS_NPOS);
	if (found) {
		removeAt(i);
	}
	return found;
}

template<class T, uint32_t G, class A> inline bool SYSvector<T, G, A>::contains(const T& t) const {
	return (index(t) != SYS_NPOS);
}

template<class T, uint32_t G, class A> inline const T* SYSvector<T, G, A>::data() const {
	return array_;
}

//
// SYSpVector: vector of pointers
//
template<class T, uint32_t G, class A = SYSallocator<T*> > class SYSpVector: public SYSvector<T*, G, A> {
public:

	SYSpVector<T, G, A>(const uint32_t size = 0) : SYSvector<T*, G, A>(size) {
	}

	// accessors
	T* find(const T* t) const;
	uint32_t index(const T* t) const;
	bool contains(const T* t) const;

	// operations
	T* remove(const T* t);
	void clearAndDestroy();

	// operators
	bool operator ==(const SYSpVector<T, G, A>& right) const;
};

template<class T, uint32_t G, class A> inline uint32_t SYSpVector<T, G, A>::index(const T* t) const {
	for (uint32_t i = 0; i < this->length(); ++i) {
		T* v = (*this)[i];
		if (*v == *t) {
			return i;
		}
	}
	return SYS_NPOS;
}

template<class T, uint32_t G, class A> inline T* SYSpVector<T, G, A>::find(const T* t) const {
	const uint32_t i = index(t);
	if (i == SYS_NPOS) {
		return 0;
	} else {
		return (*this)[i];
	}
}

template<class T, uint32_t G, class A> inline bool SYSpVector<T, G, A>::contains(const T* t) const {
	return (index(t) != SYS_NPOS);
}

template<class T, uint32_t G, class A> inline T* SYSpVector<T, G, A>::remove(const T* t) {
	T* result = 0;
	for (uint32_t i = 0; i < this->length(); ++i) {
		T* v = (*this)[i];
		if (*v == *t) {
			result = (*this)[i];
			this->removeAt(i);
			break;
		}
	}
	return result;
}

template<class T, uint32_t G, class A> inline void SYSpVector<T, G, A>::clearAndDestroy() {
	for (uint32_t i = 0; i < this->length(); ++i) {
		delete (*this)[i];
	}
	this->clear();
}

template<class T, uint32_t G, class A> inline bool SYSpVector<T, G, A>::operator ==(const SYSpVector<T, G, A>& right) const {
	if (this->length() != right.length()) {
		return false;
	}
	for (uint32_t i = 0; i < this->length(); ++i) {
		if (!(*(*this)[i] == *right[i])) {
			return false;
		}
	}
	return true;
}

//
// SYSsortedVector: sorted vector
//
template<class T, uint32_t G = 0, class A = SYSallocator<T> > class SYSsortedVector: public SYSvector<T, G, A> {
public:

	SYSsortedVector<T, G, A>(const uint32_t size = 0) : SYSvector<T, G, A>(size) {
	}

	SYSsortedVector<T, G, A>& operator = (const SYSvector<T, G, A>& right);
	SYSsortedVector<T, G, A>(const SYSvector<T, G, A>& right);

	// accessors
	uint32_t index(const T& t) const;
	bool contains(const T& t) const;

	// operations
	void insert(const T& t);
	bool remove(const T& t);
	bool insertIfAbsent(const T& t);

	// operators
	bool operator ==(const SYSsortedVector<T, G, A>& right) const;

	bool bsearch(const T& t, uint32_t& index, const int mode) const;

#ifndef NDEBUG
	bool isSorted() const;
#endif
};

template<class T, uint32_t G, class A> inline bool SYSsortedVector<T, G, A>::bsearch(const T& t, uint32_t& index, const int mode) const {
	// mode = 0: check if object exists
	// mode = 1: find first occurrence
	// mode = 2: find for insertion
	bool result = false;
	index = 0;
	if (this->n_ > 0) {
		uint32_t top = this->n_ - 1;
		uint32_t bottom = 0;
		while (top > bottom) {
			index = (top + bottom) >> 1;
			const T& v = (*this)[index];
			if (t == v) {
				result = true;
				break;
			} else if (t < v) {
				top = index ? index - 1 : 0;
			} else {
				bottom = index + 1;
			}
		}
		if (!result) {
			index = bottom;
			if (t == (*this)[index]) {
				result = true;
			}
		}
		if (result) {
			if (mode == 1) {
				// go down to the first one
				while (index > 0 && t == (*this)[index - 1]) {
					index--;
				}
			} else if (mode == 2) {
				// found; move up to the insertion position
				index++;
				while (index < this->n_ && t == (*this)[index]) {
					index++;
				}
			}
		} else {
			if (mode == 2) {
				// not found; move up to the insertion position
				while (index < this->n_ && (*this)[index] < t) {
					index++;
				}
			}
		}
	}
	return result;
}

template<class T, uint32_t G, class A> inline uint32_t SYSsortedVector<T, G, A>::index(const T& t) const {
	assert(isSorted());
	uint32_t index;
	if (bsearch(t, index, 1)) {
		return index;
	} else {
		return SYS_NPOS;
	}
}

template<class T, uint32_t G, class A> inline bool SYSsortedVector<T, G, A>::contains(const T& t) const {
	assert(isSorted());
	uint32_t index;
	return bsearch(t, index, 0);
}

template<class T, uint32_t G, class A> inline void SYSsortedVector<T, G, A>::insert(const T& t) {
	assert(isSorted());
	uint32_t index;
	bsearch(t, index, 2);
	this->insertAt(index, t);
	assert(isSorted());
}

template<class T, uint32_t G, class A> inline bool SYSsortedVector<T, G, A>::insertIfAbsent(const T& t) {
	assert(isSorted());
	uint32_t index;
	if (!bsearch(t, index, 2)) {
		this->insertAt(index, t);
		assert(isSorted());
		return true;
	} else {
		return false;
	}
}

template<class T, uint32_t G, class A> inline SYSsortedVector<T, G, A>& SYSsortedVector<T, G, A>::operator = (const SYSvector<T, G, A>& right) {
	SYSvector<T, G, A>::clear();
	const uint32_t n = right.entries();
	SYSvector<T, G, A>::resize(n, false, false);
	for (uint32_t i = 0; i < n; ++i) {
		insert(right[i]);
	}
	return *this;
}

template<class T, uint32_t G, class A> inline SYSsortedVector<T, G, A>::SYSsortedVector(const SYSvector<T, G, A>& right) {
	*this = right;
}

template<class T, uint32_t G, class A> inline bool SYSsortedVector<T, G, A>::remove(const T& t) {
	assert(isSorted());
	uint32_t index;
	if (bsearch(t, index, 1)) {
		this->removeAt(index);
		assert(isSorted());
		return true;
	} else {
		return false;
	}
}

template<class T, uint32_t G, class A> inline bool SYSsortedVector<T, G, A>::operator ==(const SYSsortedVector<T, G, A>& right) const {
	if (this->length() != right.length()) {
		return false;
	}
	for (uint32_t i = 0; i < this->length(); ++i) {
		if (!((*this)[i] == right[i])) {
			return false;
		}
	}
	return true;
}

#ifndef NDEBUG
template<class T, uint32_t G, class A> inline bool SYSsortedVector<T, G, A>::isSorted() const {
	if (this->n_ < 2) {
		return true;
	}
	for (uint32_t index = 0; index < this->n_ - 1; ++index) {
		if (!((*this)[index] < (*this)[index + 1]) && !((*this)[index] == (*this)[index + 1])) {
			return false;
		}
	}
	return true;
}
#endif

//
// SYSpSortedVector: sorted vector of pointers
//
template<class T, uint32_t G, class A = SYSallocator<T*> > class SYSpSortedVector: public SYSvector<T*, G, A> {
public:

	SYSpSortedVector<T, G, A>(const uint32_t size = 0) : SYSvector<T*, G, A>(size) {
	}

	// accessors
	uint32_t index(const T* t) const;
	bool contains(const T* t) const;
	T* find(const T* t) const;

	// operations
	void insert(T* t);
	T* remove(const T* t);
	void clearAndDestroy();

	// operators
	bool operator ==(const SYSpSortedVector<T, G, A>& right) const;

	bool bsearch(const T& t, uint32_t& index, const int mode) const;

#ifndef NDEBUG
	bool isSorted() const;
#endif
};

template<class T, uint32_t G, class A> inline bool SYSpSortedVector<T, G, A>::bsearch(const T& t, uint32_t& index, const int mode) const {
	// mode = 0: check if object exists
	// mode = 1: find first occurrence
	// mode = 2: find for insertion
	bool result = false;
	index = 0;
	if (this->n_ > 0) {
		uint32_t top = this->n_ - 1;
		uint32_t bottom = 0;
		while (top > bottom) {
			index = (top + bottom) >> 1;
			const T& v = *((*this)[index]);
			if (t == v) {
				result = true;
				break;
			} else if (t < v) {
				top = index ? index - 1 : 0;
			} else {
				bottom = index + 1;
			}
		}
		if (!result) {
			index = bottom;
			if (t == *((*this)[index])) {
				result = true;
			}
		}
		if (result) {
			if (mode == 1) {
				// go down to the first one
				while (index > 0 && t == *((*this)[index - 1])) {
					index--;
				}
			} else if (mode == 2) {
				// found; move up to the insertion position
				index++;
				while (index < this->n_ && t == *((*this)[index])) {
					index++;
				}
			}
		} else if (mode == 2) {
			// not found; move up to the insertion position
			while (index < this->n_ && *((*this)[index]) < t) {
				index++;
			}
		}
	}
	return result;
}

template<class T, uint32_t G, class A> inline uint32_t SYSpSortedVector<T, G, A>::index(const T* t) const {
	assert(isSorted());
	uint32_t index;
	if (bsearch(*t, index, 1)) {
		return index;
	} else {
		return SYS_NPOS;
	}
}

template<class T, uint32_t G, class A> inline bool SYSpSortedVector<T, G, A>::contains(const T* t) const {
	assert(isSorted());
	uint32_t index;
	return bsearch(*t, index, 0);
}

template<class T, uint32_t G, class A> inline T* SYSpSortedVector<T, G, A>::find(const T* t) const {
	assert(isSorted());
	uint32_t index;
	if (bsearch(*t, index, 1)) {
		return (*this)[index];
	} else {
		return 0;
	}
}

template<class T, uint32_t G, class A> inline void SYSpSortedVector<T, G, A>::insert(T* t) {
	assert(isSorted());
	uint32_t index;
	bsearch(*t, index, 2);
	this->insertAt(index, t);
	assert(isSorted());
}

template<class T, uint32_t G, class A> inline T* SYSpSortedVector<T, G, A>::remove(const T* t) {
	assert(isSorted());
	uint32_t index;
	if (bsearch(*t, index, 1)) {
		T* result = (*this)[index];
		this->removeAt(index);
		assert(isSorted());
		return result;
	} else {
		return 0;
	}
}

template<class T, uint32_t G, class A> inline void SYSpSortedVector<T, G, A>::clearAndDestroy() {
	uint32_t i = 0;
	for (i = 0; i < this->n_; ++i) {
		delete (*this)[i];
	}
	this->clear();
}

template<class T, uint32_t G, class A> inline bool SYSpSortedVector<T, G, A>::operator ==(const SYSpSortedVector<T, G, A>& right) const {
	if (this->length() != right.length()) {
		return false;
	}
	for (uint32_t i = 0; i < this->length(); ++i) {
		if (!(*(*this)[i] == *right[i])) {
			return false;
		}
	}
	return true;
}

#ifndef NDEBUG
template<class T, uint32_t G, class A> inline bool SYSpSortedVector<T, G, A>::isSorted() const {
	if (this->n_ < 2) {
		return true;
	}
	for (uint32_t index = 0; index < this->n_ - 1; ++index) {
		if (!(*((*this)[index]) < *((*this)[index + 1])) && !(*((*this)[index])	== *((*this)[index + 1]))) {
			return false;
		}
	}
	return true;
}
#endif

//
// SYSlarray: single linked list of arrays.
//
template<class T, uint32_t G, class A = SYSallocator<T> > class SYSlarray: private SYSslist<T*>, public A {
private:

	uint32_t length_;

public:

	SYSlarray<T, G, A>() : length_(0) {
	}

	~SYSlarray<T, G, A>();

	const T& operator[](const uint32_t index) const;

	T& operator[](const uint32_t index);

	void append(const T& t);

	T removeLast();

	void clear();

	uint32_t length() const;
};

template<class T, uint32_t G, class A> inline const T& SYSlarray<T, G, A>::operator[](const uint32_t index) const {
	assert(index < length_);
	SYSslistIterator<T*> iterator(*const_cast<SYSslist<T*>*>(static_cast<const SYSslist<T*>*>(this)));
	const uint32_t pos = index / G;
	uint32_t i = 0;
	while (i++ <= pos && ++iterator) {
	}
	T* a = iterator.key();
	return a[index % G];
}

template<class T, uint32_t G, class A> inline T& SYSlarray<T, G, A>::operator[](const uint32_t index) {
	assert(index < length_);
	SYSslistIterator<T*> iterator(*this);
	const uint32_t pos = index / G;
	uint32_t i = 0;
	while (i++ <= pos && ++iterator) {
	}
	T* a = iterator.key();
	return a[index % G];
}

template<class T, uint32_t G, class A> inline void SYSlarray<T, G, A>::append(const T& t) {
	T* a;
	if (length_ % G == 0) {
		a = this->build(G);
		SYSslist<T*>::append(a);
	} else {
		a = SYSslist<T*>::last();
	}
	a[length_++ % G] = t;
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
// TODO: solve this warning
// If T is of type pointer, then T result is an unintialized pointer. 
//	The code does not garantee that result will be assigned later a value. 
// 	In this case, the method can return an unintialized pointer. 
//	When called from engine/vec.h:1003 for example, this could lead to a crash. 
#endif 

template<class T, uint32_t G, class A> inline T SYSlarray<T, G, A>::removeLast() {
	assert(length_ > 0);
	T result = (*this)[length_ - 1];
	--length_;
	if (length_ % G == 0) {
		T* a = SYSslist<T*>::removeAt(length_ / G);
		this->destroy(a);
	}
	return result;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif 

template<class T, uint32_t G, class A> inline void SYSlarray<T, G, A>::clear() {
	SYSslistIterator<T*> iterator(*this);
	while (++iterator) {
		this->destroy(iterator.key());
	}
	SYSslist<T*>::clear();
	length_ = 0;
}

template<class T, uint32_t G, class A> inline SYSlarray<T, G, A>::~SYSlarray() {
	clear();
}

template<class T, uint32_t G, class A> inline uint32_t SYSlarray<T, G, A>::length() const {
	return length_;
}

//
// SYSlvector: vector made of multiple small blocks to limit memory usage and fragmentation.
// Allows concurrent reads and appends.
//
template<class T, class A = SYSallocator<T> > class SYSlvector: public A {
private:

	SYSlarray<T*, 512> array_;
	uint32_t length_;

protected:

	void resize(const uint32_t length);

public:

	static const int BLOCK_SIZE = 2048;

	SYSlvector<T, A>();

	~SYSlvector<T, A>();

	SYSlvector<T, A>(const SYSlvector<T, A>& right);

	SYSlvector<T, A>& operator =(const SYSlvector<T, A>& right);

	void clear();

	uint32_t length() const;

	void append(const T& t);

	const T& operator[](const uint32_t index) const;

	const T& first() const;

	const T& last() const;

	T& operator[](const uint32_t index);

	void shrink(const uint32_t length);

	int64_t getSize() const;
};

template<class T, class A> inline SYSlvector<T, A>::SYSlvector() :
	length_(0) {
}

template<class T, class A> inline void SYSlvector<T, A>::clear() {
	for (uint32_t i = 0; i < array_.length(); ++i) {
		this->destroy(array_[i]);
	}
	array_.clear();
	this->resetCount();
	length_ = 0;
}

template<class T, class A> inline SYSlvector<T, A>::~SYSlvector() {
	clear();
}

template<class T, class A> inline uint32_t SYSlvector<T, A>::length() const {
	return length_;
}

template<class T, class A> inline void SYSlvector<T, A>::resize(const uint32_t length) {
	const uint32_t oldN = array_.length();
	const uint32_t newN = length == 0 ? 0 : (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
	if (oldN < newN) {
		for (uint32_t i = oldN; i < newN; ++i) {
			array_.append(this->build(BLOCK_SIZE));
		}
	} else if (oldN > newN) {
		for (uint32_t i = newN; i < oldN; ++i) {
			this->destroy(array_.removeLast());
		}
		if (newN == 0) {
			array_.clear();
			this->resetCount();
		}
	}
	length_ = length;
}

template<class T, class A> inline void SYSlvector<T, A>::shrink(const uint32_t length) {
	if (length < this->length()) {
		resize(length);
	}
}

template<class T, class A> inline void SYSlvector<T, A>::append(const T& t) {
	const uint32_t n = length_ / BLOCK_SIZE;
	if (n == array_.length()) {
		array_.append(this->build(BLOCK_SIZE));
	}
	array_[n][length_++ % BLOCK_SIZE] = t;
}

template<class T, class A> inline const T& SYSlvector<T, A>::operator[](const uint32_t index) const {
	assert(index < length_);
	const T* v = array_[index / BLOCK_SIZE];
	return v[index % BLOCK_SIZE];
}

template<class T, class A> inline const T& SYSlvector<T, A>::first() const {
	return (*this)[0];
}

template<class T, class A> inline const T& SYSlvector<T, A>::last() const {
	return (*this)[length_ - 1];
}

template<class T, class A> inline T& SYSlvector<T, A>::operator[](const uint32_t index) {
	assert(index < length_);
	T* v = array_[index / BLOCK_SIZE];
	return v[index % BLOCK_SIZE];
}

template<class T, class A> inline SYSlvector<T, A>& SYSlvector<T, A>::operator =(const SYSlvector<T, A>& right) {
	if (this != &right) {
		clear();
		for (uint32_t i = 0; i < right.length_; ++i) {
			append(right[i]);
		}
	}
	return *this;
}

template<class T, class A> inline SYSlvector<T, A>::SYSlvector(const SYSlvector<T, A>& right) :
	length_(0) {
	*this = right;
}

template<class T, class A> inline int64_t SYSlvector<T, A>::getSize() const {
	return length() * sizeof(A);
}

//
// SYSbitVector: bit vector.
// Allows concurrent reads and appends.
//
template<class A> class SYSbitVector: private SYSlvector<uint64_t, A> {

private:

	uint32_t size_;

protected:

	void resize(const uint32_t n);

public:

	SYSbitVector<A>();
	SYSbitVector<A>(const SYSbitVector<A>& right);
	~SYSbitVector<A>();

	// accessors
	bool isEmpty() const;
	uint32_t length() const;
	bool operator [](const uint32_t index) const;
	int64_t getSize() const;
	bool areAll(bool value) const;

	// operations
	void clearBit(const uint32_t offset);
	void setBit(const uint32_t offset);
	void clear();
	void shrink(const uint32_t length);

	// operators
	SYSbitVector<A>& operator =(const SYSbitVector<A>& right);
};


template<class A> inline bool SYSbitVector<A>::areAll(bool value) const {
	if (isEmpty()) return true;
	const uint n = size_/64;
	for (uint i=0; i<n; ++i) {
		const uint64_t* p = &SYSlvector<uint64_t, A>::operator[](i);
		if (*p != (value ? ULLONG_MAX : 0))
			return false;
	}
	const uint	r = size_%64;
	if (r != 0) {
		const uint64_t* p = &SYSlvector<uint64_t, A>::operator[](n);
		for (uint i=0; i<r; ++i) {
			if ( ((*p>>i) & 1ULL) != value)
				return false;
		}
	}
	return true;
}
template<class A> inline bool SYSbitVector<A>::isEmpty() const {
	return size_ == 0;
}

template<class A> inline uint32_t SYSbitVector<A>::length() const {
	return size_;
}

template<class A> inline void SYSbitVector<A>::clearBit(const uint32_t offset) {
	if (offset < length()) {
		uint64_t* p = &SYSlvector<uint64_t, A>::operator[](offset >> 6);
		*p &= ~(1ULL << (offset & 63ULL));
	}
}

template<class A> inline void SYSbitVector<A>::resize(const uint32_t n) {
	uint32_t oldN = SYSlvector<uint64_t, A>::length();
	SYSlvector<uint64_t, A>::resize((n + 63) / 64);
	const uint32_t newN = SYSlvector<uint64_t, A>::length();

	// Reset added words.
	while (oldN < newN) {
		SYSlvector<uint64_t, A>::operator[](oldN++) = 0;
	}

	// Reset added bits.
	uint32_t nbits = 63 - (size_ % 64);
	while (size_ < n && nbits > 0) {
		clearBit(size_++);
		nbits--;
	}
	size_ = n;
}

template<class A> inline void SYSbitVector<A>::setBit(const uint32_t offset) {
	if (offset >= length()) {
		resize(offset + 1);
	}
	uint64_t* p = &SYSlvector<uint64_t, A>::operator[](offset >> 6);
        *p |= (1ULL << (offset & 63ULL));
}

template<class A> inline SYSbitVector<A>::SYSbitVector() : size_(0) {
}

template<class A> inline void SYSbitVector<A>::clear() {
	SYSlvector<uint64_t, A>::clear();
	size_ = 0;
}

template<class A> inline SYSbitVector<A>& SYSbitVector<A>::operator =(const SYSbitVector<A>& right) {
	if (this != &right) {
		SYSlvector<uint64_t, A>::operator =(right);
		size_ = right.size_;
	}
	return *this;
}

template<class A> inline SYSbitVector<A>::SYSbitVector(const SYSbitVector<A>& right) {
	*this = right;
}

template<class A> inline bool SYSbitVector<A>::operator[](const uint32_t index) const {
	assert(index < length());
	return SYSlvector<uint64_t, A>::operator[](index / 64) & (1ULL << (index % 64));
}

template<class A> inline SYSbitVector<A>::~SYSbitVector() {
	clear();
}

template<class A> inline void SYSbitVector<A>::shrink(const uint32_t length) {
	if (length < this->length()) {
		resize(length);
	}
}

template<class A> inline int64_t SYSbitVector<A>::getSize() const {
	return SYSlvector<uint64_t, A>::getSize();
}

//
// SYSxvector: vector made of multiple small blocks to limit memory usage and fragmentation.
// Allows concurrent reads, but without appends.
//
template<class T, class A = SYSallocator<T> > class SYSxvector: public A {
private:

	SYSpVector<T, 64> array_;
	uint32_t length_;

protected:

	void resize(const uint32_t length);

public:

	static const uint32_t BLOCK_SIZE = 2048;

	SYSxvector<T, A>();

	~SYSxvector<T, A>();

	SYSxvector<T, A>(const SYSxvector<T, A>& right);

	SYSxvector<T, A>(const uint32_t length);

	SYSxvector<T, A>& operator =(const SYSxvector<T, A>& right);

	void clear();

	uint32_t length() const;

	void append(const T& t);

	const T& operator[](const uint32_t index) const;

	const T& first() const;

	const T& last() const;

	uint32_t index(const T& t) const;

	bool contains(const T& t) const;

	T& operator[](const uint32_t index);

	void shrink(const uint32_t length);

	int64_t getSize() const;
};

template<class T, class A> inline SYSxvector<T, A>::SYSxvector() :
	length_(0) {
}

template<class T, class A> inline void SYSxvector<T, A>::clear() {
	for (uint32_t i = 0; i < array_.entries(); ++i) {
		this->destroy(array_[i]);
	}
	array_.clear();
	this->resetCount();
	length_ = 0;
}

template<class T, class A> inline SYSxvector<T, A>::~SYSxvector() {
	clear();
}

template<class T, class A> inline uint32_t SYSxvector<T, A>::length() const {
	return length_;
}

template<class T, class A> inline void SYSxvector<T, A>::resize(const uint32_t length) {
	const uint32_t oldN = array_.length();
	const uint32_t newN = length == 0 ? 0 : (length + BLOCK_SIZE - 1) / BLOCK_SIZE;
	if (oldN < newN) {
		for (uint32_t i = oldN; i < newN; ++i) {
			array_.append(this->build(BLOCK_SIZE));
		}
	} else if (oldN > newN) {
		for (uint32_t i = newN; i < oldN; ++i) {
			this->destroy(array_.last());
			array_.removeLast();
		}
		if (newN == 0) {
			array_.clear();
			this->resetCount();
		}
	}
	length_ = length;
}

template<class T, class A> inline SYSxvector<T, A>::SYSxvector(const uint32_t length) : length_(0) {
	resize(length);
}

template<class T, class A> inline void SYSxvector<T, A>::shrink(const uint32_t length) {
	if (length < this->length()) {
		resize(length);
	}
}

template<class T, class A> inline void SYSxvector<T, A>::append(const T& t) {
	const uint32_t n = length_ / BLOCK_SIZE;
	if (n == array_.length()) {
		array_.append(this->build(BLOCK_SIZE));
	}
	array_[n][length_++ % BLOCK_SIZE] = t;
}

template<class T, class A> inline const T& SYSxvector<T, A>::operator[](const uint32_t index) const {
	assert(index < length_);
	const T* v = array_[index / BLOCK_SIZE];
	return v[index % BLOCK_SIZE];
}

template<class T, class A> inline const T& SYSxvector<T, A>::first() const {
	return (*this)[0];
}

template<class T, class A> inline const T& SYSxvector<T, A>::last() const {
	return (*this)[length_ - 1];
}

template<class T, class A> inline bool SYSxvector<T, A>::contains(const T& t) const {
	return index(t) != SYS_NPOS;
}

template<class T, class A> inline uint32_t SYSxvector<T, A>::index(const T& t) const {
	uint32_t		k = 0, j = 0;
	uint32_t		remaining = length_;
	while (k < length_) {
		const T*	v = array_[j++];
		uint32_t		max_i = (remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining);
		for (uint i=0; i<max_i; ++i) {
			if ( *v++ == t )
				return i+j*BLOCK_SIZE;
		}
		remaining -= max_i;
		k += max_i;
	}
	return SYS_NPOS;
}

template<class T, class A> inline T& SYSxvector<T, A>::operator[](const uint32_t index) {
	assert(index < length_);
	T* v = array_[index / BLOCK_SIZE];
	return v[index % BLOCK_SIZE];
}

template<class T, class A> inline SYSxvector<T, A>& SYSxvector<T, A>::operator =(const SYSxvector<T, A>& right) {
	if (this != &right) {
		clear();
		for (uint32_t i = 0; i < right.length_; ++i) {
			append(right[i]);
		}
	}
	return *this;
}

template<class T, class A> inline SYSxvector<T, A>::SYSxvector(const SYSxvector<T, A>& right) :	length_(0) {
	*this = right;
}

template<class T, class A> inline int64_t SYSxvector<T, A>::getSize() const {
	return length() * sizeof(A);
}

}

#endif /* #ifndef _engine_vec_h_ */
