define([
	'intern!object',
	'intern/chai!assert',
	'../../../on',
	'../../../on/throttle',
	'../../../Evented',
	'sinon',
	'dojo/has!host-browser?dojo/dom-construct',
	'dojo/has!host-browser?../../../query',
	'dojo/has!host-browser?dojo/domReady!'
], function (registerSuite, assert, on, throttle, Evented, sinon, domConstruct) {
	var handles = [];
	var originalOn = on;
	on = function () {
		var handle = originalOn.apply(null, arguments);
		handles.push(handle);
		return handle;
	};
	for (var key in originalOn) {
		on[key] = originalOn[key];
	}

	function cleanUpListeners() {
		while (handles.length > 0) {
			handles.pop().remove();
		}
	}

	var testObject;
	var suite = {
		name: 'dojo/on/throttle',

		common: {
			beforeEach: function () {
				testObject = new Evented();
			},

			throttles: function () {
				var spy = sinon.spy();
				on(testObject, throttle('custom', 200), spy);

				var emitter = sinon.spy(function () {
					if (!testObject) {
						// clearInterval() will not clear an already
						// scheduled interval, so this checks if afterEach
						// has cleaned up already
						return;
					}
					testObject.emit('custom');
				});
				var interval = setInterval(emitter, 50);

				var dfd = this.async();
				setTimeout(dfd.callback(function () {
					clearInterval(interval);
					assert.ok(spy.callCount < emitter.callCount);
				}), 1000);
			},

			afterEach: function () {
				cleanUpListeners();
				testObject = null;
			}
		}
	};

	var containerDiv;
	var containerDiv2;
	var anchor;
	var button;

	suite.DOM = {
		beforeEach: function () {
			if (!domConstruct) {
				return;
			}
			containerDiv = domConstruct.create('div', null, document.body);
			containerDiv2 = domConstruct.create('div', null, containerDiv);
			anchor = domConstruct.create('a', null, containerDiv2);
			button = domConstruct.create('button', {
				type: 'button'
			}, anchor);
		},

		throttles: function () {
			if (!domConstruct) {
				this.skip('Not running browser-only tests');
			}
			var throttleSpy = sinon.spy();
			var clickSpy = sinon.spy();

			on(containerDiv, throttle('a:click', 200), throttleSpy);
			on(containerDiv2, 'a:click', clickSpy);

			var clicker = sinon.spy(function () {
				if (!button) {
					// clearInterval() will not clear an already
					// scheduled interval, so this checks if afterEach
					// has cleaned up already
					return;
				}
				button.click();
			});
			var interval = setInterval(clicker, 50);

			var dfd = this.async();
			setTimeout(dfd.callback(function () {
				clearInterval(interval);
				assert.ok(throttleSpy.callCount < clickSpy.callCount);
			}), 1000);
		},

		afterEach: function () {
			if (!domConstruct) {
				return;
			}
			cleanUpListeners();

			domConstruct.destroy(containerDiv);
			containerDiv = containerDiv2 = anchor = button = null;
		}
	};
	registerSuite(suite);
});
