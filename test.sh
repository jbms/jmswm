#!/bin/bash
cd "$(dirname "$0")"

startx ${PWD}/build/jmswm -- /usr/bin/Xephyr -resizeable :1
