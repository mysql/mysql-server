#include "memalloc.h"
#include "spw_master.h"

namespace Sparrow {

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Master
//////////////////////////////////////////////////////////////////////////////////////////////////////

spw_Master::spw_Master() 
	: size_(0), version_(0), maxLifetime_(0), aggregationPeriod_(0), autoInc_(0), 
	serial_(0), timeCreated_(0), timeUpdated_(0), dataSize_(0), indexSize_(0), 
	records_(0), indexAlterSerial_(0), indexAlterElapsed_(0), coalescingPeriod_(0),
	defaultWhere_(0), stringOptimization_(0)
{}

// Deserialization.
ByteBuffer& operator >> (ByteBuffer& buffer, spw_Master& master) {
	buffer >> master.size_;
	buffer >> master.version_;
	buffer >> master.columns_ >> master.indexes_ >> master.indexMappings_
		>> master.foreignKeys_ >> master.dnsConfiguration_;
	buffer >> master.maxLifetime_;
	buffer >> master.aggregationPeriod_ >> master.defaultWhere_ >> master.stringOptimization_;
	buffer >> master.autoInc_;
	buffer >> master.serial_ >> master.timeCreated_ >> master.timeUpdated_ 
		>> master.dataSize_ >> master.indexSize_ >> master.records_;
	buffer >> master.indexAlterSerial_ >> master.indexAlterElapsed_;
	buffer >> master.indexAlterations_ >> master.coalescingPeriod_;

	uint32_t length;
	buffer >> length;
	for (uint32_t i = 0; i < length; ++i) {
		spw_PersistentPartition* partition = new spw_PersistentPartition();
		buffer >> *partition;

		// Older versions may contain empty partitions.
		if (partition->getRecords() > 0) {
			master.partitions_.append(partition);
		} else {
			delete partition;
		}
	}

	return buffer;
}

// Serialization
ByteBuffer& operator << (ByteBuffer& buffer, const spw_Master& master) {
	buffer << master.version_;
	buffer << master.columns_ << master.indexes_ << master.indexMappings_
		<< master.foreignKeys_ << master.dnsConfiguration_;
	buffer << master.maxLifetime_
		<< master.aggregationPeriod_ << master.defaultWhere_ << master.stringOptimization_
		<< master.autoInc_ << master.serial_ << master.timeCreated_
		<< master.timeUpdated_ << master.dataSize_ << master.indexSize_ << master.records_
		<< master.indexAlterSerial_ << master.indexAlterElapsed_ << master.indexAlterations_
		<< master.coalescingPeriod_;

	const Partitions& partitions = master.partitions_;
	const uint32_t length = partitions.length();
	buffer << length;
	for ( uint32_t i=0; i<length; i++ ) {
		const Partition& partition = *partitions[i];
		buffer << static_cast<const spw_PersistentPartition&>(partition);
	}

	return buffer;
}

}
