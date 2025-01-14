/*
	Tree order.
*/

#include "types.h"
#include "treeorder.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeOrder
//////////////////////////////////////////////////////////////////////////////////////////////////////

// A TreeOrder is an array of integers designed for writing sequentially a sorted list as a perfect
// binary tree to a file, thus enabling fast search with good locality when reading from the file.
// See http://en.wikipedia.org/wiki/Binary_tree and http://en.wikipedia.org/wiki/Binary_tree#Methods_for_storing_binary_trees.
// Considering a sorted list of size 2^n - 1, for i = 0..2^n - 2, TreeOrder[i] gives the index in the
// list for node i in a perfect binary tree (i is the node index, in writing order).
// Example for depth = 3 (7 elements):
// - Sorted list:
//
// [0][1][2][3][4][5][6]
//
// - Perfect binary tree containing all list elements:
//
//         __3__
//         |   |
//       _1_   _5_
//       | |   | |
//      0   2 4   6
//
// - Related TreeOrder array:
//
// [3][1][5][0][2][4][6]

// Following the tree order indirection, we can write a sorted list as a perfect binary tree
// directly to a file.

// To avoid computing a TreeOrder array every time we need it, cache TreeOrder objects by depth.
SYSpVector<TreeOrder, 0> TreeOrder::orders_;
Lock TreeOrder::lock_(true, "TreeOrder::lock_");

// STATIC
uint32_t TreeOrder::depth(const uint32_t cardinality) {
	// Find depth of the nearest perfect tree.
	uint32_t depth = 1;
	while (cardinality > (static_cast<uint32_t>(1) << depth) - 1) {
		depth++;
	}
	return depth;
}

// Creates a new tree order for the given depth.
TreeOrder::TreeOrder(const uint32_t depth) {
	const uint32_t size = (static_cast<uint32_t>(1) << depth) - 1;
	resize(size);
	forceLength(size);

	// Compute indirection using an iterative method.
	uint32_t node = 0;
	setListIndex(node++, size >> 1);	// Starting node: middle of the list.
	uint32_t width = (1 << (depth - 1));	// Starting width.
	for (uint32_t i = 1; i < depth; ++i) {
		width >>= 1;					// Width decreases as we go down the tree.
		for (uint32_t j = 0; j < (static_cast<uint32_t>(1) << i); j += 2) {
			const uint32_t parent = (node >> 1);				// Parent node id.
			setListIndex(node++, getListIndex(parent, size) - width);	// Left child.
			setListIndex(node++, getListIndex(parent, size) + width);	// Right child.
		}
	}
}

// Gets or creates a tree order for the given cardinality.
// STATIC
const TreeOrder& TreeOrder::get(const uint32_t cardinality) {
	const uint32_t d = depth(cardinality);
	Guard guard(lock_);
	const uint32_t length = orders_.length();
	if (d >= length) {
		orders_.resize(d + 1);
		orders_.forceLength(d + 1);
		for (uint32_t i = length; i <= d; ++i) {
			orders_[i] = 0;
		}
	}
	if (orders_[d] == 0) {
		orders_[d] = new TreeOrder(d);
	}
	return *orders_[d];
}

}


