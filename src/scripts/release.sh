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

for MODEL in rpi rpiA rpi4 rpiA4
do
    # compile debug kernel
    ./clobber.sh
    ./configure_${MODEL}.sh -DDEBUG=1
    make -B -j
    mv kernel${MODEL}.img ${DIR}/kernel${MODEL}_debug.img
    # compile normal kernel
    ./clobber.sh
    ./configure_${MODEL}.sh
    make -B -j
    mv kernel${MODEL}.img ${DIR}/kernel${MODEL}.img
done


# Create a simple README.txt file
cat >${DIR}/README.txt <<EOF
RGBtoHDMI

(c) 2020 David Banks (hoglet), Ian Bradbury (IanB), Dominic Plunkett (dp11) and Ed Spittles (BigEd)

  git version: $(grep GITVERSION gitversion.h  | cut -d\" -f2)
build version: ${NAME}
EOF

cp -a config.txt ${DIR}
cp -a configA.txt ${DIR}
cp -a Use_ARM_Capture.bat ${DIR}
cp -a Use_GPU_Capture.bat ${DIR}
cp -a default_config.txt ${DIR}
cp -a firmware/* ${DIR}
cp -a cpld_firmware ${DIR}
cp -a Profiles ${DIR}
cp -a Resolutions ${DIR}
cp -a Amiga_CPLD_Readme  ${DIR}

# Convert to windows line endings
find ${DIR} -name '*.txt' -print0 | xargs -0 unix2dos

cd releases/${NAME}
zip -qr ../${NAME}.zip .
cd ../..

unzip -l releases/${NAME}.zip
