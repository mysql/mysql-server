#
# suite.pm will make sure that all tests including this file
# will be skipped unless this is a debug build.
#
# The test below is redundant

if (`select version() not like '%debug%'`) {
  --skip Needs a debug build
}
