#! /bin/bash
#
# Wrapper script for mysql_config to support multilib
#
# Only works on OEL6/RHEL6 and similar
#
# This command respects setarch

bits=$(rpm --eval %__isa_bits)

case $bits in
    32|64) status=known ;;
        *) status=unknown ;;
esac

if [ "$status" = "unknown" ] ; then
    echo "$0: error: command 'rpm --eval %__isa_bits' returned unknown value: $bits"
    exit 1
fi


if [ -x /usr/bin/mysql_config-$bits ] ; then
    /usr/bin/mysql_config-$bits "$@"
else
    echo "$0: error: needed binary: /usr/bin/mysql_config-$bits is missing. Please check your MySQL installation."
    exit 1
fi

