#!/bin/bash
export TMPDIR=$HOME/tmp
cd /home/odroid/CapstoneBuoyProject
echo -n "odroid" | gnome-keyring-daemon --replace --unlock
python3 Deckbox.py /dev/ttyUSB0
