/*
	Semaphore.
*/

#ifndef _spw_api_sema_h_
#define _spw_api_sema_h_

#include "cond.h"
#include "misc.h"
#include "str.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Sema
//////////////////////////////////////////////////////////////////////////////////////////////////////

class Sema {
private:

	Cond cond_;
	uint32_t volatile count_;

public:

	Sema(const char* name, const uint32_t count = 0) : cond_(false, (Str(name) + Str("::cond_")).c_str()), count_(count) {
	}

	~Sema() {
	}

	void post(const uint32_t count = 1) {
		Guard guard(cond_.getLock());
		count_ += count;
		cond_.signalAll(true);
	}

	bool wait(const uint64_t milliseconds, const bool all = false) {
		Guard guard(cond_.getLock());
		while (count_ == 0) {
			if (!cond_.wait(milliseconds, true)) {
				return false;
			}
		}
		if (all) {
			count_ = 0;
		} else {
			count_--;
		}
		return true;
	}

	bool wait(const bool all = false) {
		return wait(0, all);
	}
};

}

#endif /* #ifndef _spw_api_sema_h_ */
