/*
	Thread base classes.
*/

#include "sql/sql_class.h"
#include "sql/protocol_classic.h"
#include "sql/sql_lex.h"

#include "thread.h"
#include "../handler/plugin.h"		// For configuration parameters.
#include "io.h"						// For IOContext.

// Utility function to workaround C linkage issue with pthread_create.
extern "C" int createSparrowThread(PSI_thread_key key, my_thread_handle* thread, const my_thread_attr_t* attr, my_start_routine func, void* arg) {
	return mysql_thread_create(key, thread, attr, reinterpret_cast<void*(*)(void*)>(func), arg);
}

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread
//////////////////////////////////////////////////////////////////////////////////////////////////////

Lock Thread::lock_(true, "Thread::lock_");

// STATIC
void Thread::initTHD(THD*& thd, void* stackStart) {
	my_thread_init();
	thd = new THD();
	THD_CHECK_SENTRY(thd);
	thd->thread_stack = (char*)stackStart;
	thd->store_globals();
	//thd->init_for_queries();

	// Initialize security context
	thd->m_main_security_ctx.set_host_or_ip_ptr();
	thd->security_context()->set_master_access(ALL_ACCESS);
	//thd->main_security_ctx.master_access = ~0;
	//thd->main_security_ctx.priv_user[0] = 0;
	thd->security_context()->skip_grants();

	thd->get_protocol_classic()->set_client_capabilities(0);
	//thd->client_capabilities = 0;
	
	thd->get_protocol_classic()->init_net(nullptr);

	CHARSET_INFO* charset_connection = get_charset_by_csname("utf8mb4", MY_CS_PRIMARY, MYF(MY_WME));
	thd->variables.character_set_client = charset_connection;
	thd->variables.character_set_results = charset_connection;
	thd->variables.collation_connection = charset_connection;
	thd->update_charset();
	thd->set_new_thread_id();

	lex_start(thd);

	// This call should not be usefull since class MessageThread and class Thread create the threads with attribute MY_THREAD_CREATE_DETACHED
	//pthread_detach_this_thread();
	thd->real_id = my_thread_self();
}

// STATIC
void Thread::deleteThreadSpecific(THD*& thd) {
	IOContext::destroy();
	if (thd != 0) {
		// No need to delete explicitly items in THD. THD is now a class with a destructor.
		//net_end(&thd->net);
		//thd->release_resources();
		delete thd;
		thd = nullptr;
	}
	my_thread_end();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Worker
//////////////////////////////////////////////////////////////////////////////////////////////////////

const JobThreadFactory Worker::factory_("Worker");
JobThreadPool* Worker::threadPool_ = 0;

// STATIC
void Worker::initialize() _THROW_(SparrowException) {
	// The queue is not bulk because we want jobs to be distributed across workers.
	threadPool_ = new JobThreadPool(Worker::factory_, &sparrow_max_worker_threads,
		&SparrowStatus::get().workerThreads_, "Worker::Queue", false);
}

// STATIC
void Worker::shutdown() {
	threadPool_->stop();
}

// STATIC
void Worker::sendJob(Job* job) {
	threadPool_->send(job);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Flush
//////////////////////////////////////////////////////////////////////////////////////////////////////

const JobThreadFactory Flush::factory_("Flush");
JobThreadPool* Flush::threadPool_ = 0;

// STATIC
void Flush::initialize() _THROW_(SparrowException) {
	// The queue is not bulk because we want jobs to be distributed across flush threads.
	threadPool_ = new JobThreadPool(Flush::factory_, &sparrow_max_flush_threads,
		&SparrowStatus::get().flushThreads_, "Flush::Queue", false);
}

// STATIC
void Flush::shutdown() {
	threadPool_->stop();
}

// STATIC
void Flush::sendJob(Job* job) {
	threadPool_->send(job);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Writer
//////////////////////////////////////////////////////////////////////////////////////////////////////

const JobThreadFactory Writer::factory_("Writer");
JobThreadPool* Writer::threadPool_ = 0;

// STATIC
void Writer::initialize() _THROW_(SparrowException) {
	// The queue is not bulk because we want jobs to be distributed across writers.
	threadPool_ = new JobThreadPool(Writer::factory_, &sparrow_max_writer_threads,
		&SparrowStatus::get().writerThreads_, "Writer::Queue", false);
}

// STATIC
void Writer::shutdown() {
	threadPool_->stop();
}

// STATIC
void Writer::sendJob(Job* job) {
	threadPool_->send(job);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
// ApiWorker
//////////////////////////////////////////////////////////////////////////////////////////////////////

const JobThreadFactory ApiWorker::factory_("ApiWorker");
JobThreadPool* ApiWorker::threadPool_ = 0;

// STATIC
void ApiWorker::initialize() _THROW_(SparrowException) {
	// The queue is not bulk because we want jobs to be distributed across API workers.
	threadPool_ = new JobThreadPool(ApiWorker::factory_, &sparrow_max_api_worker_threads,
		&SparrowStatus::get().apiWorkerThreads_, "ApiWorker::Queue", false);
}

// STATIC
void ApiWorker::shutdown() {
	threadPool_->stop();
}

// STATIC
void ApiWorker::sendJob(Job* job) {
	threadPool_->send(job);
}

}

