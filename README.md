(lib)substitute
---------------

Substitute is a system for modifying code at runtime by substituting custom
implementations for arbitrary functions and Objective-C methods.  It is also a
Free Software substitute for [Cydia Substrate](http://www.cydiasubstrate.com).
It currently has full support for iOS and limited support for OS X; in the
(hopefully near) future I will port it more widely.

License: LGPLv2.1+ with optional extra permissiveness; see LICENSE.txt

Current status
==============

Alpha.  Currently only build tested on Mac, targeting Mac and iOS.

To compile for iOS:

    ./configure --xcode-sdk=iphoneos --enable-ios-bootstrap && make -j8 && ./script/gen-deb.sh

You may want to turn off IB_VERBOSE in darwin-bootstrap/ib-log.h, which
currently spams a lot of files to /tmp and spams the syslog.  I will turn it
off by default soon.

To compile for Mac (does not support bootstrapping/bundle loading/etc., only
direct usage as a library):

    ./configure && make -j8

In other situations, `./configure --help` should be informative.  I'm using a
build system I wrote from scratch, intended to be extensible for many use cases
rather than project specific, and therefore somewhat complex; it's currently
rather rough around the edges.  Please let me know about any problems with it.

Known issues (will be fixed soon):
    - launchd will sometimes crash when injecting Substrate while Substitute is
      already loaded.  [not sure whether this is still an issue]
    - White-on-white status bar (I think) in SafetyDance.
    - Each dylib is >100kb due mostly to zero padding (and fat binaries).  This
      is easily fixed by adding FS compression, which I need to do in the deb.

How to use on iOS:

    Extensions should be placed in /Library/Substitute/DynamicLibraries, with
    the same layout as Substrate.  If you want to quickly test whether an
    existing Substrate extension works with Substitute, you can run

    install_name_tool -change \
      /Library/Frameworks/CydiaSubstrate.framework/CydiaSubstrate \
      /usr/lib/libsubstitute.0.dylib \
      extension.dylib

    and move it to the new directory.

Substitute compared to Substrate
================================
* `+` Free software, so you can actually use it somewhere other than iOS or
      Android, e.g. by bundling whatever parts of it you need with your app.
      See below for more on this.
* `+` More sophisticated, partially automatically generated disassemblers,
      which handle a larger portion of the space of possible PC-relative
      instructions that might be found in a patch target function - though I'm
      not sure how likely this is to help in practice.\*
* `+` Identifies if a function is too short to patch.
* `+` An extra disassembly step goes through the rest of the function to
      optimistically identify jumps back to the patched region, which are
      possible in rare cases; these can't currently be fixed up, but an
      appropriate error code is returned.
* `+` API returns error codes.
* `+` Some more functionality - interposing...
* `+` cross-platform support will be high priority soon(tm)
* `?` C, not C++
* `-` not yet stable
* `-` bigger binary size (because of the disassemblers)
* `-` Android will never be supported

\* Both libraries work by overwriting a few instructions at the beginning of
such a function; to allow the substitute function to call back into the
original, they first copy those instructions elsewhere, and append a jump to
the unmodified instructions after the patch site constituting the rest of the
function.  However, some instructions depend on the current PC and will break
if run from an unexpected location, so both libraries disassemble the moved
instructions and fix up the most common such sequences.  The benefit to this
library's approach is mostly on 32-bit ARM/Thumb; ARM64 only has a few
instructions that use PC, and Substrate already borrows a full x86 disassembler
library.

Todo list (approx. priority order)
==================================
- iOS: safe mode
- iOS: ensure re-patching launchd (for upgrades) works
- iOS: install without reboot
- On-the-fly hooking and unhooking support:
    - support for optimistically trying to unhook, in the hope that no further modifications were made to that memory
    - some API to load/unload from all existing processes
- Linux

