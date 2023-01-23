notstd v0.0.2
====================

State:
======
* v0.0.4 add fuzzy search
* v0.0.3 add robin hood hash
* v0.0.2 add list
* v0.0.1 add vector
* v0.0.0 add macro, math && memory

Bug:
====
try to writing many

Require:
========
nothing for now

To test it:
==============
ut can use with: 'memory', 'delay', 'datastructure'<br><br>

$ meson setup build<br>
$ cd build<br>
now select test<br>
$ meson configure -Dut='memory'<br>
$ ninja<br>
$ ./notstd<br>


To install it:
==============
not ready for productions<br>
$ meson setup build<br>
$ cd build<br>
$ ninja<br>
$ sudo ninja install<br>

To uninstall it:
==============
do not do it
