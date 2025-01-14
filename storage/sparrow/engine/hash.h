/*
 Hash table types.
 */

#ifndef _engine_hash_h_
#define _engine_hash_h_

#include "vec.h"

namespace Sparrow {

// spread hash code
static inline uint32_t spreadHashCode(uint32_t h) {
	h += ~(h << 9);
	h ^= (h >> 14);
	h += (h << 4);
	h ^= (h >> 10);
	return h;
}

template<class T> class SYShlink {
public:

	SYShlink<T>(const T& object, const uint32_t hash, SYShlink<T>* next);
	SYShlink<T>* getNext() const;
	void setNext(SYShlink<T>* next);
	const T& getObject() const;
	T& getObject();
	void setObject(const T& object);
	void setHash(const uint32_t hash);
	uint32_t getHash() const;

protected:

	uint32_t hash_;
	SYShlink<T>* next_;
	T object_;
};

template<class T> inline SYShlink<T>::SYShlink(const T& object, const uint32_t hash, SYShlink<T>* next) :
	hash_(hash), next_(next), object_(object) {
}

template<class T> inline SYShlink<T>* SYShlink<T>::getNext() const {
	return next_;
}

template<class T> inline void SYShlink<T>::setNext(SYShlink<T>* next) {
	next_ = next;
}

template<class T> inline const T& SYShlink<T>::getObject() const {
	return object_;
}

template<class T> inline T& SYShlink<T>::getObject() {
	return object_;
}

template<class T> inline void SYShlink<T>::setObject(const T& object) {
	object_ = object;
}

template<class T> inline void SYShlink<T>::setHash(const uint32_t hash) {
	hash_ = hash;
}

template<class T> inline uint32_t SYShlink<T>::getHash() const {
	return hash_;
}

//
// Default allocator for hash tables.
//
template<class T> class SYShAllocator {
private:

	uint32_t n_;

public:

	SYShAllocator<T>() : n_(0) {
	}

	SYShlink<T>* acquire(const T& object, const uint32_t hash, SYShlink<T>* next) {
		n_++;
		return new SYShlink<T>(object, hash, next);
	}

	void release(SYShlink<T>* link) {
		n_--;
		delete link;
	}
};

//
// Pool allocator for hash tables.
//
template<class T> class SYShPoolAllocator {
private:

	SYShlink<T>* root_;

public:

	SYShPoolAllocator<T>() :
		root_(0) {
	}
	~SYShPoolAllocator<T>() {
		SYShlink<T>* link = root_;
		while (link != 0) {
			SYShlink<T>* next = link->getNext();
			delete link;
			link = next;
		}
	}
	SYShlink<T>* acquire(const T& object, const uint32_t hash, SYShlink<T>* next) {
		if (root_ == 0) {
			return new SYShlink<T>(object, hash, next);
		} else {
			SYShlink<T>* link = root_;
			root_ = root_->getNext();
			link->setObject(object);
			link->setHash(hash);
			link->setNext(next);
			return link;
		}
	}
	void release(SYShlink<T>* link) {
		link->setNext(root_);
		root_ = link;
	}
};

template<class T> class SYShashBase {
protected:

	const uint32_t initial_;
	SYSpVector<SYShlink<T> , 0> vector_;
	uint32_t items_;

protected:

	void initialize(SYSpVector<SYShlink<T> , 0>& vector);
	uint32_t initialize();
	void extend();

public:

	SYShashBase<T>(const uint32_t initial);

	// accessors
	uint32_t entries() const;
	bool isEmpty() const;
	int64_t getSize() const;
};

template<class T> inline void SYShashBase<T>::initialize(SYSpVector<SYShlink<T> , 0>& vector) {
	const uint32_t length = vector.length();
	for (uint32_t i = 0; i < length; ++i) {
		vector[i] = 0;
	}
}

template<class T> inline uint32_t SYShashBase<T>::initialize() {
	if (vector_.isEmpty()) {
		vector_.reshape(initial_);
		initialize(vector_);
	}
	return vector_.length();
}

template<class T> inline void SYShashBase<T>::extend() {
	const uint32_t length = vector_.length();
	if (length != 0) {
		SYSpVector<SYShlink<T> , 0> vector;
		const uint32_t buckets = length * 2;
		vector.reshape(buckets);
		initialize(vector);
		for (uint32_t i = 0; i < length; ++i) {
			SYShlink<T>* sl = vector_[i];
			SYShlink<T>* nsl = 0;
			while (sl != 0) {
				nsl = sl->getNext();
				const uint32_t h = sl->getHash();
				const uint32_t bucket = (spreadHashCode(h) % buckets);
				sl->setNext(vector[bucket]);
				vector[bucket] = sl;
				sl = nsl;
			}
		}
		vector_ = vector;
	}
}

template<class T> inline uint32_t SYShashBase<T>::entries() const {
	return items_;
}

template<class T> inline bool SYShashBase<T>::isEmpty() const {
	return (entries() == 0);
}

template<class T> inline int64_t SYShashBase<T>::getSize() const {
	return entries() * sizeof(SYShlink<T>);
}

template<class T> inline SYShashBase<T>::SYShashBase(const uint32_t initial) : initial_(initial == 0 ? 1 : initial), items_(0) {
}

template<class T, class A> class SYShashIterator;

template<class T, class A = SYShAllocator<T> > class SYShash: public SYShashBase<T>, public A {

	friend class SYShashIterator<T, A> ;

public:

	// constructors
	SYShash<T, A>(const uint32_t initial);
	SYShash<T, A>(const SYShash<T, A>& right);

	// destructor
	~SYShash<T, A>();

	// operations
	void insert(const T& t);
	T* insertAndReturn(const T& t);
	bool remove(const T& t);
	bool contains(const T& t) const;
	void clear();
	bool find(const T& t, T& r) const;
	T* find(const T& t) const;

	// copy
	SYShash<T, A>& operator =(const SYShash<T, A>& right);

	// equality
	bool operator ==(const SYShash<T, A>& right) const;
};

template<class T, class A> inline SYShash<T, A>::SYShash(const uint32_t initial)
	: SYShashBase<T>(initial) {
}

template<class T, class A> inline void SYShash<T, A>::clear() {
	const uint32_t length = this->vector_.length();
	for (uint32_t bucket = 0; bucket < length; ++bucket) {
		SYShlink<T>* sl = this->vector_[bucket];
		while (sl != 0) {
			SYShlink<T>* next = sl->getNext();
			this->release(sl);
			sl = next;
		}
	}
	this->vector_.clear();
	this->items_ = 0;
}

template<class T, class A> inline SYShash<T, A>::~SYShash() {
	clear();
}

template<class T, class A> inline T* SYShash<T, A>::insertAndReturn(const T& t) {
	const uint32_t buckets = this->initialize();
	const uint32_t h = t.hash();
	const uint32_t bucket = (spreadHashCode(h) % buckets);
	SYShlink<T>* sl = this->acquire(t, h, this->vector_[bucket]);
	this->vector_[bucket] = sl;
	this->items_++;
	if (this->items_ == this->vector_.length()) {
		this->extend();
	}
	return static_cast<T*>(&sl->getObject());
}

template<class T, class A> inline void SYShash<T, A>::insert(const T& t) {
	insertAndReturn(t);
}

template<class T, class A> inline bool SYShash<T, A>::contains(const T& t) const {
	const uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return false;
	}
	const uint32_t h = t.hash();
	const uint32_t bucket = (spreadHashCode(h) % buckets);
	const SYShlink<T>* sl = this->vector_[bucket];
	while (sl != 0) {
		if (sl->getHash() == h && sl->getObject() == t) {
			return true;
		}
		sl = sl->getNext();
	}
	return false;
}

template<class T, class A> inline bool SYShash<T, A>::remove(const T& t) {
	const uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return false;
	}
	const uint32_t h = t.hash();
	const uint32_t bucket = (spreadHashCode(h) % buckets);
	SYShlink<T>* sl = this->vector_[bucket];
	SYShlink<T>* psl = 0;
	while (sl != 0) {
		if (sl->getHash() == h && sl->getObject() == t) {
			if (psl == 0) {
				this->vector_[bucket] = sl->getNext();
			} else {
				psl->setNext(sl->getNext());
			}
			this->release(sl);
			this->items_--;
			return true;
		}
		psl = sl;
		sl = sl->getNext();
	}
	return false;
}

template<class T, class A> inline bool SYShash<T, A>::find(const T& t, T& r) const {
	const uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return false;
	}
	const uint32_t h = t.hash();
	const uint32_t bucket = (spreadHashCode(h) % buckets);
	const SYShlink<T>* sl = this->vector_[bucket];
	const SYShlink<T>* psl = 0;
	while (sl != 0) {
		if (sl->getHash() == h && sl->getObject() == t) {
			r = sl->getObject();
			return true;
		}
		psl = sl;
		sl = sl->getNext();
	}
	return false;
}

template<class T, class A> inline T* SYShash<T, A>::find(const T& t) const {
	const uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return 0;
	}
	const uint32_t h = t.hash();
	const uint32_t bucket = (spreadHashCode(h) % buckets);
	const SYShlink<T>* sl = this->vector_[bucket];
	while (sl != 0) {
		if (sl->getHash() == h && sl->getObject() == t) {
			return const_cast<T*>(&sl->getObject());
		}
		sl = sl->getNext();
	}
	return 0;
}

template<class T, class A> inline bool SYShash<T, A>::operator ==(const SYShash<T, A>& right) const {
	if (this->entries() != right.entries()) {
		return false;
	}
	const uint32_t length = this->vector_.length();
	for (uint32_t bucket = 0; bucket < length; ++bucket) {
		SYShlink<T>* sl = this->vector_[bucket];
		while (sl != 0) {
			if (!right.contains(sl->getObject())) {
				return false;
			}
			sl = sl->getNext();
		}
	}
	return true;
}

template<class T, class A = SYShAllocator<T> > class SYShashIterator {
public:

	// constructors
	SYShashIterator<T, A>(SYShash<T, A>& hash);
	SYShashIterator<T, A>(const SYShash<T, A>& hash);

	// operators
	bool operator ++();
	bool operator ()();

	// operations
	void reset();
	const T& key() const;
	T& key();

private:

	// copy, assignment and equality are forbidden
	SYShashIterator<T, A>(const SYShashIterator<T, A>& right);
	SYShashIterator<T, A>& operator =(const SYShashIterator<T, A>& right);
	bool operator ==(const SYShashIterator<T, A>& right) const;

protected:

	SYShash<T, A>& hash_;
	uint32_t bucket_;
	SYShlink<T>* sl_;
};

template<class T, class A> inline void SYShashIterator<T, A>::reset() {
	bucket_ = SYS_NPOS;
	sl_ = 0;
}

template<class T, class A> inline SYShashIterator<T, A>::SYShashIterator(SYShash<T, A>& hash) : hash_(hash) {
	reset();
}

template<class T, class A> inline SYShashIterator<T, A>::SYShashIterator(const SYShash<T, A>& hash) : hash_(const_cast<SYShash<T, A>& >(hash)) {
	reset();
}

template<class T, class A> inline bool SYShashIterator<T, A>::operator ++() {
	if (sl_ != 0) {
		sl_ = sl_->getNext();
	}
	while (sl_ == 0) {
		bucket_++; // wrapping
		if (bucket_ >= hash_.vector_.length()) {
			return false;
		}
		sl_ = hash_.vector_[bucket_];
	}
	return true;
}

template<class T, class A> inline bool SYShashIterator<T, A>::operator ()() {
	return ++(*this);
}

template<class T, class A> inline const T& SYShashIterator<T, A>::key() const {
	return sl_->getObject();
}

template<class T, class A> inline T& SYShashIterator<T, A>::key() {
	return sl_->getObject();
}

// copy operator/constructor for SYShash: need iterator
template<class T, class A> inline SYShash<T, A>& SYShash<T, A>::operator =(
		const SYShash<T, A>& right) {
	if (this == &right) {
		return *this;
	}
	clear();
	SYShashIterator<T, A> iterator(right);
	while (iterator()) {
		insert(iterator.key());
	}
	return *this;
}

template<class T, class A> inline SYShash<T, A>::SYShash(const SYShash<T, A>& right)
	: SYShashBase<T>(right.initial_) {
	*this = right;
}

template<class T, class A> class SYSpHashIterator;

template<class T, class A = SYShAllocator<T*> > class SYSpHash: public SYShashBase<T*>, public A {

	friend class SYSpHashIterator<T, A> ;

public:

	// constructors
	SYSpHash<T, A>(const uint32_t initial);
	SYSpHash<T, A>(const SYSpHash<T, A>& right);

	// destructor
	~SYSpHash<T, A>();

	// operations
	void insert(T* t);
	T* remove(const T* t);
	bool contains(const T* t) const;
	void clear();
	void clearAndDestroy();
	T* find(const T* t) const;

	// copy
	SYSpHash<T, A>& operator = (const SYSpHash<T, A>& right);

	// equality
	bool operator ==(const SYSpHash<T, A>& right) const;
};

template<class T, class A> inline SYSpHash<T, A>::SYSpHash(const uint32_t initial) : SYShashBase<T*>(initial) {
}

template<class T, class A> inline void SYSpHash<T, A>::clear() {
	uint32_t buckets = this->vector_.length();
	uint32_t bucket;
	for (bucket = 0; bucket < buckets; ++bucket) {
		SYShlink<T*>* sl = this->vector_[bucket];
		while (sl != 0) {
			SYShlink<T*>* next = sl->getNext();
			this->release(sl);
			sl = next;
		}
	}
	this->vector_.clear();
	this->items_ = 0;
}

template<class T, class A> inline void SYSpHash<T, A>::clearAndDestroy() {
	uint32_t buckets = this->vector_.length();
	uint32_t bucket;
	for (bucket = 0; bucket < buckets; ++bucket) {
		SYShlink<T*>* sl = this->vector_[bucket];
		while (sl != 0) {
			SYShlink<T*>* next = sl->getNext();
			delete sl->getObject();
			this->release(sl);
			sl = next;
		}
	}
	this->vector_.clear();
	this->items_ = 0;
}

template<class T, class A> inline SYSpHash<T, A>::~SYSpHash() {
	clear();
}

template<class T, class A> inline void SYSpHash<T, A>::insert(T* t) {
	uint32_t buckets = this->initialize();
	uint32_t h = t->hash();
	uint32_t bucket = (spreadHashCode(h) % buckets);
	this->vector_[bucket] = this->acquire(t, h, this->vector_[bucket]);
	this->items_++;
	if (this->items_ == this->vector_.length()) {
		this->extend();
	}
}

template<class T, class A> inline bool SYSpHash<T, A>::contains(const T* t) const {
	uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return false;
	}
	uint32_t h = t->hash();
	uint32_t bucket = (spreadHashCode(h) % buckets);
	const SYShlink<T*>* sl = this->vector_[bucket];
	while (sl != 0) {
		if (sl->getHash() == h && *(sl->getObject()) == *t) {
			return true;
		}
		sl = sl->getNext();
	}
	return false;
}

template<class T, class A> inline T* SYSpHash<T, A>::remove(const T* t) {
	uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return 0;
	}
	uint32_t h = t->hash();
	uint32_t bucket = (spreadHashCode(h) % buckets);
	SYShlink<T*>* sl = this->vector_[bucket];
	SYShlink<T*>* psl = 0;
	while (sl != 0) {
		if (sl->getHash() == h && *(sl->getObject()) == *t) {
			if (psl == 0) {
				this->vector_[bucket] = sl->getNext();
			} else {
				psl->setNext(sl->getNext());
			}
			T* result = sl->getObject();
			this->release(sl);
			this->items_--;
			return result;
		}
		psl = sl;
		sl = sl->getNext();
	}
	return 0;
}

template<class T, class A> inline T* SYSpHash<T, A>::find(const T* t) const {
	uint32_t buckets = this->vector_.length();
	if (buckets == 0) {
		return 0;
	}
	uint32_t h = t->hash();
	uint32_t bucket = (spreadHashCode(h) % buckets);
	const SYShlink<T*>* sl = this->vector_[bucket];
	while (sl != 0) {
		if (sl->getHash() == h && *(sl->getObject()) == *t) {
			return sl->getObject();
		}
		sl = sl->getNext();
	}
	return 0;
}

template<class T, class A> inline bool SYSpHash<T, A>::operator ==(const SYSpHash<T, A>& right) const {
	if (this->entries() != right.entries()) {
		return false;
	}
	const uint32_t length = this->vector_.length();
	for (uint32_t bucket = 0; bucket < length; ++bucket) {
		SYShlink<T*>* sl = this->vector_[bucket];
		while (sl != 0) {
			if (!right.contains(*sl->getObject())) {
				return false;
			}
			sl = sl->getNext();
		}
	}
	return true;
}

template<class T, class A = SYShAllocator<T*> > class SYSpHashIterator {
public:

	// constructor
	SYSpHashIterator<T, A>(SYSpHash<T, A>& hash);
	SYSpHashIterator<T, A>(const SYSpHash<T, A>& hash);

	// operators
	T* operator ++();
	T* operator ()();

	// operations
	void reset();
	const T* key() const;
	T* key();

private:

	// copy, assignment and equality are forbidden
	SYSpHashIterator<T, A>(const SYSpHashIterator<T, A>& right);
	SYSpHashIterator<T, A>& operator =(const SYSpHashIterator<T, A>& right);
	bool operator ==(const SYSpHashIterator<T, A>& right) const;

protected:

	SYSpHash<T, A>& hash_;
	uint32_t bucket_;
	SYShlink<T*>* sl_;
};

template<class T, class A> inline void SYSpHashIterator<T, A>::reset() {
	bucket_ = SYS_NPOS;
	sl_ = 0;
}

template<class T, class A> inline SYSpHashIterator<T, A>::SYSpHashIterator(SYSpHash<T, A>& hash) : hash_(hash) {
	reset();
}

template<class T, class A> inline SYSpHashIterator<T, A>::SYSpHashIterator(const SYSpHash<T, A>& hash) : hash_((SYSpHash<T, A>&)hash) {
	reset();
}

template<class T, class A> inline T* SYSpHashIterator<T, A>::operator ++() {
	if (sl_ != 0) {
		sl_ = sl_->getNext();
	}
	while (sl_ == 0) {
		bucket_++; // wrapping
		if (bucket_ >= hash_.vector_.length()) {
			return 0;
		}
		sl_ = hash_.vector_[bucket_];
	}
	return sl_->getObject();
}

template<class T, class A> inline T* SYSpHashIterator<T, A>::operator ()() {
	return ++(*this);
}

template<class T, class A> inline const T* SYSpHashIterator<T, A>::key() const {
	return sl_->getObject();
}

template<class T, class A> inline T* SYSpHashIterator<T, A>::key() {
	return sl_->getObject();
}

// copy operator/constructor for SYShash: need iterator
template<class T, class A> inline SYSpHash<T, A>& SYSpHash<T, A>::operator =(const SYSpHash<T, A>& right) {
	if (this == &right) {
		return *this;
	}
	clear();
	SYSpHashIterator<T, A> iterator(right);
	while (iterator()) {
		insert(iterator.key());
	}
	return *this;
}

template<class T, class A> inline SYSpHash<T, A>::SYSpHash(const SYSpHash<T, A>& right) : SYShashBase<T*>(right.initial_) {
	*this = right;
}

// Key/value entry to build a map.

template<class K, class V> class Entry {
private:

	K key_;
	V value_;

public:

	Entry<K, V>();
	Entry<K, V>(const K& key);
	Entry<K, V>(const K& key, const V& value);
	~Entry<K, V>();
	bool operator ==(const Entry<K, V>& right) const;
	Entry<K, V>& operator =(const Entry<K, V>& right);
	uint32_t hash() const;
	const K& getKey() const;
	const V& getValue() const;
};

template<class K, class V> inline Entry<K, V>::Entry() : key_(), value_() {
}

template<class K, class V> inline Entry<K, V>::Entry(const K& key) : key_(key), value_() {
}

template<class K, class V> inline Entry<K, V>::Entry(const K& key, const V& value) : key_(key), value_(value) {
}

template<class K, class V> inline Entry<K, V>::~Entry() {
}

template<class K, class V> inline bool Entry<K, V>::operator ==(const Entry<K, V>& right) const {
	return key_ == right.key_;
}

template<class K, class V> inline Entry<K, V>& Entry<K, V>::operator =(const Entry<K, V>& right) {
	if (this != &right) {
		key_ = right.key_;
		value_ = right.value_;
	}
	return *this;
}

template<class K, class V> inline uint32_t Entry<K, V>::hash() const {
	return key_.hash();
}

template<class K, class V> inline const K& Entry<K, V>::getKey() const {
	return key_;
}

template<class K, class V> inline const V& Entry<K, V>::getValue() const {
	return value_;
}

}

#endif /* #ifndef _engine_hash_h_ */
