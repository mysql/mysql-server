/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "util/require.h"
#include "Sysfile.hpp"

#include <cstring>
#include <EventLogger.hpp>

#define JAM_FILE_ID 512

const char Sysfile::MAGIC_v2[8] = {'N', 'D', 'B', 'S', 'Y', 'S', 'F', '2'};

/**
 * Input to unpack functions is the v1 or v2 format stored in the cdata
 * array of integers.
 * Output of pack functions is the v1 or v2 format stored in the cdata
 * array of integers.
 */

enum DataNodeStatusPacked
{
  NODE_ACTIVE = 0,
  NODE_ACTIVE_NODE_DOWN = 1,
  NODE_CONFIGURED = 2,
  NODE_UNDEFINED = 3
};

int
Sysfile::pack_sysfile_format_v2(Uint32 cdata[], Uint32* cdata_size_ptr) const
{
  /**
   * Format for COPY_GCIREQ v2:
   * --------------------------
   * 1) MAGIC_v2
   * 2) m_max_node_id
   * 3) Total size in words of packed format
   * 4) numGCIs (number of GCIs in non-packed form)
   * 5) numNodeGroups (node groups in non-packed form)
   * 6) Number of replicas
   * 7) systemRestartBits
   * 8) m_restart_seq
   * 9) keepGCI
   * 10) oldestRestorableGCI
   * 11) newestRestorabeGCI
   * 12) latestLCP_ID
   * 13) lcpActive bits (m_max_node_id bits)
   * 14) nodeStatus 4 bits * m_max_node_id
   *     3 bits is DataNodeStatusPacked
   *     1 bit is set if GCI is non-packed form
   * 15) GCIs in non-packed form
   * 16) Node group bit (m_max_node_id bits)
   * 17) Node groups in non-packed form (16 bits per node group)
   */
#ifdef VM_TRACE
  //memset(takeOver, 0, sizeof(takeOver)); // TODO clear takeover at caller??
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++)
  {
    ActiveStatus active_status = (ActiveStatus)
      getNodeStatus(i);
    require(active_status != NS_ActiveMissed_2);
    require(active_status != NS_ActiveMissed_3);
    require(active_status != NS_TakeOver);
    require(active_status != NS_NotActive_TakenOver);
  }
#endif
  Uint32 index = 0;

  std::memcpy(&cdata[index], MAGIC_v2, MAGIC_SIZE_v2);
  static_assert(MAGIC_SIZE_v2 % sizeof(Uint32) == 0);
  static_assert(MAGIC_SIZE_v2 / sizeof(Uint32) == 2);
  index += MAGIC_SIZE_v2 / sizeof(Uint32);

  require(index == 2);
  const Uint32 max_node_id = getMaxNodeId();
  cdata[index] = max_node_id;
  index++;

  require(index == 3);
  const Uint32 index_cdata_size_in_words = index;
  index++;

  require(index == 4);
  const Uint32 index_numGCIs = index;
  index++;

  require(index == 5);
  const Uint32 index_numNodeGroups = index;
  index++;

  require(index == 6);
  const Uint32 index_num_replicas = index;
  index++;

  require(index == 7);

  cdata[index] = systemRestartBits;
  index++;

  cdata[index] = m_restart_seq;
  index++;

  cdata[index] = keepGCI;
  index++;

  cdata[index] = oldestRestorableGCI;
  index++;

  cdata[index] = newestRestorableGCI;
  index++;

  cdata[index] = latestLCP_ID;
  index++;

  Uint32 lcp_active_words = ((max_node_id) + 31) / 32;
  require(index == 13);
  for (Uint32 i = 0; i < lcp_active_words; i++)
  {
    cdata[index] = lcpActive[i];
    index++;
  }
  Uint32 data = 0;
  Uint32 start_bit = 0;
  const Uint32 index_node_bit_words = index;
  Uint32 numGCIs = 0;
  Uint32 node_bit_words = ((max_node_id * 4) + 31) / 32;
  Uint32 indexGCI = index_node_bit_words + node_bit_words;
  Uint32 expectedGCI = newestRestorableGCI;
  for (Uint32 i = 1; i <= max_node_id; i++)
  {
    ActiveStatus active_status = (ActiveStatus)
      getNodeStatus(i);
    Uint32 bits = 0;
    Uint32 diff = 0;
    Uint32 nodeGCI = lastCompletedGCI[i];
    switch (active_status)
    {
      case NS_Active:
      {
        bits = NODE_ACTIVE;
        if (nodeGCI != expectedGCI)
        {
          diff = 1;
        }
        break;
      }
      case NS_ActiveMissed_1:
      case NS_NotActive_NotTakenOver:
      {
        bits = NODE_ACTIVE_NODE_DOWN;
        diff = 1;
        break;
      }
      case NS_ActiveMissed_2:
      case NS_ActiveMissed_3:
      case NS_NotActive_TakenOver:
      case NS_TakeOver:
      {
        g_eventLogger->info("active_status = %u", active_status);
        assert(false);
        bits = NODE_ACTIVE_NODE_DOWN;
        diff = 1;
        break;
      }
      case NS_NotDefined:
      {
        bits = NODE_UNDEFINED;
        if (nodeGCI != 0)
        {
          diff = 1;
        }
        break;
      }
      case NS_Configured:
      {
        bits = NODE_CONFIGURED;
        if (nodeGCI != expectedGCI)
        {
          diff = 1;
        }
        break;
      }
      default:
      {
        g_eventLogger->info("active_status = %u", active_status);
        return -1;
      }
    }
    if (diff != 0)
    {
      numGCIs++;
      bits += 8;
      cdata[indexGCI] = nodeGCI;
      indexGCI++;
    }
    data += (bits << start_bit);
    require(bits < 16);
    start_bit += 4;
    if (start_bit == 32)
    {
      cdata[index] = data;
      data = 0;
      start_bit = 0;
      index++;
    }
  }
  if (start_bit != 0)
  {
    cdata[index] = data;
    index++;
  }
  if ((index + numGCIs) != indexGCI)
  {
    return -2;
  }
  Uint32 numNodeGroups = 0;
  Uint32 num_replicas = 0;
  Uint32 replica_index = 0;
  index = indexGCI;
  Uint32 node_group_bit_words = lcp_active_words;
  const Uint32 index_ng = index + node_group_bit_words;
  data = 0;
  start_bit = 0;
  Uint16 *ng_area = (Uint16*)&cdata[index_ng];
  Uint32 predicted_ng = 0;
  Uint32 first_ng = NO_NODE_GROUP_ID;
  for (Uint32 i = 1; i <= max_node_id; i++)
  {
    ActiveStatus active_status = (ActiveStatus)
      getNodeStatus(i);
    Uint32 diff = 0;
    Uint32 nodeGroup;
    switch (active_status)
    {
      case NS_Active:
      case NS_ActiveMissed_1:
      case NS_NotActive_NotTakenOver:
      case NS_ActiveMissed_2:
      case NS_ActiveMissed_3:
      case NS_NotActive_TakenOver:
      case NS_TakeOver:
      {
        nodeGroup = getNodeGroup(i);
        if (num_replicas == 0 && first_ng == NO_NODE_GROUP_ID)
        {
          first_ng = nodeGroup;
          num_replicas++;
        }
        else if (first_ng == nodeGroup)
        {
          require(replica_index == num_replicas);
          num_replicas++;
        }
        else if (first_ng != NO_NODE_GROUP_ID)
        {
          first_ng = NO_NODE_GROUP_ID;
          // unset first_ng to mark that num_replicas now is set (and > 0).
          if (num_replicas > MAX_REPLICAS)
          {
            return -3;
          }
        }
        if (first_ng == NO_NODE_GROUP_ID && replica_index == num_replicas)
        {
          replica_index = 0;
          predicted_ng++;
        }
        if (nodeGroup != predicted_ng)
        {
          diff = 1;
        }
        replica_index++;
        break;
      }
      case NS_NotDefined:
      {
        /* If a node is not configured the node group will never be used.
         * Still the node group is expected to be NO_NODE_GROUP_ID.
         * Sometimes it seems that node group is wrongly set to zero.
         * While this is not critical, it should be examined why.
         */
        nodeGroup = getNodeGroup(i);
        if (nodeGroup != NO_NODE_GROUP_ID && nodeGroup != 0)
        {
          return -4;
        }
        break;
      }
      case NS_Configured:
      {
        nodeGroup = getNodeGroup(i);
        if (nodeGroup != NO_NODE_GROUP_ID)
        {
          return -5;
          abort();
          diff = 1;
        }
        break;
      }
      default:
      {
        return -6;
      }
    }
    if (diff != 0)
    {
      ng_area[numNodeGroups] = nodeGroup;
      numNodeGroups++;
      data += (1 << start_bit);
    }
    start_bit++;
    if (start_bit == 32)
    {
      cdata[index] = data;
      start_bit = 0;
      index++;
    }
  }
  if (start_bit != 0)
  {
    cdata[index] = data;
    index++;
  }
  require(index == index_ng);
  *cdata_size_ptr = index_ng + ((numNodeGroups + 1)/2);
  cdata[index_cdata_size_in_words] = *cdata_size_ptr;
  cdata[index_numGCIs] = numGCIs;
  cdata[index_numNodeGroups] = numNodeGroups;
  cdata[index_num_replicas] = num_replicas;

  return 0;
}

int
Sysfile::pack_sysfile_format_v1(Uint32 cdata[], Uint32* cdata_size_ptr) const
{
  if (maxNodeId == 0 || maxNodeId > 48)
  {
    return -1;
  }

  cdata[0] = systemRestartBits;
  cdata[1] = m_restart_seq;
  cdata[2] = keepGCI;
  cdata[3] = oldestRestorableGCI;
  cdata[4] = newestRestorableGCI;
  cdata[5] = latestLCP_ID;

  for (Uint32 i = 0; i < 49; i++)
    cdata[6 + i] = lastCompletedGCI[i];
  for (Uint32 i = 0; i < 49; i++)
    setNodeStatus_v1(i, getNodeStatus(i), &cdata[55]);

  memset(&cdata[62], 0, 52);
  for (Uint32 i = 1; i <= 48; i++)
  {
    NodeId ng = getNodeGroup(i);
    setNodeGroup_v1(i, &cdata[62], Uint8(ng));
  }

  memset(&cdata[75], 0, 52);
  for (Uint32 i = 1; i <= 48; i++)
  {
    NodeId nodeId = getTakeOverNode(i);
    require(nodeId <= 48);
    setTakeOverNode_v1(i, &cdata[75], Uint8(nodeId));
  }

  for (Uint32 i = 0; i < 2; i++)
    cdata[88 + i] = lcpActive[i];

  return 0;
}

int
Sysfile::unpack_sysfile_format_v2(const Uint32 cdata[], Uint32* cdata_size_ptr)
{
  initSysFile();
  Uint32 index = 0;
  if (std::memcmp(&cdata[index],
                  MAGIC_v2,
                  MAGIC_SIZE_v2) != 0)
  {
    return -1;
  }
  index += MAGIC_SIZE_v2 / sizeof(Uint32);

  const Uint32 max_node_id = cdata[index];
  index++;

  if (max_node_id >= MAX_NDB_NODES)
  {
    return -7;
  }

  const Uint32 cdata_size = cdata[index];
  if (cdata_size > *cdata_size_ptr)
  {
    return -2;
  }
  index++;

  Uint32 numGCIs = cdata[index];
  index++;

  Uint32 numNodeGroups = cdata[index];
  index++;

  Uint32 num_replicas = cdata[index];
  index++;

  systemRestartBits = cdata[index];
  index++;

  m_restart_seq = cdata[index];
  index++;

  keepGCI = cdata[index];
  index++;

  oldestRestorableGCI = cdata[index];
  index++;

  newestRestorableGCI = cdata[index];
  index++;

  latestLCP_ID = cdata[index];
  index++;

  Uint32 lcp_active_words = ((max_node_id) + 31) / 32;
  require(lcp_active_words <= NdbNodeBitmask::Size);
  require(index + lcp_active_words <= cdata_size);
  for (Uint32 i = 0; i < lcp_active_words; i++)
  {
    lcpActive[i] = cdata[index];
    index++;
  }
  Uint32 node_bit_words = ((max_node_id * 4) + 31) / 32;
  Uint32 node_group_words = lcp_active_words;

  const Uint32 index_node_bit_words = index;
  Uint32 indexGCI = index_node_bit_words + node_bit_words;
  Uint32 start_bit = 0;
  Uint32 newestGCI = newestRestorableGCI;
  for (Uint32 i = 1; i <= max_node_id; i++)
  {
    Uint32 data = cdata[index];
    Uint32 bits = (data >> start_bit) & 0xF;
    Uint32 gci_bit = bits >> 3;
    Uint32 state_bits = bits & 0x7;
    switch (state_bits)
    {
      case NODE_ACTIVE:
      {
        if (gci_bit != 0)
        {
          lastCompletedGCI[i] = cdata[indexGCI];
          indexGCI++;
        }
        else
        {
          lastCompletedGCI[i] = newestGCI;
        }
        setNodeStatus(i, NS_Active);
        break;
      }
      case NODE_ACTIVE_NODE_DOWN:
      {
        if (gci_bit == 0)
        {
          return -3;
        }
        lastCompletedGCI[i] = cdata[indexGCI];
        indexGCI++;
        setNodeStatus(i, NS_ActiveMissed_1);
        break;
      }
      case NODE_CONFIGURED:
      {
        if (gci_bit != 0)
        {
          lastCompletedGCI[i] = cdata[indexGCI];
          indexGCI++;
        }
        else
        {
          lastCompletedGCI[i] = newestGCI;
        }
        setNodeStatus(i, NS_Configured);
        break;
      }
      case NODE_UNDEFINED:
      {
        if (gci_bit != 0)
        {
          lastCompletedGCI[i] = cdata[indexGCI];
          indexGCI++;
        }
        else
        {
          lastCompletedGCI[i] = 0;
        }
        setNodeStatus(i, NS_NotDefined);
        break;
      }
      default:
      {
        return -4;
      }
    }
    start_bit += 4;
    if (start_bit == 32)
    {
      index++;
      start_bit = 0;
    }
  }
  if (start_bit != 0)
  {
    index++;
  }
  require(index == (index_node_bit_words + node_bit_words));
  require((index + numGCIs) == indexGCI);
  index = indexGCI;
  const Uint32 index_ng = index + node_group_words;
  Uint16* ng_array = (Uint16*)&cdata[index_ng];
  start_bit = 0;
  Uint32 replica_index = 0;
  Uint32 ng_index = 0;
  Uint32 current_ng = 0;
  for (Uint32 i = 1; i <= max_node_id; i++)
  {
    ActiveStatus active_status = (ActiveStatus)
      getNodeStatus(i);
    Uint32 data = cdata[index];
    Uint32 ng_bit = (data >> start_bit) & 0x1;
    Uint32 nodeGroup = NO_NODE_GROUP_ID;
    switch (active_status)
    {
      case NS_Active:
      case NS_ActiveMissed_1:
      {
        if (ng_bit == 0)
        {
          nodeGroup = current_ng;
        }
        else
        {
          nodeGroup = (Uint32) ng_array[ng_index];
          ng_index++;
        }
        replica_index++;
        if (replica_index == num_replicas)
        {
          replica_index = 0;
          current_ng++;
        }
        break;
      }
      case NS_NotDefined:
      case NS_Configured:
      {
        nodeGroup = NO_NODE_GROUP_ID;
        break;
      }
      default:
      {
        return -5;
      }
    }
    setNodeGroup(i, nodeGroup);
    start_bit++;
    if (start_bit == 32)
    {
      index++;
      start_bit = 0;
    }
  }
  if (start_bit != 0)
  {
    index++;
  }
  require(index == index_ng);
  require(ng_index == numNodeGroups);
  index = index_ng + ((ng_index + 1)/2);
  if (index > cdata_size)
  {
    return -6;
  }
  *cdata_size_ptr = cdata_size;

  // TODO YYY clear fields for unused nodes and node groups, or keep max_node/max_node_group in state!
  // nodeStatus[nodeId] = NS_NotDefined
  // nodeGroups[i] = NO_NODE_GROUP_ID
  return 0;
}

int
Sysfile::unpack_sysfile_format_v1(const Uint32 cdata[], Uint32* cdata_size_ptr)
{
  if (_SYSFILE_SIZE32_v1 > *cdata_size_ptr)
  {
    return -1;
  }
  initSysFile();
  systemRestartBits = cdata[0];
  m_restart_seq = cdata[1];
  keepGCI = cdata[2];
  oldestRestorableGCI = cdata[3];
  newestRestorableGCI = cdata[4];
  latestLCP_ID = cdata[5];

  for (Uint32 i = 0; i < 49; i++)
  {
    lastCompletedGCI[i] = cdata[6 + i];
  }
  for (Uint32 i = 0; i < 49; i++)
  {
    setNodeStatus(i, getNodeStatus_v1(i, &cdata[55]));
  }

  memset(nodeGroups, 0, sizeof(nodeGroups));
  for (NodeId i = 1; i <= 48; i++)
  {
    NodeId ng = getNodeGroup_v1(i, &cdata[62]);
    if (ng == 255)
      ng = NO_NODE_GROUP_ID;
    setNodeGroup(i, ng);
  }

  memset(takeOver, 0, sizeof(takeOver));
  for (NodeId i = 1; i <= 48; i++)
  {
    NodeId nodeId = getTakeOverNode_v1(i, &cdata[75]);
    setTakeOverNode(i, nodeId);
  }

  for (Uint32 i = 0; i < 2; i++)
  {
    lcpActive[i] = cdata[88 + i];
  }

  *cdata_size_ptr = _SYSFILE_SIZE32_v1;
  return 0;
}
