# XXX ndbapi_example4, select_all commented out until fixed
BIN_DIRS := ndbapi_example1 ndbapi_example2 ndbapi_example3  $(ndbapi_example4) \
	    ndbapi_example5 $(select_all) ndbapi_scan_example

bins: $(patsubst %, _bins_%, $(BIN_DIRS))

$(patsubst %, _bins_%, $(BIN_DIRS)) :
	$(MAKE) -C $(patsubst _bins_%, %, $@) $(OPTS)

libs:

clean:
	for f in ${BIN_DIRS}; do \
		$(MAKE) -C $$f $@;\
	done

clean_dep: clean
cleanall: clean
tidy: clean
distclean: clean
