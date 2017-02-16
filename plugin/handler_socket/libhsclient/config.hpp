
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_CONFIG_HPP
#define DENA_CONFIG_HPP

#include <string>
#include <map>

#define DENA_VERBOSE(lv, x) if (dena::verbose_level >= (lv)) { (x); }

namespace dena {

struct config : public std::map<std::string, std::string> {
  std::string get_str(const std::string& key, const std::string& def = "")
    const;
  long long get_int(const std::string& key, long long def = 0) const;
};

void parse_args(int argc, char **argv, config& conf);

extern unsigned int verbose_level;

};

#endif

