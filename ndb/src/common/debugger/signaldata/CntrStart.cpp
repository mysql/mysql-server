#include <signaldata/CntrStart.hpp>

bool
printCNTR_START_REQ(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const CntrStartReq * const sig = (CntrStartReq *)theData;
  fprintf(output, " nodeId: %x\n", sig->nodeId);
  fprintf(output, " startType: %x\n", sig->startType);
  fprintf(output, " lastGci: %x\n", sig->lastGci);
  return true;
}

bool
printCNTR_START_REF(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const CntrStartRef * const sig = (CntrStartRef *)theData;
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);
  return true;
}

bool
printCNTR_START_CONF(FILE * output, const Uint32 * theData, 
		     Uint32 len, Uint16 receiverBlockNo) {
  const CntrStartConf * const sig = (CntrStartConf *)theData;
  fprintf(output, " startType: %x\n", sig->startType);
  fprintf(output, " startGci: %x\n", sig->startGci);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);
  fprintf(output, " noStartNodes: %x\n", sig->noStartNodes);

  char buf[32*NdbNodeBitmask::Size+1];
  fprintf(output, " startedNodes: %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->startedNodes, buf));
  fprintf(output, " startingNodes: %s\n", 
	  BitmaskImpl::getText(NdbNodeBitmask::Size, sig->startingNodes, buf));
  return true;
}
