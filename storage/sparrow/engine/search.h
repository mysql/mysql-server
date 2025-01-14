/*
	Search helpers.
*/

#ifndef _engine_search_h_
#define _engine_search_h_

#include "partition.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BinarySearch
//////////////////////////////////////////////////////////////////////////////////////////////////////

template<class C> class BinarySearch {
public:

	static uint32_t find(C& comparator, const uint32_t start, const uint32_t n, const SearchFlag searchFlag);
};

// Performs a binary search to find the record matching the given context's comparator and search searchFlag.
// STATIC
template<class C> uint32_t BinarySearch<C>::find(C& comparator, const uint32_t start, const uint32_t n, const SearchFlag searchFlag) {
	if (n == 0) {
		return UINT_MAX;
	}

	bool doSearch = true;
	bool found = false;

	// Check lower record.
	const uint32_t end = start + n - 1;
	uint32_t row = start;
	int cmp = comparator.compareTo(row);
	if (cmp == 0) {
		doSearch = false;
		found = true;
	} else if (cmp > 0) {	// First record greater than key: skip search.
		doSearch = false;
	} else {
		// Check upper record.
		row = end;
		cmp = comparator.compareTo(row);
		if (cmp < 0) {	// Last record smaller than key: skip search.
			doSearch = false;
		}
	}
	
	if (doSearch) {
		uint32_t bottom = start;
		uint32_t top = end;
		while (top > bottom) {
			// Compare record and key.
			row = (top + bottom) >> 1;
			cmp = comparator.compareTo(row);
			if (cmp == 0) {
				found = true;
				break;
			}
			else if (cmp > 0) {
				top = row > start ? row - 1 : start;
			}
			else {
				bottom = row + 1;
			}
		}
		if (!found) {
			row = bottom;
			if (comparator.compareTo(row) == 0) {
				found = true;
			}
		}
	}
	if (found) {
		// Record found.
		if (searchFlag == SearchFlag::EQ || searchFlag == SearchFlag::GE
			|| searchFlag == SearchFlag::LE) {
			// Go down to the first one.
			while (row > start && comparator.compareTo(row - 1) == 0) {
				row--;
			}
		} else if (searchFlag == SearchFlag::LE_LAST) {
			// Go up to the last one.
			while (row + 1 < end && comparator.compareTo(row + 1) == 0) {
				row++;
			}
		} else if (searchFlag == SearchFlag::LT) {
			// Go down to the previous one, if any.
			if (row == start) {
				return UINT_MAX;
			}
			row--;
			while (comparator.compareTo(row) == 0) {
				if (row-- == start) {
					return UINT_MAX;
				}
			}
		} else if (searchFlag == SearchFlag::GT) {
			// Go up to the next one, if any.
			if (row == end) {
				return UINT_MAX;
			}
			++row;
			while (comparator.compareTo(row) == 0) {
				if (++row > end) {
					return UINT_MAX;
				}
			}
		}
	} else {
		// Record not found. Row is set on the previous record, if any.
		if (searchFlag == SearchFlag::EQ) {
			return UINT_MAX;
		} else if (searchFlag == SearchFlag::LE || searchFlag == SearchFlag::LE_LAST
			|| searchFlag == SearchFlag::LT) {
			// Go up to the largest previous one, if any.
			while (row <= end && comparator.compareTo(row) < 0) {
				row++;
			}
			if (row == start) {
				return UINT_MAX;
			}
			row--;
		} else if (searchFlag == SearchFlag::GE || searchFlag == SearchFlag::GT) {
			// Go up to the smallest next one, if any.
			while (row <= end && comparator.compareTo(row) < 0) {
				row++;
			}
			if (row == end + 1) {
				return UINT_MAX;
			}
		}
	}
	return row;
}

}

#endif /* #ifndef _engine_search_h_ */
