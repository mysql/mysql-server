BIN_DIRS := 	ndbapi_simple \
	 	ndbapi_async \
	 	ndbapi_async1 \
		ndbapi_retries \
		ndbapi_simple_index \
		ndbapi_s_i_ndbrecord \
		ndbapi_event \
		ndbapi_scan \
		ndbapi_blob \
		ndbapi_blob_ndbrecord \
		mgmapi_logevent \
		ndbapi_simple_dual \
		mgmapi_logevent2

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
