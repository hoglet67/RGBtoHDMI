#!/bin/bash

# exit on error
set -e

NAME=RGBtoHDMI_$(date +"%Y%m%d_%H%M")_$USER

DIR=releases/${NAME}
mkdir -p $DIR

for MODEL in rpi3 rpi2 rpi
do    
    # compile debug kernel
    ./clobber.sh
    ./configure_${MODEL}.sh
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

cp config.txt cmdline.txt ${DIR}
cd releases/${NAME}
zip -qr ../${NAME}.zip .
cd ../..

unzip -l releases/${NAME}.zip
 
