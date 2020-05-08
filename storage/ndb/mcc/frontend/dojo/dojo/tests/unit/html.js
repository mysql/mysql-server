define([
	'intern!object',
	'intern/chai!assert',
	'sinon',
	'../../html',
	'../../parser',
	'../../_base/kernel',
	'dojo/_base/declare',
	'dojo/Deferred',
	'dojo/dom-construct',
	'dojo/query',
	'dojo/_base/array',
	'dojo/text!./support/html.template.html'
], function (registerSuite, assert, sinon, html, parser, dojo, declare, Deferred, domConstruct, query,
			 arrayUtil, template) {
	/* globals ifrs, id00, id01 */

	function ieTrimSpaceBetweenTags (str){
		return str.replace(/(<[a-z]*[^>]*>)\s*/ig, '$1');
	}

	function deleteGlobal(name) {
		window[name] = undefined;
		try {
			delete window[name];
		} catch(e) { /* ie is special */ }
	}

	registerSuite(function () {
		var container;

		function testHtmlSet(node, markup) {
			if(typeof node === 'string') {
				node = query(node, container)[0];
			}
			html.set(node, markup);
			assert.strictEqual(ieTrimSpaceBetweenTags(node.innerHTML), markup);
			return node;
		}

		return {
			name: 'dojo/html',

			'before': function () {
				declare('SimpleThing', null, {
					constructor: function(params, node) {
						node.setAttribute('test', 'ok');
					}
				});
			},

			beforeEach: function () {
				container = domConstruct.place(template, document.body);
			},

			afterEach: function () {
				document.body.removeChild(container);
				container = null;

				deleteGlobal('ifrs');
				deleteGlobal('id00');
				deleteGlobal('id01');
			},

			'after': function () {
				deleteGlobal('SimpleThing');
			},

			'.set': {
				'basic usage': function () {
					var markup = 'expected';

					testHtmlSet(container, markup);
				},

				'numeric value': function () {
					var markup = 1.618;

					html.set(container, markup);
					assert.strictEqual(container.innerHTML, markup.toString());
				},

				'attach onEnd handler': function () {
					var msg = 'expected';
					var handler = sinon.stub();

					html.set(container, msg, { onEnd: handler });
					assert.isTrue(handler.calledOnce);
				},

				'parseContent: true': function () {
					var dfd = new Deferred();
					/* jshint maxlen: 130 */ // splitting this line doesn't make it more readable
					var content = '<div data-' + dojo._scopeName + '-type="SimpleThing" data-' + dojo._scopeName + '-id="ifrs" data="{}"></div>';
					var options = {
						parseContent: true,
						postscript: function () {
							this.set();

							assert.isDefined(ifrs);
							assert.strictEqual(ifrs.declaredClass, 'SimpleThing');
							assert.lengthOf(this.parseResults, 1);
							dfd.resolve();
						}
					};

					html.set(container, content, options);
					return dfd;
				},

				'change content of tr in thead': function () {
					var query = '#tableTest > thead > tr';
					var markup = '<td><div>This</div>Should<u>Work</u></td>';

					testHtmlSet(query, markup);
				},

				'change content of th in thead': function () {
					var query = '#tableTest > thead';
					var markup = '<tr><td><div>This</div>Should<u>Work</u></td></tr>';

					testHtmlSet(query, markup);
				},

				'change content of tr in tbody': function () {
					var query = '#tableTest > tbody > tr';
					var markup = '<td><div>This</div>Should<u>Work</u></td>';

					testHtmlSet(query, markup);
				},

				'change content of tbody': function () {
					var query = '#tableTest > tbody';
					var markup = '<tr><td><div>This</div>Should<u>Work</u></td></tr>';

					testHtmlSet(query, markup);
				},

				'change content of table': function () {
					var query = '#tableTest';
					var markup = '<tbody><tr><td><div>This</div>Should<u>Work</u></td></tr></tbody>';

					testHtmlSet(query, markup);
				},

				'basic NodeList': function () {
					var tmpUL = domConstruct.create('ul');
					domConstruct.create('li', { innerHTML: 'item 1' }, tmpUL);
					domConstruct.create('li', { innerHTML: 'item 2' }, tmpUL);

					html.set(container, tmpUL.childNodes);
					assert.lengthOf(query('li', container), 2);
				},

				'mixed content': function () {
					var markup = '<h4>See Jane</h4>Look at her <span>Run</span>!';

					testHtmlSet(container, markup);
				},

				'extractContent: true': function () {
					var markup = '' +
						'<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN">' +
						'<html>											' +
						'	<head>										' +
						'		<title>									' +
						'			the title							' +
						'		</title>								' +
						'	</head>										' +
						'	<body>										' +
						'		<p>										' +
						'			This is the <b>Good Stuff</b><br>	' +
						'		</p>									' +
						'	</body>										' +
						'</html>										';

					html.set(container, markup, { extractContent: true });
					assert.strictEqual(container.innerHTML.indexOf('title'), -1);
					assert.lengthOf(query('*', container), 3);
				},

				'inheritance': (function() {
					var parseSpy;

				    function runTest(options) {
						/* jshint maxlen: 130 */ // splitting this line doesn't make it more readable
						var markup = '<div data-dojo-testing-type="SimpleThing" data-dojo-testing-id="ifrs" data="{}"></div>';

						html.set(container, markup, options);
						return parseSpy.lastCall.args[0].inherited;
					}

					return {
						'beforeEach': function () {
							parseSpy = sinon.spy(parser, 'parse');
						},

						'afterEach': function () {
							parseSpy.restore();
							delete window.ifrs;
						},

						'dir, lang, textDir are not specified': function () {
							var options = { parseContent: true };
							var inherited = runTest(options);

							assert.isTrue(parseSpy.called);
							assert.isUndefined(inherited.dir, 'dir should not exist');
							assert.isUndefined(inherited.lang, 'lang should not exist');
							assert.isUndefined(inherited.textDir, 'textDir should not exist');
						},

						'dir, lang, textDir are specified': function () {
							var options = {
								parseContent: true,
								dir: 'expectedDir',
								lang: 'expectedLang',
								textDir: 'expectedTextDir'
							};
							var inherited = runTest(options);

							assert.isTrue(parseSpy.called);
							assert.strictEqual(inherited.dir, options.dir);
							assert.strictEqual(inherited.lang, options.lang);
							assert.strictEqual(inherited.textDir, options.textDir);
						}
					};
				}()),

				'_emptyNode': function () {
					container.innerHTML = '<div><span>just</span>some test<br/></div>text';
					html._emptyNode(container);
					assert.lengthOf(container.childNodes, 0);
					assert.strictEqual(container.innerHTML, '');
				},

				'_ContentSetter': function () {
					/* jshint maxlen: 130 */ // splitting these lines doesn't make it more readable
					var args = [
						[ 'simple' ],
						[
							'<div data-' + dojo._scopeName + '-type="SimpleThing" data-' + dojo._scopeName + '-id="id00">parsed content</div>',
							{ parseContent: true }
						],
						[
							'<div data-' + dojo._scopeName + '-type="SimpleThing" data-' + dojo._scopeName + '-id="id01">parsed content</div>',
							{ parseContent: true }
						]
					];
					var setter = new html._ContentSetter({ node: container });

					arrayUtil.forEach(args, function (applyArgs) {
						setter.node = container;
						setter.set.apply(setter, applyArgs);
						setter.tearDown();
					});

					assert.isDefined(id00);
					assert.isDefined(id01);
					assert.isUndefined(setter.parseResults);
				}
			}
		};
	});
});
