/*
	Tree order.
*/

#ifndef _engine_treeorder_h_
#define _engine_treeorder_h_

#include "vec.h"
#include "lock.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeNode
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TreeNode {
private:

	uint32_t start_;
	uint32_t end_;

public:

	TreeNode() : start_(0), end_(0) {
	}
	TreeNode(const uint32_t start, const uint32_t end) : start_(start), end_(end) {
	}
	uint32_t getStart() const {
		return start_;
	}
	uint32_t getEnd() const {
		return end_;
	}
};

typedef SYSvector<TreeNode> TreeNodes;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeOrderElement
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TreeOrderElement {
private:

	uint32_t listIndex_;
	uint32_t nodeIndex_;

public:

	TreeOrderElement() : listIndex_(0), nodeIndex_(0) {
	}
	uint32_t getListIndex() const {
		return listIndex_;
	}
	void setListIndex(const uint32_t listIndex) {
		listIndex_ = listIndex;
	}
	uint32_t getNodeIndex() const {
		return nodeIndex_;
	}
	void setNodeIndex(const uint32_t nodeIndex) {
		nodeIndex_ = nodeIndex;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TreeOrder
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Given a sorted list, this class gives the writing order of the related binary tree.
// It also gives the node index for a given list index.
class TreeOrder : private SYSvector<TreeOrderElement> {
private:

	static SYSpVector<TreeOrder, 0> orders_;
	static Lock lock_;

private:

	void setListIndex(const uint32_t nodeIndex, const uint32_t listIndex) {
		(*this)[nodeIndex].setListIndex(listIndex);
		(*this)[listIndex].setNodeIndex(nodeIndex);
	}

public:

	TreeOrder(const uint32_t depth);
	static uint32_t depth(const uint32_t cardinality);
	static const TreeOrder& get(const uint32_t cardinality);
	uint32_t getListIndex(const uint32_t nodeIndex, const uint32_t n) const {
		// Adjust listIndex in case of almost perfect tree (remove extra leafs).
		uint32_t listIndex = (*this)[nodeIndex].getListIndex();
		const uint32_t last = (n * 2) - length();
		if (listIndex > last) {
			listIndex -= (listIndex - last) / 2;
		}
		return listIndex;
	}

	uint32_t getNodeIndex(uint32_t listIndex, const uint32_t n) const {
		// Reverse adjustment on listIndex in case of almost perfect tree.
		const uint32_t last = (n * 2) - length();
		if (listIndex > last) {
			listIndex = 2 * listIndex - last;
		}
		const uint32_t nodeIndex = (*this)[listIndex].getNodeIndex();
		return nodeIndex;
	}
};

}

#endif /* #ifndef _engine_treeorder_h_ */
