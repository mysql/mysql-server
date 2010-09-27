#!/bin/sh

for X in `ps -efa | grep -i $1 | awk {'print $2'}`; do
  kill $X;
done

sleep 1

for X in `ps -efa | grep -i $1 | awk {'print $2'}`; do
  kill -9 $X;
done
