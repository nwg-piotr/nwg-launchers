#!/bin/env sh

SRC_README="$1"
TGT_README="$2"

BAR="$(./""$3"" -h)"
DMENU="$(./""$4"" -h)"
GRID_CLIENT="$(./""$5"" -h)"
GRID_SERVER="$(./""$6"" -h)"

UTILS=( "BAR" "DMENU" "GRID_CLIENT" "GRID_SERVER" )

cp "$SRC_README" "$TGT_README"

for UTIL in "BAR DMENU GRID_CLIENT GRID_SERVER";
    eval HELP=\${"$UTIL"}
    sed 's/HELP_OUTPUT_FOR_'"$UTIL"'/'"$HELP"'/g' "$TGT_README" "$TGT_README"".tmp"
    mv "$TGT_README"".tmp" "$TGT_README"
done
