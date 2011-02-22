
// vim:sw=2:ai

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#ifndef DENA_FATAL_HPP
#define DENA_FATAL_HPP

#include <string>

namespace dena {

void fatal_exit(const std::string& message);
void fatal_abort(const std::string& message);

};

#endif

