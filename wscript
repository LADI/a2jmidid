#! /usr/bin/env python
# encoding: utf-8

from __future__ import print_function

import os
import subprocess
import shutil
import re

from waflib import Logs, Options, TaskGen, Context, Utils
from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext

APPNAME='a2jmidid'
VERSION='9'

# these variables are mandatory ('/' are converted automatically)
srcdir = '.'
blddir = 'build'

def git_ver(self):
    bld = self.generator.bld
    header = self.outputs[0].abspath()
    if os.access('./version.h', os.R_OK):
        header = os.path.join(os.getcwd(), out, "version.h")
        shutil.copy('./version.h', header)
        data = open(header).read()
        m = re.match(r'^#define GIT_VERSION "([^"]*)"$', data)
        if m != None:
            self.ver = m.group(1)
            Logs.pprint('BLUE', "tarball from git revision " + self.ver)
        else:
            self.ver = "tarball"
        return

    if bld.srcnode.find_node('.git'):
        self.ver = bld.cmd_and_log("LANG= git rev-parse HEAD", quiet=Context.BOTH).splitlines()[0]
        if bld.cmd_and_log("LANG= git diff-index --name-only HEAD", quiet=Context.BOTH).splitlines():
            self.ver += "-dirty"

        Logs.pprint('BLUE', "git revision " + self.ver)
    else:
        self.ver = "unknown"

    fi = open(header, 'w')
    fi.write('#define GIT_VERSION "%s"\n' % self.ver)
    fi.close()

def display_msg(conf, msg="", status = None, color = None):
    if status:
        #Logs.pprint(msg, status, color)
        conf.msg(msg, status, color=color)
    else:
        Logs.pprint('NORMAL', msg)

def display_raw_text(conf, text, color = 'NORMAL'):
    Logs.pprint(color, text, sep = '')

def display_line(conf, text, color = 'NORMAL'):
    Logs.pprint(color, text, sep = os.linesep)

def options(opt):
    # options provided by the modules
    opt.load('compiler_c')
    #opt.load('autooptions')

    opt.add_option('--enable-pkg-config-dbus-service-dir', action='store_true', default=False, help='force D-Bus service install dir to be one returned by pkg-config')
    opt.add_option('--disable-dbus', action='store_true', default=False, help="Don't enable D-Bus support even if required dependencies are present")
    opt.add_option('--mandir', type='string', help="Manpage directory [Default: <prefix>/share/man]")

def configure(conf):
    conf.load('compiler_c')

    conf.check_cfg(package='alsa', mandatory=True, args='--cflags --libs')
    conf.check_cfg(package='jack', vnum="0.109.0", mandatory=True, args='--cflags --libs')

    if not Options.options.disable_dbus:
        conf.check_cfg(package='dbus-1', mandatory=False, args='--cflags --libs')
        conf.env['DBUS_ENABLED'] = 'LIB_DBUS-1' in conf.env
    else:
        conf.env['DBUS_ENABLED'] = False

    if conf.env['DBUS_ENABLED']:
        dbus_dir = conf.check_cfg(package='dbus-1', args='--variable=session_bus_services_dir', msg="Retrieving D-Bus services dir")
        if not dbus_dir:
            return

        dbus_dir = dbus_dir.strip()
        conf.env['DBUS_SERVICES_DIR_REAL'] = dbus_dir

        if Options.options.enable_pkg_config_dbus_service_dir:
            conf.env['DBUS_SERVICES_DIR'] = dbus_dir
        else:
            conf.env['DBUS_SERVICES_DIR'] = os.path.join(os.path.normpath(conf.env['PREFIX']), 'share', 'dbus-1', 'services')

    conf.env['LIB_DL'] = ['dl']
    conf.env['LIB_PTHREAD'] = ['pthread']

    # conf.c is the only user of expat and its build is disabled
    #conf.check_header('expat.h', mandatory=True)
    #conf.env['LIB_EXPAT'] = ['expat']

    conf.check(header_name='getopt.h', mandatory=True)

    if Options.options.mandir:
        conf.env['MANDIR'] = Options.options.mandir
    else:
        conf.env['MANDIR'] = conf.env['PREFIX'] + '/share/man'

    conf.define('A2J_VERSION', VERSION)
    conf.write_config_header('config.h')

    gitrev = None
    if os.access('gitversion.h', os.R_OK):
        data = file('gitversion.h').read()
        m = re.match(r'^#define GIT_VERSION "([^"]*)"$', data)
        if m != None:
            gitrev = m.group(1)

    print()
    display_msg(conf, "==================")
    version_msg = "a2jmidid-" + VERSION
    if gitrev:
        version_msg += " exported from " + gitrev
    else:
        version_msg += " git revision will checked and eventually updated during build"
    print(version_msg)
    print()

    display_msg(conf, "Install prefix", conf.env['PREFIX'], 'CYAN')
    if conf.env['DBUS_ENABLED']:
        have_dbus_status = "yes"
    else:
        have_dbus_status = "no"
    display_msg(conf, "D-Bus support", have_dbus_status)
    if conf.env['DBUS_ENABLED']:
        display_msg(conf, 'D-Bus service install directory', conf.env['DBUS_SERVICES_DIR'], 'CYAN')
        if conf.env['DBUS_SERVICES_DIR'] != dbus_dir:
            display_msg(conf)
            display_line(conf,     "WARNING: D-Bus session services directory as reported by pkg-config is", 'RED')
            display_raw_text(conf, "WARNING:", 'RED')
            display_line(conf,      conf.env['DBUS_SERVICES_DIR_REAL'], 'CYAN')
            display_line(conf,     'WARNING: but service file will be installed in', 'RED')
            display_raw_text(conf, "WARNING:", 'RED')
            display_line(conf,      conf.env['DBUS_SERVICES_DIR'], 'CYAN')
            display_line(conf,     'WARNING: You may need to adjust your D-Bus configuration after installing ladish', 'RED')
            display_line(conf,     'WARNING: You can override dbus service install directory', 'RED')
            display_line(conf,     'WARNING: with --enable-pkg-config-dbus-service-dir option to this script', 'RED')
    print()

def build(bld):
    bld(rule=git_ver,
        target='gitversion.h',
        update_outputs=True,
        always=True,
        ext_out=['.h'])

    prog = bld(features=['c', 'cprogram'])
    prog.source = [
        'a2jmidid.c',
        'log.c',
        'port.c',
        'port_thread.c',
        'port_hash.c',
        'paths.c',
        #'conf.c',
        'jack.c',
        'list.c',
        'sigsegv.c',
        ]

    if bld.env['DBUS_ENABLED']:
        prog.source.append('dbus.c')
        prog.source.append('dbus_iface_introspectable.c')
        prog.source.append('dbus_iface_control.c')

    prog.includes = '.' # make waf dependency tracking work
    prog.target = 'a2jmidid'
    prog.use = ['ALSA', 'JACK', 'DL', 'PTHREAD']
    if bld.env['DBUS_ENABLED']:
        prog.use.append('DBUS-1')

    prog = bld(features=['c', 'cprogram'])
    prog.source = 'a2jmidi_bridge.c'
    prog.target = 'a2jmidi_bridge'
    prog.use = ['ALSA', 'JACK']

    prog = bld(features=['c', 'cprogram'])
    prog.source = 'j2amidi_bridge.c'
    prog.target = 'j2amidi_bridge'
    prog.use = ['ALSA', 'JACK']

    if bld.env['DBUS_ENABLED']:
        # process org.gna.home.a2jmidid.service.in -> org.gna.home.a2jmidid.service
        obj = bld(features='subst')
        obj.source = 'org.gna.home.a2jmidid.service.in'
        obj.target = 'org.gna.home.a2jmidid.service'
        obj.install_path = '${DBUS_SERVICES_DIR}/'
        obj.BINDIR = bld.env['PREFIX'] + '/bin'

        bld.install_as(
            os.path.join(bld.env['PREFIX'], 'bin', 'a2j_control'),
            'a2j_control',
            chmod=Utils.O755)
        bld.install_as(
            os.path.join(bld.env['PREFIX'], 'bin', 'a2j'),
            'a2j',
            chmod=Utils.O755)

    # install man pages
    man_pages = [
        "a2jmidi_bridge.1",
        "a2jmidid.1",
        "j2amidi_bridge.1",
        ]

    if bld.env['DBUS_ENABLED']:
        man_pages.append("a2j.1")
        man_pages.append("a2j_control.1")

    for i in range(len(man_pages)):
        man_pages[i] = "man/" + man_pages[i]

    bld.install_files(os.path.join(bld.env['MANDIR'], 'man1'), man_pages)

def dist_hook():
    os.remove('gitversion_regenerate.sh')
    os.system('../gitversion_regenerate.sh gitversion.h')
