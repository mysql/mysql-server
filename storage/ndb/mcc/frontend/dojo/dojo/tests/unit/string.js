define([
	'intern!object',
	'intern/chai!assert',
	'../../string'
], function (registerSuite, assert, string) {
	registerSuite({
		name: 'dojo/string',

		'.pad': function () {
			assert.strictEqual(string.pad('1', 5), '00001');
			assert.strictEqual(string.pad('000001', 5), '000001');
			assert.strictEqual(string.pad('1', 5, null, true), '10000');
		},

		'.substitute': {
			normal: function () {
				assert.strictEqual(
					string.substitute(
						'File "${0}" is not found in directory "${1}".',
						['foo.html','/temp']
					),
					'File "foo.html" is not found in directory "/temp".'
				);

				assert.strictEqual(
					string.substitute(
						'File "${name}" is not found in directory "${info.dir}".',
						{
							name: 'foo.html',
							info: { dir: '/temp' }
						}
					),
					'File "foo.html" is not found in directory "/temp".'
				);

				assert.strictEqual(
					string.substitute(
						'Escaped ${}'
					),
					'Escaped $'
				);
			},

			'missing key': function () {
				assert.throws(function () {
					string.substitute('${x}', { y: 1 });
				}, /^string\.substitute could not find key "\w+" in template$/);
			},

			transform: function () {
				function getPrefix(str) {
					// try to figure out the type
					var prefix = (str.charAt(0) === '/') ? 'directory': 'file';
					if(this.____prefix){
						prefix = this.____prefix + prefix;
					}
					return prefix + ' "' + str + '"';
				}

				var obj = {
					____prefix: '...',
					getPrefix: getPrefix
				};

				assert.strictEqual(
					string.substitute(
						'${0} is not found in ${1}.',
						['foo.html','/temp'],
						getPrefix
					),
					'file "foo.html" is not found in directory "/temp".'
				);

				assert.strictEqual(
					string.substitute(
						'${0} is not found in ${1}.',
						['foo.html','/temp'],
						obj.getPrefix, obj
					),
					'...file "foo.html" is not found in ...directory "/temp".'
				);
			},

			formatter: function () {
				assert.strictEqual(
					string.substitute(
						'${0:postfix}', ['thinger'], null, {
							postfix: function(value){
								return value + ' -- howdy';
							}
						}
					),
					'thinger -- howdy'
				);
			}
		},

		'.trim': function () {
			assert.strictEqual(string.trim('   \f\n\r\t      astoria           '), 'astoria');
			assert.strictEqual(string.trim('astoria                            '), 'astoria');
			assert.strictEqual(string.trim('                            astoria'), 'astoria');
			assert.strictEqual(string.trim('astoria'), 'astoria');
			assert.strictEqual(string.trim('   a   '), 'a');
		},

		'.rep': function () {
			assert.strictEqual(string.rep('a', 5), 'aaaaa');
			assert.strictEqual(string.rep('ab', 4), 'abababab');
			assert.strictEqual(string.rep('ab', 0), '');
			assert.strictEqual(string.rep('', 3), '');
		},

		'.escape': function () {
			assert.equal(string.escape('astoria'), 'astoria');
			assert.equal(string.escape('&<>\'/'), '&amp;&lt;&gt;&#x27;&#x2F;');
			assert.equal(string.escape('oh"oh"oh'), 'oh&quot;oh&quot;oh');
		}
	});
});
