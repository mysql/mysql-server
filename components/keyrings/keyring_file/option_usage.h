/*
  Copyright (c) 2024, Oracle and/or its affiliates.
*/

#ifndef KEYRING_FILE_COMPONENT_OPTION_USAGE_H
#define KEYRING_FILE_COMPONENT_OPTION_USAGE_H

extern bool keyring_file_component_option_usage_init();
extern bool keyring_file_component_option_usage_deinit();
extern bool keyring_file_component_option_usage_set(
    unsigned long every_nth = 100);

#endif /* KEYRING_FILE_COMPONENT_OPTION_USAGE_H */
