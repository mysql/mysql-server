#include <signaldata/ReadNodesConf.hpp>

bool
printREAD_NODES_CONF(FILE * output, const Uint32 * theData, 
		     Uint32 len, Uint16 receiverBlockNo) {
  const ReadNodesConf * const sig = (ReadNodesConf *)theData;
  fprintf(output, " noOfNodes: %x\n", sig->noOfNodes);
  fprintf(output, " ndynamicId: %x\n", sig->ndynamicId);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);

  char buf[32*NdbNodeBitmask::Size+1];
  fprintf(output, " allNodes(defined): %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->allNodes, buf));
  fprintf(output, " inactiveNodes: %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->inactiveNodes, buf));
  fprintf(output, " clusterNodes: %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->clusterNodes, buf));
  fprintf(output, " startedNodes: %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->startedNodes, buf));
  fprintf(output, " startingNodes: %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->startingNodes, buf));
  return true;
}

