/*
	Sort helpers.
*/

#include "sort.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SortTest
//////////////////////////////////////////////////////////////////////////////////////////////////////

// STATIC
void SortTest::run() {
	uint32_t n = 10;
	for (;;) {
		uint32_t max = n * 10;
		IndirectorTest indirector;
		SYSxvector<uint32_t> data;
		for (uint32_t i = 0; i < n; ++i) {
			indirector.append(i);
			const uint32_t v = static_cast<uint32_t>(max * static_cast<double>(rand()) / RAND_MAX);
			data.append(v);
		}
		const ComparatorTest comparator(data);
		uint64_t t = my_micro_time();
		Sort<IndirectorTest, ComparatorTest>::quickSort(indirector, comparator, 0, n);
		spw_print_information("Quicksort: %u integers in %s", n, Str::fromDuration((my_micro_time() - t) / 1000).c_str());
		uint32_t previous = 0;
		for (uint32_t i = 0; i < n; ++i) {
			const uint32_t v = data[indirector[i]];
			if (i > 0) {
				if (v < previous) {
					spw_print_error("Data not sorted!!");
					break;
				}
			}
			previous = v;
		}
		if (n == 100000000) {
			break;
		} else {
			n *= 10;
		}
	}
}

}
