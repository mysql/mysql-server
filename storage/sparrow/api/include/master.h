#ifndef _spw_api_master_h_
#define _spw_api_master_h_

#include "global.h"
#include "types.h"

namespace Sparrow
{

class Master
{
public:
	virtual ~Master() {}

	virtual uint64_t getSize() const = 0;
	virtual uint32_t getVersion() const = 0;
	virtual uint64_t getMaxLifetime() const = 0;
	virtual uint32_t getAggregPeriod() const = 0;
	virtual uint64_t getCoalescingPeriod() const = 0;
	virtual uint64_t getDefaultWhere() const = 0;
	virtual uint64_t getStringOptimization() const = 0;
	virtual uint64_t getSerial() const = 0;
	virtual uint64_t getTimeCreated() const = 0;
	virtual uint64_t getTimeUpdated() const = 0;
	virtual uint64_t getDataSize() const = 0;
	virtual uint64_t getIndexSize() const = 0;
	virtual uint64_t getRecords() const = 0;
	virtual uint32_t getIndexAlterSerial() const = 0;

	virtual uint32_t getNbColumns() const  = 0;
	virtual const Column& getColumn(uint32_t index) const  = 0;

	virtual uint32_t getNbIndexes() const  = 0;
	virtual const Index& getIndex(uint32_t index) const  = 0;

	virtual uint32_t getNbActiveIndexes() const  = 0;
	virtual uint32_t getActiveIndexes(uint32_t index) const  = 0;

	virtual uint32_t getNbIndexMappings() const  = 0;
	virtual uint32_t getIndexMapping(uint32_t index) const  = 0;

	virtual uint32_t getNbFK() const  = 0;
	virtual const ForeignKey& getFK(uint32_t index) const  = 0;

	virtual uint32_t getNbDnsEntries() const = 0;
	virtual uint32_t getDnsEntry(uint32_t index) const = 0;
	virtual uint32_t getNbDnsServers(uint32_t index) const = 0;
	virtual const DnsServer& getDnsServer(uint32_t index, uint32_t index2) const  = 0;

	virtual uint32_t getNbIndexAlterations() const  = 0;
	virtual const Alteration& getIndexAlteration(uint32_t index) const  = 0;

	virtual uint32_t getNbPartitions() const  = 0;
	virtual const Partition& getPartition(uint32_t index) const  = 0;

};

}	// namespace Sparrow

#endif	// #define _spw_api_master_h_
