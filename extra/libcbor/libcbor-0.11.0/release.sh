#!/usr/bin/env bash

# Guides my forgetful self through the release process.
# Usage release.sh VERSION

set -e

function prompt() {
	echo "$1 Confirm with 'Yes'"
	read check
	if [ "$check" != "Yes" ]; then
		echo "Aborting..."
		exit 1
	fi
}
# http://stackoverflow.com/questions/59895/getting-the-source-directory-of-a-bash-script-from-within
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
OUTDIR=$(mktemp -d)
TAG_NAME="v$1"

cd $DIR
python3 misc/update_version.py "$1"

echo ">>>>> Checking changelog"
grep -A 10 -F "$1" CHANGELOG.md || true
prompt "Is the changelog correct and complete?"

echo ">>>>> Checking Doxyfile"
grep PROJECT_NUMBER Doxyfile
prompt "Is the Doxyfile version correct?"

echo ">>>>> Checking CMakeLists"
grep -A 2 'SET(CBOR_VERSION_MAJOR' CMakeLists.txt
prompt "Is the CMake version correct?"

echo ">>>>> Checking Bazel build"
grep -A 2 'CBOR_MAJOR_VERSION' examples/bazel/third_party/libcbor/cbor/configuration.h
prompt "Is the version correct?"

echo ">>>>> Checking docs"
grep 'version =\|release =' doc/source/conf.py
prompt "Are the versions correct?"

set -x
pushd doc
make clean
popd
doxygen
cd doc
make html
cd build

cp -r html libcbor_docs_html
tar -zcf libcbor_docs.tar.gz libcbor_docs_html

cp -r doxygen/html libcbor_api_docs_html
tar -zcf libcbor_api_docs.tar.gz libcbor_api_docs_html

mv libcbor_docs.tar.gz libcbor_api_docs.tar.gz "$OUTDIR"

pushd "$OUTDIR"
cmake "$DIR" -DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=ON
make
ctest
popd

prompt "Will proceed to tag the release with $TAG_NAME."
git commit -a -m "Release $TAG_NAME"
git tag "$TAG_NAME"
git push --set-upstream origin $(git rev-parse --abbrev-ref HEAD)
git push --tags

set +x

echo "Release ready in $OUTDIR"
echo "Add the release to GitHub at https://github.com/PJK/libcbor/releases/new *now*"
prompt "Have you added the release to https://github.com/PJK/libcbor/releases/tag/$TAG_NAME?"

echo "Update the Hombrew formula (https://github.com/Homebrew/homebrew-core/blob/master/Formula/libcbor.rb) *now*"
echo "HOWTO: https://github.com/Linuxbrew/brew/blob/master/docs/How-To-Open-a-Homebrew-Pull-Request.md"
prompt "Have you updated the formula?"
