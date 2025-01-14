/*
	AbstractInterval tree
 */

#ifndef _engine_intervaltree_h_
#define _engine_intervaltree_h_

#include "vec.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// AbstractInterval
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T> class AbstractInterval {
public:

	virtual ~AbstractInterval() {
	}
	virtual T getMin() const = 0;
	virtual T getMax() const = 0;
	virtual int compareTo(const AbstractInterval<T>& right) const = 0;

	static T getSmallest();	// Smallest possible value.
	static T getLargest();	// Largest possible value.
};

template<> inline uint64_t AbstractInterval<uint64_t>::getSmallest() {
	return 0;
}

template<> inline uint64_t AbstractInterval<uint64_t>::getLargest() {
	return ULLONG_MAX;
}

template<class T> class SimpleInterval : public AbstractInterval<T> {
private:

	const T low_;
	const T high_;

public:
	
	SimpleInterval<T>(const T& low, const T& high) : low_(low), high_(high) {
	}

	T getMin() const override {
		return low_;
	}

	T getMax() const override {
		return high_;
	}

	int compareTo(const AbstractInterval<T>& right) const {
		if (getMin() == right.getMin()) {
			return 0;
		} else if (getMin() < right.getMin()) {
			return -1;
		} else {
			return 1;
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IntervalTreeNode
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T> class IntervalTree;
template<class T> class IntervalTreeNode {
	friend class IntervalTree<T>;

private:

	AbstractInterval<T>* interval_;
	T maxHigh_;
	bool red_;
	IntervalTreeNode<T>* left_;
	IntervalTreeNode<T>* right_;
	IntervalTreeNode<T>* parent_;

public:

	IntervalTreeNode<T>();
	IntervalTreeNode<T>(AbstractInterval<T>* interval);
	~IntervalTreeNode<T>();
	AbstractInterval<T>* getInterval();
};

template<class T> inline IntervalTreeNode<T>::IntervalTreeNode() {
}

template<class T> inline IntervalTreeNode<T>::IntervalTreeNode(AbstractInterval<T>* interval)
	: interval_(interval), maxHigh_(interval->getMax()), left_(0), right_(0), parent_(0) {
}

template<class T> inline IntervalTreeNode<T>::~IntervalTreeNode() {
}

template<class T> inline AbstractInterval<T>* IntervalTreeNode<T>::getInterval() {
	return interval_;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// IntervalTree
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T> class IntervalTree {
private:

	IntervalTreeNode<T>* nil_;
	IntervalTreeNode<T>* root_;

private:

	void leftRotate(IntervalTreeNode<T>* node);
	void rightRotate(IntervalTreeNode<T>* node);
	void insertHelp(IntervalTreeNode<T>* node);
	void TreePrintHelper(IntervalTreeNode<T>* node) const;
	void fixMaxHigh(IntervalTreeNode<T>* node);
	void removeFixUp(IntervalTreeNode<T>* node);
	static bool overlap(const T low1, const T high1, const T low2, const T high2);
	void findOverlaps(IntervalTreeNode<T>* x, const AbstractInterval<T>& interval, SYSpVector<AbstractInterval<T>, 256>& intervals) const;
	IntervalTreeNode<T>* getSuccessorOf(IntervalTreeNode<T>* node) const;
	void deleteNode(IntervalTreeNode<T>* node);

#ifndef NDEBUG
	void checkMaxHighFields(IntervalTreeNode<T>* x) const;
	T checkMaxHighFieldsHelper(IntervalTreeNode<T>* y, const T currentHigh, T match) const;
	void checkAssumptions() const;
	void checkOrder(IntervalTreeNode<T>* x) const;
#endif

public:

	IntervalTree<T>();
	~IntervalTree<T>();

	void remove(const AbstractInterval<T>& interval);
	void insert(AbstractInterval<T>* interval);
	IntervalTreeNode<T>* find(const AbstractInterval<T>& interval) const;
	void findOverlaps(const AbstractInterval<T>& interval, SYSpVector<AbstractInterval<T>, 256>& intervals) const;
	void clear();
	IntervalTreeNode<T>* getMin() const;
	IntervalTreeNode<T>* getNext(IntervalTreeNode<T>* node) const;
	bool getMin(T& v) const;
	bool getMax(T& v) const;
};

template<class T> inline IntervalTree<T>::IntervalTree() {
	nil_ = new IntervalTreeNode<T>();
	nil_->left_ = nil_;
	nil_->right_ = nil_;
	nil_->parent_ = nil_;
	nil_->red_ = false;
	nil_->maxHigh_ = AbstractInterval<T>::getSmallest();
	nil_->interval_ = new SimpleInterval<T>(AbstractInterval<T>::getSmallest(), AbstractInterval<T>::getSmallest());
	root_ = new IntervalTreeNode<T>();
	root_->parent_ = nil_;
	root_->left_ = nil_;
	root_->right_ = nil_;
	root_->maxHigh_ = AbstractInterval<T>::getLargest();
	root_->interval_ = new SimpleInterval<T>(AbstractInterval<T>::getLargest(), AbstractInterval<T>::getLargest());
	root_->red_ = false;
}

template<class T> inline void IntervalTree<T>::deleteNode(IntervalTreeNode<T>* node) {
	if (node->left_ != nil_) {
		deleteNode(node->left_);
	}
	if (node->right_ != nil_) {
		deleteNode(node->right_);
	}
}

template<class T> inline IntervalTree<T>::~IntervalTree() {
	delete root_->interval_;
	deleteNode(root_);
	delete nil_->interval_;
	delete nil_;
}

template<class T> inline void IntervalTree<T>::clear() {
	if (root_->left_ != nil_) {
		deleteNode(root_->left_);
		root_->left_ = nil_;
	}
}

template<class T> inline void IntervalTree<T>::leftRotate(IntervalTreeNode<T>* x) {
	IntervalTreeNode<T>* y = x->right_;
	x->right_ = y->left_;
	if (y->left_ != nil_) {
		y->left_->parent_ = x;
	}
	y->parent_ = x->parent_;
	if (x == x->parent_->left_) {
		x->parent_->left_ = y;
	} else {
		x->parent_->right_ = y;
	}
	y->left_ = x;
	x->parent_ = y;
	x->maxHigh_ = std::max(x->left_->maxHigh_, std::max(x->right_->maxHigh_, x->interval_->getMax()));
	y->maxHigh_ = std::max(x->maxHigh_, std::max(y->right_->maxHigh_, y->interval_->getMax()));
#ifndef NDEBUG
	checkAssumptions();
#endif
}

template<class T> inline void IntervalTree<T>::rightRotate(IntervalTreeNode<T>* y) {
	IntervalTreeNode<T>* x = y->left_;
	y->left_ = x->right_;
	if (nil_ != x->right_) {
		x->right_->parent_ = y;
	}
	x->parent_ = y->parent_;
	if (y == y->parent_->left_) {
		y->parent_->left_ = x;
	} else {
		y->parent_->right_ = x;
	}
	x->right_ = y;
	y->parent_ = x;
	y->maxHigh_ = std::max(y->left_->maxHigh_, std::max(y->right_->maxHigh_, y->interval_->getMax()));
	x->maxHigh_ = std::max(x->left_->maxHigh_, std::max(y->maxHigh_, x->interval_->getMax()));
#ifndef NDEBUG
	checkAssumptions();
#endif
}

template<class T> inline void IntervalTree<T>::insertHelp(IntervalTreeNode<T>* z) {
	z->left_ = nil_;
	z->right_ = nil_;
	IntervalTreeNode<T>* x = root_->left_;
	IntervalTreeNode<T>* y = root_;
	while (x != nil_) {
		y = x;
		if (x->interval_->compareTo(*z->interval_) > 0) {
			x = x->left_;
		} else {
			x = x->right_;
		}
	}
	z->parent_ = y;
	if (y == root_ || y->interval_->compareTo(*z->interval_) > 0) {
		y->left_ = z;
	} else {
		y->right_ = z;
	}
	assert(!nil_->red_);
	assert(nil_->maxHigh_ == AbstractInterval<T>::getSmallest());
}

template<class T> inline void IntervalTree<T>::fixMaxHigh(IntervalTreeNode<T>* x) {
	while (x != root_) {
		x->maxHigh_ = std::max(x->interval_->getMax(), std::max(x->left_->maxHigh_, x->right_->maxHigh_));
		x = x->parent_;
	}
#ifndef NDEBUG
	checkAssumptions();
#endif
}

template<class T> inline void IntervalTree<T>::insert(AbstractInterval<T>* interval) {
	IntervalTreeNode<T>* x = new IntervalTreeNode<T>(interval);
	insertHelp(x);
	fixMaxHigh(x->parent_);
	assert(x != nil_);
	x->red_ = true;
	while (x->parent_->red_) {
		if (x->parent_ == x->parent_->parent_->left_) {
			IntervalTreeNode<T>* y = x->parent_->parent_->right_;
			if (y->red_) {
				x->parent_->red_ = false;
				y->red_ = false;
				assert(x->parent_->parent_ != nil_);
				x->parent_->parent_->red_ = true;
				x = x->parent_->parent_;
			} else {
				if (x == x->parent_->right_) {
					x = x->parent_;
					leftRotate(x);
				}
				x->parent_->red_ = false;
				assert(x->parent_->parent_ != nil_);
				x->parent_->parent_->red_ = true;
				rightRotate(x->parent_->parent_);
			}
		} else {
			IntervalTreeNode<T>* y = x->parent_->parent_->left_;
			if (y->red_) {
				x->parent_->red_ = false;
				y->red_ = false;
				assert(x->parent_->parent_ != nil_);
				x->parent_->parent_->red_ = true;
				x = x->parent_->parent_;
			} else {
				if (x == x->parent_->left_) {
					x = x->parent_;
					rightRotate(x);
				}
				x->parent_->red_ = false;
				assert(x->parent_->parent_ != nil_);
				x->parent_->parent_->red_ = true;
				leftRotate(x->parent_->parent_);
			}
		}
	}
	root_->left_->red_ = false;
#ifndef NDEBUG
	checkAssumptions();
#endif
}

template<class T> inline IntervalTreeNode<T>* IntervalTree<T>::find(const AbstractInterval<T>& interval) const {
	IntervalTreeNode<T>* x = root_->left_;
	while (x != nil_) {
		const int cmp = x->interval_->compareTo(interval);
		if (cmp > 0) {
			x = x->left_;
		} else if (cmp < 0) {
			x = x->right_;
		} else {
			return x;
		}
	}
	return 0;
}

// STATIC
template<class T> inline bool IntervalTree<T>::overlap(const T low1, const T high1, const T low2, const T high2) {
	if (low1 <= low2) {
		return low2 <= high1;
	} else {
		return low1 <= high2;
	}
}

template<class T> inline void IntervalTree<T>::findOverlaps(IntervalTreeNode<T>* x, const AbstractInterval<T>& interval, SYSpVector<AbstractInterval<T>, 256>& intervals) const {
	if (x == nil_) {
		return;
	}
	const T low = interval.getMin();
	if (low > x->maxHigh_) {
		return;
	}
	findOverlaps(x->left_, interval, intervals);
	const T high = interval.getMax();
	if (IntervalTree<T>::overlap(low, high, x->interval_->getMin(), x->interval_->getMax())) {
		intervals.append(x->interval_);
	}
	if (high < x->interval_->getMin()) {
		return;
	}
	findOverlaps(x->right_, interval, intervals);
}

template<class T> inline void IntervalTree<T>::findOverlaps(const AbstractInterval<T>& interval, SYSpVector<AbstractInterval<T>, 256>& intervals) const {
	findOverlaps(root_->left_, interval, intervals);
}

template<class T> inline IntervalTreeNode<T>* IntervalTree<T>::getSuccessorOf(IntervalTreeNode<T>* x) const {
	IntervalTreeNode<T>* y = x->right_;
	if (y != nil_) {	
		while (y->left_ != nil_) {
			y = y->left_;
		}
		return y;
	} else {
		y = x->parent_;
		while (x == y->right_) {
			x = y;
			y = y->parent_;
		}
		if (y == root_) {
			return nil_;
		}
		return y;
	}
}
template<class T> inline void IntervalTree<T>::removeFixUp(IntervalTreeNode<T>* x) {
	IntervalTreeNode<T>* rootLeft = root_->left_;
	while (!x->red_ && rootLeft != x) {
		if (x == x->parent_->left_) {
			IntervalTreeNode<T>* w = x->parent_->right_;
			if (w->red_) {
				w->red_ = false;
				assert(x->parent_ != nil_);
				x->parent_->red_ = true;
				leftRotate(x->parent_);
				w = x->parent_->right_;
			}
			if (!w->right_->red_ && !w->left_->red_) {
				assert(w != nil_);
				w->red_ = true;
				x = x->parent_;
			} else {
				if (!w->right_->red_) {
					w->left_->red_ = false;
					assert(w != nil_);
					w->red_ = true;
					rightRotate(w);
					w = x->parent_->right_;
				}
				assert(!x->parent_->red_ || w != nil_);
				w->red_ = x->parent_->red_;
				x->parent_->red_ = false;
				w->right_->red_ = false;
				leftRotate(x->parent_);
				x = rootLeft;
			}
		} else {
			IntervalTreeNode<T>* w = x->parent_->left_;
			if (w->red_) {
				w->red_ = false;
				assert(x->parent_ != nil_);
				x->parent_->red_ = true;
				rightRotate(x->parent_);
				w = x->parent_->left_;
			}
			if (!w->right_->red_ && !w->left_->red_) {
				assert(w != nil_);
				w->red_ = true;
				x = x->parent_;
			} else {
				if (!w->left_->red_) {
					w->right_->red_ = false;
					assert(w != nil_);
					w->red_ = true;
					leftRotate(w);
					w = x->parent_->left_;
				}
				assert(!x->parent_->red_ || w != nil_);
				w->red_ = x->parent_->red_;
				x->parent_->red_ = false;
				w->left_->red_ = false;
				rightRotate(x->parent_);
				x=rootLeft;
			}
		}
	}
	x->red_ = false;
#ifndef NDEBUG
	checkAssumptions();
#endif
}

template<class T> inline void IntervalTree<T>::remove(const AbstractInterval<T>& interval) {
	IntervalTreeNode<T>* z = find(interval);
	if (z == 0) {
		// Not found: do nothing.
		return;
	}
	IntervalTreeNode<T>* y = (z->left_ == nil_ || z->right_ == nil_) ? z : getSuccessorOf(z);
	IntervalTreeNode<T>* x = (y->left_ == nil_) ? y->right_ : y->left_;
	x->parent_ = y->parent_;
	if (root_ == x->parent_) {
		root_->left_ = x;
	} else {
		if (y == y->parent_->left_) {
			y->parent_->left_ = x;
		} else {
			y->parent_->right_ = x;
		}
	}
	if (y != z) {
		assert(y != nil_);
		y->maxHigh_ = AbstractInterval<T>::getSmallest();
		y->left_ = z->left_;
		y->right_ = z->right_;
		y->parent_ = z->parent_;
		z->left_->parent_ = y;
		z->right_->parent_ = y;
		if (z == z->parent_->left_) {
			z->parent_->left_ = y;
		} else {
			z->parent_->right_ = y;
		}
		fixMaxHigh(x->parent_);
		if (!y->red_) {
			assert(!z->red_ || z != nil_);
			y->red_ = z->red_;
			removeFixUp(x);
		} else {
			assert(!z->red_ || z != nil_);
			y->red_ = z->red_;
		}
		z->left_ = nil_;
		z->right_ = nil_;
		delete z;
	} else {
		fixMaxHigh(x->parent_);
		if (!y->red_) {
			removeFixUp(x);
		}
		y->left_ = nil_;
		y->right_ = nil_;
		delete y;
	}
#ifndef NDEBUG
	checkAssumptions();
	assert(find(interval) == 0);
#endif
}

template<class T> inline IntervalTreeNode<T>* IntervalTree<T>::getMin() const {
	IntervalTreeNode<T>* x = root_->left_;
	if (x == nil_) {
		return 0;
	} else {
		while (x->left_ != nil_) {
			x = x->left_;
		}
		return x;
	}
}

template<class T> inline IntervalTreeNode<T>* IntervalTree<T>::getNext(IntervalTreeNode<T>* node) const {
	IntervalTreeNode<T>* x = getSuccessorOf(node);
	return x == nil_ ? 0 : x;
}

template<class T> inline bool IntervalTree<T>::getMin(T& v) const {
	IntervalTreeNode<T>* x = root_->left_;
	if (x == nil_) {
		return false;
	} else {
		while (x->left_ != nil_) {
			x = x->left_;
		}
		v = x->interval_->getMin();
		return true;
	}
}

template<class T> inline bool IntervalTree<T>::getMax(T& v) const {
	if (root_->left_ == nil_) {
		return false;
	} else {
		v = root_->left_->maxHigh_;
		return true;
	}
}

#ifndef NDEBUG

template<class T> inline T IntervalTree<T>::checkMaxHighFieldsHelper(IntervalTreeNode<T>* y, const T currentHigh, T match) const {
	if (y != nil_) {
		match = checkMaxHighFieldsHelper(y->left_, currentHigh, match) ? 1 : match;
		assert(y->interval_->getMax() <= currentHigh);
		if (y->interval_->getMax() == currentHigh) {
			match = 1;
		}
		match = checkMaxHighFieldsHelper(y->right_, currentHigh, match) ? 1 : match;
	}
	return match;
}

template<class T> inline void IntervalTree<T>::checkMaxHighFields(IntervalTreeNode<T>* x) const {
	if (x != nil_) {
		checkMaxHighFields(x->left_);
		assert(checkMaxHighFieldsHelper(x, x->maxHigh_, 0) > 0);
		checkMaxHighFields(x->right_);
	}
}

template<class T> inline void IntervalTree<T>::checkOrder(IntervalTreeNode<T>* x) const {
	if (x != nil_) {
		assert(x->left_ == nil_ || x->interval_->compareTo(*x->left_->interval_) > 0);
		checkOrder(x->left_);
		assert(x->right_ == nil_ || x->interval_->compareTo(*x->right_->interval_) <= 0);
		checkOrder(x->right_);
	}
}

template<class T> inline void IntervalTree<T>::checkAssumptions() const {
	assert(nil_->interval_->getMin() == AbstractInterval<T>::getSmallest());
	assert(nil_->interval_->getMax() == AbstractInterval<T>::getSmallest());
	assert(nil_->maxHigh_ == AbstractInterval<T>::getSmallest());
	assert(root_->interval_->getMin() == AbstractInterval<T>::getLargest());
	assert(root_->interval_->getMax() == AbstractInterval<T>::getLargest());
	assert(root_->maxHigh_ == AbstractInterval<T>::getLargest());
	assert(nil_->red_ == false);
	assert(root_->red_ == false);
#if 0
	// This can be very expensive if there are a lot of nodes!
	checkMaxHighFields(root_->left_);
	checkOrder(root_->left_);
#endif
}

#endif

}

#endif /* #ifndef _engine_intervaltree_h_ */
