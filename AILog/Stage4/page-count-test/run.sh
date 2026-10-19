#!/bin/bash
# The Stage 4 page-count test, complete: makes a fresh instrumented copy of
# Main/ (patch.py), builds CountTest.cc against it twice (the Stage 4 search,
# and the Stage 3 search for comparison), runs both from a directory holding
# supplier.tbl, and writes their reports to report_new.txt / report_old.txt.
# Exit status: the number of failed checks over both runs.
#   usage: run.sh
C="$(cd "$(dirname "$0")" && pwd)"
M=/e/Git/A4/Main
SHIM=$C/../shim
export PATH=/d/MSYS2/ucrt64/bin:$PATH

python -I "$C/patch.py" "$M" "$C/src" || exit 100
INC="-I$SHIM -include $SHIM/winposix.h"
for d in Catalog BufferMgr Qunit Record DatabaseTable BPlusTest; do INC="$INC -I$C/src/$d/headers"; done

fails=0
for variant in new old; do
	flags=""; [ $variant = old ] && flags="-DOLD_DISCOVER"
	obj="$C/obj_$variant"; rm -rf "$obj"; mkdir -p "$obj"
	for f in "$C"/src/{Catalog,BufferMgr,Record,DatabaseTable}/source/*.cc "$C/CountTest.cc"; do
		g++ -std=c++14 -O0 $flags $INC -c "$f" -o "$obj/$(basename "$f" .cc).o" &
	done; wait
	g++ -static "$obj"/*.o -o "$C/count_$variant.exe" || exit 101
	mkdir -p "$C/run"; cp "$M/../Build/supplier.tbl" "$C/run/"
	(cd "$C/run" && "$C/count_$variant.exe" > "$C/report_$variant.txt" 2> "$C/qunit_$variant.txt")
	n=$?; fails=$((fails + n))
	echo "count_$variant: $(grep -o 'Testing.*' "$C/qunit_$variant.txt")"
done
exit $fails
