define([
	'intern!object',
	'intern/chai!assert',
	"../../AdapterRegistry"
], function (registerSuite, assert, AdapterRegistry) {

	registerSuite({
		name: 'dojo/AdapterRegistry',

		'constructor': function () {
			var registry = new AdapterRegistry();
			assert.strictEqual(registry.pairs.length, 0);
			assert.isFalse(registry.returnWrappers);

			var registry = new AdapterRegistry(true);
			assert.isTrue(registry.returnWrappers);
		},
		'.register': function () {
			var registry = new AdapterRegistry();
			registry.register("blah",
				function(str){ return str == "blah"; },
				function(){ return "blah"; }
			);
			assert.strictEqual(registry.pairs.length, 1);
			assert.strictEqual(registry.pairs[0][0], "blah");

			registry.register("thinger");
			registry.register("prepend", null, null, true, true);
			assert.strictEqual(registry.pairs[0][0], "prepend");
			assert.isTrue(registry.pairs[0][3]);
		},
		'.match' : {
			'no match': function () {
				var registry = new AdapterRegistry();
				assert.throws(function() {
					registry.match("blah");
				});
			},
			'returnWrappers': function () {
				var registry = new AdapterRegistry();
				registry.register("blah",
					function(str){ return str == "blah"; },
					function(){ return "blah"; }
				);
				assert.strictEqual(registry.match("blah"), "blah");

				registry.returnWrappers = true;
				assert.strictEqual(registry.match("blah")(), "blah");
			}
		},
		'.unregister': function () {
			var registry = new AdapterRegistry();
			registry.register("blah",
				function(str){ return str == "blah"; },
				function(){ return "blah"; }
			);
			registry.register("thinger");
			registry.register("prepend", null, null, true, true);
			registry.unregister("prepend");
			assert.strictEqual(registry.pairs.length, 2);
			assert.strictEqual(registry.pairs[0][0], "blah");
		}
	});

});
