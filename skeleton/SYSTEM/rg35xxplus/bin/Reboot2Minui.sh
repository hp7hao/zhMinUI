#!/bin/sh
# Script to restore MinUI by renaming _dmenu.bin back to dmenu.bin
# This can be run from stock firmware to re-enable MinUI

TF1_PATH=/mnt/mmc

if [ -f $TF1_PATH/_dmenu.bin ]; then
    echo "Restoring MinUI..."
    mv $TF1_PATH/_dmenu.bin $TF1_PATH/dmenu.bin
    sync
    echo "MinUI restored. Reboot to use MinUI."
    reboot
else
    echo "MinUI is already active or not installed."
fi

sync && reboot
