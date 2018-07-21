#!/bin/bash

# exit on error
set -e

# Check if any uncommitted changes in tracked files
if [ -n "$(git status --untracked-files=no --porcelain)" ]; then 
    echo "Uncommitted changes; exiting....."
    exit
fi

# Lookup the last commit ID
VERSION="$(git rev-parse --short HEAD)"

NAME=RGBtoHDMI_$(date +"%Y%m%d")_$VERSION

DIR=releases/${NAME}
mkdir -p $DIR

for MODEL in rpi
do    
    # compile debug kernel
    ./clobber.sh
    ./configure_${MODEL}.sh -DDEBUG=1
    make -B -j
    mv kernel*.img ${DIR}
done

cp -a firmware/* ${DIR}

# Create a simple README.txt file
cat >${DIR}/README.txt <<EOF
RGBtoHDMI

(c) 2017 David Banks (hoglet), Dominic Plunkett (dp11), Ed Spittles (BigEd) and other contributors

  git version: $(grep GITVERSION gitversion.h  | cut -d\" -f2)
build version: ${NAME}
EOF

cp config.txt ${DIR}
cd releases/${NAME}
zip -qr ../${NAME}.zip .
cd ../..

unzip -l releases/${NAME}.zip
 
