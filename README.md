(lib)substitute
---------------

NOT EVEN ALPHA YET:

    I wrote the below disclaimer assuming I would at least fix the boot process
    before publishing this.  I need to sleep, so that will happen tomorrow, but
    for now it's simply broken.  Please don't install the package yet, but feel
    free to look at the code.


ALPHA VERSION:

    This code has not yet been tested anywhere near adequately and is probably
    completely broken.  Especially sharp edges are the iOS bootstrap stuff and
    the disassemblers.

    Automatically entering a safe mode after SpringBoard crashes is not
    implemented yet (though you can see the UI in progress in
    ios-bootstrap/SafetyDance).  Manually disabling the system by holding
    volume up while booting (the same shortcut as Substrate) should work,
    though.

Substitute is a system for modifying code at runtime by substituting custom
implementations for arbitrary functions and Objective-C methods.  It is also a
Free Software substitute for [Cydia Substrate](http://www.cydiasubstrate.com).
It currently has full support for iOS and limited support for OS X; in the
(hopefully near) future I will port it more widely.

License: LGPLv2.1+ with optional extra permissiveness; see LICENSE.txt

Substitute compared to Substrate
================================
* `+` Free software, so you can actually use it somewhere other than iOS or
      Android, e.g. by bundling whatever parts of it you need with your app.
      (Well, you could if it didn't lack x86 support, which will be fixed
      quite soon.)  See below for more on this.
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
* `+` cross-platform
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
- x86
- On-the-fly hooking and unhooking support:
    - support for optimistically trying to unhook, in the hope that no further modifications were made to that memory
    - some API to load/unload from all existing processes
- Linux

Why rewrite Substrate?
======================

iOS jailbreak community drama incoming.  I hope that this library eventually
becomes useful to a wide variety of people that have no knowledge or interest
in the following, because that is itself one of the answers to the above
question.  But if you are interested, read on.

As a bit of background: Substrate used to be open source.  (This is where the
LGPL version of the compatibility header comes from; the current one in the
Debian package has the GPL on it, not that it really matters.)  In fact, it was
open source for two(?) contiguous periods, but it has been closed for years
now, and looks to remain that way.

Do I really have to have a reason to create free software alternatives to
proprietary software?  I was recently watching Richard Stallman's talk at 31c3,
and reflecting on the fact that while I'd been worrying about this, he'd
consider it immoral *not* to work on such a project.  - Yes, I do?  Well,
then...

In the short view, because iMods asked me to.  The reason they can't use
Substrate (allegedly; I haven't myself discussed this specific issue with
saurik, and don't really care, due to the other use cases for this library; I
hope I am not fundamentally misrepresenting anyone's view here) is that saurik
does not want to support them because he does not think competition is healthy
for the ecosystem.  This is described in detail in his article [Competition vs.
Community](http://www.saurik.com/id/20), and it appears to be an essentially
irreconcilable difference between the two.

After reading that, you may ask, why am I trying to subvert saurik?  After he
correctly says -

> I could sit around and first-party the entire stack: this would be much
> easier than you'd even think; after all, Dustin already did the "hard work"
> of figuring out what was even possible and what kind of solution solved the
> problem.

why am I proceeding to write a new library, which even goes to the extent of
having an API compatibility later, once saurik did the hard work of figuring
out what kind of solution did a good job solving the runtime code modification
problem on iOS?  When saurik worries that competitors to Cydia will sap the
funding he relies on to support his essential, but less glamorous, software
plumbing, and his many jailbreak community initiatives, why am I enabling
exactly that?

Well, I'd be lying if I said I was sure it was a good idea.  But to be honest,
with all respect to iMods, I don't think it's likely to supplant Cydia, not in
the foreseeable future or, in its current form, ever.  And the kind of group
that could hypothetically supplant Cydia would likely not have difficulty
replicating Substrate by themselves.  So I consider my own involvement to be a
low-stakes game in terms of practical consequences, which makes the upsides I
perceive more compelling.

What are those upsides?

It's worth noting that between the aforementioned two open source periods for
Substrate, a younger version of me repeatedly expressed a desire for it to be
open source, and when the source was released again, was quite pleased.
(Being slightly less enthusiastic in practice than principle - although much of
that was just timing - I don't think I noticed when it went back to closed
source.)  So this really goes back a lot longer than iMods, and the upsides I
perceive now are pretty much the same ones I did then.

Starting on the practical end and ending on the idealistic one:

First of all, it was during that period that I was working on tools to cleanly
hook into the iOS *kernel* for reverse engineering purposes.  As part of that,
I wanted a function hooker.  If Substrate had been open source, I could have
easily copied out the (pretty small) bit I wanted into my kernel code; instead
I had to write my own, inferior version.  This is notable because, whether or
not it was actually useful, the work I was doing was unambiguously pro-social
and not harmful to the jailbreak community.  saurik has on [various
occasions](http://forum.xda-developers.com/showthread.php?t=2466101&page=2)
mentioned the possibility (history?) of forks of his software hoarding fixes or
whatever, and stated that there isn't much legitimate reason to modify
Substrate, as opposed to building on it; certainly this is mostly true, but
'not much' is not the same as 'none'.  If my software is useful to anyone else
doing good work along the same lines, I will be very happy.

I vaguely remember saurik suggesting that an alternative would be cooperating
to come up with an official way for Substrate to be integrated into the kernel,
or something.  But I thought and think this really wasn't a good idea - since
I'm the only one I know of who actually used those tools (except for an
implementation of unionfs that formed part of JailbreakMe 3.0), it would be
quite pointless for saurik to maintain, and I wanted to preserve agility for
that little project.

Second, while iOS-related environments other than userland are a pretty niche
use case, there are innumerable environments unrelated to iOS in which
Substrate-like hooking would be useful; it is for this reason that I intend to
port Substitute to OS X and Linux, and anyone who just wants function hooking
in some embedded environment can copy that bit easily enough.  (To be fair,
Substrate already is ported to both of the former platforms, but that doesn't
really help if it's not freely available.)  Sure, it would be nice if there
were an official Cydia Store for OS X and Windows and toasters, but saurik
doesn't seem to have time to work on that (maybe there were other reasons to
stop working on OS X? don't remember), and I don't think it's reasonable to
expect everyone who wants this fairly low-level functionality to work with
saurik on what is otherwise their own project.

This isn't just theoretical.  Several years ago, I wrote more primitive version
of a subset of the functionality found in this library as
'inject_and_interpose':

https://github.com/comex/inject_and_interpose

Since it's been around for years, it's had time to gather attention.  Despite
me doing absolutely no promotion of it, and the repo not even having a readme
or license (which is not a good thing), three different people have posted
issues implying they have tried to use it.  More importantly, after I bought
the Flavours app for OS X, I was surprised to receive an email from the
developer offering me a refund, on the basis that I had already supported its
development by writing inject_and_interpose!  (In case you're reading this:
Sorry I didn't respond; I tend to be very shy and so often don't reply to
emails I should.)  I think it's incredible that I was able to help a project
totally unknown to me get developed; this is the kind of outcome only free
software can achieve.  I doubt it would have worked out as well if I had
demanded coordination first.

Third... this one is more subjective, but it's also probably the most
important.  The way I see it, jailbreaking is *fundamentally* about taking
something closed and fixed and opening it up to hacking and modification:
perhaps allowing a mess to be made, but quite possibly ending up with something
unique and different.  This ideal of openness is very similar to that of free
software, and I therefore believe that it's in the spirit of jailbreaking to
make as much low-level stuff open as possible, both for inspection and
modification by curious users (who, after gaining knowledge that way, might end
up becoming quite valuable to the community).  Polished tweaks that are sold
commercially are one thing (although they too benefit from general openness,
especially the ones with a lot of reverse engineering behind them, since the
same reverse engineering can often support multiple use cases), but the
underlying framework is another - especially since it's free of charge,
removing at least the most obvious motivation for closing source.

(Like most people, I don't entirely agree with Stallman's rhetoric - otherwise
I wouldn't have just praised a commercial application! - but all in all my
views are aligned in a very similar direction, just with less magnitude.)

Incidentally, this affects the jailbreaks themselves even more than Substrate.
I have often advocated for open source jailbreaks, and all of my own jailbreak
code has been open source.  There would be practical benefits there, too; past
experiences aside, a certain project I've had on the backburner for years will,
if I ever get to it, certainly require customizing an existing jailbreak, which
currently is likely to mean reimplementing it.  Not that that would be
particularly hard compared to the *rest* of the project, but it's just an
unnecessary speed bump, and unnecessary things frustrate the hell out of me.
Especially when the jailbreaks are distributed for free!

Oh, and when I say "the spirit of jailbreaking", I don't mean to change history
by implying "original plan" - iOS jailbreak has always suffered from
balkanization of closed source tools.  Indeed, I'm happy that the open source
Cydia supplanted the closed Installer, and with it saurik's generally open,
community-based management style.

But when I say spirit, I do mean the best part of it.

comex, 30 January 2015
