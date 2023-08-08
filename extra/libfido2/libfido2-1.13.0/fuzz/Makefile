# Copyright (c) 2019-2023 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

IMAGE		:= libfido2-coverage:1.13.1
RUNNER		:= libfido2-runner
PROFDATA	:= llvm-profdata
COV		:= llvm-cov
TARGETS		:= fuzz_assert fuzz_bio fuzz_cred fuzz_credman fuzz_hid \
		   fuzz_largeblob fuzz_netlink fuzz_mgmt fuzz_pcsc
CORPORA		:= $(foreach f,${TARGETS},${f}/corpus)
MINIFY		:= $(foreach f,${TARGETS},/minify/${f}/corpus)
REMOTE		:= gs://libfido2-corpus.clusterfuzz-external.appspot.com
.DEFAULT_GOAL	:= all

all: ${TARGETS}

build:
	docker build -t ${IMAGE} - < Dockerfile

run: build
	-docker run -it -d --name ${RUNNER} ${IMAGE}
	docker start ${RUNNER}

sync: run
	tar Ccf .. - src fuzz | docker exec -i ${RUNNER} tar Cxf /libfido2 -
	docker exec ${RUNNER} make -C /libfido2/build

corpus: sync
	docker exec ${RUNNER} /bin/sh -c 'cd /libfido2/fuzz && rm -rf ${TARGETS}'
	docker exec ${RUNNER} tar Czxf /libfido2/fuzz /libfido2/fuzz/corpus.tgz

${TARGETS}: corpus sync
	docker exec -e LLVM_PROFILE_FILE=/profraw/$@ ${RUNNER} \
		/bin/sh -c 'rm -f /profraw/$@ && /libfido2/build/fuzz/$@ \
		-runs=1 /libfido2/fuzz/$@'

${MINIFY}: /minify/%/corpus: %
	docker exec ${RUNNER} /bin/sh -c 'rm -rf $@ && mkdir -p $@ && \
		/libfido2/build/fuzz/$< -use_value_profile=1 -merge=1 $@ \
		/libfido2/fuzz/$</corpus'

corpus.tgz-: ${MINIFY}
	docker exec -i ${RUNNER} tar Czcf /minify - ${TARGETS} > $@

profdata: run
	docker exec ${RUNNER} /bin/sh -c 'rm -f /$@ && ${PROFDATA} \
		merge -sparse /profraw/* -o /$@'

report.tgz: profdata
	docker exec ${RUNNER} /bin/sh -c 'rm -rf /report && mkdir /report && \
		${COV} show -format=html -tab-size=8 -instr-profile=/$< \
		-ignore-filename-regex=pcsclite.h --show-branch-summary=false \
		-output-dir=/report /libfido2/build/src/libfido2.so'
	docker exec -i ${RUNNER} tar Czcf / - report > $@

summary.txt: profdata
	docker exec ${RUNNER} ${COV} report -use-color=false \
		-ignore-filename-regex=pcsclite.h --show-branch-summary=false \
		/libfido2/build/src/libfido2.so -instr-profile=/$< > $@

functions.txt: profdata
	docker exec ${RUNNER} /bin/sh -c '${COV} report -use-color=false \
		-ignore-filename-regex=pcsclite.h -show-functions \
		--show-branch-summary=false -instr-profile=/$< \
		/libfido2/build/src/libfido2.so /libfido2/src/*.[ch]' > $@

clean: run
	docker exec ${RUNNER} /bin/sh -c 'rm -rf /profraw /profdata && \
		make -C /libfido2/build clean'
	-docker stop ${RUNNER}
	rm -rf ${TARGETS}

${CORPORA}:
	-mkdir -p $@
	gsutil -q -m rsync -d -r ${REMOTE}/libFuzzer/libfido2_$(@:/corpus=) $@

fetch-oss-fuzz: ${CORPORA}
	find ${TARGETS} -type f -size +8192c -print0 | xargs -0 rm

fetch-franz:
	ssh franz tar -C corpus -cf- . | tar -xf-

corpus.tgz:
	tar zcf $@ ${TARGETS}

.PHONY: build run sync corpus ${TARGETS} ${CORPORA}
.PHONY: report.tgz summary.txt functions.txt
.PHONY: fetch-oss-fuzz fetch-franz corpus.tgz
