============
Installation
============

(LADI) *a2jmidid* primary build system is the |waf| build system.
*a2jmidid* can use the |meson| build system.


Configure, build & install using |waf| with a2jmidid
-------------------

Configure it::

  ./waf configure

This will configure for installation to /usr/local prefix.
If you want to use other prefix, use --prefix option::

  ./waf configure --prefix=/usr

For full list of options, run::

  ./waf configure --help

There are two custom options:

 * "--disable-dbus will" force disable dbus support, even if dependencies are present
 * "--enable-pkg-config-dbus-service-dir" will force D-Bus service install
   dir to be one returned by pkg-config. This is usually needed when
   prefix is /usr/local because dbus daemon scans /usr for service
   files but does not in /usr/local

Build it::

  ./waf

You can use -j option to enable building on more than one CPU::

  ./waf -j 4

Install it::

  ./waf install

You probably want to run later as superuser to install system-wide

Configure and build using |meson|
-------------------

To configure the project, |meson|'s |meson_universal_options| (e.g. *prefix*)
can be used to prepare a build directory::

  meson --prefix=/usr build

One additional - project specific - option enables for building without |dbus|
support::

  meson --prefix=/usr -Ddisable-dbus=true build

To build the application |ninja| is required::

  ninja -C build

Install using |meson|
-------

|meson| is able to install the project components to the system directories
(when run as root), while honoring the *DESTDIR* environment variable::

  DESTDIR="/some/other/location" meson install -C build

.. |waf| raw:: html

  <a href="https://waf.io/" target="_blank">WAF</a>

.. |meson| raw:: html

  <a href="https://mesonbuild.com/" target="_blank">Meson</a>

.. |meson_universal_options| raw:: html

  <a href="https://mesonbuild.com/Builtin-options.html#universal-options" target="_blank">universal options</a>

.. |dbus| raw:: html

  <a href="https://www.freedesktop.org/wiki/Software/dbus/" target="_blank">D-Bus</a>

.. |ninja| raw:: html

  <a href="https://ninja-build.org/" target="_blank">Ninja</a>

