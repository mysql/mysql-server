## Quick orientation for an AI coding agent

This repository is the MySQL server (monolithic C++/C codebase) with multiple
components (server core, clients, tests, router, storage engines). Keep edits
small, build and test locally, and prefer non-invasive refactors.

### Big-picture architecture (what matters)
- `sql/` — core server logic; `mysqld.cc` is the main entry point. Look here
  for request handling, THD/thread model and global system variables.
- `client/` — command-line client tools (mysql, mysqldump, mysqladmin, ...).
- `mysql-test/` — the MTR test harness and test suites. Most CI relies on
  `mysql-test-run.pl` (aka mtr) and collections under `mysql-test/collections/`.
- `router/`, `libmysql/`, `plugin/`, `storage/`, `mysys/`, `unittest/` —
  important supporting components and independent tests.

### Build & local test workflow (concrete commands)
- Always do an out-of-source CMake build. Example (zsh/macOS):

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(sysctl -n hw.ncpu)
```

- For a debug build use `-DWITH_DEBUG=ON` or `-DCMAKE_BUILD_TYPE=Debug`.
- Minimum CMake version is declared in `CMakeLists.txt` (>= 3.17.5; macOS
  >= 11 may need >= 3.18/3.19). If builds fail, read the top of
  `CMakeLists.txt` for platform-specific notes.

### Running tests
- Most tests use the MTR harness. From the source tree you can run:

```bash
cd mysql-test
./mysql-test-run.pl --help
perl mysql-test-run.pl --vardir=var <testname>
```

- Test collections are in `mysql-test/collections/` and typical daily
  invocations use flags like `--force --timer --parallel=8 --vardir=var-...`.
- In an out-of-source build CMake provides a wrapper (`mysql-test/mtr.out-of-source`)
  — but invoking `mysql-test-run.pl` directly from `mysql-test/` is the simplest.
- Do NOT add `--debug-server` to MTR invocations (there's an explicit comment
  in `mysql-test/collections/default.daily`).

### Project-specific patterns & conventions
- Global server state is centralized in `sql/mysqld.h` and `sys_vars.cc` —
  changing global variables has wide impact and can break MTR tests.
- Tests are sensitive to logging and process output. Many files intentionally
  suppress or vary log output for MTR; avoid adding noisy prints unless
  gated by a debug flag.
- Error codes/messages use `ER_*` macros and message tables; see
  `include/mysqld_errmsg.h` and `share/messages_to_error_log.txt` before
  changing text.
- Lock-order and concurrency: the test harness relies on deterministic
  locking in many places (see `sql/sp_head.h` and `mysql-test` lock-order
  helpers). Use `MTR_LOCK_ORDER` or `--lock-order` when debugging.

### Integration points & configuration knobs
- External deps are wired via CMake flags: `WITH_SSL`, `WITH_KERBEROS`,
  `WITH_SASL`, `WITH_LDAP`, `WITH_CURL`, etc. Check top-level `CMakeLists.txt`
  for search logic and platform fallbacks.
- `mysqld_safe` / `mysqld` invocation and packaging scripts live under `man/`
  and `support-files/` — useful when writing startup or packaging changes.

### Where to look for examples
- Adding a server option: search `sql/options_mysqld.h` and `bootstrap.cc`.
- Tests and collections: `mysql-test/collections/*` and individual suites in
  `mysql-test/suite/` (examples of how MTR expects output and vardirs).
- Router tests use CTest; see `router/src/...` and use `ctest -R routertest_`
  from the build directory for small router-specific test runs.

### Safety rules for automated edits
1. Prefer minimal, local changes and build to validate compilation.
2. Run affected MTR tests where feasible; otherwise run a relevant subset
   (`--suite=` or a single test) to avoid expensive full suites.
3. Avoid changing global strings, public ABI, or exported error codes
   without matching tests and documentation updates.

If anything here is unclear or you'd like more detail (example: a quick
walk-through of adding a mysqld option and its tests), tell me which area
to expand and I'll iterate.
