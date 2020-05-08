define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'../support/ready'
], function (require, registerSuite, assert, ready) {
	/* global behavior, behaviorObject, applyCount, topicCount */
	registerSuite({
		name: 'dojo/behavior',

		before: function () {
			return ready(this.get('remote'), require.toUrl('./behavior.html'));
		},

		'.add': function () {
			return this.get('remote')
				.execute(function () {
					return {
						bar: !!behavior._behaviors['.bar'],
						foo: !!behavior._behaviors['.foo > span']
					};
				})
				.then(function (result) {
					assert.ok(!result.bar);
					assert.ok(!result.foo);
				})
				.execute(function () {
					behavior.add(behaviorObject);
					return {
						bar: behavior._behaviors['.bar'] && behavior._behaviors['.bar'].length,
						foo: behavior._behaviors['.foo > span'] && behavior._behaviors['.foo > span'].length,
						applyCount: applyCount
					};
				})
				.then(function (result) {
					assert.strictEqual(result.bar, 1);
					assert.strictEqual(result.foo, 1);
					assert.strictEqual(result.applyCount, 0);
				})
			;
		},

		'.apply': function () {
			return this.get('remote')
				.execute(function () {
					behavior.apply();
					return applyCount;
				})
				.then(function (applyCount) {
					assert.strictEqual(applyCount, 2);
				})
				.execute(function () {
					behavior.apply();
					return applyCount;
				})
				.then(function (applyCount) {
					// assure it only matches once
					assert.strictEqual(applyCount, 2);
				})
			;
		},

		'reapply': function () {
			return this.get('remote')
				.execute(function () {
					behavior.add(behaviorObject);
					behavior.apply();
					return applyCount;
				})
				.then(function (applyCount) {
					assert.strictEqual(applyCount, 4);
				})
			;
		},

		'events': function () {
			return this.get('remote')
				.execute(function () {
					behavior.add({
						'.foo': '/foo'
					});
					behavior.apply();
					return topicCount;
				})
				.then(function (topicCount) {
					assert.strictEqual(topicCount, 2);
				})
				.findById('another')
					.click()
				.end()
				.execute(function () {
					behavior.add({
						'.foo': {
							'onfocus': '/foo'
						}
					});
					behavior.apply();
					return topicCount;
				})
				.then(function (topicCount) {
					assert.strictEqual(topicCount, 2);
				})
				.findById('blah')
					.click()
				.end()
				.sleep(500)
				.execute(function () {
					return topicCount;
				})
				.then(function (topicCount) {
					assert.strictEqual(topicCount, 3);
				})
				.findById('another')
					.click()
				.end()
				.sleep(500)
				.findById('blah')
					.click()
				.end()
				.sleep(500)
				.execute(function () {
					return topicCount;
				})
				.then(function (topicCount) {
					assert.strictEqual(topicCount, 4);
				})
			;
		}
	});
});
