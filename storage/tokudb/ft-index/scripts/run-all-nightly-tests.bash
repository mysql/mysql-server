#!/bin/bash

pushd $(dirname $0) &>/dev/null
scriptdir=$PWD
popd &>/dev/null

bash $scriptdir/run-nightly-release-tests.bash
bash $scriptdir/run-nightly-drd-tests.bash
bash $scriptdir/run-nightly-coverage-tests.bash

