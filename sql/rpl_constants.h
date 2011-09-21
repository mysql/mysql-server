#ifndef RPL_CONSTANTS_H
#define RPL_CONSTANTS_H

/**
   Enumeration of the incidents that can occur for the server.
 */
enum Incident {
  /** No incident */
  INCIDENT_NONE = 0,

  /** There are possibly lost events in the replication stream */
  INCIDENT_LOST_EVENTS = 1,

  /** Shall be last event of the enumeration */
  INCIDENT_COUNT
};

#ifndef MCP_WL5353
/**
   Enumeration of the reserved formats of Binlog extra row information
*/
enum ExtraRowInfoFormat {
  /** Ndb format */
  ERIF_NDB          =   0,

  /** Reserved formats  0 -> 63 inclusive */
  ERIF_LASTRESERVED =  63,

  /**
      Available / uncontrolled formats
      64 -> 254 inclusive
  */
  ERIF_OPEN1        =  64,
  ERIF_OPEN2        =  65,

  ERIF_LASTOPEN     =  254,

  /**
      Multi-payload format 255

      Length is total length, payload is sequence of
      sub-payloads with their own headers containing
      length + format.
  */
  ERIF_MULTI        =  255
};

/*
   1 byte length, 1 byte format
   Length is total length in bytes, including 2 byte header
   Length values 0 and 1 are currently invalid and reserved.
*/
#define EXTRA_ROW_INFO_LEN_OFFSET 0
#define EXTRA_ROW_INFO_FORMAT_OFFSET 1
#define EXTRA_ROW_INFO_HDR_BYTES 2
#define EXTRA_ROW_INFO_MAX_PAYLOAD (255 - EXTRA_ROW_INFO_HDR_BYTES)

#endif

#endif /* RPL_CONSTANTS_H */
