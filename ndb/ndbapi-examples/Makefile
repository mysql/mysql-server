BIN_DIRS := 	ndbapi_simple_example \
	 	ndbapi_async_example \
	 	ndbapi_async_example1 \
		ndbapi_retries_example \
		ndbapi_simple_index_example \
		ndbapi_event_example \
		ndbapi_scan_example \
		mgmapi_logevent_example

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
