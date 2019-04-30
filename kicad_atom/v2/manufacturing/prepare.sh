#!/bin/bash

SRC=rgb-to-hdmi
DST=AtomVideoToHDMI

mv $SRC-B.Cu.gbl      $DST.gbl
mv $SRC-B.Mask.gbs    $DST.gbs
mv $SRC-B.SilkS.gbo   $DST.gbo
mv $SRC.drl           $DST.xln
mv $SRC-Edge.Cuts.gm1 $DST.gko
mv $SRC-F.Cu.gtl      $DST.gtl
mv $SRC-F.Mask.gts    $DST.gts
mv $SRC-F.SilkS.gto   $DST.gto

rm -f manufacturing.zip
zip -qr manufacturing.zip $DST.*
