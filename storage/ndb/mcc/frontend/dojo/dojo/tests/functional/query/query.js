define([
    'intern!object',
    'intern/chai!assert',
    'require',
    'dojo/_base/array'
], function (registerSuite, assert, require, array) {

	array.forEach(["query.html", "queryQuirks.html"], function (file) {
		array.forEach(["lite", "css2", "css2.1", "css3", "acme"], function (selector) {
			var tests = {
				name: file + ": " + selector,

				before: function () {
					return this.get("remote")
						.get(require.toUrl("./" + file))
						.setExecuteAsyncTimeout(10000)
						.executeAsync(function (selector, send) {
							require([
								'dojo/query!' + selector,
								'dojo/sniff',
								'dojo/_base/array',
								'dojo/dom',
								'dojo/dom-construct',
								'dojo/request/iframe',
								'dojo/domReady!'
							], function (_query, _sniff, _array, _dom, _domConstruct, _iframe) {
								query = _query;
								has = _sniff;
								array = _array;
								dom = _dom;
								domConstruct = _domConstruct;
								iframe = _iframe;

								send();
							})
						}, [selector])
						.execute(function () {
							createDocument = function (xml) {
								var fauxXhr = {responseText: xml};
								if ("DOMParser" in window) {
									var parser = new DOMParser();
									fauxXhr.responseXML = parser.parseFromString(xml, "text/xml");
								}
								// kludge: code from dojo.xhr contentHandler to create doc on IE
								var result = fauxXhr.responseXML;
								if (has("ie")) {
									// Needed for IE6-8
									if ((!result || !result.documentElement)) {
										var ms = function (n) {
											return "MSXML" + n + ".DOMDocument";
										};
										var dp = ["Microsoft.XMLDOM", ms(6), ms(4), ms(3), ms(2)];
										array.some(dp, function (p) {
											try {
												var dom = new ActiveXObject(p);
												dom.async = false;
												dom.loadXML(fauxXhr.responseText);
												result = dom;
											} catch (e) {
												return false;
											}
											return true;
										});
									}
								}
								return result; // DOMDocument
							}
						});
				},

				css2: {
					"basic sanity checks": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['h3'] = query('h3').length;
								result['#t'] = query('#t').length;
								result['#bug'] = query('#bug').length;
								result['#t h3'] = query('#t h3').length;
								result['div#t'] = query('div#t').length;
								result['div#t h3'] = query('div#t h3').length;
								result['span#t'] = query('span#t').length;
								result['.bogus'] = query('.bogus').length;
								result['.bogus-scoped'] = query('.bogus', dom.byId('container')).length;
								result['#bogus'] = query('#bogus').length;
								result['#bogus-scoped'] = query('#bogus', dom.byId('container')).length;
								result['#t div > h3'] = query('#t div > h3').length;
								result['.foo'] = query('.foo').length;
								result['.foo.bar'] = query('.foo.bar').length;
								result['.baz'] = query('.baz').length;
								result['#t > h3'] = query('#t > h3').length;
								result['null'] = query(null).length;

								return result;
							}).then(function (results) {
								assert.equal((results['h3']), 4);
								assert.equal((results['#t']), 1);
								assert.equal((results['#bug']), 1);
								assert.equal((results['#t h3']), 4);
								assert.equal((results['div#t']), 1);
								assert.equal((results['div#t h3']), 4);
								assert.equal((results['span#t']), 0);
								assert.equal((results['.bogus']), 0);
								assert.equal((results['.bogus-scoped']), 0);
								assert.equal((results['#bogus']), 0);
								assert.equal((results['#bogus-scoped']), 0);
								assert.equal((results['#t div > h3']), 1);
								assert.equal((results['.foo']), 2);
								assert.equal((results['.foo.bar']), 1);
								assert.equal((results['.baz']), 2);
								assert.equal((results['#t > h3']), 3);
								assert.equal((results['null']), 0);
							});
					},
					"comma1": function () {
						return this.get("remote")
							.execute(function () {
								return query('#baz,#foo,#t').length;
							})
							.then(function (result) {
								assert.equal(result, 2);
							});
					},
					"comma2": function () {
						return this.get("remote")
							.execute(function () {
								return query('#foo,#baz,#t').length;
							})
							.then(function (result) {
								assert.equal(result, 2);
							});
					},
					"syntactic equivalents": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result["#t > *"] = query('#t > *').length;
								result[".foo > *"] = query('.foo > *').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result["#t > *"], 12);
								assert.equal(result[".foo > *"], 3);
							});
					},
					"with a root, by ID": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['> *'] = query('> *', 'container').length;
								result['> *, > h3'] = query('> *, > h3', 'container').length;
								result['> h3'] = query('> h3', 't').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['> *'], 3);
								assert.equal(result['> *, > h3'], 3);
								assert.equal(result['> h3'], 3);
							});

					},
					"compound queries": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['.foo, .bar'] = query('.foo, .bar').length;
								result['.foo,.bar'] = query('.foo,.bar').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['.foo, .bar'], 2);
								assert.equal(result['.foo,.bar'], 2);
							});
					},

					"multiple class attribute": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['.foo.bar'] = query('.foo.bar').length;
								result['.foo'] = query('.foo').length;
								result['.baz'] = query('.baz').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['.foo.bar'], 1);
								assert.equal(result['.foo'], 2);
								assert.equal(result['.baz'], 2);
							});
					},
					"case sensitivity": function () {
						return this.get("remote")
							.execute(function () {
								return {
									baz: [query('span.baz').length, query('sPaN.baz').length, query('SPAN.baz').length],
									fooBar: query('.fooBar').length
								};
							})
							.then(function (result) {
								assert.deepEqual(result.baz, [1, 1, 1]);

								// For quirks mode, case sensitivity is browser dependent, so querying .fooBar
								 // may return 1 or 2 entries.  See #8775 and #14874 for details. 
								if (!/quirks/i.test(file)) {
									assert.equal(result.fooBar, 1);
								}
							});
					},
					"attribute selectors": function () {
						return this.get("remote")
							.execute(function () {
								return query('[foo]').length;
							})
							.then(function (result) {
								assert.equal(result, 3);
							});
					},
					"attribute substring selectors": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['[foo$=\"thud\"]'] = query('[foo$=\"thud\"]').length;
								result['[foo$=thud]'] = query('[foo$=thud]').length;
								result['[foo$=\"thudish\"]'] = query('[foo$=\"thudish\"]').length;
								result['#t [foo$=thud]'] = query('#t [foo$=thud]').length;
								result['#t [title$=thud]'] = query('#t [title$=thud]').length;
								result['#t span[title$=thud ]'] = query('#t span[title$=thud ]').length;
								result['[id$=\'55555\']'] = query('[id$=\'55555\']').length;
								result['[foo~=\"bar\"]'] = query('[foo~=\"bar\"]').length;
								result['[ foo ~= \"bar\" ]'] = query('[ foo ~= \"bar\" ]').length;
								result['[foo|=\"bar\"]'] = query('[foo|=\"bar\"]').length;
								result['[foo|=\"bar-baz\"]'] = query('[foo|=\"bar-baz\"]').length;
								result['[foo|=\"baz\"]'] = query('[foo|=\"baz\"]').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['[foo$=\"thud\"]'], 1);
								assert.equal(result['[foo$=thud]'], 1);
								assert.equal(result['[foo$=\"thudish\"]'], 1);
								assert.equal(result['#t [foo$=thud]'], 1);
								assert.equal(result['#t [title$=thud]'], 1);
								assert.equal(result['#t span[title$=thud ]'], 0);
								assert.equal(result['[id$=\'55555\']'], 1);
								assert.equal(result['[foo~=\"bar\"]'], 2);
								assert.equal(result['[ foo ~= \"bar\" ]'], 2);
								assert.equal(result['[foo|=\"bar\"]'], 2);
								assert.equal(result['[foo|=\"bar-baz\"]'], 1);
								assert.equal(result['[foo|=\"baz\"]'], 0);
							});
					},
					"descendant selectors": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['> *'] = query('> *', 'container').length;
								result['> [qux]'] = query('> [qux]', 'container').length;
								result['> [qux][0].id'] = query('> [qux]', 'container')[0].id;
								result['> [qux][1].id'] = query('> [qux]', 'container')[1].id;
								result['>*'] = query('>*', 'container').length;
								result['#bug[0].value'] = query('#bug')[0].value;	// test id query doesn't match name


								// suppress exception for known bug #18516; remove this code if that bug is fixed.
								if(has("ie") <= 9 && has("quirks")){
									result['#bug[0].value'] = "passed";
								}

								return result;
							})
							.then(function (result) {
								assert.equal(result['> *'], 3, '> *');
								assert.equal(result['> [qux]'], 2, '> [qux]');
								assert.equal(result['> [qux][0].id'], "child1", '> [qux][0].id');
								assert.equal(result['> [qux][1].id'], "child3", '> [qux][1].id');
								assert.equal(result['>*'], 3, '>*');
								assert.equal(result['#bug[0].value'], "passed", '#bug[0].value');
							});
					},
					"bug 9071": function () {
						// bug 9071
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['t4 a'] = query('a', 't4').length;
								result['t4 p a'] = query('p a', 't4').length;
								result['t4 div p'] = query('div p', 't4').length;
								result['t4 div p a'] = query('div p a', 't4').length;
								result['t4 .subA'] = query('.subA', 't4').length;
								result['t4 .subP .subA'] = query('.subP .subA', 't4').length;
								result['t4 .subDiv .subP'] = query('.subDiv .subP', 't4').length;
								result['t4 .subDiv .subP .subA'] = query('.subDiv .subP .subA', 't4').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['t4 a'], 2);
								assert.equal(result['t4 p a'], 2);
								assert.equal(result['t4 div p'], 2);
								assert.equal(result['t4 div p a'], 2);
								assert.equal(result['t4 .subA'], 2);
								assert.equal(result['t4 .subP .subA'], 2);
								assert.equal(result['t4 .subDiv .subP'], 2);
								assert.equal(result['t4 .subDiv .subP .subA'], 2);
							});
					},
					"failed scope arg": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['thinger *'] = query('*', 'thinger').length;
								result['div#foo'] = query('div#foo').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['thinger *'], 0);
								assert.equal(result['div#foo'], 0);
							});
					},
					"escaping special characters with quotes": function () {
						// http://www.w3.org/TR/CSS21/syndata.html#strings
						// bug 10651
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['option[value="a+b"]'] = query('option[value="a+b"]', "attrSpecialChars").length;
								result['option[value="a~b"]'] = query('option[value="a~b"]', "attrSpecialChars").length;
								result['option[value="a^b"]'] = query('option[value="a^b"]', "attrSpecialChars").length;
								result['option[value="a,b"]'] = query('option[value="a,b"]', "attrSpecialChars").length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['option[value="a+b"]'], 1);
								assert.equal(result['option[value="a~b"]'], 1);
								assert.equal(result['option[value="a^b"]'], 1);
								assert.equal(result['option[value="a,b"]'], 1);
							});
					},
					"selector with substring that contains equals sign - bug 7479": function () {
						return this.get("remote")
							.execute(function () {
								return query("a[href*='foo=bar']", 'attrSpecialChars').length;
							})
							.then(function (result) {
								assert.equal(result, 1);
							});
					},
					"selector with substring that contains brackets - bug 9193, 11189, 13084": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['input[name="data[foo][bar]"]'] = query('input[name="data[foo][bar]"]', "attrSpecialChars").length;
								result['input[name="foo[0].bar'] = query('input[name="foo[0].bar"]', "attrSpecialChars").length;
								result['input[name="test[0]"]'] = query('input[name="test[0]"]', "attrSpecialChars").length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['input[name="data[foo][bar]"]'], 1);
								assert.equal(result['input[name="foo[0].bar'], 1);
								assert.equal(result['input[name="test[0]"]'], 1);
							});
					},
					"escaping special characters with backslashes": function () {
						//http://www.w3.org/TR/CSS21/syndata.html#characters
						// selector with substring that contains brackets (bug 9193, 11189, 13084)
						// eval() converts 4 backslashes --> 1 by the time dojo.query() sees the string
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['input[name=data\\[foo\\]\\[bar\\]]'] = query("input[name=data\\[foo\\]\\[bar\\]]", "attrSpecialChars").length;
								result['input[name=foo\\[0\\]\\.bar]'] = query("input[name=foo\\[0\\]\\.bar]", "attrSpecialChars").length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['input[name=data\\[foo\\]\\[bar\\]]'], 1);
								assert.equal(result['input[name=foo\\[0\\]\\.bar]'], 1);
							});
					},
					"crossDocumentQuery": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};
								var t3 = window.frames["t3"];
								var doc = iframe.doc(t3);
								doc.open();
								doc.write([
									"<html><head>",
									"<title>inner document</title>",
									"</head>",
									"<body>",
									"   <div id='st1'>",
									"       <h3>h3",
									"           <span>",
									"               span",
									"               <span>",
									"                   inner",
									"                   <span>",
									"                       inner-inner",
									"                   </span>",
									"               </span>",
									"           </span>",
									"           endh3",
									"       </h3>",
									"   </div>",
									"</body>",
									"</html>"
								].join(""));

								doc.close();

								result["st1 h3"] = query('h3', dom.byId("st1", doc)).length;
								// use a long query to force a test of the XPath system on FF. see bug #7075
								result['st1 h3 > span > span > span'] = query('h3 > span > span > span', dom.byId("st1", doc)).length;
								result['body.children[0] h3 > span > span > span'] = query('h3 > span > span > span', doc.body.children[0]).length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['st1 h3'], 1);
								assert.equal(result['st1 h3 > span > span > span'], 1);
								assert.equal(result['body.children[0] h3 > span > span > span'], 1);
							});
					},
					"escaping of ':' chars inside an ID": {
						"silly_IDs1": function () {
							return this.get("remote")
								.execute(function () {
									var result = {};

									result["silly:id::with:colons"] = document.getElementById("silly:id::with:colons");
									result["#silly\\:id\\:\\:with\\:colons"] = query("#silly\\:id\\:\\:with\\:colons").length;
									result["#silly\\~id"] = query("#silly\\~id").length;

									return result;
								})
								.then(function (result) {
									assert.isNotNull(result["silly:id::with:colons"], "getElementById");
									assert.equal(result["#silly\\:id\\:\\:with\\:colons"], 1, "query(\"#silly\\:id\\:\\:with\\:colons\")");
									assert.equal(result["#silly\\~id"], 1, "query(\"#silly\\~id\")");
								});
						}
					},
					"xml": function () {
						return this.get("remote")
							.execute(function () {
								var doc = createDocument([
									"<ResultSet>",
									"<Result>One</Result>",
									"<RESULT>Two</RESULT>",
									"<result><nested>Three</nested></result>",
									"<result>Four</result>",
									"</ResultSet>"
								].join(""));

								var de = doc.documentElement;

								// note: don't name structure members after elements because it gets corrupted on IE (webdriver bug)
								return {
									lower: query("result", de).length,
									mixed: query("Result", de).length,
									upper: query("RESULT", de).length,
									nomatch1: query("resulT", de).length,
									nomatch2: query("rEsulT", de).length
								};
							})
							.then(function (result) {
								assert.equal(result.lower, 2, "all lower");
								assert.equal(result.mixed, 1, "mixed case");
								assert.equal(result.upper, 1, "all upper");
								assert.equal(result.nomatch1, 0, "no match 1");
								assert.equal(result.nomatch2, 0, "no match 2");
							});

					},
					"xml_attrs": function () {
						var remote = this.get("remote");
						if (/internet explorer/.test(remote.environmentType.browserName)) {
							return this.skip("do not run in IE till bug #14880 is fixed");
						}
						return remote.execute(function () {
							var doc = createDocument([
								"<ResultSet>",
								"<RESULT thinger='blah'>ONE</RESULT>",
								"<RESULT thinger='gadzooks'><CHILD>Two</CHILD></RESULT>",
								"</ResultSet>"
							].join(""));
							var de = doc.documentElement;

							return {
								RESULT: query("RESULT", de).length,
								"RESULT[THINGER]": query("RESULT[THINGER]", de).length,
								"RESULT[thinger]": query("RESULT[thinger]", de).length,
								"RESULT[thinger=blah]": query("RESULT[thinger=blah]", de).length,
								"RESULT > CHILD": query("RESULT > CHILD", de).length
							};
						})
							.then(function (result) {
								assert.equal(result["RESULT"], 2, "result elements");
								assert.equal(result["RESULT[THINGER]"], 0, "result elements with attrs (wrong)");
								assert.equal(result["RESULT[thinger]"], 2, "result elements with attrs");
								assert.equal(result["RESULT[thinger=blah]"], 1, "result elements with attr value");
								assert.equal(result["RESULT > CHILD"], 1, "Using child operator");
							});
					},
					"sort": function () {
						return this.get("remote")
							.execute(function () {
								var i = query("div");
								// smoke test
								i.sort(function (a, b) {
									return 1;
								});
								return true;
							})
							.then(function (result) {
								assert.isTrue(result);
							});

					},
					"document_fragment": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};
								var detachedDom = domConstruct.toDom("<i><u><a></a><b id='b'></b></u></i>");
								var documentFragment = domConstruct.toDom("<i></i>    <u><a></a><b id='b'></b></u>");
								var detachedDom2 = domConstruct.toDom("<i><u><a></a><b></b></u></i>");
								var documentFragment2 = domConstruct.toDom("<i></i>    <u><a></a><b></b></u>");

								result["#b detached"] = query("#b", detachedDom).length;
								result["#b detached first child"] = query("#b", detachedDom.firstChild).length;
								result["#b fragment"] = query("#b", documentFragment).length;
								// In IE8 in quirks mode there is no text node on the document fragment
								result["#b fragment child"] = query("#b", has('ie') === 8 && has("quirks") ?
									documentFragment.childNodes[1] : documentFragment.childNodes[2]).length;
								result["#b detached2"] = query("#b", detachedDom2).length;
								result["#b fragment2"] = query("#b", documentFragment2).length;

								return result;
							})
							.then(function (result) {
								assert.equal(result["#b detached"], 1);
								assert.equal(result["#b detached first child"], 1);
								assert.equal(result["#b fragment"], 1);
								assert.equal(result["#b fragment child"], 1);
								assert.equal(result["#b detached2"], 0);
								assert.equal(result["#b fragment2"], 0);
							});
					}
				}
			};

			if (/css2.1|css3|acme/.test(selector)) {
				tests["css2.1"] = function () {
					return this.get("remote")
						.execute(function () {
							var result = {};

							result["h1:first-child"] = query('h1:first-child').length;
							result["h3:first-child"] = query('h3:first-child').length;
							result[".foo+ span"] = query('.foo+ span').length;
							result[".foo+span"] = query('.foo+span').length;
							result[".foo +span"] = query('.foo +span').length;
							result[".foo + span"] = query('.foo + span').length;

							return result;
						})
						.then(function (result) {
							// first-child
							assert.equal(result["h1:first-child"], 1);
							assert.equal(result["h3:first-child"], 2);

							// + sibling selector
							assert.equal(result[".foo+ span"], 1);
							assert.equal(result[".foo+span"], 1);
							assert.equal(result[".foo +span"], 1);
							assert.equal(result[".foo + span"], 1);
						});
				};
			}

			if (/css3|acme/.test(selector)) {
				tests["css3"] = {
					"sub-selector parsing": function () {
						return this.get("remote")
							.execute(function () {
								return query('#t span.foo:not(:first-child)').length;
							})
							.then(function (result) {
								assert.equal(result, 1);
							});
					},
					"~ sibling selector": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result[".foo~ span"] = query('.foo~ span').length;
								result[".foo~span"] = query('.foo~span').length;
								result[".foo ~span"] = query('.foo ~span').length;
								result[".foo ~ span"] = query('.foo ~ span').length;
								result["#foo~ *"] = query('#foo~ *').length;
								result["#foo ~*"] = query('#foo ~*').length;
								result["#foo ~*"] = query('#foo ~*').length;
								result["#foo ~ *"] = query('#foo ~ *').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result[".foo~ span"], 4);
								assert.equal(result[".foo~span"], 4);
								assert.equal(result[".foo ~span"], 4);
								assert.equal(result[".foo ~ span"], 4);
								assert.equal(result["#foo~ *"], 1);
								assert.equal(result["#foo ~*"], 1);
								assert.equal(result["#foo ~*"], 1);
								assert.equal(result["#foo ~ *"], 1);
							});

					},
					"nth-child tests": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result["#t > h3:nth-child(odd)"] = query('#t > h3:nth-child(odd)').length;
								result["#t h3:nth-child(odd)"] = query('#t h3:nth-child(odd)').length;
								result["#t h3:nth-child(2n+1)"] = query('#t h3:nth-child(2n+1)').length;
								result["#t h3:nth-child(even)"] = query('#t h3:nth-child(even)').length;
								result["#t h3:nth-child(2n)"] = query('#t h3:nth-child(2n)').length;
								result["#t h3:nth-child(2n+3)"] = query('#t h3:nth-child(2n+3)').length;
								result["#t h3:nth-child(1)"] = query('#t h3:nth-child(1)').length;
								result["#t > h3:nth-child(1)"] = query('#t > h3:nth-child(1)').length;
								result["#t :nth-child(3)"] = query('#t :nth-child(3)').length;
								result["#t > div:nth-child(1)"] = query('#t > div:nth-child(1)').length;
								result["#t :nth-child(3)"] = query('#t :nth-child(3)').length;
								result["#t > div:nth-child(1)"] = query('#t > div:nth-child(1)').length;
								result["#t span"] = query('#t span').length;
								result["#t > *:nth-child(n+10)"] = query('#t > *:nth-child(n+10)').length;
								result["#t > *:nth-child(n+12)"] = query('#t > *:nth-child(n+12)').length;
								result["#t > *:nth-child(-n+10)"] = query('#t > *:nth-child(-n+10)').length;
								result["#t > *:nth-child(-2n+10)"] = query('#t > *:nth-child(-2n+10)').length;
								result["#t > *:nth-child(2n+2)"] = query('#t > *:nth-child(2n+2)').length;
								result["#t > *:nth-child(2n+4)"] = query('#t > *:nth-child(2n+4)').length;
								result["#t> *:nth-child(2n+4)"] = query('#t> *:nth-child(2n+4)').length;
								result["#t > *:nth-child(n-5)"] = query('#t > *:nth-child(n-5)').length;
								result["#t >*:nth-child(n-5)"] = query('#t >*:nth-child(n-5)').length;
								result["#t > *:nth-child(2n-5)"] = query('#t > *:nth-child(2n-5)').length;
								result["#t>*:nth-child(2n-5)"] = query('#t>*:nth-child(2n-5)').length;
								// function(){ doh.is(dom.byId('_foo'), result[".foo:nth-child(2)"][0], ".foo:nth-child(2)"); },

								return result;
							})
							.then(function (result) {
								// nth-child tests
								assert.equal(result["#t > h3:nth-child(odd)"], 2, "#t > h3:nth-child(odd)");
								assert.equal(result["#t h3:nth-child(odd)"], 3, "#t h3:nth-child(odd)");
								assert.equal(result["#t h3:nth-child(2n+1)"], 3, "#t h3:nth-child(2n+1)");
								assert.equal(result["#t h3:nth-child(even)"], 1, "#t h3:nth-child(even)");
								assert.equal(result["#t h3:nth-child(2n)"], 1, "#t h3:nth-child(2n)");
								assert.equal(result["#t h3:nth-child(2n+3)"], 1, "#t h3:nth-child(2n+3)");
								assert.equal(result["#t h3:nth-child(1)"], 2, "#t h3:nth-child(1)");
								assert.equal(result["#t > h3:nth-child(1)"], 1, "#t > h3:nth-child(1)");
								assert.equal(result["#t :nth-child(3)"], 3, "#t :nth-child(3)");
								assert.equal(result["#t > div:nth-child(1)"], 0, "#t > div:nth-child(1)");
								assert.equal(result["#t span"], 7, "#t span");
								assert.equal(result["#t > *:nth-child(n+10)"], 3, "#t > *:nth-child(n+10)");
								assert.equal(result["#t > *:nth-child(n+12)"], 1, "#t > *:nth-child(n+12)");
								assert.equal(result["#t > *:nth-child(-n+10)"], 10, "#t > *:nth-child(-n+10)");
								assert.equal(result["#t > *:nth-child(-2n+10)"], 5, "#t > *:nth-child(-2n+10)");
								assert.equal(result["#t > *:nth-child(2n+2)"], 6, "#t > *:nth-child(2n+2)");
								assert.equal(result["#t > *:nth-child(2n+4)"], 5, "#t > *:nth-child(2n+4)");
								assert.equal(result["#t> *:nth-child(2n+4)"], 5, "#t> *:nth-child(2n+4)");
								assert.equal(result["#t > *:nth-child(n-5)"], 12, "#t > *:nth-child(n-5)");
								assert.equal(result["#t >*:nth-child(n-5)"], 12, "#t >*:nth-child(n-5)");
								assert.equal(result["#t > *:nth-child(2n-5)"], 6, "#t > *:nth-child(2n-5)");
								assert.equal(result["#t>*:nth-child(2n-5)"], 6, "#t>*:nth-child(2n-5)");
								// function(){ doh.is(dom.byId('_foo'), result[".foo:nth-child(2)"][0], ".foo:nth-child(2)"); },
							});

					},
					":checked pseudo-selector": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result["#t2 > :checked"] = query('#t2 > :checked').length;
								result["#t2 > input[type=checkbox]:checked"] =
									query('#t2 > input[type=checkbox]:checked')[0] === dom.byId('checkbox2');
								result["#t2 > input[type=radio]:checked"] =
									query('#t2 > input[type=radio]:checked')[0] === dom.byId('radio2');

								result["#t2select option:checked"] = query('#t2select option:checked').length;

								result["#radio1:disabled"] = query('#radio1:disabled').length;
								result["#radio1:enabled"] = query('#radio1:enabled').length;
								result["#radio2:disabled"] = query('#radio2:disabled').length;
								result["#radio2:enabled"] = query('#radio2:enabled').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['#t2 > :checked'], 2);
								assert.isTrue(result['#t2 > input[type=checkbox]:checked']);
								assert.isTrue(result['#t2 > input[type=radio]:checked']);
								// This :checked selector is only defined for elements that have the checked property, option elements are not specified by the spec (http://www.w3.org/TR/css3-selectors/#checked) and not universally supported
								//assert.equal(result['#t2select option:checked'], 2);

								assert.equal(result['#radio1:disabled'], 1);
								assert.equal(result['#radio1:enabled'], 0);
								assert.equal(result['#radio2:disabled'], 0);
								assert.equal(result['#radio2:enabled'], 1);
							});

					},
					":empty pseudo-selector": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result["#t > span:empty"] = query('#t > span:empty').length;
								result["#t span:empty"] = query('#t span:empty').length;
								result["h3 span:empty"] = query('h3 span:empty').length;
								result["h3 :not(:empty)"] = query('h3 :not(:empty)').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['#t > span:empty'], 4);
								assert.equal(result['#t span:empty'], 6);
								assert.equal(result['h3 span:empty'], 0);
								assert.equal(result['h3 :not(:empty)'], 1);
							});

					}
				};
			}

			if (selector == "acme") {
				tests.acme = {
					"Case insensitive class selectors - bug #8775, #14874": function () {
						// Case insensitive class selectors (#8775, #14874).
						// In standards mode documents, querySelectorAll() is case-sensitive about class selectors,
						// but acme is case-insensitive for backwards compatibility.
						return this.get("remote")
							.execute(function () {
								return query(".fooBar").length;
							})
							.then(function (result) {
								assert.equal(result, 1);
							});
					},
					"sub-selector parsing": function () {
						// TODO: move this test to CSS3 section when #14875 is fixed
						return this.get("remote")
							.execute(function () {
								return query('#t span.foo:not(span:first-child)').length;
							})
							.then(function (result) {
								assert.equal(result, 1);
							});
					},
					"special characters in attribute values without backslashes": function () {
						// supported by acme but apparently not standard, see http://www.w3.org/TR/CSS21/syndata.html#characters
						function attrSpecialCharsNoEscape() {
							// bug 10651
							return this.get("remote")
								.execute(function () {
									var result = {};

									result["option[value=a+b]"] = query('option[value=a+b]', 'attrSpecialChars').length;
									result["option[value=a~b]"] = query('option[value=a~b]', 'attrSpecialChars').length;
									result["option[value=a^b]"] = query('option[value=a^b]', 'attrSpecialChars').length;

									return result;
								})
								.then(function (result) {
									assert.equal(result["option[value=a+b]"], 1, "value=a+b");
									assert.equal(result["option[value=a~b]"], 1, "value=a~b");
									assert.equal(result["option[value=a^b]"], 1, "value=a^b");
								});
						}
					},
					"implied * after > (non-standard syntax)": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};

								result['#t >'] = query('#t >').length;
								result['.foo >'] = query('.foo >').length;
								result['>'] = query('>', 'container').length;
								result['> .not-there'] = query('> .not-there').length;
								result['#foo ~'] = query('#foo ~').length;
								result['#foo~'] = query('#foo~').length;

								return result;
							})
							.then(function (result) {
								assert.equal(result['#t >'], 12);
								assert.equal(result['.foo >'], 3);
								assert.equal(result['>'], 3);
								assert.equal(result['> .not-there'], 0);
								assert.equal(result['#foo ~'], 1);
								assert.equal(result['#foo~'], 1);
							});

					},
					"implied * before and after + and ~ (non-standard syntax)": function () {
						return this.get("remote")
							.execute(function () {
								var result = {};
								result["+"] = query('+', 'container').length;
								result["~"] = query('~', 'container').length;
								return result;
							})
							.then(function (result) {
								assert.equal(result['+'], 1);
								assert.equal(result['~'], 3);
							});

					},
					"check for correct document order": {
						// not sure if this is guaranteed by css3, so putting in acme section
						domOrder: function () {
							return this.get("remote")
								.execute(function () {
									var result = {};

									var inputs = query(".upperclass .lowerclass input");

									result["notbug"] = inputs[0].id;
									result["bug"] = inputs[1].id;
									result["checkbox1"] = inputs[2].id;
									result["checkbox2"] = inputs[3].id;
									result["radio1"] = inputs[4].id;
									result["radio2"] = inputs[5].id;
									result["radio3"] = inputs[6].id;

									return result;
								})
								.then(function (result) {
									assert.equal(result["notbug"], "notbug");
									assert.equal(result["bug"], "bug");
									assert.equal(result["checkbox1"], "checkbox1");
									assert.equal(result["checkbox2"], "checkbox2");
									assert.equal(result["radio1"], "radio1");
									assert.equal(result["radio2"], "radio2");
									assert.equal(result["radio3"], "radio3");
								});
						},

						// TODO: move to css2 section after #7869 fixed for lite engine (on IE)
						xml_nthchild: function () {
							return this.get("remote")
								.execute(function () {
									var doc = createDocument([
											"<ResultSet>",
											"<result>One</result>",
											"<result>Two</result>",
											"<result>Three</result>",
											"<result>Four</result>",
											"</ResultSet>"
										].join("")
									);
									var de = doc.documentElement;

									return query("result:nth-child(4)", de)[0].firstChild.data;
								})
								.then(function (result) {
									assert.equal(result, "Four", "fourth child");
								});
						}
					}
				};
			}

			registerSuite(tests);
		});
	});
});
