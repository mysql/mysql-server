doh/plugins - plugins to configure and extend the DOH runner

These are loaded after the test runner and test url, but before the runner begins. This provides an opportunity to wrap and monkey-patch doh, the test harness and the test runner.

Usage - e.g.: 
	util/doh/runner.html?testModule=tests.cache&dohPlugins=doh/plugins/hello
    util/doh/runner.html?testModule=tests.cache&dohPlugins=doh/plugins/hello;doh/plugins/alwaysAudio

Android robot testing
doh/plugins/remoteRobot bridges the doh.robot API with the WebDriver API.
On your PC, load android-webdriver-robot.html and follow the instructions to set up your Android phone for robot testing.
The page links to a patched version of WebDriver that enables cross-domain scripting from the browser; this enables the browser to essentially drive itself.
Sadly, the corresponding iPhone WebDriver is not as powerful as the Android version and does not use native events; do not expect it to work.

Known issues:
The mobile tests were broken during the AMD refactor, so don't expect to load the dojox.mobile tests and see them run just yet.