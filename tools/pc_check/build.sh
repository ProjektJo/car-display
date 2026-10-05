#!/bin/bash
# Baut LVGL 9.2.2 + Firmware-UI + Harness für den PC
set -e
LV=/tmp/claude-0/lv/lightvgl-sys-9.2.2/vendor/lvgl
P=$(cd "$(dirname "$0")/../.." && pwd)
H=$P/tools/pc_check
O=/tmp/claude-0/host/obj
mkdir -p $O/lv $O/fw
AJ=${AJ_DIR:-/tmp/claude-0/ajinc}  # Ordner mit ArduinoJson.h (Einzeldatei der Version 7)
COMMON="-DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -I$P/include -I$P/src -I$H/stubs -I$AJ -I$LV -include $P/include/board_fnk0104b.h -O1 -g"
if [ ! -f $O/liblvgl.a ]; then
  find $LV/src -name '*.c' | xargs -P 16 -I{} sh -c 'f={}; o='$O'/lv/$(echo $f | md5sum | cut -c1-12).o; gcc -c '"$COMMON"' -w $f -o $o'
  ar rcs $O/liblvgl.a $O/lv/*.o
fi
FW_CPP="$P/src/ui/ui.cpp $P/src/ui/theme.cpp $P/src/ui/values.cpp $P/src/ui/history.cpp $P/src/ui/linechart.cpp $P/src/ui/range_bar.cpp $P/src/ui/tank_dialog.cpp $P/src/ui/ui_prefs.cpp $P/src/ui/eco_popups.cpp $P/src/ui/live.cpp $P/src/ui/statusbar.cpp $P/src/ui/overlay.cpp $P/src/ui/menu.cpp $P/src/ui/start_screen.cpp $P/src/ui/vehicle_dialog.cpp $P/src/util/link_text.cpp $(ls $P/src/ui/pages/*.cpp) $P/src/hw/display.cpp $P/src/hw/touch.cpp $P/src/core/car_state_store.cpp $P/src/core/commands.cpp $P/src/core/calc_task.cpp $P/src/util/format.cpp $P/src/sim/simulator.cpp $(ls $P/src/calc/*.cpp) $(ls $P/src/storage/*.cpp)"
EXTRA="${EXTRA_DEFS:-}"
rm -f $O/fw/*.o
for f in $FW_CPP $H/harness.cpp; do
  g++ -std=gnu++17 -c $COMMON $EXTRA -Wall -Wextra -Wshadow -Wno-unused-parameter $f -o $O/fw/$(basename $f .cpp)_$(echo $f | md5sum | cut -c1-6).o &
done
for f in $P/src/ui/fonts/*.c; do gcc -c $COMMON -Wall $f -o $O/fw/$(basename $f .c).o & done
wait
g++ -o /tmp/claude-0/host/harness $O/fw/*.o $O/liblvgl.a -lm
echo built
