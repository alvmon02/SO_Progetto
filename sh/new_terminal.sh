#!/bin/bash

TERM_LAUNCH=gnome-terminal;
if (($BASH_ARGC>0));
then TERM_LAUNCH=${BASH_ARGV[0]};
fi
echo $TERM_LAUNCH;
echo ${BASH_ARGC};
$TERM_LAUNCH -- bash -c "./bin/hmi-output; exec bash"

