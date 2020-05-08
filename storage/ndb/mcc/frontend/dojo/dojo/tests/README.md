# Dojo Tests

This directory has the following structure:
```
tests/
    dojo.intern.js - SauceLabs configuration
    dojo.intern.local.js - Local Selenium configuration
    dojo.intern.proxy.js - Proxy configuration without instrumentation
    functional/ - Functional tests
        all.js - Module referencing all functional tests to run
    unit/ - Unit tests
        all.js - Module referencing all unit tests to run
    support/ - Supporting files such as pre-run scripts for SauceLabs
    services/ - Service proxy server plus service modules
```

## Running the tests locally

To get started, simply run the following commands in the dojo directory:
```
npm install
```

This will install Intern and some supporting libraries in `node_modules`.

Once complete, intern tests may be tested by several `npm run` scripts issued
from the root of the repository. To run the unit test suite in Node, run the
following command:
```
npm run test
```

To run against the browsers, install "selenium-standalone-server"
and the drivers for the browsers to test, launch Selenium on port 4444, and issue
the following command:
```
npm run test-local
```

During development of tests, it is often desirable to run the test suite
or portions of the test suite in a local browser. To do this, simply run
the test runner in proxy-only mode:
```
npm run test-proxy
```

With the proxy running, navigate to the following URL:
```
http://localhost:9001/__intern/client.html?config=tests/dojo.intern
```

This will run the entire unit test suite and output the results in the
console. To only run certain modules, use the "suites" query parameter.
The following URL will only run the dojo/request/script tests:
```
http://localhost:9001/__intern/client.html?config=tests/dojo.intern&suites=tests/request/script
```

Intern can also produce code coverage reports in HTML format. Simply append
`-coverage` to any of the test run commands:
```
npm run test-coverage # Coverage for unit tests run in node
npm run test-remote-coverage # Coverage for unit + functional via SauceLabs
npm run test-local-coverage # Coverage for unit + functional via local Selenium
```

This will output HTML files to the `html-report` directory which can be
viewed to see code coverage information for individual files (including a
view of the source code with which lines were not covered).

More information about running intern tests can be found at
https://github.com/theintern/intern/wiki/Running-Intern.

## Running against a VM

If you are developing on a mac, you can use selenium grid mode to launch tests in a PC VM,
by following these steps, replacing MAC_IP_ADDRESS with the IP address of your mac.

### Setup mac

a) Modify dojo.intern.local.js to add "internet explorer" as browser to test:

	{ browserName: 'internet explorer',
		'prerun': 'http://MAC_IP_ADDRESS:9001/tests/support/prerun.bat' },


b) Modify dojo.intern.local.js to have:

	intern.proxyUrl = "http://MAC_IP_ADDRESS:9000";

c) Launch hub server

	$ java -jar selenium-server-standalone-2.45.0.jar -role hub


### Setup PC VM

a) Make sure PC can connect to the mac's IP address.

My VM is running in VirtualBox.  I had to change the properties of the VM, specifically the
network settings tab, so the VM used "bridged adapter".

b) Download selenium from http://www.seleniumhq.org/download/.

c) Downgrade FF to the ESR release (https://www.mozilla.org/en-US/firefox/organizations/all/),
   version 31, because the latest stable release doesn't work with Intern.

d) Download chromedriver from https://sites.google.com/a/chromium.org/chromedriver/downloads,
   and internet explorer driver from https://code.google.com/p/selenium/wiki/InternetExplorerDriver.
   Put both somewhere in your path, for example `C:\Windows\System32`.

e) Launch selenium in the node role:

	$ java -jar selenium-server-standalone-2.45.0.jar -role node -hub http://MAC_IP_ADDRESS:4444/grid/register

(Filename of JAR may vary according to its version.)

On Windows 7+, optionally you can launch the server automatically on startup
by creating a task in the "Task Scheduler" application.


### Run regression

Run this command from your dojo root directory:

	$ npm run test-local

You will get security dialogs on the PC that you have to click "OK" to.


## Running tests against SauceLabs

Although the dojo tests can be run against SauceLabs, the intermittent failures and timeouts
essentially make it impossible to get an accurate result.  Nonetheless, here are the instructions to do it.

To run unit and functional tests via SauceLabs run the following command:
```
npm run test-remote
```

This command will attempt to open a tunnel to SauceLabs and run the test
suite in all of the browsers defined in `tests/dojo.intern.js`. SauceLabs
requires an account (SL offers free accounts). The JS Foundation has an
account, but please use your own when running your own tests.

## Writing tests

To add a test suite to the suites to automatically run when the runner
executes, add the module ID of the suite to either tests/unit/all.js (for unit tests)
or tests/functional/all.js (for functional tests).

For information on how to write Intern tests, see
https://github.com/theintern/intern/wiki/Writing-Tests-with-Intern. Please
follow the style of the tests currently written.

If tests are required to communicate with a server (for instance,
dojo/request), services can be written for the test runner. Simply
create an AMD module and be sure the filename ends in "service.js". The
module must return a function that accepts a
[JSGI](https://github.com/kriszyp/jsgi-node/) request object and
returns either an object describing the response or a promise that will
resolve to an object describing the response.

Once a service is written, it can be accessed from the proxy via the
following URL pattern:
```
http://localhost:9001/__services/path/to/service
```

Taking "tests/request/xhr.service.js" as an example, its URL
endpoint would be:
```
http://localhost:9001/__services/request/xhr
```

While this is very useful, *most tests should be mocking their data instead of
using live data from a service*. Services should only be written for tests that
absolutely *MUST* communicate with a server, such as tests for IO functionality.
