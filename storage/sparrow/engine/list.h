/*
 Non-intrusive single-linked list and intrusive double-linked lists.
 */

#ifndef _engine_list_h_
#define _engine_list_h_

#include "my_base.h"
#include <assert.h>
#include "log.h"

namespace Sparrow {

// constant for "not found"
#ifndef SYS_NPOS
#define SYS_NPOS (~(static_cast<uint32_t>(0)))
#endif

template<class T> class SYSslink {
public:

	SYSslink<T>(const T& object, SYSslink<T>* next);
	SYSslink<T>* getNext() const;
	void setNext(SYSslink<T>* next);
	const T& getObject() const;
	T& getObject();
	void setObject(const T& object);

protected:

	T object_;
	SYSslink<T>* next_;
};

template<class T> inline SYSslink<T>::SYSslink(const T& object, SYSslink<T>* next) : object_(object), next_(next) {
}

template<class T> inline SYSslink<T>* SYSslink<T>::getNext() const {
	return next_;
}

template<class T> inline void SYSslink<T>::setNext(SYSslink<T>* next) {
	next_ = next;
}

template<class T> inline const T& SYSslink<T>::getObject() const {
	return object_;
}

template<class T> inline T& SYSslink<T>::getObject() {
	return object_;
}

template<class T> inline void SYSslink<T>::setObject(const T& object) {
	object_ = object;
}

//
// Default allocator for single-linked lists.
//
template<class T> class SYSslAllocator {
public:

	SYSslAllocator<T>() {
	}
	SYSslink<T>* acquire(const T& object, SYSslink<T>* next) {
		return new SYSslink<T>(object, next);
	}
	void release(SYSslink<T>* link) {
		delete link;
	}
};

//
// Pool allocator for single-linked lists.
//
template<class T> class SYSslPoolAllocator {
private:

	SYSslink<T>* root_;

public:

	SYSslPoolAllocator<T>() :
		root_(0) {
	}
	~SYSslPoolAllocator<T>() {
		SYSslink<T>* link = root_;
		while (link != 0) {
			SYSslink<T>* next = link->getNext();
			delete link;
			link = next;
		}
	}
	SYSslink<T>* acquire(const T& object, SYSslink<T>* next) {
		if (root_ == 0) {
			SYSslink<T>* l = new SYSslink<T>(object, next);
			if (l == 0) {
				spw_print_error("SYShPoolAllocator::acquire: cannot allocate %llu bytes of memory", static_cast<ulonglong>(sizeof(*l)));
			}
			return l;
		} else {
			SYSslink<T>* link = root_;
			root_ = root_->getNext();
			link->setObject(object);
			link->setNext(next);
			return link;
		}
	}
	void release(SYSslink<T>* link) {
		link->setNext(root_);
		root_ = link;
	}
};

template<class T, class A> class SYSslistIterator;

template<class T, class A = SYSslAllocator<T> > class SYSslist: public A {
	friend class SYSslistIterator<T, A> ;

public:

	// constructors
	SYSslist<T, A>();
	SYSslist<T, A>(const SYSslist<T, A>& right);

	// destructor
	~SYSslist<T, A>();

	// accessors
	uint32_t entries() const;
	bool isEmpty() const;
	const T& operator [](const uint32_t index) const;
	T& operator [](const uint32_t index);
	const T& first() const;
	const T& last() const;
	T& first();
	T& last();

	// operations
	void insert(const T& t);
	void append(const T& t);
	void prepend(const T& t);
	void insertAt(const uint32_t index, const T& t);
	bool remove(const T& t);
	T removeAt(uint32_t index);
	void removeLast();
	void removeFirst();
	uint32_t index(const T& t) const;
	bool contains(const T& t) const;
	bool find(const T& t, T& result) const;
	void clear();
	void getAll(SYSslist<T, A>& list) {
		list.first_ = first_;
		list.last_ = last_;
		list.n_ = n_;
		first_ = 0;
		last_ = 0;
		n_ = 0;
	}
	void appendAll(SYSslist<T, A>& list) {
		if (!list.isEmpty()) {
			if (isEmpty()) {
				first_ = list.first_;
				last_ = list.last_;
			} else {
				last_->setNext(list.first_);
				last_ = list.last_;
			}
			n_ += list.n_;
			list.first_ = 0;
			list.last_ = 0;
			list.n_ = 0;
		}
	}

	// copy
	SYSslist<T, A>& operator =(const SYSslist<T, A>& right);

protected:

	SYSslink<T>* atPosition(uint32_t index) const;

private:

	// equality not implemented
	bool operator ==(const SYSslist<T, A>& right) const;

protected:

	SYSslink<T>* first_;
	SYSslink<T>* last_;
	uint32_t n_;
};

template<class T, class A> inline uint32_t SYSslist<T, A>::entries() const {
	return n_;
}

template<class T, class A> inline bool SYSslist<T, A>::isEmpty() const {
	return (n_ == 0);
}

template<class T, class A> inline SYSslink<T>* SYSslist<T, A>::atPosition(uint32_t index) const {
	assert(index < n_);
	SYSslink<T>* sl = first_;
	while (sl != 0) {
		if (index-- == 0) {
			return sl;
		}
		sl = sl->getNext();
	}
	return 0; // not reached
}

template<class T, class A> inline void SYSslist<T, A>::insert(const T& t) {
	SYSslink<T>* sl = this->acquire(t, 0);
	if (n_ == 0) {
		first_ = sl;
		last_ = first_;
	} else {
		last_->setNext(sl);
		last_ = sl;
	}
	n_++;
}

template<class T, class A> inline void SYSslist<T, A>::append(const T& t) {
	insert(t);
}

template<class T, class A> inline void SYSslist<T, A>::insertAt(const uint32_t index, const T& t) {
	assert(index <= n_);
	SYSslink<T>* nsl = this->acquire(t, 0);
	SYSslink<T>* psl = index == 0 ? 0 : atPosition(index - 1);
	if (psl == 0) {
		nsl->setNext(first_);
		first_ = nsl;
		if (n_ == 0) {
			last_ = first_;
		}
	} else {
		nsl->setNext(psl->getNext());
		psl->setNext(nsl);
		if (psl == last_) {
			last_ = nsl;
		}
	}
	n_++;
}

template<class T, class A> inline void SYSslist<T, A>::prepend(const T& t) {
	insertAt(0, t);
}

template<class T, class A> inline T SYSslist<T, A>::removeAt(uint32_t index) {
	assert(index < n_);
	T result{};
	SYSslink<T>* sl = first_;
	if (n_ == 1) {
		result = sl->getObject();
		this->release(sl);
		first_ = 0;
		last_ = 0;
		n_ = 0;
	} else {
		SYSslink<T>* psl = 0;
		while (sl != 0) {
			if (index-- == 0) {
				if (psl != 0) {
					psl->setNext(sl->getNext());
				}
				if (sl == first_) {
					first_ = sl->getNext();
				} else if (sl == last_) {
					last_ = psl;
				}
				n_--;
				result = sl->getObject();
				this->release(sl);
				break;
			} else {
				psl = sl;
				sl = sl->getNext();
			}
		}
	}
	return result;
}

template<class T, class A> inline void SYSslist<T, A>::removeLast() {
	assert(n_ > 0);
	removeAt(n_ - 1);
}

template<class T, class A> inline void SYSslist<T, A>::removeFirst() {
	assert(n_ > 0);
	removeAt(0);
}

template<class T, class A> inline bool SYSslist<T, A>::remove(const T& t) {
	bool result = false;
	SYSslink<T>* sl = first_;
	if (n_ == 1) {
		if (sl->getObject() == t) {
			this->release(sl);
			first_ = 0;
			last_ = 0;
			n_ = 0;
			result = true;
		}
	} else if (n_ > 1) {
		SYSslink<T>* psl = 0;
		while (sl != 0) {
			if (sl->getObject() == t) {
				if (psl != 0) {
					psl->setNext(sl->getNext());
				}
				if (sl == first_) {
					first_ = sl->getNext();
				} else if (sl == last_) {
					last_ = psl;
				}
				n_--;
				result = true;
				this->release(sl);
				break;
			} else {
				psl = sl;
				sl = sl->getNext();
			}
		}
	}
	return result;
}

template<class T, class A> inline void SYSslist<T, A>::clear() {
	SYSslink<T>* sl = first_;
	SYSslink<T>* psl = 0;
	while (sl != 0) {
		psl = sl;
		sl = sl->getNext();
		this->release(psl);
	}
	first_ = 0;
	last_ = 0;
	n_ = 0;
}

template<class T, class A> inline SYSslist<T, A>::~SYSslist() {
	clear();
}

template<class T, class A> inline uint32_t SYSslist<T, A>::index(const T& t) const {
	uint32_t result = 0;
	SYSslink<T>* sl = first_;
	while (sl != 0) {
		if (sl->getObject() == t) {
			return result;
		}
		sl = sl->getNext();
		result++;
	}
	return SYS_NPOS;
}

template<class T, class A> inline bool SYSslist<T, A>::find(const T& t, T& result) const {
	SYSslink<T>* sl = first_;
	while (sl != 0) {
		if (sl->getObject() == t) {
			result = sl->getObject();
			return true;
		}
		sl = sl->getNext();
	}
	return false;
}

template<class T, class A> inline bool SYSslist<T, A>::contains(const T& t) const {
	return (index(t) != SYS_NPOS);
}

template<class T, class A> inline SYSslist<T, A>::SYSslist() : first_(0), last_(0), n_(0) {
}

template<class T, class A> inline const T& SYSslist<T, A>::operator [](const uint32_t index) const {
	const SYSslink<T>* sl = atPosition(index);
	return sl->getObject();
}

template<class T, class A> inline T& SYSslist<T, A>::operator [](const uint32_t index) {
	SYSslink<T>* sl = atPosition(index);
	return sl->getObject();
}

template<class T, class A> inline const T& SYSslist<T, A>::first() const {
	assert(n_ > 0);
	return first_->getObject();
}

template<class T, class A> inline const T& SYSslist<T, A>::last() const {
	assert(n_ > 0);
	return last_->getObject();
}

template<class T, class A> inline T& SYSslist<T, A>::first() {
	assert(n_ > 0);
	return first_->getObject();
}

template<class T, class A> inline T& SYSslist<T, A>::last() {
	assert(n_ > 0);
	return last_->getObject();
}

template<class T, class A = SYSslAllocator<T> > class SYSslistIterator : public A {
public:

	// constructor
	SYSslistIterator<T, A>(SYSslist<T, A>& list);

	// operators
	bool operator ++();
	bool operator ()();

	// operations
	void reset();
	const T& key() const;
	T& key();
	bool remove();

private:

	// copy, assignment and equality are forbidden
	SYSslistIterator<T, A>(const SYSslistIterator<T, A>& right);
	SYSslistIterator<T, A>& operator =(const SYSslistIterator<T, A>& right);
	bool operator ==(const SYSslistIterator<T, A>& right) const;

protected:

	SYSslist<T, A>& list_;
	SYSslink<T>* psl_;
	SYSslink<T>* sl_;
};

template<class T, class A> inline void SYSslistIterator<T, A>::reset() {
	psl_ = 0;
	sl_ = 0;
}

template<class T, class A> inline SYSslistIterator<T, A>::SYSslistIterator(SYSslist<T, A>& list) :
	list_(list) {
	reset();
}

template<class T, class A> inline bool SYSslistIterator<T, A>::operator ++() {
	// first time?
	if (psl_ == 0 && sl_ == 0) {
		sl_ = list_.first_;
	} else if (sl_ != 0) {
		psl_ = sl_;
		sl_ = sl_->getNext();
	}
	return (sl_ != 0);
}

template<class T, class A> inline bool SYSslistIterator<T, A>::operator ()() {
	return ++(*this);
}

template<class T, class A> inline const T& SYSslistIterator<T, A>::key() const {
	return sl_->getObject();
}

template<class T, class A> inline T& SYSslistIterator<T, A>::key() {
	return sl_->getObject();
}

template<class T, class A> bool SYSslistIterator<T, A>::remove() {
	if (sl_ != 0) {
		if (list_.entries() == 1) {
			list_.clear();
			psl_ = 0;
			sl_ = 0;
		} else if (sl_ == list_.first_) {
			list_.removeAt(0);
			psl_ = 0;
			sl_ = list_.first_;
		} else if (sl_ == list_.last_) {
			sl_ = 0;
			list_.removeLast();
		} else {
			// remove current
			psl_->setNext(sl_->getNext());
			this->release(sl_);
			sl_ = psl_->getNext();
			list_.n_--;
		}
		return true;
	} else {
		return false;
	}
}

// copy operator/constructor for SYSslist: need iterator
template<class T, class A> SYSslist<T, A>& SYSslist<T, A>::operator =(const SYSslist<T, A>& right) {
	clear();
	SYSslistIterator<T, A> iterator((SYSslist<T, A>&) right);
	while (iterator()) {
		insert(iterator.key());
	}
	return *this;
}

template<class T, class A> SYSslist<T, A>::SYSslist(const SYSslist<T, A>& right) : first_(0), last_(0), n_(0) {
	*this = right;
}

template<class T, class A> class SYSpSlistIterator;

template<class T, class A = SYSslAllocator<T*> > class SYSpSlist: public SYSslist<T*, A> {
	friend class SYSpSlistIterator<T, A> ;

public:

	// constructors
	SYSpSlist<T, A>();

	// accessors
	T* first() const;
	T* last() const;

	// operations
	T* remove(const T* t);
	uint32_t index(const T* t) const;
	bool contains(const T* t) const;
	T* find(const T* t) const;
	void clearAndDestroy();

private:

	// equality not implemented
	bool operator ==(const SYSpSlist<T, A>& right) const;
};

template<class T, class A> inline SYSpSlist<T, A>::SYSpSlist() : SYSslist<T*>() {
}

template<class T, class A> inline T* SYSpSlist<T, A>::first() const {
	return (this->isEmpty() ? 0 : this->first_->getObject());
}

template<class T, class A> inline T* SYSpSlist<T, A>::last() const {
	return (this->isEmpty() ? 0 : this->last_->getObject());
}

template<class T, class A> inline T* SYSpSlist<T, A>::remove(const T* t) {
	T* result = 0;
	SYSslink<T*>* sl = this->first_;
	if (this->n_ == 1) {
		if (*sl->getObject() == *t) {
			result = sl->getObject();
			this->release(sl);
			this->first_ = 0;
			this->last_ = 0;
			this->n_ = 0;
		}
	} else if (this->n_ > 1) {
		SYSslink<T*>* psl = 0;
		while (sl != 0) {
			if (*sl->getObject() == *t) {
				result = sl->getObject();
				if (psl != 0) {
					psl->setNext(sl->getNext());
				}
				if (sl == this->first_) {
					this->first_ = sl->getNext();
				} else if (sl == this->last_) {
					this->last_ = psl;
				}
				this->n_--;
				this->release(sl);
				break;
			} else {
				psl = sl;
				sl = sl->getNext();
			}
		}
	}
	return result;
}

template<class T, class A> inline void SYSpSlist<T, A>::clearAndDestroy() {
	SYSslink<T*>* sl = this->first_;
	SYSslink<T*>* psl = 0;
	while (sl != 0) {
		psl = sl;
		sl = sl->getNext();
		T* object = psl->getObject();
		delete object;
		this->release(psl);
	}
	this->first_ = 0;
	this->last_ = 0;
	this->n_ = 0;
}

template<class T, class A> inline uint32_t SYSpSlist<T, A>::index(const T* t) const {
	uint32_t result = 0;
	SYSslink<T*>* sl = this->first_;
	while (sl != 0) {
		if (*sl->getObject() == *t) {
			return result;
		}
		sl = sl->getNext();
		result++;
	}
	return SYS_NPOS;
}

template<class T, class A> inline T* SYSpSlist<T, A>::find(const T* t) const {
	T* result = 0;
	SYSslink<T*>* sl = this->first_;
	while (sl != 0) {
		if (*sl->getObject() == *t) {
			result = sl->getObject();
			break;
		}
		sl = sl->getNext();
	}
	return result;
}

template<class T, class A> inline bool SYSpSlist<T, A>::contains(const T* t) const {
	return (this->index(t) != SYS_NPOS);
}

template<class T, class A = SYSslAllocator<T*> > class SYSpSlistIterator {
public:

	// constructor
	SYSpSlistIterator<T, A>(SYSpSlist<T, A>& list);

	// operators
	bool operator ++();
	T* operator ()();

	// operations
	void reset();
	const T* key() const;
	T* key();
	bool remove();

private:

	// copy, assignment and equality are forbidden
	SYSpSlistIterator<T, A>(const SYSpSlistIterator<T, A>& right);
	SYSpSlistIterator<T, A>& operator =(const SYSpSlistIterator<T, A>& right);
	bool operator ==(const SYSpSlistIterator<T, A>& right) const;

protected:

	SYSpSlist<T, A>& list_;
	SYSslink<T*>* psl_;
	SYSslink<T*>* sl_;
};

template<class T, class A> inline void SYSpSlistIterator<T, A>::reset() {
	psl_ = 0;
	sl_ = 0;
}

template<class T, class A> inline SYSpSlistIterator<T, A>::SYSpSlistIterator(SYSpSlist<T, A>& list) :
	list_(list) {
	reset();
}

template<class T, class A> inline bool SYSpSlistIterator<T, A>::operator ++() {
	// first time?
	if (psl_ == 0 && sl_ == 0) {
		sl_ = list_.first_;
	} else if (sl_ != 0) {
		psl_ = sl_;
		sl_ = sl_->getNext();
	}
	return (sl_ != 0);
}

template<class T, class A> inline T* SYSpSlistIterator<T, A>::operator ()() {
	if (++(*this)) {
		return sl_->getObject();
	}
	return 0;
}

template<class T, class A> inline const T* SYSpSlistIterator<T, A>::key() const {
	return sl_->getObject();
}

template<class T, class A> inline T* SYSpSlistIterator<T, A>::key() {
	return sl_->getObject();
}

template<class T, class A> inline bool SYSpSlistIterator<T, A>::remove() {
	if (sl_ != 0) {
		if (list_.entries() == 1) {
			list_.clear();
			psl_ = 0;
			sl_ = 0;
		} else if (sl_ == list_.first_) {
			list_.removeAt(0);
			psl_ = 0;
			sl_ = list_.first_;
		} else if (sl_ == list_.last_) {
			sl_ = 0;
			list_.removeLast();
		} else {
			// remove current
			psl_->setNext(sl_->getNext());
			this->release(sl_);
			sl_ = psl_->getNext();
			list_.n_--;
		}
		return true;
	} else {
		return false;
	}
}

template<class T> class SYSidlink {
public:

	SYSidlink();

public:

	T* prev_;
	T* next_;
};

template<class T> inline SYSidlink<T>::SYSidlink() :
	prev_(0), next_(0) {
}

template<class T> class SYSidlistIterator;

template<class T> class SYSidlist {
	friend class SYSidlistIterator<T> ;

public:

	// constructor
	SYSidlist<T>();

	// destructor
	~SYSidlist<T>();

	// accessors
	uint32_t entries() const;
	bool isEmpty() const;
	T* last() const;
	T* first() const;

	// operations
	T* remove(T* t);
	T* removeFirst();
	void prepend(T* t);
	void append(T* t);
	void clear() {
		first_ = 0;
		last_ = 0;
		n_ = 0;
	}
	void append(SYSidlist<T>& list) {
		n_ += list.n_;
		if (last_ == 0) {
			first_ = list.first_;
			last_ = list.last_;
		} else {
			last_->next_ = list.first_;
			if (list.first_ != 0) {
				list.first_->prev_ = last_;
			}
			if (list.last_ != 0) {
				last_ = list.last_;
			}
		}
	}
	bool contains(T* t) const {
		T* it = first_;
		while (it != 0) {
			if (it == t) {
				return true;
			}
			it = it->next_;
		}
		return false;
	}

	bool contains(const T& t) const {
		T* it = first_;
		while (it != 0) {
			if (*it == t) {
				return true;
			}
			it = it->next_;
		}
		return false;
	}

protected:

	// internal link/unlink operations
	void unlink(T* t);
	void link(T* t, T* right);

private:

	// copy, assignment and equality are forbidden
	SYSidlist<T>(const SYSidlist<T>& right);
	SYSidlist<T>& operator =(const SYSidlist<T>& right);
	bool operator ==(const SYSidlist<T>& right) const;

protected:

	T* first_;
	T* last_;
	uint32_t n_;
};

// remove t from list
template<class T> inline void SYSidlist<T>::unlink(T* t) {
	assert(n_ > 0);
	if (t->prev_ != 0) {
		assert(t->prev_->next_ == t);
		t->prev_->next_ = t->next_;
	} else {
		assert(first_ == t);
		first_ = t->next_;
	}
	if (t->next_ != 0) {
		assert(t->next_->prev_ == t);
		t->next_->prev_ = t->prev_;
	} else {
		assert(last_ == t);
		last_ = t->prev_;
	}
	t->prev_ = 0;
	t->next_ = 0;
	n_--;
}

// link right to the right of t (list must not be empty)
template<class T> inline void SYSidlist<T>::link(T* t, T* right) {
	assert(first_ != 0 && last_ != 0 && t != 0 && right != 0);
	right->prev_ = t;
	right->next_ = t->next_;
	t->next_ = right;
	right->next_->prev_ = right;
	if (last_ == t) {
		last_ = right;
	}
	n_++;
}

template<class T> inline uint32_t SYSidlist<T>::entries() const {
	return n_;
}

template<class T> inline bool SYSidlist<T>::isEmpty() const {
	return (n_ == 0);
}

template<class T> inline T* SYSidlist<T>::first() const {
	return first_;
}

template<class T> inline T* SYSidlist<T>::last() const {
	return last_;
}

template<class T> inline void SYSidlist<T>::append(T* t) {
	if (last_ == 0) {
		assert(first_ == 0);
		last_ = t;
		first_ = t;
		t->prev_ = 0;
		t->next_ = 0;
	} else {
		assert(first_ != 0 && last_->next_ == 0);
		last_->next_ = t;
		t->prev_ = last_;
		t->next_ = 0;
		last_ = t;
	}
	n_++;
}

template<class T> inline void SYSidlist<T>::prepend(T* t) {
	if (last_ == 0) {
		assert(first_ == 0);
		last_ = t;
		first_ = t;
		t->prev_ = 0;
		t->next_ = 0;
	} else {
		first_->prev_ = t;
		t->prev_ = 0;
		t->next_ = first_;
		first_ = t;
	}
	n_++;
}

template<class T> inline T* SYSidlist<T>::removeFirst() {
	if (n_ == 0) {
		return 0;
	} else {
		T* t = first_;
		unlink(first_);
		return t;
	}
}

// remove an element from the list (undefined results if the element is not in the list)
template<class T> inline T* SYSidlist<T>::remove(T* t) {
	unlink(t);
	return t;
}

template<class T> inline SYSidlist<T>::~SYSidlist() {
}

template<class T> inline SYSidlist<T>::SYSidlist() :
	first_(0), last_(0), n_(0) {
}

template<class T> class SYSidlistIterator {
public:

	// constructor
	SYSidlistIterator<T>(SYSidlist<T>& list);

	// operators
	T* operator ++();
	T* operator --();
	T* operator ()();

	// operations
	void reset();
	T* key() const;
	void insert(T* t);

private:

	// copy, assignment and equality are forbidden
	SYSidlistIterator<T>(const SYSidlistIterator<T>& right);
	SYSidlistIterator<T>& operator =(const SYSidlistIterator<T>& right);
	bool operator ==(const SYSidlistIterator<T>& right) const;

protected:

	SYSidlist<T>& list_;
	T* l_;
};

template<class T> inline void SYSidlistIterator<T>::reset() {
	l_ = 0;
}

template<class T> inline SYSidlistIterator<T>::SYSidlistIterator(SYSidlist<T>& list) :
	list_(list) {
	reset();
}

template<class T> inline T* SYSidlistIterator<T>::operator ++() {
	l_ = (l_ == 0 ? list_.first_ : l_->next_);
	return l_;
}

template<class T> inline T* SYSidlistIterator<T>::operator --() {
	if (l_ != 0) {
		l_ = l_->prev_;
	}
	return l_;
}

template<class T> inline void SYSidlistIterator<T>::insert(T* t) {
	assert(l_ != 0);
	list_.link(l_, t);
}

template<class T> inline T* SYSidlistIterator<T>::operator ()() {
	return ++(*this);
}

template<class T> inline T* SYSidlistIterator<T>::key() const {
	return l_;
}

}

#endif /* #ifndef _engine_list_h_ */
