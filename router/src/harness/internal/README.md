MySQL Harness - Internal notes {#mysql_harness_internal_notes}
==============================

These are internal notes regarding the MySQL Harness. They are mostly
related to procedural issues for managing the repository and code that
are not intended (nor interesting) for the public.

These notes does not (and should not) contain any internal or
confidential information.

For the avoidance of doubt, this particular copy of the software
(including the files in this directory) is released under the version
2 of the GNU General Public License.

Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.


Internal Files
--------------

Files that are not going to be part of a release should normally be
placed in the `internal/` directory. If necessary, they can be placed
elsewhere but Release Engineering need to know about these files.

The file `exclude-from-release.txt` contain files and directories that
should be excluded from the release and not uploaded to, for example,
GitHub. Please make sure to add internal files here.


Creating feature and issue branches
-----------------------------------

Feature branches are almost always based on the `master` branch. To
create a feature branch, use the command:

    git checkout -b feature/my-precious master

If the current branch is already `master`, you can omit it from the
line above.

To activate pre-commit hooks it is necessary to copy them from the
`internal` directory to the `.git` directory:

    cp internal/hooks/pre-commit .git/hooks

There is more information about what the Git hooks below.


Preparations for Code Review
----------------------------

Patches are usually uploaded to ReviewBoard and reviewed there. Before
uploading the patch, ensure that:

- It build in all build configurations on your machine.

- There are no code style issues

To check that it builds in all build configurations on your platform
there is a script available in the directory `internal/checks`.

    python internal/checks/build_all.py

If you have installed the pre-commit hook then committing will run the
pre-commit hooks and catch mistakes that can be caught by the
automated checks.

Once the patch is committed, you can generate a diff between the
`master` branch and the committed work using:

    git diff master..HEAD >my-precious-1.diff


Preparations for Merging
------------------------

When it is time to merge the branch with either the master branch or
one of the version branches, please
[rebase and squash](https://help.github.com/articles/about-git-rebase/)
the patches in the branch before pushing to the repository to combine
several patches for the same issue. Each feature and each issue should
be one single patch.

You can rebase and squash the branch using:

    git rebase -i master

or

    git rebase -i master feature/my-precious

if your current branch is already `feature/my-precious`.


Coding Style
------------

For the code we follow the
[Google C++ Style Guide](http://google.github.io/styleguide/cppguide.html),
with some exceptions. To check that the code follow the style guide,
you can install `cpplint` in your path and CMake will automatically
construct a `check` target that will run `cpplint` on the source
files. When looking for `cpplint` it will both look for `cpplint` and
`cpplint.py`.

### Git Hooks ###

There are a few useful hooks in this directory, but you have to set up
using these explicitly using the following line:

    git config core.hooksPath internal/hooks

Or if your version (before 2.9) of Git does not support
`core.hooksPath`, you can either link or copy the hook files:

    ln -s internal/hooks/* .git/hooks
    cp internal/hooks/* .git/hooks

There is a pre-commit hook that will run the equivalent of `make
check` before allowing the commit to proceed. This will catch any
coding-style related issues before a patch is created. If you do not
want to run this check when committing (for example, because this is
just an intermediate commit), you can disable the checking using the
`--no-verify` option when committing.

Since the pre-commit hook can be circumvented, there is also a
`update` hook that is used by the repository. It will not accept any
patches into the master branch or the version branches that do not
pass the equivalent of `make check`.
