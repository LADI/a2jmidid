=========
Changelog
=========

LADI Version 12 on 2024-01-08
-----------------------
* Fix meson build with when dbus support is disabled
* a2jmidid.c: fix include of siginfo.h

LADI Version 11 on 2024-01-07
-----------------------
* Fix bunch of warnings
* Upgrade waf to 2.0.26

LADI Version 10 on 2023-11-09
-----------------------
* wscript: Use WafToolchainFlags
* remove "legacy" wording
* Documentation fixes

LADI Version 9.1 on 2023-05-04
-----------------------
* Allow bypass of the conversion of MIDI note on events with velocity 0
  While these are invalid events and the conversion helps hearing them,
  some MIDI hardware vendors do use them anyway.
  The bypass is toggable via command a2jmidid line -n option.
  The default is not changed. Kudos to Guido Aulisi
  https://github.com/linuxaudio/a2jmidid/issues/14

LADI Version 9 on 2022-08-28
-----------------------

* port a2j_control to python3
* add meson as possible build system
* man page spelling fixes
* control unique port names over D-Bus

Version 8 "Sophronius of Vratsa" on 2012-07-05
----------------------------------------------

* -u commandline option
* D-Bus method for checking whether hw export is enabled. Kudos to Danni Coy
* Fix for resource leak. Kudos to Dan A. Muresan
* Improved error message for snd_seq_connect_to() failures
* --mandir= option in waf. Kudos to Dan Church

Version 7 "Paisius of Hilendar" on 2011-01-16
---------------------------------------------

* MIDI processing improvements
* Use the JACK limit for max port name size (sr #2526)
* Adopt to shared library policy changes in some distros (sr #2547)
* dbus support can now be disabled at configure stage
* fix build on not so common platforms (LP: #556351)
* man pages (from Debian)
* reopen log file when it is deleted or replaced

Version 6 "Indzhe Voyvoda" on 2009-12-29
----------------------------------------

* MIDI processing improvements
* Handle large number of ports
* a2j script (non-dbus-like behaviour for dbus environment)
* Allow tweaking through dbus of the hardware port export option
* Fix a use of invalid memory
* Fix port miss that can occur if port appears during bridge startup

Version 5 "Athos" on 2009-06-13
-------------------------------

* Fix thight loop on D-Bus disconnect
* D-Bus signals for bridge start and stop
* Fixed alsamidi "disappearing output" bug. (backport from jack1)
* MIDI note-off normalization fix from Dave Robillard (Backport from jack1)
* Removed wrong assert from alsa_seqmidi.c reported by Ken Ellinwood (Backport
  from jack1)
* Mark anything that looks like a hardware port as physical&terminal (Backport
  from jack1/jack2)
* Fix potential crash when D-Bus is not used
* Support for multiple ALSA clients with same name
* Merge midibridge changeset by Paul Davis that is expected to fix midi event
  timing problems that some people have reported.

Version 4 "Devsirme" on 2008-08-03
----------------------------------

* Fix typos in docs
* Disable use of C++ demangling in sigsegv. Fix for sr #2074
* Fix a2j_control help text (thanks kfoltman!)
* Request fixed JACK client name. Fix for bug #12139
* Handle missing svnversion executable nicely. Fixes bug #12138

Version 3 "Bodrum" on 2008-08-03
--------------------------------

* Improved port naming, support for bidirectional ports
* Allow exporting of hardware ports (disabled by default)
* Switch from autotools to waf
* Remove support for old JACK MIDI API variants
* Remove usage of posix semaphores that could cause sleep in realtime context,
  in rare circumstances
* D-Bus background service mode. The old manual mode is still working.
* Log file when running in background service mode.
* Improved documentation
* Import, with slight modifications, static bridges created by Sean Bolton and
  Lars Luthman.

Version 2 on 2007-10-27
-----------------------

* Improved build system (autotools) and support for older JACK variants

Version 1 on 2007-08-26
-----------------------

* Initial release
