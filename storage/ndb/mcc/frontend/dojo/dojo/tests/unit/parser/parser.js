define([
	'intern!object',
	'intern/chai!assert',
	'../../../parser',
	'dojo/dom-construct',
	'dojo/_base/array',
	'dojo/aspect',
	'dojo/_base/declare',
	'dojo/dom',
	'dojo/dom-attr',
	'dojo/_base/lang',
	'dojo/on',
	'dojo/_base/window',
	'dojo/date/stamp',
	'dojo/Stateful',
	'dojo/Evented',
	'dojo/text!./parser.html',
	'./support/util'
], function (
	registerSuite,
	assert,
	parser,
	domConstruct,
	array,
	aspect,
	declare,
	dom,
	domAttr,
	lang,
	on,
	win,
	dstamp,
	Stateful,
	Evented,
	template,
	util
) {
	var container;

	/* global tests */
	function setup(id, shouldReturn) {
		return function () {
			delete window.tests;

			var MyNonDojoClass = window.MyNonDojoClass = function () {};
			MyNonDojoClass.extend = function () {
				var args = arguments;
				return function () {
					this.expectedClass = true;
					this.params = args;
				};
			};

			declare('tests.parser.Widget', null, {
				constructor: function (args) {
					this.params = args;
				}
			});

			declare('tests.parser.Class1', null, {
				constructor: function (args) {
					this.params = args;
					lang.mixin(this, args);
				},
				preambleTestProp: 1,
				preamble: function () {
					this.preambleTestProp++;
				},
				intProp: 1,
				callCount: 0, // for connect testing
				callInc: function () { this.callCount++; },
				callCount2: 0, // for assignment testing
				strProp1: 'original1',
				strProp2: 'original2',
				arrProp: [],
				arrProp2: ['foo'],
				boolProp1: false,
				boolProp2: true,
				boolProp3: false,
				boolProp4: true,
				dateProp1: dstamp.fromISOString('2007-01-01'),
				dateProp2: dstamp.fromISOString('2007-01-01'),
				dateProp3: dstamp.fromISOString('2007-01-01'),
				funcProp: function () {},
				funcProp2: function () {},
				funcProp3: function () {},
				onclick: function () { this.prototypeOnclick = true; }
				// FIXME: have to test dates!!
				// FIXME: need to test the args property!!
			});

			declare('tests.parser.Class2', null, {
				constructor: function () {
					this.fromMarkup = false;
				},
				fromMarkup: false,
				markupFactory: function () {
					var i = new tests.parser.Class2();
					i.fromMarkup = true;
					return i;
				}
			});

			declare('tests.parser.Class3', tests.parser.Class2, {
				fromMarkup: false,
				markupFactory: function (args, node, ClassCtor) {
					var i = new ClassCtor();
					i.classCtor = ClassCtor;
					i.params = args;
					return i;
				}
			});

			declare('tests.parser.InputClass', null, {
				constructor: function (args) {
					this.params = args;
					lang.mixin(this, args);
				},

				// these attributes are special in HTML, they don't have a value specified
				disabled: false,
				readonly: false,
				checked: false,

				// other attributes native to HTML
				value: 'default value',
				title: 'default title',
				tabIndex: '0',		// special because mixed case

				// custom widget attributes that don't match a native HTML attributes
				custom1: 123,
				custom2: 456
			});

			// Test that dir, lang, etc. attributes can be inherited from ancestor node
			declare('tests.parser.BidiClass', tests.parser.Widget, {
				constructor: function (args) { lang.mixin(this, args); },
				dir: '',
				lang: '',
				textdir: '',
				name: ''
			});

			// For testing that parser recurses correctly, except when the prototype has a
			// stopParser flag
			declare('tests.parser.NormalContainer', null, {
				constructor: function (args) { lang.mixin(this, args); }
			});
			declare('tests.parser.ShieldedContainer', null, {
				constructor: function (args) { lang.mixin(this, args); },

				// flag to tell parser not to instantiate nodes inside of me
				stopParser: true
			});

			declare('tests.parser.HTML5Props', null, {
				constructor: function (args) { lang.mixin(this, args); },
				simple: false,
				a: 2,
				b: null,
				c: null,
				d: null,
				e: null,
				f: null,
				afn: function () {
					return this.a * 2;
				}
			});

			// not on .prototype:
			tests.parser.HTML5Props._aDefaultObj = {
				a: 1,
				b: 2,
				simple: true
			};

			declare('tests.parser.HTML5withMethod', null, {
				constructor: function (args) { lang.mixin(this, args); },
				baseValue: 10,
				someMethod: function () {
					return this.baseValue;
				},
				diffMethod: function () {
					this._ran = true;
				}
			});

			declare('tests.parser.StatefulClass', [Evented, Stateful], {
				strProp1: '',
				objProp1: {},
				boolProp1: false,
				prototypeOnclick: false,
				onclick: function () { this.prototypeOnclick = true; }
			});

			declare('tests.parser.MethodClass', null, {
				method1ran: false,
				method1after: false,
				method2ran: false,
				method2before: false,
				method2after: false,
				method3result: '',
				method4ran: false,
				method4after: false,
				method1: function () { this.method1ran = true; },
				method2: function () { this.method2ran = true; },
				method3: function (result) { this.method3result = result; },
				method4: function () { this.method4ran = true; }
			});

			declare('tests.parser.ClassForMixins', null, {
				classDone: true
			});

			declare('tests.parser.Mixin1', null, {
				mixin1Done: true
			});

			declare('tests.parser.Mixin2', null, {
				mixin2Done: true
			});

			declare('tests.resources.AMDWidget', null, {
				constructor: function (args) {
					this.params = args;
				}
			});

			declare('tests.resources.AMDWidget2', null, {
				constructor: function (args) {
					this.params = args;
				},

				method1: function (value) {
					value++;
					return value;
				}
			});

			declare('tests.resources.AMDWidget3', null, {
				constructor: function (args) {
					this.params = args;
				}
			});

			window.deepTestProp = {
				blah: {
					thinger: 1
				}
			};

			tests.parser.FormClass = declare(tests.parser.Widget, {
				encType: ''
			});

			window.foo = function () {
				this.fooCalled = true;
			};

			container = domConstruct.place(util.fixScope(template), document.body);
			var el = id ? dom.byId(id) : null,
				ret = parser.parse(el);

			return shouldReturn && ret;
		};
	}

	function teardown() {
		domConstruct.destroy(container);
		container = null;
		delete window.foo;
		delete window.tests;
	}

	registerSuite({
		name: 'dojo/parser basic tests',
		setup: setup('main', true),

		teardown: teardown,

		/* global obj */
		'data-dojo-id': function () {
			assert.isObject(obj);
		},

		/* global obj3 */
		'JsId': function () {
			assert.isObject(obj3);
		},

		'string property': function () {
			assert.isString(obj.strProp1);
			assert.equal(obj.strProp1, 'text');
		},

		'int property': function () {
			assert.isNumber(obj.intProp);
			assert.equal(obj.intProp, 5);
		},

		'array property': function () {
			assert.lengthOf(obj.arrProp, 3);
			assert.lengthOf(obj.arrProp[1], 3);
			assert.equal(obj.arrProp[1], 'bar');
		},

		'boolean property': function () {
			// boolProp1
			assert.isBoolean(obj.boolProp1);
			assert.isTrue(obj.boolProp1);

			// boolProp2
			assert.isBoolean(obj.boolProp2);
			assert.isFalse(obj.boolProp2);

			// boolProp3 not specified (prototype says false)
			assert.isBoolean(obj.boolProp3);
			// assert.isFalse(obj.boolprop3);

			// boolProp4 not specified (prototype says true)
			// assert.isBoolean(obj.boolProp4);
			// assert.isTrue(obj.boolProp4);
		},

		'date property': function () {
			assert.equal(dstamp.toISOString(obj.dateProp1, { selector: 'date' }), '2006-01-01');
			// dateProp2='', should map to NaN (a blank value on DateTextBox)
			assert.ok(isNaN(obj.dateProp2));

			// dateProp3='now', should map to current date
			assert.equal(dstamp.toISOString(obj.dateProp3, { selector: 'date' }),
				dstamp.toISOString(new Date(), { selector: 'date' }));
		},

		'unwanted params': function () {
			// Make sure that parser doesn't pass any unwanted parameters to
			// widget constructor, especially 'toString' or 'constructor'.
			// Make exception for dir/lang which parser gleans from document itself.
			for (var param in obj.params) {
				assert.ok(array.indexOf(
					[
						'strProp1', 'strProp2',
						'intProp',
						'arrProp', 'arrProp2',
						'boolProp1', 'boolProp2',
						'dateProp1', 'dateProp2', 'dateProp3',
						'funcProp2', 'funcProp3',
						'preamble',
						'callInc1', 'callInc2', 'dir', 'lang', 'textDir'
					],
				param) >= 0, param + ' should not be in the parameters passed to the widget constructor');
			}
		},

		'disabled flag': function () {
			/* global disabledObj */
			assert.isBoolean(disabledObj.disabled);
			assert.isTrue(disabledObj.disabled);
			assert.isFalse(disabledObj.checked);
		},

		'checked flag': function () {
			/* global checkedObj */
			assert.isBoolean(checkedObj.checked);
			assert.isFalse(checkedObj.disabled);
			assert.isTrue(checkedObj.checked);
		},

		'function property': function () {
			// make sure that unspecified functions (even with common names)
			// don't get overridden (bug #3074)
			obj.onclick();
			assert.isTrue(obj.prototypeOnclick, 'prototypeOnClick');

			// funcProp2='foo'
			obj.funcProp2();
			assert.isTrue(obj.fooCalled, 'fooCalled');

			// funcProp3='this.func3Called=true;'
			obj.funcProp3();
			assert.isTrue(obj.func3Called, 'func3Called');
		},

		'connect': function () {
			obj.callInc();
			assert.equal(obj.callCount, 2);
		},

		'function assignment': function () {
			obj.callInc2();
			assert.equal(obj.callCount2, 1);
		},

		'subnode parse': function () {
			assert.isFalse(lang.exists('obj2'), 'exists before parse');
			var toParse = dom.byId('toParse');
			parser.parse(toParse.parentNode);
			assert.isTrue(lang.exists('obj2'), 'exists after parse');
			assert.equal(obj.declaredClass, 'tests.parser.Class1');
		},

		'markup factory': function () {
			assert.isTrue(lang.exists('obj3'), 'obj3 exists');
			assert.isTrue(obj3.fromMarkup);
		},

		/* global obj4 */
		'markup factory class': function () {
			assert.isTrue(lang.exists('obj4'), 'obj4 exists');
			assert.equal(obj4.classCtor, tests.parser.Class3);
			assert.instanceOf(obj4, tests.parser.Class3);
			assert.instanceOf(obj4, tests.parser.Class2);
		},

		'no start': function () {
			var started = false;
			declare('SampleThinger', null, {
				startup: function () {
					started = true;
				}
			});

			domConstruct.create('div', {
				dojoType: 'SampleThinger'
			}, 'parsertest');

			parser.parse('parsertest', { noStart: true });
			assert.isFalse(started, 'first started check');

			domConstruct.empty('parsertest');

			started = false;

			domConstruct.create('div', {
				dojoType: 'SampleThinger'
			}, 'parsertest');

			parser.parse('parsertest', { noStart: true, rootNode: 'parserTest' });
			assert.isFalse(started, 'second started check');
		},

		// test the varios iterations of parser test
		// TODO: the following test doesn't acutally test anything because
		// the parser is calling dojo/query's function instead of dojo.query
		// This also causes other tests to fail because the aspect call replaces
		// dojo.query which has functions on it (like dojo.query.matches)
		/*'root test': function () {
			var tmp = aspect.after(dojo, 'query', function (sel, root) {
				assert.equal(root, 'parsertest2');
			});

			parser.parse('parsertest2');
			parser.parse({ rootNode: 'parsertest2' });
			parser.parse('parsertest2', { noStart: true });
			tmp.remove();
		},*/

		// Test that when BorderContainer etc. extends _Widget,
		// parser is aware of the new parameters added (to _Widget
		// and all of it's subclasses)
		'cache refresh': function () {
			// Add new node to be parsed, referencing a widget that the parser has already
			// dealt with (and thus cached)
			var wrapper = domConstruct.place(
				util.fixScope('<div><div ${dojo}Type="tests.parser.Class3" newParam=12345>hi</div></div>'),
				win.body(), 'last');

			try {
				// Modify Class3's superclass widget to have new parameter (thus Class3 inherits it)
				lang.extend(tests.parser.Class2, {
					newParam: 0
				});

				// Run the parser and see if it reads in newParam
				var widgets = parser.parse({ rootNode: wrapper });
				assert.lengthOf(widgets, 1);
				assert.equal(widgets[0].params.newparam || widgets[0].params.newParam, 12345);
			}
			finally {
				domConstruct.destroy(wrapper);
			}
		},

		// Test that parser recurses correctly, except when there's a stopParser flag not to
		/* global container1, contained1, container2, contained2 */
		'recurse': function () {
			assert.isDefined(container1, 'normal container created');
			assert.isDefined(container1.incr, 'script tag works too');
			assert.isDefined(contained1, 'child widget also created');
			assert.isDefined(contained2, 'child widget 2 also created');

			assert.isDefined(container2, 'shielded container created');
			assert.isDefined(container2.incr, 'script tag works too');
			assert.isUndefined(window.contained3, 'child widget not created');
			assert.isUndefined(window.contained4, 'child widget 2 not created');
		},

		/* global html5simple, html5simple2 */
		'simple HTML5': function () {
			assert.isObject(html5simple, 'data-dojo-id export');
			assert.isObject(html5simple2, 'data-dojo-id export');

			assert.isTrue(html5simple.simple, 'default respecified in props=""');
			assert.isFalse(html5simple2.simple, 'default overridden by props=""');

			// test data-dojo-props='simple:false, a:1, b:'two', c:[1,2,3], d:function(){ return this; }, e:{ f:'g' }'
			var it = html5simple2;
			assert.equal(it.a, 1, 'number in param');
			assert.equal(it.b, 'two', 'string in param');
			assert.isArray(it.c, 'array in param');
			assert.lengthOf(it.c, 3, 'array sanity');
			assert.equal(it.e.f, 'g', 'nested object with string');

			// test the function
			assert.equal(it.d(), it, 'simple \'return this\' function');
		},

		/* global html5simple3 */
		'HTML5 inherited': function () {
			assert.isObject(html5simple3);
			var val = html5simple3.afn();
			assert.equal(val, html5simple3.a * 2, 'afn() overrides default but calls inherited');
		},

		/* global htmldojomethod */
		'HTML5 with method': function () {
			// testing data-dojo-event and data-dojo-args support for dojo/method and dojo/connect
			assert.isObject(htmldojomethod);
			assert.isTrue(htmldojomethod._methodRan, 'plain dojo/method ran');

			var x = htmldojomethod.someMethod(2, 2);
			assert.equal(x, 14, 'overridden dojo/method');

			htmldojomethod.diffMethod(2);
			assert.isTrue(htmldojomethod._ran, 'ensures original was called first');
			assert.equal(htmldojomethod._fromvalue, 2, 'ensures connected was executed in scope');
		},

		/* global objOnWatch */
		'test watch': function () {
			// testing script-type dojo/watch and dojo/on
			assert.isObject(objOnWatch);
			objOnWatch.set('strProp1', 'newValue1');
			assert.equal(objOnWatch.arrProp.newValue, 'newValue1', 'ensures watch executed');

			objOnWatch.onclick();
			assert.isTrue(objOnWatch.prototypeOnclick, 'ensures original was called');
			assert.isTrue(objOnWatch.boolProp1, 'ensure on executed in scope');
		},

		/* global on_form */
		'on': function () {
			/*jshint camelcase:false*/
			// testing script-type dojo/on, when script comes after another element
			parser.parse('on');
			assert.property(window, 'on_form', 'widget created');
			on_form.emit('click');
			assert.isTrue(on_form.clicked, 'on callback fired');
		},


		/* global objAspect */
		'aspect': function () {
			// testing script-type dojo/aspect
			assert.isObject(objAspect);
			assert.isFalse(objAspect.method1ran, 'ensures method unfired');
			assert.isFalse(objAspect.method2ran, 'ensures method unfired');
			assert.equal(objAspect.method3result, '', 'ensures method unfired');
			assert.isFalse(objAspect.method4ran, 'ensures method unfired');

			objAspect.method1();
			objAspect.method2();
			objAspect.method3('something');
			objAspect.method4();

			assert.isTrue(objAspect.method1ran, 'method fired');
			assert.isTrue(objAspect.method1after, 'after advice fired');
			assert.isTrue(objAspect.method2ran, 'method fired');
			assert.isTrue(objAspect.method2before, 'around before advice fired');
			assert.isTrue(objAspect.method2after, 'around after advice fired');
			assert.equal(objAspect.method3result, 'before', 'before argument passed');
			assert.isTrue(objAspect.method4ran, 'method fired');
			assert.isTrue(objAspect.method4after, 'after advice fired');
		},

		/* global objAMDWidget */
		'mid': function () {
			// testing specifying data-dojo-type as mid
			assert.isObject(objAMDWidget);
			assert.equal(objAMDWidget.params.value, 'Value1', 'ensure object was properly parsed using MID');
		}
	});

	registerSuite({
		name: 'bidi',

		setup: setup('main'),

		teardown: teardown,

		// Test that dir=rtl or dir=ltr setting trickles down from root node
		/* global setRtl, inheritRtl, inheritRtl2, inheritLtr, setLtr */
		'dir attribute': function () {
			parser.parse('dirSection1');
			parser.parse('dirSection2');
			assert.equal(setRtl.dir, 'rtl', 'direct setting of dir=rtl works');
			assert.equal(inheritRtl.dir, 'rtl', 'inherited rtl works');
			assert.equal(inheritLtr.dir, 'ltr', 'inherited ltr works (closest ancestor wins)');
			assert.equal(inheritRtl2.dir, 'rtl', 'inherited rtl works, from grandparent');
			assert.equal(setLtr.dir, 'ltr', 'direct setting of dir=ltr overrides inherited RTL');
		},

		/* global noLang, inheritedLang, specifiedLang */
		'lang attribute': function () {
			parser.parse('langSection');
			assert.notProperty(noLang.params, 'lang', 'no lang');
			assert.equal(inheritedLang.lang, 'it_it', 'inherited lang works');
			assert.equal(specifiedLang.lang, 'en_us', 'direct setting of lang overrides inherited');
		},

		/* global noTextdir, inheritedTextdir, specifiedTextdir */
		'textDir attribute': function () {
			parser.parse('textDirSection');
			assert.notProperty(noTextdir.params, 'textDir', 'no textdir');
			assert.equal(inheritedTextdir.textDir, 'rtl', 'inherited textdir works');
			assert.equal(specifiedTextdir.textDir, 'ltr', 'direct setting of textdir overrides inherited');
		},

		'inheritance from HTML': function () {
			// Test that calling parser.parse(nodeX) will inherit dir/lang/etc. settings
			// even from <html>

			var textdirAttr = util.fixScope('data-${dojo}-textdir');
			var attrs = { dir: 'rtl', lang: 'ja-jp' };
			attrs[textdirAttr] = 'auto';
			domAttr.set(win.doc.documentElement, attrs);
			parser.parse('bidiInheritanceFromHtml');

			/* global inheritedFromHtml */
			assert.equal(inheritedFromHtml.params.dir, 'rtl', 'dir');
			assert.equal(inheritedFromHtml.params.lang, 'ja-jp', 'lang');
			assert.equal(inheritedFromHtml.params.textDir, 'auto', 'textDir');

			// teardown
			array.forEach(['dir', 'lang', textdirAttr], function (attr) {
				win.doc.documentElement.removeAttribute(attr);
			});
		}
	});

	registerSuite({
		name: 'IE Attribute Detection',

		setup: setup('main'),

		teardown: teardown,

		'input1': function () {
			var widgets = parser.instantiate([dom.byId('ieInput1')]);
			var params = widgets[0].params;

			assert.equal(params.type, 'checkbox', 'type');
			assert.isTrue(params.disabled, 'disabled');
			assert.isTrue(params.checked, 'checked');
			assert.isTrue(params.readonly, 'readonly');
			assert.equal(params.foo, 'bar', 'foo');
			assert.equal(params.bar, 'zaz', 'bar');
			assert.equal(params.bob, 'escaped\"dq', 'bob');
			assert.equal(params.frank, 'escaped\'sq', 'frank');
			//assert.isFalse('value' in params, 'value not specified');	// fails in IE8, thinks value=='on'
		},

		'input2': function () {
			var widgets = parser.instantiate([dom.byId('ieInput2')]);
			var params = widgets[0].params;

			assert.notProperty(params, 'type', 'type');
			assert.notProperty(params, 'name', 'name');
			assert.notProperty(params, 'value', 'value');
			assert.notProperty(params, 'data-dojo-type', 'data-dojo-type');
			assert.notProperty(params, 'data-dojo-props', 'data-dojo-props');
			assert.equal(params.foo, 'hi', 'foo');
			assert.notProperty(params, 'value', 'value not specified');
		},

		'input3': function () {
			var widgets = parser.instantiate([dom.byId('ieInput3')]);
			var params = widgets[0].params;

			assert.equal(params.type, 'password', 'type');
			assert.equal(params.name, 'test', 'name');
			assert.equal(params.value, '123', 'value');
			assert.equal(params['class'], 'myClass', 'class');
			assert.equal(params.style.replace(/[ ;]/g, '').toLowerCase(), 'display:block', 'style');
			assert.equal(params.tabIndex, '3', 'tabIndex');
		},

		'textarea': function () {
			var widgets = parser.instantiate([dom.byId('ieTextarea')]);
			var params = widgets[0].params;

			assert.equal(params.value, 'attrVal', 'value');
		},

		'button1': function () {
			var widgets = parser.instantiate([dom.byId('ieButton1')]);
			var params = widgets[0].params;

			assert.isTrue(params.checked, 'checked');
			assert.equal(params.value, 'button1val', 'value');
		},

		'button2': function () {
			var widgets = parser.instantiate([dom.byId('ieButton2')]);
			var params = widgets[0].params;
			assert.notProperty(params, 'checked', 'checked');
			assert.notProperty(params, 'value', 'value');
		},

		'button3': function () {
			var widgets = parser.instantiate([dom.byId('ieButton3')]);
			var params = widgets[0].params;
			assert.isTrue(params.checked, 'checked');
		},

		'button4': function () {
			var widgets = parser.instantiate([dom.byId('ieButton4')]);
			var params = widgets[0].params;
			assert.notProperty(params, 'checked');
		},

		'form1': function () {
			var widgets = parser.instantiate([dom.byId('ieForm1')]);
			var params = widgets[0].params;

			assert.equal(params.encType, 'foo', 'encType is specified');
		},

		'form2': function () {
			var widgets = parser.instantiate([dom.byId('ieForm2')]);
			var params = widgets[0].params;

			assert.notProperty(params, 'encType', 'encType not specified');
		},

		'li': function () {
			var widgets = parser.instantiate([dom.byId('li')]);
			var params = widgets[0].params;
			assert.equal(params.value, 'home');
		}
	});

	registerSuite({
		name: 'mixed attribute specification',

		setup: setup('main'),

		teardown: teardown,

		/* global mixedObj */
		'mixed': function () {
			parser.parse(dom.byId('mixedContainer'));
			assert.isObject(mixedObj, 'widget created');
			assert.equal(mixedObj.value, 'mixedValue', 'native attribute');
			assert.equal(mixedObj.custom1, 999, 'data-dojo-props attribute');
			assert.equal(mixedObj.title, 'custom title', 'data-dojo-props overrides native');
		}
	});

	registerSuite({
		name: 'functions',

		setup: setup('main'),

		teardown: teardown,

		'onclick': function () {
			declare('tests.parser.Button', null, {
				onClick: function () {
					console.log('prototype click');
				},
				constructor: function (args, node) {
					lang.mixin(this, args);
					this.domNode = node;
					aspect.after(this.domNode, 'onclick', lang.hitch(this, 'onClick'));
				}
			});

			buttonClicked = function () {
				console.log('markup click');
			};	// markup says onClick='buttonClicked'

			// Parse markup inside 'functions' div
			parser.parse('functions');

			// Should have created an instance called 'button' where button.onClick == buttonClicked
			/* global button, buttonClicked */
			assert.isObject(button, 'widget created');
			assert.isFunction(button.onClick, 'created as function');
			assert.isTrue(buttonClicked === button.onClick, 'points to specified function');
		}
	});

	registerSuite({
		name: 'parser.construct()',

		setup: setup('main'),

		teardown: teardown,

		/* global objC1, objC2 */
		'construct1': function () {
			// var nodes = [dom.byId('objC1'), dom.byId('objC2')];

			parser.construct(tests.parser.Class1, dom.byId('objC1'));
			assert.isObject(objC1, 'widget 1 created');
			assert.equal(objC1.intProp, 5, 'objC1.intProp');

			parser.construct(tests.parser.Class1, dom.byId('objC2'));
			assert.isObject(objC2, 'widget 2 created');
			assert.equal(objC2.intProp, 5, 'objC2.intProp');
		}
	});

	registerSuite({
		name: 'data-dojo-mixins support',

		setup: setup('mixins', true),

		teardown: teardown,

		'mixins': function () {
			/* global resultMixins1, resultMixins2 */
			assert.ok(resultMixins1, 'object using data-dojo-mixins created from an already parsed type');
			assert.isTrue(resultMixins1.mixin1Done, 'mixin1 correctly mixed in');
			assert.isTrue(resultMixins1.mixin2Done, 'mixin2 correctly mixed in');
			assert.isTrue(resultMixins1.amdMixinDone, 'amd mixin correctly mixed in');
			assert.ok(resultMixins2, 'object using data-dojo-mixins created from a non parsed type');
			assert.isTrue(resultMixins2.classDone, 'class correctly created');
			assert.isTrue(resultMixins2.mixin1Done, 'mixin1 correctly mixed in');
			assert.isTrue(resultMixins2.mixin2Done, 'mixin2 correctly mixed in');
			assert.isTrue(resultMixins2.amdMixinDone, 'amd mixin correctly mixed in');

			/* global resultNonDojoMixin */
			assert.isTrue(resultNonDojoMixin.expectedClass, 'correct class is returned for composeJS mixin');
			assert.equal(resultNonDojoMixin.params.length, 2, 'correct # of params were passed to compose JS');
			assert.equal(resultNonDojoMixin.params[0], tests.parser.Mixin1, 'correct param 1');
			assert.equal(resultNonDojoMixin.params[1], tests.parser.Mixin2, 'correct param 2');
		}
	});

	registerSuite({
		name: 'behavorial',

		setup: setup('main'),

		teardown: teardown,

		'doubleConnect': function () {
			// Class used in 'behavioral' <div>
			/* global Behavioral1:true */
			Behavioral1 = declare(null, {
				constructor: function (params, node) {
					on(node, 'click', lang.hitch(this, 'onClick'));
					if (typeof params.onClick !== 'function') {
						throw new Error('onClick not passed to constructor');
					}
					lang.mixin(this, params);
				},
				onClick: function () {
					console.log('original onnClick handler');
				},
				foo: ''
			});

			parser.parse('behavioral');

			// Setup global accessed by Behavioral1.onclick handler
			behavioralClickCounter = 0;

			// Trigger click event, and make sure that handler was only called once.
			on.emit(dom.byId('bh1'), 'click', {bubbles: true, cancelable: true});

			/* global behavioralClickCounter */
			assert.equal(behavioralClickCounter, 1, 'one click event processed');

			assert.equal(dom.byId('bh1').getAttribute('foo'), 'bar', 'foo attribute not removed from widget DOMNode');
		}
	});

	registerSuite({
		name: 'script type=dojo/require support',

		setup: setup('main'),

		teardown: teardown,

		'declarativeRequire': function () {
			var td = this.async();

			parser.parse('declarativeRequire').then(td.callback(function () {
				/* global dr1, dr2, dr3, dr4, dr5 */
				assert.isObject(dr1, 'object using MID mapped to return var');
				assert.equal(dr1.params.foo, 'bar', 'parameters set on instantiation');
				assert.isObject(dr2, 'object using MID mapped to return var');
				assert.equal(dr2.params.foo, 'bar', 'parameters set on instantiation');
				assert.isObject(dr3, 'object using fully required');
				assert.equal(dr3.params.foo, 'bar', 'parameters set on instantiation');
				assert.equal(dr4.params.foo, 2, 'module loaded and executed');
				assert.equal(dr5.method1(1), 3, 'declarative script has access to parser scope');
			}));
		},

		'contextRequire': function () {
			var td = this.async();

			parser.parse('contextRequire', {
				contextRequire: require
			}).then(td.callback(function () {
				/* global cr1, cr2, cr3, cr4 */
				assert.isObject(cr1, 'object using relative MID mapped to return var');
				assert.equal(cr1.params.foo, 'bar', 'parameters set on instantiation');
				assert.isObject(cr2, 'object using relative MID mapped to return var');
				assert.equal(cr2.params.foo, 'bar', 'parameters set on instantiation');
				assert.isObject(cr3, 'object using relative MID mapped to return var');
				assert.equal(cr3.params.foo, 'bar', 'parameters set on instantiation');
				assert.isObject(cr4, 'object using relative MID mapped to return var');
				assert.equal(cr4.params.foo, 'bar', 'parameters set on instantiation');
			}));
		}
	});

	registerSuite({
		name: 'promise error handling support',

		setup: setup('main'),

		teardown: teardown,

		'asyncError': function () {
			var td = this.async();

			parser.parse('errorHandling').then(td.rejectOnError(function () {
				throw new Error('shouldn\'t get here');
			}), td.callback(function (e) {
				assert.equal(typeof e, 'object', 'error object returned');
			}));
		},

		'missingCtor': function () {
			var td = this.async();

			parser.parse('missingCtor').then(td.rejectOnError(function () {
				throw new Error('shouldn\'t get here');
			}), td.callback(function (e) {
				assert.equal(typeof e, 'object', 'error object returned');
				assert.equal(e.toString(),
					'Error: Unable to resolve constructor for: \'some.type\'', 'proper error value returned');
			}));
		}
	});
});

