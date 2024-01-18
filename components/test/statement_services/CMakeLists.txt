# Copyright (c) 2023, 2024, Oracle and/or its affiliates.

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_COMPONENT(test_execute_regular_statement
    test_execute_regular_statement.cc
    utils.cc
    MODULE_ONLY
    TEST_ONLY
)

MYSQL_ADD_COMPONENT(test_execute_prepared_statement
    test_execute_prepared_statement.cc
    utils.cc
    MODULE_ONLY
    TEST_ONLY
)
