The Makefile should make it easy to install the tools on Unix-like systems.

For Linux and other Unix systems.
=================================
Compile using:
	make
for Linux or other Unix systems.

Install (and compile first if necessary) with
	make install

Set PREFIX (distributed as /usr/local) in Makefile to the base directory
where you want to do the installation. Installs in $(PREFIX)/bin and
($PREFIX)/lib/perl.

You may not normally have privileges to imstall in /usr/local.

Either edit PREFIX or use
	make PREFIX=/my/install/directory ...
to install somewhere else.

For MacOS X.
============
Compile using:
	make -f Makefile.osx

Install (and compile first if necessary) with
	make -f Makefile.osx install

Set PREFIX in Makefile.osx as for Linux to install somewhere other than
/usr/local or use PREFIX=... as for Linux.

For Cygwin.
===========
Compile using:
	make -f Makefile.win32

Install (and compile first if necessary) with
	make -f Makefile.win32 install

Set PREFIX in Makefile.win32 as for Linux to install somewhere other than
/usr/local or use PREFIX=... as for Linux.

The .exe files built under Cygwin will run under Windows, provided
/lib/cygwin1.dll is copied from a Cygwin installation into the directory
where the .exe file are installed.

For Windows.
============
If you have Cygwin installed, see the Cygwin instructions.

Otherwise use the pre-built .exe file and cygwin1.dll in
the ZIP package from
http://www.beyonwizsoftware.net/software-b28/wiz-firmware-tools/
