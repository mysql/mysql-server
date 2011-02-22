
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.hpp"

namespace dena {

unsigned int verbose_level = 0;

std::string
config::get_str(const std::string& key, const std::string& def) const
{
  const_iterator iter = this->find(key);
  if (iter == this->end()) {
    DENA_VERBOSE(10, fprintf(stderr, "CONFIG: %s=%s(default)\n", key.c_str(),
      def.c_str()));
    return def;
  }
  DENA_VERBOSE(10, fprintf(stderr, "CONFIG: %s=%s\n", key.c_str(),
    iter->second.c_str()));
  return iter->second;
}

long long
config::get_int(const std::string& key, long long def) const
{
  const_iterator iter = this->find(key);
  if (iter == this->end()) {
    DENA_VERBOSE(10, fprintf(stderr, "CONFIG: %s=%lld(default)\n", key.c_str(),
      def));
    return def;
  }
  const long long r = atoll(iter->second.c_str());
  DENA_VERBOSE(10, fprintf(stderr, "CONFIG: %s=%lld\n", key.c_str(), r));
  return r;
}

void
parse_args(int argc, char **argv, config& conf)
{
  for (int i = 1; i < argc; ++i) {
    const char *const arg = argv[i];
    const char *const eq = strchr(arg, '=');
    if (eq == 0) {
      continue;
    }
    const std::string key(arg, eq - arg);
    const std::string val(eq + 1);
    conf[key] = val;
  }
  config::const_iterator iter = conf.find("verbose");
  if (iter != conf.end()) {
    verbose_level = atoi(iter->second.c_str());
  }
}

};

