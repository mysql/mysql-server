define([
	'intern!object',
	'intern/chai!assert',
	'../../query',
	'../../_base/kernel',
	'dojo/dom-construct',
	'dojo/_base/declare',
	'dojo/text!./support/html.template.html',
	'../../NodeList-html',
	'../../NodeList-dom'
], function (registerSuite, assert, query, dojo, domConstruct, declare, template) {
	function deleteGlobal(name) {
		window[name] = undefined;
		try {
			delete window[name];
		} catch(e) { /* ie is special */ }
	}

	registerSuite(function () {
		var container;

		return {
			name: 'dojo/NodeList-html',

			'before': function () {
				declare('SimpleThing', null, {
					constructor: function(params, node) {
						node.setAttribute('test', 'ok');
					}
				});
			},

			'beforeEach': function () {
				container = domConstruct.place(template, document.body);
			},

			'afterEach': function () {
				document.body.removeChild(container);
				container = null;
			},

			'after': function () {
				deleteGlobal('SimpleThing');
			},

			'simple query usage': function () {
				var markup = '<p>expected</p>';
				var options = { onEnd: onEnd };

				query('#container').html(markup, options);

				function onEnd() {
					var node = query('#container p');
					assert.lengthOf(node, 1);
					assert.strictEqual(node[0].innerHTML, 'expected');
				}
			},

			'nodelist html': function () {
				var options = { parseContent: true, onBegin: onBegin };
				var markup = '<li data-' + dojo._scopeName + '-type="SimpleThing">1</li>' +
					'<li data-' + dojo._scopeName + '-type="SimpleThing">2</li>' +
					'<li data-' + dojo._scopeName + '-type="SimpleThing">3</li>';
				var liNodes;

				query('.zork').html(markup, options).removeClass('notdone').addClass('done');

				liNodes = query('.zork > li');

				assert.lengthOf(liNodes, 9);
				assert.isTrue(textReplacementHappened(liNodes));
				assert.isTrue(doneIsAddedToEveryNode(query('.zork')));
				assert.isTrue(hasATestAttribute(liNodes));

				function onBegin() {
					this.content = this.content.replace(/([0-9])/g, 'MOOO');
					this.inherited('onBegin', arguments);
				}

				function doneIsAddedToEveryNode(nodes) {
					return nodes.every(function (node) {
						return node.className.indexOf('zork') >= 0 && node.className.indexOf('done') >= 0;
					});
				}

				function textReplacementHappened(nodes) {
					return nodes.every(function(node) {
						return node.innerHTML.match(/MOOO/);
					});
				}

				function hasATestAttribute(nodes) {
					return nodes.every(function (node) {
						return node.getAttribute('test') === 'ok';
					});
				}
			}
		};
	});
});
