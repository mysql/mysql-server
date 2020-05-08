define([
	'intern!object',
	'intern/chai!assert',
	'../../../on',
	'../../../on/debounce',
	'../../../Evented',
	'sinon',
	'dojo/has!host-browser?dojo/dom-construct',
	'dojo/has!host-browser?../../../query',
	'dojo/has!host-browser?dojo/domReady!'
], function (registerSuite, assert, on, debounce, Evented, sinon, domConstruct) {
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
		name: 'dojo/on/debounce',

		common: {
			beforeEach: function () {
				testObject = new Evented();
			},

			debounces: function () {
				var spy = sinon.spy();
				on(testObject, debounce('custom', 100), spy);

				testObject.emit('custom');
				testObject.emit('custom');
				testObject.emit('custom');
				testObject.emit('custom');

				setTimeout(this.async().callback(function () {
					assert.equal(spy.callCount, 1);
				}), 500);
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
			button = domConstruct.create('button', null, anchor);
		},

		bubbling: function () {
			if (!domConstruct) {
				this.skip('Not running browser-only tests');
			}
			var spy1 = sinon.spy(function (event) {
				assert.ok(Boolean(event));
				assert.ok(Boolean(event.target));
				assert.equal(event.target.nodeType, 1);
			});
			var spy2 = sinon.spy();
			var spy3 = sinon.spy();
			var spy4 = sinon.spy();

			on(containerDiv, debounce('a:click', 100), spy1);
			on(containerDiv2, debounce('click', 100), spy2);
			on(containerDiv, debounce('click, a:click', 100), spy3);
			on(containerDiv, 'a:click', spy4);

			try {
				button.click();
				button.click();
				button.click();
				button.click();
			}
			catch (e) {}

			setTimeout(this.async().callback(function () {
				assert.equal(spy1.callCount, 1);
				assert.equal(spy2.callCount, 1);
				assert.equal(spy3.callCount, 1);
				assert.equal(spy4.callCount, 4);
			}), 500);
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
