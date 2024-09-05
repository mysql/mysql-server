/*
  Copyright (c) 2024, Oracle and/or its affiliates.
*/

#ifndef VALIDATE_PASSWORD_COMPONENT_OPTION_USAGE_H
#define VALIDATE_PASSWORD_COMPONENT_OPTION_USAGE_H

extern bool validate_password_component_option_usage_init();
extern bool validate_password_component_option_usage_deinit();
extern bool validate_password_component_option_usage_set(
    unsigned long every_nth = 100);

#endif /* VALIDATE_PASSWORD_COMPONENT_OPTION_USAGE_H */
