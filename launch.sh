#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
ROM="$1"
ROM_NAME="$(basename "$ROM")"

export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
export SDL_VIDEODRIVER=dummy
export SDL_RENDER_DRIVER=software
export SDL_JOYSTICK_LINUX_DEVNAMES="none"
export DOOMWADDIR="/mnt/SDCARD/Roms/DOOM (DOOM)"
export DOOMWADPATH="$DIR/bin:$DIR/base:/mnt/SDCARD/Roms/DOOM (DOOM)"
export HOME="$DIR/config"

exec > "$DIR/log.txt" 2>&1


echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null
killall -STOP keymon.elf batmon.elf 2>/dev/null

SAVE_BASE="$DIR/config/.local/share/woof/savegames"
RESUME_MARKER="$DIR/config/.autoresume_active"
RESUME_FLAG=""

# Only auto-load if the previous session ended via Power-button Save & Off
if [ -f "$RESUME_MARKER" ]; then
    rm -f "$RESUME_MARKER"
    for f in "$SAVE_BASE"/*/woofq1.dsg; do
        if [ -f "$f" ]; then
            cp -f "$f" "$(dirname "$f")/autosave.dsg"
        fi
    done
    for f in "$SAVE_BASE"/*/autosave.dsg; do
        if [ -f "$f" ]; then
            RESUME_FLAG="-loadgame 255"
            break
        fi
    done
fi

echo "Launching ROM=$ROM RESUME_FLAG=$RESUME_FLAG"

REL_ROM="${ROM#/mnt/SDCARD}"
STATE_DIR="/mnt/SDCARD/.userdata/shared/DOOM-doom"
MINUI_TAG_DIR="/mnt/SDCARD/.userdata/shared/.minui/DOOM"

mkdir -p "$DIR/config" /mnt/SDCARD/.userdata/shared/.minui "$STATE_DIR" "$MINUI_TAG_DIR"
echo "1" > "$RESUME_MARKER"
printf "%s" "$REL_ROM" > /mnt/SDCARD/.userdata/shared/.minui/auto_resume.txt
printf "0" > "$MINUI_TAG_DIR/${ROM_NAME}.txt"
printf "%s" "$ROM_NAME" > "$MINUI_TAG_DIR/${ROM_NAME}.0.txt"
echo "woof" > "$STATE_DIR/${ROM_NAME}.st0"
sync

cd "$DIR/bin"
case "$ROM" in
    *.sh|*.doom)
        sh "$ROM" "$DIR/bin/woof" $RESUME_FLAG
        ;;
    *)
        "$DIR/bin/woof" -iwad "$ROM" $RESUME_FLAG
        ;;
esac

killall -CONT keymon.elf batmon.elf 2>/dev/null
echo ondemand > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null

# Power button shutdown: keep .autoresume_active + auto_resume.txt + .st0 so next boot auto-resumes!
if [ -f /tmp/woof_poweroff ]; then
    rm -f /tmp/woof_poweroff
    sync
    sleep 1
    sync
    poweroff
    exit 0
fi

# Normal exit via Doom's Quit menu: clear auto-resume files so next launch starts fresh at title screen
rm -f "$RESUME_MARKER" /mnt/SDCARD/.userdata/shared/.minui/auto_resume.txt 2>/dev/null
rm -f "$STATE_DIR/${ROM_NAME}.st0" "$MINUI_TAG_DIR/${ROM_NAME}.txt" "$MINUI_TAG_DIR/${ROM_NAME}.0.txt" 2>/dev/null
rm -f "$SAVE_BASE"/*/autosave.dsg "$SAVE_BASE"/*/woofq1.dsg 2>/dev/null
sync
