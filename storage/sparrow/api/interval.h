/*
	Generic interval.
*/

#ifndef _engine_interval_h_
#define _engine_interval_h_

#include "intervaltree.h"

namespace Sparrow {

template<typename T> class Interval : public AbstractInterval<T> {
private:
	T lower_;
	T upper_;
	unsigned int lowerIncluded_:1;
	unsigned int upperIncluded_:1;
	unsigned int lowerSet_:1;
	unsigned int upperSet_:1;
	unsigned int pad_:28;

private:

	// Creates a void (empty) interval.
	// This constructor is private and takes a dummy parameter to distinguish from default constructor.
	Interval(const bool foo) {
		lowerSet_ = false;
		upperSet_ = false;
		lowerIncluded_ = true;
		upperIncluded_ = true;
	}

	// Checks whether the given interval info is valid.
	static bool check(const T* lower, const bool lowerIncluded, const T* upper, const bool upperIncluded) {
		if ((lower == 0 && lowerIncluded) || (upper == 0 && upperIncluded)) {
			// Infinite bound cannot be included in interval.
			return false;
		}
		if (lower != 0 && upper != 0) {
			if (*lower > *upper) {
				// Lower bound cannot be greater than upper bound.
				return false;
			} else if (!(*upper > *lower)) {
				// Bounds are equal.
				if (!lowerIncluded || !upperIncluded) {
					// Single value with lower or upper bound excluded.
					return false;
				}
			}
		}
		return true;
	}

	// Compare two bounds.
	static bool compareBounds(const T* bound1, const T* bound2) {
		if (bound1 == 0) {
			return bound2 == 0;
		} else {
			return bound2 != 0 && *bound1 == *bound2;
		}
	}

	// Compare two upper or lower bounds.
	static int compareBounds(const T* bound1, const T* bound2, const bool upper) {
		if (bound1 == 0) {
			if (bound2 == 0) {
				return 0;
			} else if (upper) {
				return 1;
			} else {
				return -1;
			}
		} else if (bound2 != 0) {
			if (*bound1 < *bound2) {
				return -1;
			} else if (*bound2 < *bound1) {
				return 1;
			}
			return 0;
		} else if (upper) {
			return -1;
		} else {
			return 1;
		}
	}

public:

	Interval() : lowerIncluded_(false), upperIncluded_(false), lowerSet_(false), upperSet_(false) {
	}
	Interval(const T bound) : lower_(bound), upper_(bound), lowerIncluded_(true), upperIncluded_(true), lowerSet_(true), upperSet_(true) {
	}
	Interval(const T lower, const T upper) : lower_(lower), upper_(upper), lowerIncluded_(true), upperIncluded_(true), lowerSet_(true), upperSet_(true) {
	}
	Interval(const T* lower, const T* upper, const bool lowerIncluded, const bool upperIncluded) : lowerIncluded_(lowerIncluded), upperIncluded_(upperIncluded),
		lowerSet_(lower != 0), upperSet_(upper != 0) {
		if (lower != 0) {
			lower_ = *lower;
		}
		if (upper != 0) {
			upper_ = *upper;
		}
	}

	// Gets lower bound, returns 0 if -infinite.
	const T* getLow() const {
		return lowerSet_ ? &lower_ : 0;
	}

	// Gets upper bound, returns 0 if +infinite.
	const T* getUp() const {
		return upperSet_ ? &upper_ : 0;
	}

	T getLength() const {
		return *getUp() - *getLow();
	}

	bool isLowerIncluded() const {
		return lowerIncluded_;
	}

	bool isUpperIncluded() const {
		return upperIncluded_;
	}

	bool isVoid() const {
		return lowerIncluded_ && upperIncluded_ && !lowerSet_ && !upperSet_;
	}

	bool isAll() const {
		return !lowerIncluded_ && !upperIncluded_ && !lowerSet_ && !upperSet_;
	}

	bool isPoint() const {
		return lowerSet_ && upperSet_ && lowerIncluded_ && upperIncluded_ && lower_ == upper_;
	}

	bool contains(const Interval<T>& interval) const {
		if (interval.isVoid()) {
			return true;
		}
		if (isVoid()) {
			return false;
		}
		const int lowerCmp = compareBounds(getLow(), interval.getLow(), false);
		if (lowerCmp > 0 || (lowerCmp == 0 && !lowerIncluded_ && interval.lowerIncluded_)) {
			return false;
		}
		const int upperCmp = compareBounds(getUp(), interval.getUp(), true);
		if (upperCmp < 0 || (upperCmp == 0 && !upperIncluded_ && interval.upperIncluded_)) {
			return false;
		}
		return true;
	}

	bool contains(const T& value) const {
		if (isVoid()) {
			return false;
		}
		const int lowerCmp = compareBounds(getLow(), &value, false);
		if (lowerCmp > 0 || (lowerCmp == 0 && !lowerIncluded_)) {
			return false;
		}
		const int upperCmp = compareBounds(getUp(), &value, true);
		if (upperCmp < 0 || (upperCmp == 0 && !upperIncluded_)) {
			return false;
		}
		return true;
	}

	bool isAdjacent(const Interval<T>& interval) const {
		if (compareBounds(getUp(), interval.getLow())
				&& upperIncluded_ == !interval.lowerIncluded_) {
			return true;
		}
		if (compareBounds(getLow(), interval.getUp())
				&& lowerIncluded_ == !interval.upperIncluded_) {
			return true;
		}
		return false;
	}

	Interval<T> makeIntersection(const Interval<T>& interval) const {
		if (isVoid() || interval.isVoid()) {
			// One of the interval is void: return void.
			return Interval<T>(false);
		} else if (isAll()) {
			// One of the interval is all: return the other one.
			return interval;
		} else if (interval.isAll()) {
			return *this;
		} else if (isAdjacent(interval)) {
			// Intervals are adjacent: return void.
			return Interval<T>(false);
		}
		const int lowerCmp = compareBounds(getLow(), interval.getLow(), false);
		const T* lowerBound = lowerCmp >= 0 ? getLow() : interval.getLow();
		const bool lowerBoundIncluded = lowerCmp >= 0 ? lowerIncluded_ : interval.lowerIncluded_;
		const int upperCmp = compareBounds(getUp(), interval.getUp(), true);
		const T* upperBound = upperCmp >= 0 ? interval.getUp() : getUp();
		const bool upperBoundIncluded = upperCmp >= 0 ? interval.upperIncluded_ : upperIncluded_;
		if (check(lowerBound, lowerBoundIncluded, upperBound, upperBoundIncluded)) {
			return Interval<T>(lowerBound, upperBound, lowerBoundIncluded, upperBoundIncluded);
		} else {
			// No intersection: return void interval.
			return Interval<T>(false);
		}
	}

	bool intersects(const Interval<T>& interval) const {
		if (isVoid() || interval.isVoid()) {
			return false;
		} else if (isAll() || interval.isAll()) {
			return true;
		} else if (isAdjacent(interval)) {
			return false;
		}
		const int lowerCmp = compareBounds(getLow(), interval.getLow(), false);
		const T* lowerBound = lowerCmp >= 0 ? getLow() : interval.getLow();
		const bool lowerBoundIncluded = lowerCmp >= 0 ? lowerIncluded_ : interval.lowerIncluded_;
		const int upperCmp = compareBounds(getUp(), interval.getUp(), true);
		const T* upperBound = upperCmp >= 0 ? interval.getUp() : getUp();
		const bool upperBoundIncluded = upperCmp >= 0 ? interval.upperIncluded_ : upperIncluded_;
		return check(lowerBound, lowerBoundIncluded, upperBound, upperBoundIncluded);
	}

	Interval<T> makeUnion(const Interval<T>& interval) const {
		// One of the interval is void: return the other one.
		if (isVoid()) {
			return interval;
		}
		if (interval.isVoid()) {
			return *this;
		}

		// One of the interval is all: return all.
		if (isAll()) {
			return *this;
		}
		if (interval.isAll()) {
			return interval;
		}

		// Cannot merge intervals that do not intersect.
		if (!intersects(interval) && !isAdjacent(interval)) {
			return Interval<T>(false);
		}
		const int lowerCmp = compareBounds(getLow(), interval.getLow(), false);
		const int upperCmp = compareBounds(getUp(), interval.getUp(), true);
		const T* lowerBound = lowerCmp >= 0 ? interval.getLow() : getLow();
		const bool lowerBoundIncluded = lowerCmp >= 0 ? interval.lowerIncluded_ : lowerIncluded_;
		const T* upperBound = upperCmp >= 0 ?  getUp() : interval.getUp();
		const bool upperBoundIncluded = upperCmp >= 0 ? upperIncluded_ : interval.upperIncluded_;
		return Interval<T>(lowerBound, upperBound, lowerBoundIncluded, upperBoundIncluded);
	}

	// Returns one or two intervals adjacent to this interval so the union of
	// all those intervals represent all values.
	// There are two result intervals if and only if !result[1].isVoid().
	void makeNot(Interval<T>* result) const {
		if (isAll()) {
			result[0] = Interval<T>(false);
			result[1] = Interval<T>(false);
		} else if (isVoid()) {
			result[0] = Interval<T>();
			result[1] = Interval<T>(false);
		} else {
			const T* lowerBound = getLow();
			const T* upperBound = getUp();
			int index = 0;
			if (lowerBound == 0 || upperBound != 0) {
				result[index++] = Interval<T>(upperBound, 0, !isUpperIncluded(), false);
			}
			if (lowerBound != 0 || upperBound == 0) {
				result[index] = Interval<T>(0, lowerBound, false, !isLowerIncluded());
			}
		}
	}

	// For sorting, use the lower bound only.
	bool operator < (const Interval<T>& interval) const {
		const int lowerCmp = compareBounds(getLow(), interval.getLow(), false);
		if (lowerCmp != 0) {
			return lowerCmp < 0;
		}
		if (lowerIncluded_) {
			return !interval.lowerIncluded_;
		} else {
			return false;
		}
	}

	bool operator == (const Interval<T>& interval) const {
		return lower_ == interval.lower_ && upper_ == interval.upper_
			&& lowerIncluded_ == interval.lowerIncluded_ && upperIncluded_ == interval.upperIncluded_
			&& lowerSet_ == interval.lowerSet_ && upperSet_ == interval.upperSet_;
	}

	// Implementation of AbstractInterval<T>.
	T getMin() const override {
		const T* low = getLow();
		if (low == 0) {
			return AbstractInterval<T>::getSmallest();
		} else {
			return *low;
		}
	}

	T getMax() const override {
		const T* up = getUp();
		if (up == 0) {
			return AbstractInterval<T>::getLargest();
		} else {
			return *up;
		}
	}

	int compareTo(const AbstractInterval<T>& right) const override {
		if (getMin() == right.getMin()) {
			return 0;
		} else if (getMin() < right.getMin()) {
			return -1;
		} else {
			return 1;
		}
	}
};

}

#endif /* #ifndef _engine_interval_h_ */
