#!/bin/bash

download_and_untar()
{
	FOLDER_SRC=$1
	PACKAGE=$2
	FOLDER_DST=$3
	echo "Downloading ${RELEASE_GENERIC_REPO}/${FOLDER_SRC}/${PACKAGE} and decompressing in ${FOLDER_DST}"
	curl --user ${RELEASE_GENERIC_USER}:${RELEASE_GENERIC_PASSWORD}  ${RELEASE_GENERIC_REPO}/${FOLDER_SRC}/${PACKAGE}  -Lo ${PACKAGE}
    if [[ $? -ne 0 ]]; then
        echo Error: ${PACKAGE} was not downloaded. Aborting.
        rm -f ${PACKAGE}
        exit 1
    fi
    # Just for debugging purposes
    echo "curl result is " `ls -l ${PACKAGE}`
    # if [ -f ${PACKAGE} ] ; then
    #     echo "Content of downloaded file is: " `head -n5 ${PACKAGE}`
    # fi

    if [ -z "$FOLDER_DST" ]; then
        echo "Decompressing ${PACKAGE}"
        tar -xf ${PACKAGE}
    else
        echo "Decompressing ${PACKAGE} into ${FOLDER_DST}"
        tar -xf ${PACKAGE} -C ${FOLDER_DST}
    fi
}
