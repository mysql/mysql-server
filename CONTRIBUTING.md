# Contributing to MySQL Server

We welcome your code contributions. This guide gets you from a fresh clone to a
merged pull request with as little friction as possible.

> **TL;DR** — Sign the [OCA](https://oca.opensource.oracle.com), run
> `scripts/ci/bootstrap.sh`, make your change, run `scripts/ci/mtr.sh`,
> and open a PR. CI builds your branch and runs MTR automatically. A
> maintainer is assigned within the triage SLA below.

---

## 1. Sign the Oracle Contributor Agreement (once)

Before any contribution can be merged you must have signed the
[Oracle Contributor Agreement (OCA)](https://oca.opensource.oracle.com).

1. Create or reuse a user account at <https://bugs.mysql.com>.
2. Sign the OCA, referencing that account.
3. Use the **same email** on your Git commits (`git config user.email`).

Oracle verifies OCA status separately from this repository's GitHub Actions
workflows. Follow any OCA guidance reported on the pull request before the
change is merged.

## 2. Get a build in one command

```bash
# Reproducible toolchain + Boost, identical to CI:
scripts/ci/bootstrap.sh        # installs/pins deps
scripts/ci/build.sh debug      # configures + builds into build/
```

These scripts pin the same compiler, CMake, Ninja, Boost, and test tooling used
by CI, so "works locally" tracks "passes in CI."

## 3. Find something to work on

- Issues labeled [`good first issue`](../../labels/good%20first%20issue) are
  scoped, have reproduction steps, and a named area owner.
- [`help wanted`](../../labels/help%20wanted) marks larger items the team would
  welcome help on.

## 4. Make the change

- Match existing style; formatting is enforced by `.clang-format`. Run
  `scripts/ci/format.sh` (or install the pre-commit hook below) so you never get
  a review comment about whitespace.
- Add or update tests. Every behavior change ships with MTR coverage under
  `mysql-test/`.
- Keep commits focused and write a clear message body explaining *why*.

Optional but recommended — install the format pre-commit hook:

```bash
ln -s ../../scripts/ci/format.sh .git/hooks/pre-commit
```

## 5. Run tests locally (the same ones CI runs)

```bash
scripts/ci/mtr.sh            # default MTR test selection, mirrors the PR check
scripts/ci/mtr.sh --suite=innodb     # pass MTR args straight through
```

## 6. Open the pull request

Push your branch and open a PR against `trunk`. The
[pull request template](.github/PULL_REQUEST_TEMPLATE.md) prompts for the few
things reviewers always need. On open, CI automatically:

- checks formatting,
- builds Debug on gcc and clang,
- runs MTR,
- auto-labels the affected area and assigns a reviewer.

CI reports completion or actionable failures after the build and MTR run finish;
use the pull request's Checks tab to follow live progress.

## What to expect from us (triage SLAs)

These are the response targets the maintainers hold themselves to. They are
published here so the contract is mutual and visible:

| Stage                                   | Target            |
|-----------------------------------------|-------------------|
| First maintainer response on a new PR   | 3 business days   |
| First response on a `good first issue`  | 2 business days   |
| Review round-trip after you push        | 5 business days   |

If a PR goes quiet past these windows, ping `@mysql/triage` on the thread.

## Alternative submission path

You may still attach a patch to a bug record at <https://bugs.mysql.com> via the
*contribution* tab. GitHub pull requests are now the recommended path because
they get automated CI feedback and public review history.


Submitting None-Code Contributions
----------------------------------

Submissions Other than Code. These terms apply to all of Your Submissions
other than code contributions. "You" means you personally, as well as any
person or entity on whose behalf you are Using the Site. "You" does not
include Oracle or its employees using the Site on Oracle's behalf. "Use" and
its variants are to be interpreted in their broadest sense and include,
without limitation, the acts of using, accessing,
receiving, browsing, downloading from, and uploading to. A "User" is a person or
entity who Uses the site.

"Submissions" means any materials (other than code contributions), including
but not limited to technology specifications, technical materials,
documentation, discussion thread postings, blogs, wikis, data, and any other
content, information, technology
or services submitted to by You to the site.

You hereby grant to Oracle and all Users a royalty-free, perpetual, irrevocable,
worldwide, non-exclusive and fully sub-licensable right and license under Your
intellectual property rights to reproduce, modify, adapt, publish, translate,
create derivative works from, distribute, perform, display and use Your
Submissions (in whole or part) and to incorporate or implement them in other
works in any form, media, or technology now known or later developed. This
includes, without limitation, the right to incorporate or implement the
Submission into any product or service, and to display, market, sublicense and
distribute the Submissions as incorporated or embedded in any
product or service distributed or offered by Oracle without compensation to you.
All Users, Oracle, and their sublicensees are responsible for any
modifications they make to the Submissions of others.
