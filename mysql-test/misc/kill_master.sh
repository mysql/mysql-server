kill -9 `cat var/run/master.pid`
# The kill may fail if process has already gone away,
# so don't use the exit code of the kill. Use 0.
exit 0
