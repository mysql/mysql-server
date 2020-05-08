define([
	'intern!object',
	'intern/chai!assert',
	'../../hash',
	'../../topic'
], function (registerSuite, assert, hash, topic) {

	var _subscriber = null;
	// utilities for the tests:
	function setHash(h) {
		h = h || '';
		location.replace('#'+h);
	}

	function getHash() {
		var h = location.href, i = h.indexOf('#');
		return (i >= 0) ? h.substring(i + 1) : '';
	}

	registerSuite({
		name: 'dojo/hash',

		'get hash': {
			beforeEach: function () {
				setHash('');
			},
			afterEach: function () {
				setHash('');
			},
			'empty': function () {
				assert.strictEqual(hash(), '');
			},
			'text': function () {
				setHash('text');
				assert.strictEqual(hash(), 'text');
			},
			'text%20with%20spaces': function () {
				setHash('text%20with%20spaces');
				assert.strictEqual(hash(), 'text%20with%20spaces');
			},
			'text%23with%23encoded%23hashes': function () {
				setHash('text%23with%23encoded%23hashes');
				assert.strictEqual(hash(), 'text%23with%23encoded%23hashes');
			},
			'text+with+pluses': function () {
				setHash('text+with+pluses');
				assert.strictEqual(hash(), 'text+with+pluses');
			},
			'%20leadingSpace': function () {
				setHash('%20leadingSpace');
				assert.strictEqual(hash(), '%20leadingSpace');
			},
			'trailingSpace%20': function () {
				setHash('trailingSpace%20');
				assert.strictEqual(hash(), 'trailingSpace%20');
			},
			'under_score': function () {
				setHash('under_score');
				assert.strictEqual(hash(), 'under_score');
			},
			'extra&instring': function () {
				setHash('extra&instring');
				assert.strictEqual(hash(), 'extra&instring');
			},
			'extra?instring': function () {
				setHash('extra?instring');
				assert.strictEqual(hash(), 'extra?instring');
			},
			'?testa=3&testb=test': function () {
				setHash('?testa=3&testb=test');
				assert.strictEqual(hash(), '?testa=3&testb=test');
			}
		},

		'set hash': {
			beforeEach: function () {
				setHash('');
			},
			afterEach: function () {
				setHash('');
			},
			'empty': function () {
				hash('');
				assert.strictEqual(getHash(), '');
			},
			'text': function () {
				hash('text');
				assert.strictEqual(getHash(), 'text');
			},
			'text%20with%20spaces': function () {
				hash('text%20with%20spaces');
				assert.strictEqual(getHash(), 'text%20with%20spaces');
			},
			'text%23with%23encoded%23hashes': function () {
				hash('text%23with%23encoded%23hashes');
				assert.strictEqual(getHash(), 'text%23with%23encoded%23hashes');
			},
			'text+with+pluses': function () {
				hash('text+with+pluses');
				assert.strictEqual(getHash(), 'text+with+pluses');
			},
			'%20leadingSpace': function () {
				hash('%20leadingSpace');
				assert.strictEqual(getHash(), '%20leadingSpace');
			},
			'trailingSpace%20': function () {
				hash('trailingSpace%20');
				assert.strictEqual(getHash(), 'trailingSpace%20');
			},
			'under_score': function () {
				hash('under_score');
				assert.strictEqual(getHash(), 'under_score');
			},
			'extra&instring': function () {
				hash('extra&instring');
				assert.strictEqual(getHash(), 'extra&instring');
			},
			'extra?instring': function () {
				hash('extra?instring');
				assert.strictEqual(getHash(), 'extra?instring');
			},
			'?testa=3&testb=test': function () {
				hash('?testa=3&testb=test');
				assert.strictEqual(getHash(), '?testa=3&testb=test');
			},
			'#leadingHash': function () {
				hash('#leadingHash');
				assert.strictEqual(getHash(), 'leadingHash');
			}
		},

		'topic publish': {
			before: function () {
				_subscriber = null;
				setHash('');
			},
			after: function () {
				_subscriber && _subscriber.remove();
				setHash('');
			},
			'text': function () {
				var dfd = this.async();
				_subscriber = topic.subscribe('/dojo/hashchange',
					dfd.callback(function (value) {
						assert.strictEqual(value, 'text');
						return value;
					})
				);
				hash('text');
				return dfd;
			}
		}
	});
});
