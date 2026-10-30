#!/bin/sh
# verify.sh - end-to-end check of the CoCoVGA emulation on frames from XRoar
#
#   XROAR_SRC=/path/to/xroar [D32_ROM=d32.rom DDOS_ROM=ddos11c.rom] ./verify.sh [nitros9.dsk]
#
# XROAR_SRC: an XRoar source tree with xroar-verify.patch applied and built
# headless (./configure --without-x --without-gtk3 --without-gtk2
# --without-sdl2 --without-sdl3 ... && make), so src/xroar exists.
# The optional disk is a CoCo NitrOS-9 Level 1 disk with the CoVGA driver.
# D32_ROM / DDOS_ROM (Dragon 32 BASIC and DragonDOS ROM images) add a run of
# the Dragon BASIC test disk (../dragon) on XRoar's PAL Dragon 32; XRoar
# must know the BASIC ROM's CRC for -type to work (see README.md).
# Needs: python3 with numpy and Pillow, a C compiler.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)                 # RGBtoHDMI src/
XR=${XROAR_SRC:?set XROAR_SRC}/src/xroar
FONTS=$XROAR_SRC/src/mc6847
OUT=${OUT:-$HERE/out}
mkdir -p "$OUT"
cd "$OUT"

echo "== building ROMs and runner"
python3 "$HERE/mkrom.py" "$OUT" >/dev/null
python3 "$HERE/mkboot.py" "$OUT/boot.rom" >/dev/null
cc -O2 -std=c99 -I"$SRC" -o runner "$HERE/runner.c" "$SRC/cocovga.c" "$SRC/cocovga_font.c" \
   "$FONTS/font-6847.c" "$FONTS/font-6847t1.c"
ARTS=0
grep -q CV_HAVE_LOWERCASE_32 "$SRC/cocovga.h" && ARTS="0 1 2"

echo "== test ROM on XRoar's NTSC Dragon (tano)"
XROAR_VDG_DUMP=$OUT/test.raw XROAR_VDG_DUMP_FRAMES=760 \
   "$XR" -ui null -ao null -machine tano -extbas "$OUT/covga_test.rom" -no-altbas -no-ratelimit >/dev/null 2>&1 || true
fail=0
for sh in 0 1 2; do
   ox=$((64 - 2 * sh))
   python3 "$HERE/xroar2fb.py" test.raw t$sh.fb --dx $sh >/dev/null
   python3 "$HERE/truth.py" t$sh.fb t$sh.truth --ox $ox --xroar-src "$XROAR_SRC/src" >/dev/null
   for a in $ARTS; do for nb in 0 3; do
      r=$(./runner t$sh.fb t$sh.truth -nb $nb -art $a -ox $ox | grep RESULT)
      echo "capture +$sh dot, artifacts $a, num buffers $nb: $r"
      case "$r" in *PASS*) ;; *) fail=1 ;; esac
   done; done
done

if [ -n "$1" ]; then
   echo "== NitrOS-9 (CoVGA) on XRoar's NTSC CoCo"
   cp "$1" nos9.dsk
   XROAR_VDG_DUMP=$OUT/nos9.raw XROAR_VDG_DUMP_FRAMES=1800 \
      "$XR" -ui null -ao null -machine cocous -bas "$OUT/boot.rom" -no-extbas \
      -cart verifydos -cart-type rsdos -cart-rom "$OUT/dummy_cart.rom" \
      -load-fd0 "$OUT/nos9.dsk" -no-disk-write-back -no-ratelimit >/dev/null 2>&1 || true
   python3 "$HERE/xroar2fb.py" nos9.raw nos9.fb >/dev/null
   python3 "$HERE/truth.py" nos9.fb nos9.truth --xroar-src "$XROAR_SRC/src" >/dev/null
   for a in $ARTS; do for nb in 0 3; do
      r=$(./runner nos9.fb nos9.truth -nb $nb -art $a | grep RESULT)
      echo "NitrOS-9, artifacts $a, num buffers $nb: $r"
      case "$r" in *PASS*) ;; *) fail=1 ;; esac
   done; done
fi
if [ -n "$D32_ROM" ] && [ -n "$DDOS_ROM" ]; then
   echo "== Dragon BASIC test disk (dragon/W64TEST.VDK, auto run) on XRoar's PAL Dragon 32"
   D=$HERE/../dragon
   python3 "$D/mklib.py" w64lib.raw >/dev/null
   python3 "$D/mkdisk.py" "$D/w64test.bas" w64lib.raw W64TEST.VDK >/dev/null
   XROAR_VDG_DUMP=$OUT/d32.raw XROAR_VDG_DUMP_FRAMES=6400 \
      "$XR" -ui null -ao null -machine dragon32 -extbas "$D32_ROM" \
      -cart dd -cart-type dragondos -cart-rom "$DDOS_ROM" \
      -load-fd0 "$OUT/W64TEST.VDK" -no-disk-write-back -no-ratelimit \
      -type 'RUN"W64TEST"\rA' >/dev/null 2>&1 || true
   python3 "$HERE/xroar2fb.py" d32.raw d32.fb >/dev/null
   rm -f d32.raw
   python3 "$HERE/truth.py" d32.fb d32.truth --xroar-src "$XROAR_SRC/src" >/dev/null
   for a in $ARTS; do for nb in 0 3; do
      r=$(./runner d32.fb d32.truth -nb $nb -art $a | grep RESULT)
      echo "Dragon 32 BASIC, artifacts $a, num buffers $nb: $r"
      case "$r" in *PASS*) ;; *) fail=1 ;; esac
      case "$r" in *"(0 64-column"*) echo "  (no 64 column frames: the test disk did not run)"; fail=1 ;; esac
   done; done
   r=$(./runner d32.fb d32.truth -nb 3 -maxchars 128 -decoderows 8 | grep RESULT)
   echo "Dragon 32 BASIC, work limit 128 characters / 8 rows per frame: $r"
   case "$r" in *PASS*) ;; *) fail=1 ;; esac
   echo "== Dragon BASIC test disk, custom colours (menu key C = B in auto mode)"
   XROAR_VDG_DUMP=$OUT/pal.raw XROAR_VDG_DUMP_FRAMES=3200 \
      "$XR" -ui null -ao null -machine dragon32 -extbas "$D32_ROM" \
      -cart dd -cart-type dragondos -cart-rom "$DDOS_ROM" \
      -load-fd0 "$OUT/W64TEST.VDK" -no-disk-write-back -no-ratelimit \
      -type 'RUN"W64TEST"\rC' >/dev/null 2>&1 || true
   python3 "$HERE/xroar2fb.py" pal.raw pal.fb >/dev/null
   rm -f pal.raw
   python3 "$HERE/truth.py" pal.fb pal.truth --xroar-src "$XROAR_SRC/src" >/dev/null
   SG="0=8846 1=fffa 2=fec2 3=999f 4=fcca 5=e318 6=8b5a 7=d85f 8=fda1 9=844e 10=b8c1"
   ART="25=ffa1 26=c89f 41=ffa1 42=c89f"
   cat > pal.want <<EOW
art 0d extras 40 unreadable 0:
art 0d extras 40 unreadable 0: $SG
art 0d extras 00 unreadable 0: $SG
art 0d extras 40 unreadable 0:
art 0d extras 40 unreadable 0: $ART
art 0f extras 40 unreadable 0: $ART
art 0c extras 40 unreadable 0: $ART
art 0d extras 40 unreadable 0:
EOW
   for slow in 0 100; do
      if [ $slow = 0 ]; then unset RUNNER_SLOW; else export RUNNER_SLOW=$slow; fi
      r=$(RUNNER_PALLOG=1 ./runner pal.fb pal.truth -nb 3 -art 1)
      unset RUNNER_SLOW
      echo "$r" | grep '^PAL' | sed 's/^PAL frame [0-9]* //' > pal.got
      if echo "$r" | grep -q 'RESULT PASS' && cmp -s pal.got pal.want; then res=PASS; else res=FAIL; fail=1; fi
      echo "Custom colours (register palettes, artifact register), work limit ${slow:-0}: RESULT $res"
   done
fi
[ $fail = 0 ] && echo "ALL PASS" || { echo "FAILURES"; exit 1; }
