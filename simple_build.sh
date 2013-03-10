#!/bin/bash -x

cd "$(dirname "$0")"

mkdir -p build/release

CXXFLAGS="-ggdb -Wall -march=native -DCHAOS_PP_VARIADICS=1 -pthread -DNDEBUG -DBOOST_DISABLE_ASSERTS -O2 -Isrc -Iexternal/chaos-pp"
LIBS="-liw -lboost_filesystem -lboost_serialization -lboost_signals -levent -lboost_random -lboost_regex -lboost_thread"
PKGS=( x11 xft pango pangoxft alsa xrandr )


SOURCES=$(find src -name \*.cpp)
TARGET=build/release/jmswm
CXX=g++

${CXX} -o $TARGET $SOURCES $CXXFLAGS $(pkg-config ${PKGS[@]} --cflags --libs) $LIBS
