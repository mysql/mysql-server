/*
	Database automatic purge.
*/

#ifndef _engine_purge_h_
#define _engine_purge_h_

#include "../handler/plugin.h"	// For configuration parameters.
#include "thread.h"
#include "sema.h"
#include "master.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Purge
//////////////////////////////////////////////////////////////////////////////////////////////////////

enum PurgeMode {
	PURGE_MODE_ON_INSERTION,
	PURGE_MODE_CONSTANTLY
};

class Purge : public Thread {
private:

	static Purge* purge_;

	Sema sema_;

	uint64_t last_;

	bool logResult_;

	static const uint64_t securityMargin_;

protected:

	bool process() override;

	bool notifyStop() override;

	bool deleteAfterExit() override {
		return false;
	}

public:

	Purge() : Thread("Purge::purge_"), sema_("Purge::sema_", 0), last_(ULLONG_MAX / 2), logResult_(false) {
	}

	~Purge() {
	}

	static void initialize() _THROW_(SparrowException);

	static void shutdown() {
		if ( purge_ != 0 )
			purge_->stop();
	}

	static void wakeUp(bool logResult = false);

	static uint64_t getSecurityMargin() {
		return sparrow_purge_security_margin;
	}

	static uint64_t getLimit(const uint64_t freeDiskSpace, const uint64_t total);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// PurgeTask
//////////////////////////////////////////////////////////////////////////////////////////////////////

class PurgeTask : public Task {
public:

	PurgeTask() : Task(Worker::getQueue()) {
	}

	virtual bool operator == (const PurgeTask& right) const {
		return true;
	}

	virtual bool operator == (const Task& right) const override {
		return false;
	}

	uint64_t getPeriod() const override {
		return 86400000L;
	}

	void run(const uint64_t timestamp) override _THROW_(SparrowException);
};

}

#endif /* #ifndef _engine_purge_h_ */
