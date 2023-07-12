#!/bin/bash

TERM_LAUNCH=gnome-terminal;
if (($BASH_ARGC>0));
then TERM_LAUNCH=${BASH_ARGV[0]};
fi
$TERM_LAUNCH -e ./bin/hmi-output

