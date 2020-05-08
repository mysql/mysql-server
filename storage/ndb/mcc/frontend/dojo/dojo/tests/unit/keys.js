define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'../../keys',
	'../../_base/kernel',
	'dojo/has',
	'dojo/Deferred',
	'dojo/_base/array'
], function (require, registerSuite, assert, keys, dojo, has, Deferred, array) {
	var keysMid = require.toAbsMid('../../keys');

	/**
	 * Handles the process of redefining dojo/keys with an altered state
	 * @param hasStates a collection of dojo/has states to alter for the test
	 * @returns {Deferred.promise|*} provides a handle to restore the previous state
	 */
	function mockKeys(hasStates) {
		var dfd = new Deferred();
		var handle = { restore: restore };
		var oldStates = {};
		var hasName, hasValue;

		for(hasName in hasStates) {
			if(hasStates.hasOwnProperty(hasName)) {
				hasValue = hasStates[hasName];
				oldStates[hasName] = has(hasName);
				has.add(hasName, hasValue, undefined, true);
			}
		}
		require.undef(keysMid);
		require([keysMid], function (_keys) {
			keys = _keys;
			dfd.resolve(handle);
		});

		return dfd.promise;

		function restore() {
			var hasName, hasValue;

			require.undef(keysMid);
			delete dojo.keys;
			for(hasName in oldStates) {
				if(oldStates.hasOwnProperty(hasName)) {
					hasValue = oldStates[hasName];
					has.add(hasName, hasValue, undefined, true);
				}
			}
		}
	}

	registerSuite(function () {
		var suite = {
			name: 'dojo/keys',

			'construction': function () {
				assert.isDefined(keys);
				assert.equal(keys, dojo.keys);
			}
		};

		// Parameterized Tests
		array.forEach([
			// name            states            expectations
			['Webkit browser', { webkit: true }, [{ 'META': 91 }]],
			['Non-Webkit browser', { webkit: false }, [{ 'META': 224 }]],
			['Non-Mac', { mac: false }, [{ 'copyKey': 17 }]],
			['Safari browser', { mac: true, air: false, safari: true }, [{ 'copyKey': 91 }]],
			['Not Safari', { mac: true, air: false, safari: false }, [{ 'copyKey': 224 }]],
			['Air on Mac', { mac: true, air: true }, [{ 'copyKey': 17 }]]
		], parameterizedKeyTest);

		return suite;

		function parameterizedKeyTest(parameters) {
			var name = parameters[0];
			var hasStates = parameters[1];

			suite[name] = (function() {
				var restoreHandle;

				return {
					before: function () {
						return mockKeys(hasStates).then(function (handle) {
							restoreHandle = handle;
						});
					},

					'after': function () {
						restoreHandle.restore();
					}
				};
			}());
			array.forEach(parameters[2], function (keymap) {
				for (var key in keymap) {
					if(keymap.hasOwnProperty(key)) {
						suite[name][key] = function() {
							assert.equal(keys[key], keymap[key]);
						};
					}
				}
			});
		}
	});
});
