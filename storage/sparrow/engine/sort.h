/*
	Sort helpers.
*/

#ifndef _engine_sort_h_
#define _engine_sort_h_

#include "search.h"
#include "thread.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Sort
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class I, class C> class Sort {
private:

	static int med3(const I& indirector, const C& comparator, const int a, const int b, const int c);

	static void swap(I& indirector, const int a, const int b);

	static void vecswap(I& indirector, int a, int b, const int n);

public:

	static void quickSort(I& indirector, const C& comparator, int off, int len);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Sort
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Stacked ranges to avoid recursion.
class SortRange {
public:
	int off_;
	int len_;

	SortRange() : off_(0), len_(0) {
	}

	SortRange(const int off, const int len) : off_(off), len_(len) {
	}
};

// STATIC
template<class I, class C> inline void Sort<I, C>::swap(I& indirector, const int a, const int b) {
	if (a != b) {
		uint32_t* va = &indirector[a];
		uint32_t* vb = &indirector[b];
		const uint32_t t = *va;
		*va = *vb;
		*vb = t;
	}
}

// STATIC
template<class I, class C> inline void Sort<I, C>::vecswap(I& indirector, int a, int b, const int n) {
	for (int i = 0; i < n; ++i, ++a, ++b) {
		swap(indirector, a, b);
	}
}

// Returns the index of the median of the three indexed integers.
// STATIC
template<class I, class C> inline int Sort<I, C>::med3(const I& indirector, const C& comparator, const int a, const int b, const int c) {
	if (comparator.compare(indirector[a], indirector[b]) < 0) {
		if (comparator.compare(indirector[b], indirector[c]) < 0) {
			return b;
		} else if (comparator.compare(indirector[a], indirector[c]) < 0) {
			return c;
		} else {
			return a;
		}
	} else {
		if (comparator.compare(indirector[b], indirector[c]) > 0) {
			return b;
		} else if (comparator.compare(indirector[a], indirector[c]) > 0) {
			return c;
		} else {
			return a;
		}
	}
}

// Quick sort (same code as in JDK, but not recursive).
// STATIC
template<class I, class C> void Sort<I, C>::quickSort(I& indirector, const C& comparator, int off, int len) {
	SortRange stack[32];
	int top = 0;
	bool recurse = true;
	while (recurse) {
		recurse = false;

		// Insertion sort on smallest arrays.
		if (len < 7) {
			const int hi = off + len - 1;
			for (int i = off; i <= hi; ++i) {
				for (int j = i; j > off && comparator.compare(indirector[j - 1], indirector[j]) > 0; --j) {
					swap(indirector, j, j - 1);
				}
			}
		} else {
			// Choose a partition element, v.
			int m = off + (len >> 1); // Small arrays, middle element.
			if (len > 7) {
				int l = off;
				int n = off + len - 1;
				if (len > 40) { // Big arrays, pseudomedian of 9.
					const int s = len / 8;
					l = Sort::med3(indirector, comparator, l, l + s, l + 2 * s);
					m = Sort::med3(indirector, comparator, m - s, m, m + s);
					n = Sort::med3(indirector, comparator, n - 2 * s, n - s, n);
				}
				m = Sort::med3(indirector, comparator, l, m, n); // Mid-size, med of 3.
			}
			const int v = indirector[m];

			// Establish Invariant: v* (<v)* (>v)* v*.
			int a = off, b = a, c = off + len - 1, d = c;
			while (true) {
				while (b <= c) {
					const int cmp = comparator.compare(indirector[b], v);
					if (cmp > 0) {
						break;
					} else if (cmp == 0) {
						swap(indirector, a++, b);
					}
					b++;
				}
				while (c >= b) {
					const int cmp = comparator.compare(indirector[c], v);
					if (cmp < 0) {
						break;
					} else if (cmp == 0) {
						swap(indirector, c, d--);
					}
					c--;
				}
				if (b > c) {
					break;
				}
				swap(indirector, b++, c--);
			}

			// Swap partition elements back to middle.
			int s;
			const int n = off + len;
			s = std::min(a - off, b - a);
			vecswap(indirector, off, b - s, s);
			s = std::min(d - c, n - d - 1);
			vecswap(indirector, b, n - s, s);

			const int s1 = b - a;
			const int s2 = d - c;
			if (s1 > s2) {
				if (s1 > 1) {
					stack[top++] = SortRange(off, s1);
				}
				if (s2 > 1) {
					off = n - s2;
					len = s2;
					recurse = true;
				}
			} else {
				if (s2 > 1) {
					stack[top++] = SortRange(n - s2, s2);
				}
				if (s1 > 1) {
					len = s1;
					recurse = true;
				}
			}
		}
		if (!recurse) {
			if (--top >= 0) {
				const SortRange& r = stack[top];
				off = r.off_;
				len = r.len_;
				recurse = true;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SortTest
//////////////////////////////////////////////////////////////////////////////////////////////////////

class SortTest {
public:

	static void run();
};

typedef SYSxvector<uint32_t> IndirectorTest;

class ComparatorTest {
private:

	const SYSxvector<uint32_t>& data_;

public:

	ComparatorTest(const SYSxvector<uint32_t>& data) : data_(data) {
	}

	int compare(const uint32_t row1, const uint32_t row2) const {
		const uint32_t v1 = data_[row1];
		const uint32_t v2 = data_[row2];
		return v1 < v2 ? -1 : (v1 > v2 ? 1 : 0);
	}

};

}

#endif /* #ifndef _engine_sort_h_ */
