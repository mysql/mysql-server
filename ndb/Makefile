include .defs.mk

DIRS := src test tools examples

# hack before full autoconf
replace-targets := all clean
NDB_RELEASE := $(shell ../scripts/mysql_config --version)

include $(NDB_TOP)/Epilogue.mk

_libs_test : _bins_src
_libs_tools : _libs_test
_libs_examples : _bins_src
_bins_src : _libs_src
_bins_tools : _bins_src

# always release compile except for ndbapi static lib
all:
	$(MAKE) -C src/ndbapi libs
	$(MAKE) libs NDB_VERSION=RELEASE
	$(MAKE) bins NDB_VERSION=RELEASE
ifeq ($(NDB_OS),LINUX)
	NDB_RELEASE=$(NDB_RELEASE) $(MAKE) -j1 -C docs all </dev/null || :
endif

# old distclean matches clean better
clean: distclean
	$(MAKE) -C docs clean

nuke-deps:
	find . -name '.depend*' | xargs rm -rf

vim-tags:
	bk sfiles -g | ctags --c-types=+p --extra=+fq -L -

cvs-update:
ifeq ($(NDB_VERSION),main)
	-cvs update -d
else
ifeq ($(NDB_TAG),HEAD)
	-cvs -q update
	-cd include && cvs -q update -d
	-cd src && cvs -q update -d
	-cd test && cvs -q update -d
	-cd tools && cvs -q update -d
else
	-cvs -q update -r $(NDB_TAG)
	-cd include && cvs -q update -d -r $(NDB_TAG)
	-cd src && cvs -q update -d -r $(NDB_TAG)
	-cd test && cvs -q update -d -r $(NDB_TAG)
	-cd tools && cvs -q update -d -r $(NDB_TAG)
endif
endif
	make nuke-deps
	make vim-tags
	make TAGS

bk-update:
	bk pull
	make nuke-deps
	make vim-tags
	make TAGS
