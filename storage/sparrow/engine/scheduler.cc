/*
	Scheduler thread.
*/

#include "scheduler.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler
//////////////////////////////////////////////////////////////////////////////////////////////////////

Scheduler* Scheduler::scheduler_ = 0;

// STATIC
void Scheduler::initialize() _THROW_(SparrowException) {
	scheduler_ = new Scheduler();
	if (!scheduler_->start()) {
		throw SparrowException::create(false, "Cannot start scheduler thread");
	}
}

bool Scheduler::process() {
	Guard guard(cond_.getLock());
	SYSpHashIterator<Tasks> iterator(tasks_);
	uint64_t minTimestamp = ULLONG_MAX;
	while (++iterator) {
		minTimestamp = std::min(iterator.key()->getTimestamp(), minTimestamp);
	}
	const uint64_t now = Scheduler::now();
	if (now >= minTimestamp) {
		const Tasks key(minTimestamp);
		Tasks* tasks = tasks_.remove(&key);
		while (!tasks->isEmpty()) {
			Task* task = tasks->removeFirst();
			task->getQueue().send(new TaskJob(minTimestamp, task));
		}
		delete tasks;
	} else {
		const uint64_t sleepDuration = std::min(static_cast<uint64_t>(5000), minTimestamp - now);
		cond_.wait(sleepDuration, true);
	}
	return true;
}

bool Scheduler::notifyStop() {
	cond_.signalAll();
	return true;
}

// STATIC
void Scheduler::addTask(Task* task) {
	addTask(task, Scheduler::now(), false);
}

// STATIC
void Scheduler::addTask(Task* task, const uint64_t timestamp, const bool onlyOne /* = false */) {
	Guard guard(scheduler_->cond_.getLock());
	addTaskNoLock(task, timestamp, onlyOne);
}

// STATIC
bool Scheduler::moveTask(Task* task, const uint64_t timestamp) {
	Guard guard(scheduler_->cond_.getLock());
	if (!removeTaskNoLock(task)) {
		return false;
	}
	addTaskNoLock(task, timestamp, false);
	return true;
}

// To stop a task, search it in the scheduler queue. If present, re-schedule it asap to
// have the task deleted the normal way.
// STATIC
void Scheduler::stopTask(Task* task) {
	Guard guard(scheduler_->cond_.getLock());
	if (removeTaskNoLock(task)) {
		addTaskNoLock(task, 0, false, true);
	}
}

// STATIC
bool Scheduler::removeTaskNoLock(Task* task) {
	bool found = false;
	SYSpHashIterator<Tasks> iterator(scheduler_->tasks_);
	while (++iterator) {
		Tasks* tasks = iterator.key();
		if (tasks->remove(task)) {
			if (tasks->isEmpty()) {
				delete scheduler_->tasks_.remove(tasks);
			}
			found = true;
			iterator.reset();
		}
	}
	return found;
}

// STATIC
void Scheduler::addTaskNoLock(Task* task, const uint64_t timestamp, const bool remove, const bool first) {
	if (remove) {
		removeTaskNoLock(task);
	}
	const Tasks key(timestamp);
	Tasks* tasks = scheduler_->tasks_.find(&key);
	if (tasks == 0) {
		tasks = new Tasks(timestamp);
		scheduler_->tasks_.insert(tasks);
	}
	tasks->add(task, first);
	scheduler_->cond_.signalAll(true);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Task
//////////////////////////////////////////////////////////////////////////////////////////////////////

void Task::stop() {
	stopping_ = true;
	Scheduler::stopTask(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TaskJob
//////////////////////////////////////////////////////////////////////////////////////////////////////

void TaskJob::process() {
	if (task_->isStopping()) {
		delete task_;
	} else {
		try {
			task_->run(timestamp_);
		} catch(const SparrowException& e) {
			e.toLog();
		}
		const uint64_t period = task_->getPeriod();
		if (period == 0) {
			delete task_;
		} else {
			uint64_t timestamp = timestamp_ + period;
			while (timestamp < Scheduler::now()) {
				timestamp += period;
			}
			Scheduler::addTask(task_, timestamp, false);
		}
	}
}

}
