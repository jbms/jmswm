#!/bin/bash
cd "$(dirname "$0")"

startx ${PWD}/jmswm -- /usr/bin/Xephyr -resizeable :1
