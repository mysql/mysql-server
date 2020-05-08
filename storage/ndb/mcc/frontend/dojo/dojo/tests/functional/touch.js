define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'../support/ready'
], function (require, registerSuite, assert, ready) {
	/* globals tracker */

	function loadPage(driver) {
		return driver.get('remote')
			.setExecuteAsyncTimeout(5000)
			.get(require.toUrl('./support/touch.html'));
	}

	function clickElement(element) {
		return element.click();
	}

	function tapElement(element) {
		return element.tap();
	}

	function moveMouseToElement(session, id) {
		return function () {
			return session.findById(id).then(function (element) {
				return session.moveMouseTo(element);
			}).end();
		};
	}

	registerSuite(function () {
		var tapOrClick;

		return {
			name: 'dojo/touch',

			'before': function () {
				// Not all browsers or drivers support touch events
				tapOrClick = this.get('remote').environmentType.touchEnabled ?
					tapElement :
					clickElement;
			},

			'beforeEach': function () {
				return loadPage(this);
			},

			'click/tap': function () {
				var remote = this.get('remote');
				var elementPromise = remote
					.executeAsync(function (done) {
						require(['dojo/on', 'dojo/touch'], function (on, touch) {
							window.log = [];
							var node = document.getElementById('upper-right');
							on(node, touch.press, function () {
								log.push('touch.press');
							});
							on(node, touch.release, function () {
								log.push('touch.release');
							});
							on(node, 'mousedown', function () {
								log.push('mousedown');
							});
							on(node, 'mouseup', function () {
								log.push('mouseup');
							});
							on(node, 'click', function () {
								log.push('click');
							});
							done();
						});
					})
					.findById('upper-right');

				return tapOrClick(elementPromise)
					.execute(function () {
						return window.log.join(', ');
					})
					.then(function (result) {
						// Test that proper touch.* events fired, and also that mousedown and mouseup weren't suppressed.
						// Tricky though because the order mousedown/mouseup events occurs varies by browser.
						// See https://github.com/dojo/dojo/pull/138.
						assert(/touch\.press.*touch\.release.*click/.test(result),
							"touch.press --> touch.release --> click");
						assert(/mousedown.*mouseup.*click/.test(result),
							"mousedown --> mouseup --> click");
					});
			},

			'move': function () {
				var session = this.get('remote');

				return session
					.then(moveMouseToElement(session, 'upper-left'))
					.executeAsync(function (done) {
						require(['sinon', 'dojo/on', 'dojo/touch'], function (sinon, on, touch) {
							window.tracker = sinon.stub();
							var node = document.getElementById('upper-right');
							on(node, touch.move, tracker);
							done();
						});
					})
					.execute(function () {
						return tracker.callCount;
					})
					.then(function (result) {
						assert.equal(result, 0);
					})
					.then(moveMouseToElement(session, 'upper-right'))
					.execute(function () {
						return tracker.called;
					})
					.then(function (result) {
						assert.ok(result);
					});
			},

			'cancel': function () {
				var session = this.get('remote');

				return this.get('remote')
					.then(moveMouseToElement(session, 'upper-left'))
					.executeAsync(function (done) {
						require(['sinon', 'dojo/on', 'dojo/touch'], function (sinon, on, touch) {
							var node = document.getElementById('upper-left');
							window.tracker = sinon.stub();
							on(node, touch.cancel, tracker);
							done();
						});
					})
					.then(moveMouseToElement(session, 'upper-right'))
					.then(moveMouseToElement(session, 'upper-left'))
					.execute(function () {
						return tracker.called;
					})
					.then(function (result) {
						assert.isTrue(result);
					});
			},

			'over': function () {
				var session = this.get('remote');

				return session
					.then(moveMouseToElement(session, 'upper-left'))
					.executeAsync(function (done) {
						require(['sinon', 'dojo/on', 'dojo/touch'], function (sinon, on, touch) {
							var node = document.getElementById('upper-right');
							window.tracker = sinon.stub();
							on(node, touch.over, tracker);
							done();
						});
					})
					.then(moveMouseToElement(session, 'upper-right'))
					.execute(function () {
						return tracker.called;
					})
					.then(function (result) {
						assert.isTrue(result);
					});
			},

			'out': function () {
				var session = this.get('remote');

				return session
					.then(moveMouseToElement(session, 'upper-left'))
					.executeAsync(function (done) {
						require(['sinon', 'dojo/on', 'dojo/touch'], function (sinon, on, touch) {
							var node = document.getElementById('upper-left');
							window.tracker = sinon.stub();
							on(node, touch.out, tracker);
							done();
						});
					})
					.then(moveMouseToElement(session, 'upper-right'))
					.execute(function () {
						return tracker.called;
					})
					.then(function (result) {
						assert.isTrue(result);
					});
			},

			'enter': function () {
				var session = this.get('remote');

				return session
					.then(moveMouseToElement(session, 'upper-left'))
					.executeAsync(function (done) {
						require(['sinon', 'dojo/on', 'dojo/touch'], function (sinon, on, touch) {
							var node = document.getElementById('upper-right');
							window.tracker = sinon.stub();
							on(node, touch.enter, tracker);
							done();
						});
					})
					.then(moveMouseToElement(session, 'upper-right'))
					.execute(function () {
						return tracker.calledOnce;
					})
					.then(function (result) {
						assert.isTrue(result);
					});
			},

			'leave': function () {
				var session = this.get('remote');

				return session
					.then(moveMouseToElement(session, 'upper-left'))
					.executeAsync(function (done) {
						require(['sinon', 'dojo/on', 'dojo/touch'], function (sinon, on, touch) {
							var node = document.getElementById('upper-left');
							window.tracker = sinon.stub();
							on(node, touch.leave, tracker);
							done();
						});
					})
					.then(moveMouseToElement(session, 'upper-right'))
					.execute(function () {
						return tracker.calledOnce;
					})
					.then(function (result) {
						assert.isTrue(result);
					});
			}
		};
	});

	registerSuite(function () {
		var tapOrClick;

		return {
			name: 'dojo/touch dojoClick tests',

			'before': function () {
				// Not all browsers or drivers support touch events
				tapOrClick = this.get('remote').environmentType.touchEnabled ?
					tapElement :
					clickElement;
			},

			'beforeEach': function () {
				return ready(this.get('remote'), require.toUrl('./support/touch_dojoclick.html'));
			},

			'press': function () {
				return tapOrClick(this.get('remote').findById('dojoClickBtn'))
						.execute(function () {
							return dojoClicks.value;
						})
						.then(function (result) {
							assert.equal(result, 1, 'dojoClicks');
						})
						.end()
					.findById('nativeClickBtn')
						.click()
						.execute(function () {
							return nativeClicks.value;
						})
						.then(function (result) {
							assert.equal(result, 1, 'nativeClicks');
						})
			}
		};
	});

	// Tests for #18352
	// Checks that dojo/touch with dojoClick=true does not break interaction with "native" HTML input
	// of type checkbox and radio (used to be broken on iOS and Android).
	registerSuite(function () {
		var tapOrClick;

		return {
			name: 'dojo/touch dojoClick2 tests',

			'before': function () {
				// Not all browsers or drivers support touch events
				tapOrClick = this.get('remote').environmentType.touchEnabled ?
					tapElement :
					clickElement;
			},

			'beforeEach': function () {
				return ready(this.get('remote'), require.toUrl('./support/touch_dojoclick2.html'));
			},

			'press': function () {
				return tapOrClick(this.get('remote').findById('dojoClickCheckbox'))
					.execute(function () {
						return dojoClickCheckbox.checked;
					})
					.then(function (result) {
						assert.isTrue(result, 'dojoClicks2 - checkbox');
					})
					.end()
					.findById('dojoClickRadio2')
					.click()
					.execute(function () {
						return dojoClickRadio2.checked;
					})
					.then(function (result) {
						assert.isTrue(result, 'dojoClicks2 - radio');
					});
			}
		};
	});

});