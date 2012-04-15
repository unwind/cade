Cade
====
Cade is a __c__ycle-__a__ccurate __D__CPU-16 __e__mulator, written in C by Emil Brink. The DCPU-16 is an imaginary 16-bit processor for an upcoming game called [0x10c](http://0x10c.com/).


Background
==========

It was developed just after Notch released the [first specs](http://0x10c.com/doc/dcpu-16.txt), and hasn't received too much attention. It should be feature-complete with regard to core CPU features, but nothing has been done in terms of I/O support.

It defines a minimalistic public API, so the hosting program *should* be able to implement I/O anyway, but it won't be as efficient as it would be if the emulator helped out more.


Status
======
Cade is mostly a(nother) fun project on the side for me, it's not very high on my list of life priorites. It's on github just to join the mini-bandwagon of DCPU-16-related projects; it's always fun to group. Please think twice (and fork!) before deciding on using Cade as the core CPU emulation technology for anything serious.


Notes
=====
The reason I decided to write Cade is that most of the early emulators that popped up (written in various languages) seemed to ignore the timing requirements and just emulate the functionality as quickly/easily as possible. I had never written a CPU emulator in that tried to be cycle-accurate, so it was an interesting [itch to scratch](http://e27.sg/2010/07/09/hacker-monthly/).