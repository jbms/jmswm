#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2010 (ita)

import os

#VERSION='0.0.1'
APPNAME='jmswm'

top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('cxx_autodep', tooldir='waftools')

def v_release(env):
    env.append_unique('DEFINES', ['NDEBUG', 'BOOST_DISABLE_ASSERTS'])
    env.append_unique('CXXFLAGS', ['-O2'])

def v_debug(env):
    env.append_unique('CXXFLAGS', ['-ggdb'])
    pass

def v_profile(env):
    env.append_unique('CXXFLAGS', ['-pg'])
    env.append_unique('LINKFLAGS', ['-pg'])



build_variants = [ {'release': v_release, 'debug': v_debug},
                   {'': None, 'profile': v_profile}]

def variant_list(variants = build_variants, base = [('', [])]):
    if len(variants) == 0: return base
    all_out = []
    for name, callback in variants[0].items():
        out = []
        for vname, vcallbacks in base:
            if len(vname) > 0 and len(name) > 0:
                vname += '_'
            vname += name
            vcallbacks = vcallbacks + [callback]
            out.append((vname, vcallbacks))
        all_out.extend(variant_list(variants[1:], out))
    return all_out

def configure(conf):
    conf.load('compiler_cxx')
    conf.load('cxx_autodep', tooldir='waftools')
    conf.env.append_unique('INCLUDES', ['src'])
    conf.env.CXXFLAGS = ['-ggdb',
#                         '-DBOOST_RESULT_OF_USE_DECLTYPE',
                         '-Wall', '-march=native']

    conf.env.cxx_autodep_header_uselib_map = {
                                              'boost/atomic.hpp': ['BOOST_ATOMIC'],
                                              'chaos': ['CHAOS']}

    def do_map(name, *headers):
        libname = name.upper()
        for header in headers:
            conf.env.cxx_autodep_header_uselib_map.setdefault(header, []).append(libname)

    def do_pkgconfig(name, *headers):
        conf.check_cfg(package=name, args=['--cflags', '--libs'])
        do_map(name, *headers)

    do_pkgconfig('x11', 'X11')
    do_pkgconfig('xft', 'X11/Xft')
    do_pkgconfig('pango', 'pango')
    do_pkgconfig('pangoxft', 'pango/pangoxft.h')
    do_pkgconfig('alsa', 'alsa')
    do_pkgconfig('xrandr', 'X11/extensions/Xrandr.h')

    #conf.env.INCLUDES_BOOST = ['external/boost']

    conf.env.LIB_IW = ['iw']
    do_map('iw', 'iwlib.h')


    conf.env.INCLUDES_CHAOS = ['external/chaos-pp']
    conf.env.DEFINES_CHAOS = ['CHAOS_PP_VARIADICS=1']

    conf.env.INCLUDES_BOOST_ATOMIC = ['external/boost.atomic']

    conf.env.LIB_BOOST_THREAD = ['boost_thread']
    conf.env.LINKFLAGS_BOOST_THREAD = ['-pthread']
    do_map('boost_thread', 'boost/thread.hpp', 'boost/thread')

    conf.env.LIB_BOOST_SERIALIZATION = ['boost_serialization']
    do_map('boost_serialization', 'boost/serialization')

    conf.env.LIB_BOOST_REGEX = ['boost_regex']
    do_map('boost_regex', 'boost/regex', 'boost/regex.hpp')

    conf.env.LIB_BOOST_SIGNALS = ['boost_signals']
    do_map('boost_signals', 'boost/signals', 'boost/signals.hpp')

    conf.env.LIB_BOOST_FILESYSTEM = ['boost_filesystem']
    do_map('boost_filesystem', 'boost/filesystem', 'boost/filesystem.hpp')

    conf.env.LIB_EVENT = ['event']
    do_map('event', 'event.h')

    conf.env.LIB_BOOST_RANDOM = ['boost_random']

    base_env = conf.env
    for name, callbacks in variant_list():
        conf.setenv(name, base_env)
        for callback in callbacks:
            if callback is not None:
                callback(conf.env)

def build(bld):
    if not bld.variant:
        bld.fatal('specify a build variant')
    bld.program(source='src/wm/main.cpp', target = 'jmswm')

from waflib.Build import BuildContext, CleanContext, \
        InstallContext, UninstallContext, ListContext

for x, callbacks in variant_list():
        for y in (BuildContext, CleanContext, InstallContext, UninstallContext, ListContext):
                name = y.__name__.replace('Context','').lower()
                class tmp(y):
                        cmd = name + '_' + x
                        variant = x
