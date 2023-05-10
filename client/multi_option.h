/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.
*/

#include "mysql.h"
#include "mysql/service_mysql_alloc.h"
#include "prealloced_array.h"

#ifndef _MULTI_OPTION_H_
#define _MULTI_OPTION_H_

/**
  Class for handling multiple options like e.g. --init-command,
  --init-command-add
*/
class Multi_option {
  /**
    Type of the internal container
  */
  using Multi_option_container = Prealloced_array<char *, 5>;

 public:
  /**
    Constaexpr constructor
  */
  constexpr Multi_option() : option_values(nullptr) {}

  /**
    Adds option value to the container

    @param value [in]: value of the option
    @param clear [in]: if true the container will be cleared before adding the
    command
  */
  void add_value(char *value, bool clear);

  /**
    Sets options to MYSQL structure.

    @param mysql [in, out]: pointer to MYSQL structure to be augmented with the
                            option
    @param option [in]: option to be set
  */
  void set_mysql_options(MYSQL *mysql, mysql_option option);

  /**
    Free the commands
  */
  void free();

 private:
  /**
    The internal container with values
  */
  Multi_option_container *option_values;
};

#endif  //_MULTI_OPTION_H_
