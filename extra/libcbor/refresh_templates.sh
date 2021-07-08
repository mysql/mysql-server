#!/usr/bin/env bash
erb $(dirname $0)/test/stream_expectations.c.erb > \
    $(dirname $0)/test/stream_expectations.c
clang-format -style=file -i $(dirname $0)/test/stream_expectations.c
