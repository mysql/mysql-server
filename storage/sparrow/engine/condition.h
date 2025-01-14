/*
	Analyzer for WHERE condition: find timestamp intervals.
*/

#ifndef _engine_condition_h_
#define _engine_condition_h_

#include "types.h"

struct TABLE;

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TimePeriods
//////////////////////////////////////////////////////////////////////////////////////////////////////

class TimePeriods : public SYSsortedVector<TimePeriod> {
public:

	TimePeriods(const uint32_t size = 0) : SYSsortedVector<TimePeriod>(size) {
	}

	void makeAnd(const TimePeriods& right);

	void makeOr(const TimePeriod& right);

	void makeOr(const TimePeriods& right);

	void makeNot();

	void compact();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Condition
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Condition {
private:

	static TimePeriods get(TABLE* table, Item* item, const bool returnAllIfNone);

	static uint64_t getBound(Item* item, const bool isTimestamp);

public:

	static TimePeriods getPeriods(TABLE* table, Item* cond, const uint64_t lower);
};

}

#endif /* #ifndef _engine_condition_h_ */
