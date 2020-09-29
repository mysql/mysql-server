//#include "ha_warp.h"
#include "mysql.h"
#include "string.h"
#include "dirent.h"
#include <string>
#include <iostream>

extern "C" bool warp_get_partitions_init(UDF_INIT *initid, UDF_ARGS *args, char* message) {
  if(args->arg_count != 2) {
    strcpy(message,"This function requires two arguments: schema (string), table (string)"); 
    return 1;
  }

  if (args->arg_type[0] != STRING_RESULT && args->arg_type[1] != STRING_RESULT) { 
    strcpy(message, "Both arguments must be string");
    return 1;
  } 
    
  initid->max_length = 1024 * 1024 * 1024;

  return 0;
}

extern "C" char *warp_get_partitions(UDF_INIT *, UDF_ARGS *args, char* result, unsigned long *length, char *is_null, char *) {
  if(args->args[1] == NULL || args->args[2] == NULL) {
    *is_null=1;
    return 0;
  } 

  std::string schema(args->args[0], args->lengths[0]);
  std::string table(args->args[1], args->lengths[1]);
  std::string path = schema + "/" + table + ".data/";
  std::string parts = "";
  std::cout << path << "\n";
  
  DIR* dir = opendir(path.c_str());
  if(dir == NULL) {
    std::cout << "OPENDIR FAILED\n";
    *is_null = 1;
    return 0;
  }

  struct dirent *ent = NULL;
  while((ent = readdir(dir)) != NULL) {
    std::cout << "HERE!!\n";
    if(ent->d_type == DT_DIR) {
      if(ent->d_name[0] != 'p') {
        continue;
      }

      if(parts != "") {
        parts += " ";
      }

      parts += std::string(ent->d_name);
    }
    
  }
  *is_null = 0;
  *length = parts.length();
  if(*length == 0) {
    *is_null = 1;
    return 0;
  }
  
  std::cout << "PARTS:" << parts << "\n";
  if(*length <= 766) {
    std::cout << "LT 766\n";
    std::cout << "LENGTH: " << *length << "\n";
    result[*length+1] = 0;
    strncpy(result,parts.c_str(),*length);
    return result;
  }
  
  char *retval = (char*)malloc(*length);
  memset(retval, 0, *length);
  strncpy(retval, parts.c_str(), *length);
  result = retval;
  return retval;
}

extern "C" void warp_get_partitions_deinit(UDF_INIT *) { 
  
}
