Mirror of the subversion repository of ngsolve
====================================================

This is an unofficial mirror of the ngsolve subversion repository and
is updated every 24 hours. The original repository can be found at

<pre>
svn://svn.code.sf.net/p/ngsolve/code
</pre>

The trunk of the svn repository is mirrored to the master branch. All
old branches 4.9,5.0,... are mirrored to the respective git branches
v4.9,v5.0,... .

Since the git repository is created from the original subversion
repository using git filter-branch, fast-forwarding of the individual
branches might not work all the time. In comparison to the svn repo
one extremely large mesh file (cube.vol, ~200MB!) and several
mistakenly committed auto-generated autotools scripts have been
removed in order to reduce the size of the repository. The files can
be reproduced from the source files which are still included in the
repository. Note, that for a final conversion one should remove all
mesh files from the repository and instead generate them directly from
the geometry input files in the build process.

The shell script convert.sh and the authors file for converting the
subversion repository is located together with this README file in the
mirrorinfo branch.

Since this repository is only a mirror, pull requests will _not_ be
looked at.  For submitting bug reports/patches, see the [original
website](http://sourceforge.net/projects/ngsolve) or contact Joachim
Schöberl directly.

