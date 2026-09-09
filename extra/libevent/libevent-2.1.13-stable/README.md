<p align="center">
  <img src="https://strcpy.net/libevent3.png" alt="libevent logo"/>
</p>



[![Appveyor Win32 Build Status](https://ci.appveyor.com/api/projects/status/ng3jg0uhy44mp7ik?svg=true)](https://ci.appveyor.com/project/libevent/libevent)
[![Travis Build Status](https://travis-ci.org/libevent/libevent.svg?branch=master)](https://travis-ci.org/libevent/libevent)
[![Coverage Status](https://coveralls.io/repos/github/libevent/libevent/badge.svg)](https://coveralls.io/github/libevent/libevent)
[![Join the chat at https://gitter.im/libevent/libevent](https://badges.gitter.im/libevent/libevent.svg)](https://gitter.im/libevent/libevent?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)



# 0. BUILDING AND INSTALLATION (Briefly)

## Autoconf

     $ ./configure
     $ make
     $ make verify   # (optional)
     $ sudo make install

## CMake (General)


The following Libevent specific CMake variables are as follows (the values being
the default).

```
# Type of the library to build (SHARED or STATIC)
# Default is: SHARED for MSVC, otherwise BOTH
EVENT__LIBRARY_TYPE:STRING=DEFAULT

# Installation directory for CMake files
EVENT_INSTALL_CMAKE_DIR:PATH=lib/cmake/libevent

# Enable running gcov to get a test coverage report (only works with
# GCC/CLang). Make sure to enable -DCMAKE_BUILD_TYPE=Debug as well.
EVENT__COVERAGE:BOOL=OFF

# Defines if Libevent should build without the benchmark executables
EVENT__DISABLE_BENCHMARK:BOOL=OFF

# Define if Libevent should build without support for a debug mode
EVENT__DISABLE_DEBUG_MODE:BOOL=OFF

# Define if Libevent should not allow replacing the mm functions
EVENT__DISABLE_MM_REPLACEMENT:BOOL=OFF

# Define if Libevent should build without support for OpenSSL encryption
EVENT__DISABLE_OPENSSL:BOOL=OFF

# Disable the regress tests
EVENT__DISABLE_REGRESS:BOOL=OFF

# Disable sample files
EVENT__DISABLE_SAMPLES:BOOL=OFF

# If tests should be compiled or not
EVENT__DISABLE_TESTS:BOOL=OFF

# Define if Libevent should not be compiled with thread support
EVENT__DISABLE_THREAD_SUPPORT:BOOL=OFF

# Enables verbose debugging
EVENT__ENABLE_VERBOSE_DEBUG:BOOL=OFF

# When cross compiling, forces running a test program that verifies that Kqueue
# works with pipes. Note that this requires you to manually run the test program
# on the the cross compilation target to verify that it works. See CMake
# documentation for try_run for more details
EVENT__FORCE_KQUEUE_CHECK:BOOL=OFF
```

__More variables can be found by running `cmake -LAH <sourcedir_path>`__


## CMake (Windows)

Install CMake: <https://www.cmake.org>


     $ md build && cd build
     $ cmake -G "Visual Studio 10" ..   # Or whatever generator you want to use cmake --help for a list.
     $ start libevent.sln

## CMake (Unix)

     $ mkdir build && cd build
     $ cmake ..     # Default to Unix Makefiles.
     $ make
     $ make verify  # (optional)


# 1. BUILDING AND INSTALLATION (In Depth)

## Autoconf

To build Libevent, type

     $ ./configure && make


 (If you got Libevent from the git repository, you will
  first need to run the included "autogen.sh" script in order to
  generate the configure script.)

You can run the regression tests by running

     $ make verify

Install as root via

     $ make install

Before reporting any problems, please run the regression tests.

To enable low-level tracing, build the library as:

     $ CFLAGS=-DUSE_DEBUG ./configure [...]

Standard configure flags should work.  In particular, see:

     --disable-shared          Only build static libraries.
     --prefix                  Install all files relative to this directory.


The configure script also supports the following flags:

     --enable-gcc-warnings     Enable extra compiler checking with GCC.
     --disable-malloc-replacement
                               Don't let applications replace our memory
                               management functions.
     --disable-openssl         Disable support for OpenSSL encryption.
     --disable-thread-support  Don't support multithreaded environments.

## CMake (Windows)

(Note that autoconf is currently the most mature and supported build
environment for Libevent; the CMake instructions here are new and
experimental, though they _should_ be solid.  We hope that CMake will
still be supported in future versions of Libevent, and will try to
make sure that happens.)

First of all install <https://www.cmake.org>.

To build Libevent using Microsoft Visual studio open the "Visual Studio Command prompt" and type:

```
$ cd <libevent source dir>
$ mkdir build && cd build
$ cmake -G "Visual Studio 10" ..   # Or whatever generator you want to use cmake --help for a list.
$ start libevent.sln
```

In the above, the ".." refers to the dir containing the Libevent source code. 
You can build multiple versions (with different compile time settings) from the same source tree
by creating other build directories. 

It is highly recommended to build "out of source" when using
CMake instead of "in source" like the normal behaviour of autoconf for this reason.

The "NMake Makefiles" CMake generator can be used to build entirely via the command line.

To get a list of settings available for the project you can type:

```
$ cmake -LH ..
```

### GUI

CMake also provides a GUI that lets you specify the source directory and output (binary) directory
that the build should be placed in.

# 2. USEFUL LINKS:

For the latest released version of Libevent, see the official website at
<http://libevent.org/> .

There's a pretty good work-in-progress manual up at
   <http://www.wangafu.net/~nickm/libevent-book/> .

For the latest development versions of Libevent, access our Git repository
via

```
$ git clone https://github.com/libevent/libevent.git
```

You can browse the git repository online at:

<https://github.com/libevent/libevent>

To report bugs, issues, or ask for new features:

__Patches__: https://github.com/libevent/libevent/pulls
> OK, those are not really _patches_. You fork, modify, and hit the "Create Pull Request" button.
> You can still submit normal git patches via the mailing list.

__Bugs, Features [RFC], and Issues__: https://github.com/libevent/libevent/issues
> Or you can do it via the mailing list.

There's also a libevent-users mailing list for talking about Libevent
use and development: 

<http://archives.seul.org/libevent/users/>

# 3. ACKNOWLEDGMENTS

The following people have helped with suggestions, ideas, code or
fixing bugs:

 * Samy Al Bahra
 * Antony Antony
 * Jacob Appelbaum
 * Arno Bakker
 * Weston Andros Adamson
 * William Ahern
 * Ivan Andropov
 * Sergey Avseyev
 * Avi Bab
 * Joachim Bauch
 * Andrey Belobrov
 * Gilad Benjamini
 * Stas Bekman
 * Denis Bilenko
 * Julien Blache
 * Kevin Bowling
 * Tomash Brechko
 * Kelly Brock
 * Ralph Castain
 * Adrian Chadd
 * Lawnstein Chan
 * Shuo Chen
 * Ka-Hing Cheung
 * Andrew Cox
 * Paul Croome
 * George Danchev
 * Andrew Danforth
 * Ed Day
 * Christopher Davis
 * Mike Davis
 * Frank Denis
 * Antony Dovgal
 * Mihai Draghicioiu
 * Alexander Drozdov
 * Mark Ellzey
 * Shie Erlich
 * Leonid Evdokimov
 * Juan Pablo Fernandez
 * Christophe Fillot
 * Mike Frysinger
 * Remi Gacogne
 * Artem Germanov
 * Alexander von Gernler
 * Diego Giagio
 * Artur Grabowski
 * Diwaker Gupta
 * Kuldeep Gupta
 * Sebastian Hahn
 * Dave Hart
 * Greg Hazel
 * Nicholas Heath
 * Michael Herf
 * Savg He
 * Mark Heily
 * Maxime Henrion
 * Michael Herf
 * Greg Hewgill
 * Andrew Hochhaus
 * Aaron Hopkins
 * Tani Hosokawa
 * Jamie Iles
 * Xiuqiang Jiang
 * Claudio Jeker
 * Evan Jones
 * Marcin Juszkiewicz
 * George Kadianakis
 * Makoto Kato
 * Phua Keat
 * Azat Khuzhin
 * Alexander Klauer
 * Kevin Ko
 * Brian Koehmstedt
 * Marko Kreen
 * Ondřej Kuzník
 * Valery Kyholodov
 * Ross Lagerwall
 * Scott Lamb
 * Christopher Layne
 * Adam Langley
 * Graham Leggett
 * Volker Lendecke
 * Philip Lewis
 * Zhou Li
 * David Libenzi
 * Yan Lin
 * Moshe Litvin
 * Simon Liu
 * Mitchell Livingston
 * Hagne Mahre
 * Lubomir Marinov
 * Abilio Marques
 * Nicolas Martyanoff
 * Abel Mathew
 * Nick Mathewson
 * James Mansion
 * Nicholas Marriott
 * Andrey Matveev
 * Caitlin Mercer
 * Dagobert Michelsen
 * Andrea Montefusco
 * Mansour Moufid
 * Mina Naguib
 * Felix Nawothnig
 * Trond Norbye
 * Linus Nordberg
 * Richard Nyberg
 * Jon Oberheide
 * John Ohl
 * Phil Oleson
 * Alexey Ozeritsky
 * Dave Pacheco
 * Derrick Pallas
 * Tassilo von Parseval
 * Catalin Patulea
 * Patrick Pelletier
 * Simon Perreault
 * Dan Petro
 * Pierre Phaneuf
 * Amarin Phaosawasdi
 * Ryan Phillips
 * Dimitre Piskyulev
 * Pavel Plesov
 * Jon Poland
 * Roman Puls
 * Nate R
 * Robert Ransom
 * Balint Reczey
 * Bert JW Regeer
 * Nate Rosenblum
 * Peter Rosin
 * Maseeb Abdul Qadir
 * Wang Qin
 * Alex S
 * Gyepi Sam
 * Hanna Schroeter
 * Ralf Schmitt
 * Mike Smellie
 * Steve Snyder
 * Nir Soffer
 * Dug Song
 * Dongsheng Song
 * Hannes Sowa
 * Joakim Soderberg
 * Joseph Spadavecchia
 * Kevin Springborn
 * Harlan Stenn
 * Andrew Sweeney
 * Ferenc Szalai
 * Brodie Thiesfield
 * Jason Toffaletti
 * Brian Utterback
 * Gisle Vanem
 * Bas Verhoeven
 * Constantine Verutin
 * Colin Watt
 * Zack Weinberg
 * Jardel Weyrich
 * Jay R. Wren
 * Zack Weinberg
 * Mobai Zhang
 * Alejo
 * Alex
 * Taral
 * propanbutan
 * masksqwe
 * mmadia
 * yangacer
 * Andrey Skriabin
 * basavesh.as
 * billsegall
 * Bill Vaughan
 * Christopher Wiley
 * David Paschich
 * Ed Schouten
 * Eduardo Panisset
 * Jan Heylen
 * jer-gentoo
 * Joakim Söderberg
 * kirillDanshin
 * lzmths
 * Marcus Sundberg
 * Mark Mentovai
 * Mattes D
 * Matyas Dolak
 * Neeraj Badlani
 * Nick Mathewson
 * Rainer Keller
 * Seungmo Koo
 * Thomas Bernard
 * Xiao Bao Clark
 * zeliard
 * Zonr Chang
 * Kurt Roeckx
 * Seven
 * Simone Basso
 * Vlad Shcherban
 * Tim Hentenaar
 * Breaker
 * johnsonlee
 * Philip Prindeville
 * Vis Virial
 * Andreas Gustafsson
 * Andrey Okoshkin
 * an-tao
 * baixiangcpp
 * Bernard Spil
 * Bogdan Harjoc
 * Carlo Marcelo Arenas Belón
 * David Benjamin
 * David Disseldorp
 * Dmitry Alimov
 * Dominic Chen
 * dpayne
 * ejurgensen
 * Fredrik Strupe
 * Gonçalo Ribeiro
 * James Synge
 * Jan Beich
 * Jesse Fang
 * Jiri Luznicky
 * José Luis Millán
 * Kiyoshi Aman
 * Leo Zhang
 * lightningkay
 * Luke Dashjr
 * Marcin Szewczyk
 * Maximilian Brunner
 * Maya Rashish
 * Murat Demirten
 * Nathan French
 * Nikolay Edigaryev
 * Philip Herron
 * Redfoxmoon
 * stenn
 * SuckShit
 * The Gitter Badger
 * tim-le
 * Vincent JARDIN
 * Xiang Zhang
 * Xiaozhou Liu
 * yongqing.jiao
 * Enji Cooper
 * linxiaohui
 * Seong-Joong Kim
 * Tobias Stoeckmann
 * Yury Korzhetsky
 * zhuizhuhaomeng
 * Pierce Lopez
 * yuangongji
 * Keith Smiley
 * jeremyerb
 * Fabrice Fontaine
 * wenyg
 * Aleksandr-Melnikov
 * ayuseleznev
 * chenguolong
 * Dimo Markov
 * dota17
 * fanquake
 * Jan Kasiak
 * Kamil Rytarowski
 * Mario Emmenlauer
 * Michael Davidsaver
 * mohuang
 * Nick Grifka
 * Nicolas J. Bouliane
 * Paul Osborne
 * Philip Homburg
 * Wataru Ashihara
 * William A Rowe Jr
 * yangyongsheng


If we have forgotten your name, please contact us.
