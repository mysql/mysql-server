define([
	'require',
	'dojo/Deferred',
	'intern!object',
	'intern/chai!assert',
	'intern/dojo/node!leadfoot/helpers/pollUntil',
	'./window/iframe_content/scrollDocuments'
], function (
	require,
	Deferred,
	registerSuite,
	assert,
	pollUntil,
	testScenarioDocuments
) {
	// There are many permutations of document layouts that need to be tested.
	// This function will generate a test for every layout scenario
	function generateTestsForScenarios(suite) {
		testScenarioDocuments.forEach(function (doc) {
			suite[doc.label] = function () {
				var visibleBefore;
				var visibleAfter;
				var hasScrolled;

				return this.get('remote').findById(doc.id)
					.findByClassName('before')
					.getProperty('value').then(function (visible) {
						visibleBefore = parseInt(visible, 10);
					})
					.end()
					.findByClassName('scrollBtn')
					.click()
					.end()
					.findByClassName('hasScrolled')
					.getProperty('value').then(function (scrolled) {
						hasScrolled = parseInt(scrolled, 10);
					})
					.end()
					.findByClassName('after')
					.getProperty('value').then(function (visible) {
						visibleAfter = parseInt(visible, 10);
					})
					.then(function () {
						if (hasScrolled) {
							assert.notOk(visibleBefore, 'scrolled, target should not be visible before');
							assert.ok(visibleAfter, 'scrolled, target should be visible after');
						}
						else {
							assert.ok(visibleBefore, 'not scrolled, target should be visible before');
							assert.ok(visibleAfter, 'not scrolled, target should be visible after');
						}
					});
			};
		});
	}

	function getPage(suite, url) {
		return suite.get('remote')
			.get(require.toUrl(url))
			.setExecuteAsyncTimeout(5000)
			.setFindTimeout(1000)
			.then(pollUntil('return ready;'));
	}

	registerSuite(function () {
		var suite = {
			name: 'dojo/window'
		};

		suite['.scrollIntoView()'] = {
			setup: function () {
				return getPage(this, './window/scroll.html');
			}
		};

		generateTestsForScenarios(suite['.scrollIntoView()']);

		suite['.getBox'] = {
			setup: function () {
				return getPage(this, './window/viewport.html');
			},

			initial: function () {
				var viewportHeight;
				var documentHeight;

				return this.get('remote').execute('compute()')
					.findById('viewportHeight')
					.getProperty('value').then(function (height) {
						viewportHeight = parseInt(height, 10);
					})
					.end()
					.findById('documentHeight')
					.getProperty('value').then(function (height) {
						documentHeight = parseInt(height, 10);
					}).then(function () {
						assert.isTrue(viewportHeight > documentHeight, 'expected viewport to be bigger than document');
					});
			},

			expand: function () {
				var viewportHeightBefore;
				var viewportHeightAfter;

				return this.get('remote')
					.execute('compute();')
					.findById('viewportHeight')
					.getProperty('value').then(function (height) {
						viewportHeightBefore = parseInt(height, 10);
					})
					.end()
					.execute('addText(); compute();')
					.findById('viewportHeight')
					.getProperty('value').then(function (height) {
						viewportHeightAfter = parseInt(height, 10);
					}).then(function () {
						assert.isTrue(viewportHeightAfter <= viewportHeightBefore, 'viewport increased in size before: ' + viewportHeightBefore + ' after: ' + viewportHeightAfter);
						assert.isTrue(viewportHeightAfter + 20  >= viewportHeightBefore, 'viewport didn\'t shrink, except for space taken by scrollbars');
					});
			}
		};

		return suite;
	});
});
