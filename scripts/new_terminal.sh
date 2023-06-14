#!/bin/bash

echo "--- Opening output shell ---"
echo
gnome-terminal --geometry=81x40-1 -- bash -c "../bin/hmi-output; exec bash"

