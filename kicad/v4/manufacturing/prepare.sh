#!/bin/bash

BOARD=rgb-to-hdmi

mv $BOARD-B.Cu.gbl      $BOARD.gbl
mv $BOARD-B.Mask.gbs    $BOARD.gbs
mv $BOARD-B.SilkS.gbo   $BOARD.gbo
mv $BOARD.drl           $BOARD.txt
mv $BOARD-Edge.Cuts.gm1 $BOARD.gml
mv $BOARD-F.Cu.gtl      $BOARD.gtl
mv $BOARD-F.Mask.gts    $BOARD.gts
mv $BOARD-F.SilkS.gto   $BOARD.gto

rm -f manufacturing.zip
zip -qr manufacturing.zip $BOARD.*

