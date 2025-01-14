#ifndef _spw_api_impl_master_h_
#define _spw_api_impl_master_h_

#include "include/master.h"
#include "spw_types.h"


namespace Sparrow
{

typedef SYSvector<int, 0> IndexMappings;
typedef SYSvector<int, 0> ActiveIndexes;

class spw_Master : public Master, public RefCounted
{
	friend ByteBuffer& operator >> (ByteBuffer& buffer, spw_Master& master);
	friend ByteBuffer& operator << (ByteBuffer& buffer, const spw_Master& master);

private:

	// Size of the master file, in bytes.
	uint64_t	size_;

	// Master file version
	uint32_t version_;

	// Table columns
	Columns columns_;

	// Indexes on the table. Contains also dropped indexes.
	Indexes indexes_;

	// Active indexes on the table. Does not contain dropped indexes.
	ActiveIndexes activeIndexes_;

	// Index mappings: relations between Sparrow indexes and MySQL indexes.
	IndexMappings indexMappings_;	

	// Foreign keys.
	ForeignKeys foreignKeys_;

	// DNS configuration.
	DnsConfiguration dnsConfiguration_;

	// Maximum lifetime of data in this table, in milliseconds.
	uint64_t maxLifetime_;

	// Aggregation period, in seconds, of data in this table. If 0, there is no aggregation.
	uint32_t aggregationPeriod_;

	// Current value of the auto-incremental column.
	int64_t autoInc_;

	// Current partition serial number.
	uint64_t serial_;

	// Creation date, in milliseconds since epoch.
	uint64_t timeCreated_;

	// Update date, in milliseconds since epoch.
	uint64_t timeUpdated_;

	// Total size of data files, in bytes.
	uint64_t dataSize_;

	// Total size of index files, in bytes.
	uint64_t indexSize_;

	// Total number of records (rows).
	uint64_t records_;

	// Current index alter serial number.
	uint32_t indexAlterSerial_;

	// Index alter duration.
	uint64_t indexAlterElapsed_;

	// Online index modifications.
	Alterations indexAlterations_;

	// List of partitions.
	Partitions partitions_;

	// Coalescing period, in milliseconds.
	uint64_t coalescingPeriod_;

	// Default where period, in milliseconds.
	uint64_t defaultWhere_;

	// String optimizatino size, in bytes.
	uint64_t stringOptimization_;

public:
	// Deserialization constructor.
	spw_Master();

	uint64_t getSize() const override { return size_; }
	uint32_t getVersion() const override { return version_; }
	uint64_t getMaxLifetime() const override { return maxLifetime_; }
	uint32_t getAggregPeriod() const override { return aggregationPeriod_; }
	uint64_t getCoalescingPeriod() const override { return coalescingPeriod_; }
	uint64_t getDefaultWhere() const override { return defaultWhere_; }
	uint64_t getStringOptimization() const override { return stringOptimization_; }
	uint64_t getSerial() const override { return serial_; }
	uint64_t getTimeCreated() const override { return timeCreated_; }
	uint64_t getTimeUpdated() const override { return timeUpdated_; }
	uint64_t getDataSize() const override { return dataSize_; }
	uint64_t getIndexSize() const override { return indexSize_; }
	uint64_t getRecords() const override { return records_; }
	uint32_t getIndexAlterSerial() const override { return indexAlterSerial_; }
	

	const Columns& getColumns() const { return columns_; }
	uint32_t getNbColumns() const override { return columns_.length(); }
	const Column& getColumn(uint32_t index) const override {
		SPW_ASSERT(index < columns_.length());
		return columns_[index];
	}

	const Indexes& getIndexes() const { return indexes_; }
	uint32_t getNbIndexes() const override { return indexes_.length(); }
	const Index& getIndex(uint32_t index) const override {
		SPW_ASSERT(index < indexes_.length());
		return indexes_[index];
	}

	const ActiveIndexes& getActiveIndexes() const { return activeIndexes_; }
	uint32_t getNbActiveIndexes() const override { return activeIndexes_.length(); }
	uint32_t getActiveIndexes(uint32_t index) const override {
		SPW_ASSERT(index < activeIndexes_.length());
		return activeIndexes_[index];
	}

	const IndexMappings& getIndexMappings() const { return indexMappings_; }
	uint32_t getNbIndexMappings() const override { return indexMappings_.length(); }
	uint32_t getIndexMapping(uint32_t index) const override {
		SPW_ASSERT(index < indexMappings_.length());
		return indexMappings_[index];
	}

	const ForeignKeys& getForeignKeys() const { return foreignKeys_; }
	uint32_t getNbFK() const override { return foreignKeys_.length(); }
	const ForeignKey& getFK(uint32_t index) const override {
		SPW_ASSERT(index < foreignKeys_.length());
		return foreignKeys_[index];
	}

	const DnsConfiguration& getDnsConfiguration() const { return dnsConfiguration_; }
	uint32_t getNbDnsEntries() const override { return dnsConfiguration_.length(); }
	uint32_t getDnsEntry(uint32_t index) const override {
		SPW_ASSERT(index < dnsConfiguration_.length());
		return dnsConfiguration_[index].getId();
	}
	uint32_t getNbDnsServers(uint32_t index) const override {
		SPW_ASSERT(index < dnsConfiguration_.length());
		return dnsConfiguration_[index].getServers().length();
	}
	const DnsServer& getDnsServer(uint32_t index, uint32_t index2) const override {
		SPW_ASSERT(index < dnsConfiguration_.length());
		SPW_ASSERT(index < dnsConfiguration_[index].getServers().length());
		return dnsConfiguration_[index].getServers()[index2];
	}

	const Alterations& getIndexAlterations() const { return indexAlterations_; }
	uint32_t getNbIndexAlterations() const override { return indexAlterations_.length(); }
	const Alteration& getIndexAlteration(uint32_t index) const override {
		SPW_ASSERT(index < indexAlterations_.length());
		return indexAlterations_[index];
	}

	const Partitions& getPartitions() const { return partitions_; }
	uint32_t getNbPartitions() const override { return partitions_.length(); }
	const Partition& getPartition(uint32_t index) const override {
		SPW_ASSERT(index < partitions_.length());
		return *partitions_[index];
	}

};

typedef RefPtr<Master> MasterGuard;

}	// namespace Sparrow

#endif	// #define _spw_api_impl_master_h_
