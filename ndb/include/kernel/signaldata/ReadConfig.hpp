#ifndef READ_CONFIG_HPP
#define READ_CONFIG_HPP

/**
 */
class ReadConfigReq {
public:
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 noOfParameters; // 0 Means read all relevant for block
  Uint32 parameters[1];  // see mgmapi_config_parameters.h
};

class ReadConfigConf {
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 senderData;
};

#endif
