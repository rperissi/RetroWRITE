#!/bin/sh
# Build RetroWrite II with cc65 (https://cc65.github.io).
# Writes build/RW2.SYSTEM, RW2.MAIN, RW2.LC, RW2.FONTS and RW2.OVL1-9, RW2.OVLA-C.
# Set CC65_BIN if cl65/ca65/ld65 are not on your PATH.
set -e
cd "$(dirname "$0")"
BIN=${CC65_BIN:+$CC65_BIN/}
mkdir -p build
# the loader / splash program
${BIN}ca65 -o build/loader.o loader/loader.s
${BIN}ld65 -C loader/loader.cfg -o build/RW2.SYSTEM build/loader.o
# the program and its overlays
cd src
${BIN}cl65 -t apple2enh -C rw2gfx.cfg -Os -Or -Cl -m ../build/rw2.map -o ../build/RW2.MAIN \
  main.c core.c ovl_file.c ovl_spell.c ovl_menu.c ovl_edit.c ovl_print.c ovl_acc.c ovl_dlg.c \
  ovl_odtout.c ovl_odtzip.c ovl_odtin.c ovl_odtrd.c ovl_net.c \
  prtio.s accio.s gfx.s lowlevel.s fonts.s sys.s crc32.s inflate.s net.s
cd ../build
for n in 1 2 3 4 5 6 7 8 9 A B C; do mv RW2.MAIN.$n RW2.OVL$n; done
mv RW2.MAIN.F RW2.FONTS
mv RW2.MAIN.L RW2.LC
rm -f *.o
# documents and data that are not compiled
cp ../prebuilt/RW2.SPLASH ../prebuilt/SPELL.DICT ../prebuilt/LETTER .
python3 ../art/mktutorial.py TUTORIAL
cp ../prebuilt/GLOSSARY .
ls -l
