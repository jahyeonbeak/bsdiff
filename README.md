bsdiff/bspatch
==============
bsdiff and bspatch are libraries for building and applying patches to binary
files.

The original algorithm and implementation was developed by Colin Percival.  The
algorithm is detailed in his paper, [Naïve Differences of Executable Code](http://www.daemonology.net/papers/bsdiff.pdf).  For more information, visit his
website at <http://www.daemonology.net/bsdiff/>.

I maintain this project separately from Colin's work, with the goal of making
the core functionality easily embeddable in existing projects.

Contact
-------
https://github.com/jahyeonbeak/bsdiff

License
-------
Copyright 2003-2005 Colin Percival  
Copyright 2021 Jahyeon Beak

This project is governed by the BSD 2-clause license. For details see the file
titled LICENSE in the project root folder.

Overview
--------
There are two separate libraries in the project, bsdiff and bspatch. Each are
self contained in bsdiff.c and bspatch.c The easiest way to integrate is to
simply copy the c file to your source folder and build it.

The overarching goal was to modify the original bsdiff/bspatch code from Colin
and provide a simple interface to the core functionality when I use.

Reference
---------
### bsdiff

	int bsdiff(u_char *old, off_t oldsize, u_char *new, off_t newsize, const char *patchname);

`bsdiff` returns `0` on success and `-1` on failure.

### bspatch

	int main_bspatch_mem(u_char *patchBuf, unsigned int patchLen, ssize_t patchOffset, u_char *oldBuf, unsigned int oldLen, u_char *newBuf, unsigned int newLen);

The `bspatch` function transforms the data for a file using data generated from
`bsdiff`. The caller takes care of loading the old file and allocating space for
new file data.  The `stream` parameter controls the process for reading binary
patch data.

`bspatch` returns size of `newBuf` on success and `-1` on failure. On success, `newBuf` contains
the data for the patched file.