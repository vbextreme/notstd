test your code
====================
code for generate executable for testing.<br>
in valgrind && canbuild dir have script for testing particular part of !std.<br>

Options:
========
for generate testing versione need to add one or two flags, ut and utvalue.<br>
ut can have this value:<br>
* memory, test memory part, no utvalue used
* delay, test time function, no utvalue used
* datastructure, utvalue assume (1 vector, 2 list, 4 doublylist, 8 chi² hash, 0x10 rbhash, 0x20 fuzzy search, 0x40 input fuzzy)

Build example:
==============
$ meson setup build -Dut='datastructure' -Dutvalue='0x40' <br>
$ cd build<br>
$ ninja<br>
$ ./notstd<br>

