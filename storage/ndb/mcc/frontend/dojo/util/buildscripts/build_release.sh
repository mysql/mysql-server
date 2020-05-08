#!/usr/bin/env bash

set -e

usage() {
	echo "Usage: $0 [-c] [-t] [-b branch] version"
	echo
	echo "-b  Branch to archive. Defaults to 'master'."
	echo "-t  Include themes repo in release."
	echo "-c  Build CDN release as well."
	exit 1
}

while getopts ctb:u: OPTION; do
	case $OPTION in
	c)
		CDN=1
		shift 1
		OPTIND=1
		;;
	b)
		BRANCH=$OPTARG
		shift 2
		OPTIND=1
		;;
	t)
		THEMES=1
		shift 1
		OPTIND=1
		;;
	\?)
		echo "Invalid option -$OPTARG"
		exit 1
		;;
	:)
		echo "Missing argument for -$OPTARG"
		exit 1
		;;
	esac
done

# Version should be something like 0.9.0-beta or 0.9.0. See http://semver.org.
VERSION=$1

OLDIFS=$IFS
IFS="."
PRE_VERSION=($VERSION)
IFS=$OLDIFS

if [[ ${PRE_VERSION[2]} == *-* ]]; then
	PRE_VERSION="${PRE_VERSION[0]}.${PRE_VERSION[1]}.$((PRE_VERSION[2]))-pre"
elif [[ ${PRE_VERSION[2]} -eq 0 ]]; then
	PRE_VERSION="${PRE_VERSION[0]}.$((PRE_VERSION[1] + 1)).0-pre"
else
	PRE_VERSION="${PRE_VERSION[0]}.${PRE_VERSION[1]}.$((PRE_VERSION[2] + 1))-pre"
fi

BRANCH=${BRANCH=master}

# Name used for directory and tar/zip file of prebuilt version
BUILD_NAME=dojo-release-$VERSION

# Name used for directory and tar/zip file of source version
SOURCE_NAME=$BUILD_NAME-src

# Name used for tar/zip file of demos
DEMOS_NAME=$BUILD_NAME-demos

# Name used for tarball of temporary archive for downloads.dojotoolkit.org
OUTPUT_NAME=release-$VERSION

# Name used for directory and zip file of CDN version
CDN_NAME=$VERSION

# Name used for directory and zip archive for archive.dojotoolkit.org
CDN_OUTPUT_NAME=$VERSION-cdn

# Build scripts directory (own directory)
UTIL_DIR=$(cd $(dirname $0) && pwd)

# Directory where all build operations will be rooted
ROOT_DIR=$UTIL_DIR/build

###########
# NOTE: OUTPUT_DIR, SOURCE_DIR, and CDN_OUTPUT_DIR *must* be children of ROOT_DIR for certain operations to work
# properly. They must also use $*_NAME variables as the last directory part too.
###########

# Directory into which final output for downloads.dojotoolkit.org is placed
OUTPUT_DIR=$ROOT_DIR/$OUTPUT_NAME

# Directory into which Dojo source is checked out
SOURCE_DIR=$ROOT_DIR/$SOURCE_NAME

# Directory where source Dojo build scripts exist
SOURCE_BUILD_DIR=$SOURCE_DIR/util/buildscripts

# Directory into which the source Dojo writes built data
SOURCE_RELEASE_DIR=$SOURCE_DIR/release

# Directory into which the source CDN writes built data for CDN
CDN_OUTPUT_DIR=$ROOT_DIR/$CDN_OUTPUT_NAME

# Repositories that are a part of the Dojo Toolkit

if [ $THEMES ]; then
	ALL_REPOS="demos dijit dojo dojox dijit-themes util"
# Dojo 1.10-
else
	ALL_REPOS="demos dijit dojo dojox util"
fi

zip="zip -dd -ds 1m -rq"

if [ $(echo $(zip -dd test 2>&1) |grep -c "one action") -eq 1 ]; then
	# Zip 2.3 does not support progress output
	zip="zip -rq"
fi

tar="tar --checkpoint=1000 --checkpoint-action=dot"
ant=~/.ant/bin/ant

if [ "$VERSION" == "" ]; then
	usage
	exit 1
fi

if [ -d $SOURCE_DIR -o -d $OUTPUT_DIR ]; then
	echo "Existing build directories detected at $ROOT_DIR"
	echo "Aborted."
	exit 1
fi

echo "This is an internal Dojo release script. You probably meant to run build.sh!"
echo "If you want to create Dojo version $VERSION from branch $BRANCH, press 'y'."
echo "The source version will be updated to $PRE_VERSION after the build."
echo "(You will have an opportunity to abort pushing upstream later on if something"
echo "goes wrong.)"
read -s -n 1

if [ "$REPLY" != "y" ]; then
	echo "Aborted."
	exit 0
fi

if [ ! -d $ROOT_DIR ]; then
	mkdir $ROOT_DIR
fi

mkdir $SOURCE_DIR
mkdir $OUTPUT_DIR

VERSION_EXISTS=0

for REPO in $ALL_REPOS; do
	# Clone pristine copies of the repository for the desired branch instead of trying to copy a local repo
	# which might be outdated, on a different branch, or containing other unpushed/uncommitted code
	git clone --recursive --single-branch --branch=$BRANCH git@github.com:dojo/$REPO.git $SOURCE_DIR/$REPO

	cd $SOURCE_DIR/$REPO

	set +e
	git checkout $VERSION > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		set -e
		echo "Tag $VERSION already exists for $REPO; using existing tag"
		VERSION_EXISTS=1
		continue
	fi
	set -e

	REVISION=$(git log -n 1 --format='%h')
	VERSION_FILES=

	if [ -f "package.json" ]; then
		VERSION_FILES=package.json
	fi

	if [ -f "bower.json" ]; then
		VERSION_FILES="$VERSION_FILES bower.json"
	fi

	if [ $REPO == "dojo" ]; then
		# Dojo 1.7+
		if [ -f "_base/kernel.js" ]; then
			VERSION_FILES="$VERSION_FILES _base/kernel.js"
		# Dojo 1.6-
		elif [ -f "_base/_loader/bootstrap.js" ]; then
			VERSION_FILES="$VERSION_FILES _base/_loader/bootstrap.js"
		fi
	fi

	if [ $REPO == "util" ]; then
		for FILENAME in doh/package.json doh/_rhinoRunner.js doh/mobileRunner.html doh/runner.html doh/_nodeRunner.js; do
			if [ -f $FILENAME ]; then
				VERSION_FILES="$VERSION_FILES $FILENAME"
			fi
		done

		if [ -f "build/version.js" ]; then
			VERSION_FILES="$VERSION_FILES build/version.js"
		fi
	fi

	if [ -n "$VERSION_FILES" ]; then
		for FILENAME in $VERSION_FILES; do
			java -jar $UTIL_DIR/../shrinksafe/js.jar $UTIL_DIR/changeVersion.js $VERSION $REVISION $FILENAME
		done

		# These will be pushed later, once it is confirmed the build was successful, in order to avoid polluting
		# the origin repository with failed build commits and tags
		git commit -m "Updating metadata for $VERSION" $VERSION_FILES
	fi

	git tag -a -m "Release $VERSION" $VERSION

	if [ -n "$VERSION_FILES" ]; then
		for FILENAME in $VERSION_FILES; do
			java -jar $UTIL_DIR/../shrinksafe/js.jar $UTIL_DIR/changeVersion.js $PRE_VERSION "" $FILENAME
		done

		git commit -m "Updating source version to $PRE_VERSION" $VERSION_FILES
	fi

	git checkout $VERSION
done

cd $ROOT_DIR

# Archive all source except for demos, which are provided separately so people do not have to download them
# with the source
echo -n "Archiving source..."
$zip $OUTPUT_DIR/$SOURCE_NAME.zip $SOURCE_NAME/ -x "*/.git" -x "*/.git/*" -x "$SOURCE_NAME/demos/*"
$tar --exclude="$SOURCE_NAME/demos" --exclude-vcs -zcf $OUTPUT_DIR/$SOURCE_NAME.tar.gz $SOURCE_NAME/
echo "Done"

# Temporarily rename $SOURCE_NAME ($SOURCE_DIR) to $BUILD_NAME to archive demos backwards-compatibly
mv $SOURCE_NAME $BUILD_NAME
echo -n "Archiving demos..."
$zip $OUTPUT_DIR/$DEMOS_NAME.zip $BUILD_NAME/demos/ -x "*/.git" -x "*/.git/*"
$tar --exclude-vcs -zcf $OUTPUT_DIR/$DEMOS_NAME.tar.gz $BUILD_NAME/demos/
mv $BUILD_NAME $SOURCE_NAME
echo "Done"

# Create the built release archive using the checked out release code
cd $SOURCE_BUILD_DIR
echo "Building release..."
./build.sh action=release profile=standard version=$VERSION releaseName=$BUILD_NAME cssOptimize=comments.keepLines optimize=shrinksafe.keepLines insertAbsMids=1 mini=true
cd $SOURCE_RELEASE_DIR
echo -n "Archiving release..."
$zip $OUTPUT_DIR/$BUILD_NAME.zip $BUILD_NAME/
$tar -zcf $OUTPUT_DIR/$BUILD_NAME.tar.gz $BUILD_NAME/
echo "Done"

# For backwards-compatibility, Dojo Base is also copied for direct download
cp $BUILD_NAME/dojo/dojo.js* $OUTPUT_DIR

# Second build with tests that is kept unarchived and placed directly on downloads.dojotoolkit.org
rm -rf $SOURCE_RELEASE_DIR
cd $SOURCE_BUILD_DIR
echo "Building downloads release..."
./build.sh action=release profile=standard version=$VERSION releaseName=$BUILD_NAME cssOptimize=comments.keepLines optimize=shrinksafe.keepLines insertAbsMids=1 copyTests=true mini=false
mv $SOURCE_RELEASE_DIR/$BUILD_NAME $OUTPUT_DIR
rmdir $SOURCE_RELEASE_DIR
echo "Done"

if [ $CDN ]; then
	mkdir $CDN_OUTPUT_DIR

	# Only this super-specific Ant version works!
	if [ ! -d ~/.ant ]; then
		mkdir ~/.ant
		cd ~/.ant
		echo "Installing ant 1.7.1 to ~/.ant..."
		curl http://archive.apache.org/dist/ant/binaries/apache-ant-1.7.1-bin.tar.gz |tar --strip-components=1 -zx
		echo "Done"
	fi

	echo "Building CDN release..."

	# Build all locales
	cd $SOURCE_BUILD_DIR/cldr
	sed -i -e '/<property name="locales"/d' build.xml

	$ant clean

	# On first run of Ant, build.xml does some bad stuff that requires running ant twice
	set +e
	set -o pipefail
	TMPFILE=$(mktemp)
	$ant 2>&1 | tee $TMPFILE
	EXITCODE=$?
	set -e
	set +o pipefail
	if [ $EXITCODE -gt 0 ]; then
		if [ $(grep -c "please re-run" $TMPFILE) -gt 0 ]; then
			$ant
		else
			rm $TMPFILE
			exit 1
		fi
	fi
	rm $TMPFILE
	unset TMPFILE EXITCODE

	cd $SOURCE_BUILD_DIR

	# Dojo 1.7+
	if [ -f "profiles/cdn.profile.js" ]; then
		./build.sh action=release profile=standard profile=cdn version=$VERSION releaseName=$CDN_NAME cssOptimize=comments.keepLines optimize=closure layerOptimize=closure stripConsole=normal copyTests=false mini=true
	# Dojo 1.6
	else
		java -classpath ../shrinksafe/js.jar:../shrinksafe/shrinksafe.jar org.mozilla.javascript.tools.shell.Main build.js profile=standard version=$VERSION releaseName=$CDN_NAME cssOptimize=comments.keepLines optimize=shrinksafe layerOptimize=shrinksafe stripConsole=normal copyTests=false mini=true action=release loader=xdomain xdDojoPath="//ajax.googleapis.com/ajax/libs/dojo/$VERSION" xdDojoScopeName=window[\(typeof\(djConfig\)\!\=\"undefined\"\&\&djConfig.scopeMap\&\&djConfig.scopeMap[0][1]\)\|\|\"dojo\"]
	fi

	mv $SOURCE_RELEASE_DIR/$CDN_NAME $CDN_OUTPUT_DIR
	rmdir $SOURCE_RELEASE_DIR
	echo "Done"

	echo -n "Generating CDN checksums..."
	cd $CDN_OUTPUT_DIR
	$zip $CDN_NAME.zip $CDN_NAME/
	sha1sum $CDN_NAME.zip > sha1.txt
	cd $CDN_OUTPUT_DIR/$CDN_NAME
	find . -type f -exec sha1sum {} >> ../sha1.txt +
	echo "Done"

	cd $ROOT_DIR
	echo -n "Creating CDN archive..."
	$tar -cf $CDN_OUTPUT_NAME.tar $CDN_OUTPUT_NAME/
	echo "Done"

	cd $ROOT_DIR
fi

cd $OUTPUT_DIR

# Checksums, because who doesn't love checksums?!
md5=$(which md5 md5sum 2>/dev/null || true)
if [ -x $md5 ]; then
	echo -n "Generating release checksums..."
	for FILENAME in *.zip *.gz *.js; do
		$md5 $FILENAME > $FILENAME.md5
		echo -n "."
	done
	echo "Done"
else
	echo "MD5 utility missing; cannot generate checksums"
fi

# Archive file for download.dojotoolkit.org
cd $ROOT_DIR
echo -n "Creating downloads archive..."
$tar -cf $OUTPUT_NAME.tar $OUTPUT_NAME/
echo "Done"

echo "Build complete."

# Confirmaton
if [ $VERSION_EXISTS -eq 1 ]; then
	echo "Please confirm build success, then press 'y' key to clean up archives"
	echo "and upload, or any other key to bail."
else
	echo "Please confirm build success, then press 'y' key to clean up archives, push"
	echo "tags, and upload, or any other key to bail."
fi
read -p "> "

if [ "$REPLY" != "y" ]; then
	echo "Aborted."
	exit 0
fi

# Flush archives
echo -n "Cleaning up archives..."
rm -rf $OUTPUT_DIR
if [ $CDN ]; then
	rm -rf $CDN_OUTPUT_DIR
fi
echo "Done"

if [ $VERSION_EXISTS -eq 1 ]; then
	echo "Skipping tag push, since tags already exist"
else
	for REPO in $ALL_REPOS; do
		cd $SOURCE_DIR/$REPO
		echo "Pushing to repo $REPO"
		git push origin $BRANCH
		git push origin --tags
	done
fi

echo "Release is now built and tagged.  There rest from here is paperwork."

cd $ROOT_DIR

echo "Copying archive to downloads.dojotoolkit.org..."
cp $OUTPUT_NAME.tar  /srv/www/vhosts.d/download.dojotoolkit.org
cd /srv/www/vhosts.d/download.dojotoolkit.org && tar -xf $OUTPUT_NAME.tar && rm $OUTPUT_NAME.tar

cd $ROOT_DIR

if [ $CDN ]; then
        echo "Copying cdn to archive.dojotoolkit.org..."
        cp $CDN_OUTPUT_NAME.tar /srv/www/vhosts.d/archive.dojotoolkit.org/cdn
        cd /srv/www/vhosts.d/archive.dojotoolkit.org/cdn && tar -xf $CDN_OUTPUT_NAME.tar && rm $CDN_OUTPUT_NAME.tar
fi

cd $ROOT_DIR

echo "Upload complete. download.dojotoolkit.org should be automatically udpated. archive.dojotoolkit.org/cdn"
echo "This site should be automagically updated: download.dojotoolkit.org"
echo "CDNs can be found here: archive.dojotoolkit.org/cdn"
echo "To deploy to dojotoolkit.org, make sure appropriate changes are done and merged to master, then redeploy."
