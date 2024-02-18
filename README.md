This is a minimally modernized version of the venerable xcpustate
utility.

The initial commit is of the original source for 2.9.

The modifications are:

* Defaults to modern illumos, in 64-bit
* Defaults to RSTAT disabled
* Minimal code changes to placate modern gcc
* A Makefile is provide, Imakefile removed
* The new Makefile has an install target
* A solaris/illumos specific Manual page is provided as xcpustate.1
