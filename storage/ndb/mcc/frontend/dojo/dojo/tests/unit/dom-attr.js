define([
    'intern!object',
    'intern/chai!assert',
        '../../dom-attr',
        '../../dom-prop',
        '../../dom-style',
        'dojo/dom-construct',
        'dojo/sniff'

], function (registerSuite, assert, domAttr, domProp, domStyle, domConstruct, has, Deferred) {

    var attr = "data-dojo-test-attribute";
    var value = "the value";
    var node;
    var nodeIdIndex = 1;

    function generateId() {
        return "nodeid_" + nodeIdIndex++;
    }

    registerSuite({
        name: 'dojo/dom-attr',

        ".get": {
            beforeEach: function () {
                node = domConstruct.toDom("<div " +
                  attr + "='" + value + "'></div>");
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            "node + valid attribute": function () {
                //arrange

                //act
                var result = domAttr.get(node, attr);

                //assert
                assert.equal(result, value, "when a Node and valid attribute are passed, then the correct value is returned");

            },

            "string + valid attribute": function () {
                //arrange
                var nodeId = generateId();
                node.setAttribute("id", nodeId);

                //act
                var result = domAttr.get(nodeId, attr);

                //assert
                assert.equal(result, value, "when a Node and valid attribute are passed, then the correct value is returned");

            },

            "node + 'innerHTML' returns property": function () {
                //arrange
                var expected = "the expected value";
                node.innerHTML = expected;
                node.setAttribute("innerHTML", "not the expected value");

                //act
                var result = domAttr.get(node, "innerHTML");

                //assert
                assert.equal(result, expected,
                    "when an attribute and a property exist on the node, then the property is returned");

            },

            "returns correct values": function () {
                //arrange
                var testAttributes = {
                    "title": "the first value",
                    "name": "the second value",
                    "data-dojo-type": "the third value"
                };

                for (var a in testAttributes) {
                    node.setAttribute(a, testAttributes[a]);
                }

                //act
                var result = {};
                for (var a in testAttributes) {
                    result[a] = domAttr.get(node, a);
                }

                //assert
                for (var a in result) {
                    assert.equal(result[a], testAttributes[a], "when an attribute is requested, then the correct value is returned");
                }

            },

            "forcedProp": function () {
                //arrange
                var propName = "innerHTML";
                node[propName] = "the property value";

                //act
                var result = domAttr.get(node, propName);

                //assert
                assert.equal(result, node[propName], "when the requested attribute is a 'forced prop', then the correct value is returned");

            },

            "textContent": function () {

                //arrange
                var expected = "the expected result";
                node.innerHTML = expected;

                //act
                var result = domAttr.get(node, "textContent");

                //assert
                assert.equal(result, expected, "when the textContent is requested, then the result from dom-prop::get is returned");
            },

            "boolean": function () {
                //arrange
                var name = "the boolean attribute";
                var value = true;
                node[name] = value;

                //assert
                var result = domAttr.get(node, name);

                //act
                assert.strictEqual(result, value, "when the attribute's value is a boolean, then it is returned correctly");
            },

            "function": function () {
                //arrange
                var name = "the boolean attribute";
                var value = function () { };
                node[name] = value;

                //assert
                var result = domAttr.get(node, name);

                //act
                assert.strictEqual(result, value, "when the attribute's value is a function, then it is returned correctly");
            },

            "missing attribute": function () {
                //arrange
                var name = "the none existent attribute";
                node.getAttributeNode = function () { return null; };
                node[name] = undefined;

                //act
                var result = domAttr.get(node, name);

                //assert
                assert.isNull(result, "when the attribute doesn't exist, then 'null' is returned");
            }
        },

        ".set": {

            beforeEach: function () {
                node = domConstruct.toDom("<div></div>");
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            "node + name + value": function () {
                //arrange

                //act
                domAttr.set(node, attr, value);

                //assert
                assert.equal(node.getAttribute(attr), value,
                    "when a node's attribute is set, then the node is updated properly");

            },
            "string + name + value": function () {
                //arrange
                var nodeId = generateId();
                node.setAttribute("id", nodeId);

                //act
                domAttr.set(nodeId, attr, value);

                //assert
                assert.equal(node.getAttribute(attr), value,
                    "when a node's id is provided to set, then the node is updated properly");
            },
            "node + dictionary": function () {
                //arrange
                var attributes = {
                    name: "the name",
                    "data-foo": "the foo value",
                    title: "the title"
                };

                //act
                domAttr.set(node, attributes);

                //assert
                for (var a in attributes) {
                    assert.equal(attributes[a], node.getAttribute(a),
                      "when a hash of attributes is set onto a node, then the attributes are set properly");
                }
            },
            "node + forcedProp": function () {
                //arrange
                var attributeName = "class";
                var value = "the-css-class";

                //act
                domAttr.set(node, attributeName, value);

                //assert
                assert.equal(node.className, value,
                    "when the attribute is a forcedProp, then it is set properly");

            },
            "node + name + boolean": function () {
                //arrange
                var attr = "data-boolean";
                var value = true;

                //act
                domAttr.set(node, attr, value);

                //assert
                assert.equal(node[attr], value,
                    "when the attribute's value is a boolean, then it is set properly");
            },
            "node + name + function": function () {
                //arrange
                var attr = "data-function";
                var value = function () { };
                var origDomPropSet = domProp.set;
                var setNode, setName, setValue;

                domProp.set = function (node, name, value) {
                    setNode = node;
                    setName = name;
                    setValue = value;
                }

                //act
                domAttr.set(node, attr, value);

                //assert
                assert.equal(setValue, value,
                    "when the attribute's value is a function, then it is set properly");
                domProp.set = origDomPropSet;
            },

            "node + 'style' + string": function () {
                //arrange
                var style = "color: black;";
                var styleNode, styleValue;

                //act
                domAttr.set(node, "style", style);

                //assert
                assert.equal(node.getAttribute("style"), style,
                    "when the attribute is 'style' and a dictionary is set, then it is set properly");
            },

            "node + 'style' + dictionary": function () {
                //arrange
                var styles = {
                    color: "white",
                    backgroundColor: "red",
                    opacity: 0
                };
                var styleNode, styleValue;

                var origDomStyle = domStyle.set;
                domStyle.set = function (node, value) {
                    setNode = node;
                    setValue = value;
                }

                //act
                domAttr.set(node, "style", styles);

                //assert
                assert.equal(setValue, styles,
                    "when the attribute is 'style' and a dictionary is set, then it is set properly");
                domStyle.set = origDomStyle;
            }
        },

        ".remove": {
            beforeEach: function () {
                node = domConstruct.toDom("<div " +
                  attr + "='" + value + "'></div>");
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            "node + attribute": function () {
                //arrange

                //act
                domAttr.remove(node, attr);

                //assert
                assert.isNull(node.getAttribute(attr),
                    "when an attribute is removed from a node, then its value becomes 'null'");
            },
            "string + attribute": function () {
                //arrange
                var nodeId = generateId();
                node.setAttribute("id", nodeId);

                //act
                domAttr.remove(nodeId, attr);

                //assert
                assert.isNull(node.getAttribute(attr),
                    "when a node's id is used, then the requested attribute is removed");
            },
            "node + 'className'": function () {
                //arrange

                //act
                domAttr.remove(node, "className");

                //assert
                assert.isNull(node.getAttribute("class"),
                    "when a class in the 'attrNames' array, then the proper attribute is removed");
            }
        },

        ".getNodeProp": {
            beforeEach: function () {
                node = domConstruct.toDom("<div " +
                  attr + "='" + value + "'></div>");
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },

            "node + name": function () {
                //arrange

                //act
                var result = domAttr.getNodeProp(node, attr);

                //assert
                assert.equal(result, value,
                    "when an attribute is requested from a node, then the correct value is returned");

            },
            "string + name": function () {
                //arrange
                var nodeId = generateId();
                node.setAttribute("id", nodeId);

                //act
                var result = domAttr.getNodeProp(nodeId, attr);

                //assert
                assert.equal(result, value,
                    "when a node's id is passed, then the correct value is returned");
            },
            "node + 'className'": function () {
                //arrange
                var value = "the-css-class";
                node.setAttribute("class", value);

                //act
                var result = domAttr.getNodeProp(node, "className");

                //assert
                assert.equal(result, value,
                    "when 'className' is requested, then the node's class is returned");

            },
            "node + 'innerHTML' returns property": function () {
                //arrange
                var value = "the innerHTML value";
                node.innerHTML = value;
                node.setAttribute("innerHTML", "not the expected value");

                //act
                var result = domAttr.getNodeProp(node, "innerHTML");

                //assert
                assert.equal(result, value,
                    "when 'innerHTML' is requested and there is both a property and attribute with that name, " +
                   "then the node's property is returned");
            },
            "node + 'name' returns property": function () {
                //arrange
                var expected = "the value";
                var attr = "name";
                node[attr] = expected;
                node.setAttribute(attr, "not the expected value");

                //act
                var result = domAttr.getNodeProp(node, attr);

                //assert
                assert.equal(result, value,
                    "when an attribute is requested and there is both a property and attribute with that name, " +
                   "then the node's property is returned");
            }
        },

        ".has": {
            beforeEach: function () {
                node = domConstruct.toDom("<div " +
                  attr + "='" + value + "'></div>");
                domConstruct.place(node, document.body);
            },

            afterEach: function () {
                domConstruct.destroy(node);
            },
            "node + name": function () {
                //arrange

                //acct
                var result = domAttr.has(node, attr);

                //assert
                assert.isTrue(result,
                    "when an existing attribute on a node is checked for, then the call returns 'true'");
            },
            "string + name": function () {
                //arrange
                var nodeId = generateId();
                node.setAttribute("id", nodeId);

                //acct
                var result = domAttr.has(nodeId, attr);

                //assert
                assert.isTrue(result,
                    "when a node's id is provided and the requested attribute exists, then the call returns 'true'");
            },
            "node + 'innerHTML'": function () {
                //arrange
                node.innerHTML = null;

                //act
                var result = domAttr.has(node, "innerHTML");

                //assert
                assert.isTrue(!!result,
                    "when the 'innerHTML' is requested, then it always returns truthy");

            },
            "node + non existent name": function () {
                //arrange

                //acct
                var result = domAttr.has(node, "doesnotexist");

                //assert
                assert.isFalse(!!result,
                    "when an attribute that doesn't exist on a node is checked for, then the call returns 'true'");
            }
        },
        "validation tests": (function () {
            var container,
                inputNoType,
                inputWithType,
                inputNoTabindex,
                inputTabindexMinusOne,
                inputTabindexZero,
                inputTabindexOne,
                inputTextValue,
                inputNoDisabled,
                inputWithDisabled,
                inputWithDisabledTrue,
                divNoTabindex,
                divTabindexMinusOne,
                divTabindexZero,
                divTabindexOne,
                labelNoFor,
                labelWithFor,
                inputWithLabel;

            return {
                setup: function () {
                    container = domConstruct.toDom("<div></div>");
                    inputNoType = domConstruct.toDom('<input id="input-no-type">');
                    inputWithType = domConstruct.toDom('<input id="input-with-type" type="checkbox">');
                    inputNoTabindex = domConstruct.toDom('<input id="input-no-tabindex">');
                    inputTabindexMinusOne = domConstruct.toDom('<input id="input-tabindex-minus-1" tabIndex="-1">');
                    inputTabindexZero = domConstruct.toDom('<input id="input-tabindex-0" tabIndex="0">');
                    inputTabindexOne = domConstruct.toDom('<input id="input-tabindex-1" tabIndex="1">');
                    inputTextValue = domConstruct.toDom('<input id="input-text-value" type="text" value="123">');
                    inputNoDisabled = domConstruct.toDom('<input id="input-no-disabled" type="text">');
                    inputWithDisabled = domConstruct.toDom('<input id="input-with-disabled" type="text" disabled>');
                    inputWithDisabledTrue = domConstruct.toDom('<input id="input-with-disabled-true" disabled="disabled">');
                    divNoTabindex = domConstruct.toDom('<div id="div-no-tabindex"></div>');
                    divTabindexMinusOne = domConstruct.toDom('<div id="div-tabindex-minus-1" tabIndex="-1"></div>');
                    divTabindexZero = domConstruct.toDom('<div id="div-tabindex-0" tabIndex="0"></div>');
                    divTabindexOne = domConstruct.toDom('<div id="div-tabindex-1" tabIndex="1"></div>');
                    labelNoFor = domConstruct.toDom('<label id="label-no-for">label with no for </label><input type="text" id="label-test-input">');
                    labelWithFor = domConstruct.toDom('<label id="label-with-for" for="input-with-label">label with for </label>');
                    inputWithLabel = domConstruct.toDom('<input type="text" id="input-with-label">');

                    domConstruct.place(container, document.body);
                    domConstruct.place(inputNoType, container);
                    domConstruct.place(inputWithType, container);
                    domConstruct.place(inputNoTabindex, container);
                    domConstruct.place(inputTabindexMinusOne, container);
                    domConstruct.place(inputTabindexZero, container);
                    domConstruct.place(inputTabindexOne, container);
                    domConstruct.place(inputTextValue, container);
                    domConstruct.place(inputNoDisabled, container);
                    domConstruct.place(inputWithDisabled, container);
                    domConstruct.place(inputWithDisabledTrue, container);
                    domConstruct.place(divNoTabindex, container);
                    domConstruct.place(divTabindexMinusOne, container);
                    domConstruct.place(divTabindexZero, container);
                    domConstruct.place(divTabindexOne, container);
                    domConstruct.place(labelNoFor, container);
                    domConstruct.place(labelWithFor, container);
                    domConstruct.place(inputWithLabel, container);
                },
                teardown: function () {
                    domConstruct.destroy(container);
                },
                "getTypeInput": function () {
                    assert.isFalse(domAttr.has(inputNoType, "type"));
                    assert.isNull(domAttr.get(inputNoType, "type"));
                    assert.isTrue(domAttr.has(inputWithType, "type"));
                    assert.equal(domAttr.get(inputWithType, "type"), "checkbox");
                },
                "getWithString": function () {
                    assert.isFalse(domAttr.has(inputNoType.id, "type"));
                    assert.isNull(domAttr.get(inputNoType.id, "type"));
                    assert.isTrue(domAttr.has(inputWithType.id, "type"));
                    assert.equal(domAttr.get(inputWithType.id, "type"), "checkbox");
                },
                "attrId": function () {
                    assert.isTrue(domAttr.has(divNoTabindex.id, "id"));
                    assert.equal(divNoTabindex.id, domAttr.get(divNoTabindex.id, "id"));
                    var div = document.createElement("div");
                    assert.isFalse(domAttr.has(div, "id"));
                    assert.isNull(domAttr.get(div, "id"));
                    var nodeId = generateId();
                    domAttr.set(div, "id", nodeId);
                    assert.isTrue(domAttr.has(div, "id"));
                    assert.equal(domAttr.get(div, "id"), nodeId);
                    domAttr.remove(div, "id");
                    assert.isFalse(domAttr.has(div, "id"));
                    assert.isNull(domAttr.get(div, "id"));
                },
                "getTabindexDiv": function () {
                    assert.isFalse(domAttr.has(divNoTabindex, "tabIndex"));
                    assert.isTrue(domAttr.get(divNoTabindex, "tabIndex") <= 0);
                    assert.isTrue(domAttr.has(divTabindexMinusOne, "tabIndex"));
                    if (!has("opera")) {
                        // Opera (at least <= 9) does not support tabIndex="-1"
                        assert.equal(domAttr.get(divTabindexMinusOne, "tabIndex"), -1);
                    }
                    assert.isTrue(domAttr.has(divTabindexZero, "tabIndex"));
                    assert.equal(domAttr.get(divTabindexZero, "tabIndex"), 0);
                    assert.equal(domAttr.get(divTabindexOne, "tabIndex"), 1);
                },
                "getTabindexInput": function () {
                    assert.isFalse(domAttr.has("input-no-tabindex", "tabIndex"));
                    assert.isFalse(!!domAttr.get("input-no-tabindex", "tabIndex"));
                    assert.isTrue(domAttr.has("input-tabindex-minus-1", "tabIndex"));
                    if (!has("opera")) {
                        // Opera (at least <= 9) does not support tabIndex="-1"
                        assert.equal(domAttr.get("input-tabindex-minus-1", "tabIndex"), -1);
                    }
                    assert.isTrue(domAttr.has("input-tabindex-0", "tabIndex"));
                    assert.equal(domAttr.get("input-tabindex-0", "tabIndex"), 0);
                    assert.equal(domAttr.get("input-tabindex-1", "tabIndex"), 1);
                },
                "setTabindexDiv": function () {
                    var div = document.createElement("div");
                    assert.isNull(domAttr.get(div, "tabIndex"));
                    domAttr.set(div, "tabIndex", -1);
                    if (!has("opera")) {
                        // Opera (at least <= 9) does not support tabIndex="-1"
                        assert.equal(domAttr.get(div, "tabIndex"), -1);
                    }
                    domAttr.set(div, "tabIndex", 0);
                    assert.equal(domAttr.get(div, "tabIndex"), 0);
                    domAttr.set(div, "tabIndex", 1);
                    assert.equal(domAttr.get(div, "tabIndex"), 1);
                },
                "setTabindexInput": function () {
                    var input = document.createElement("input");
                    assert.isTrue(domAttr.get(input, "tabIndex") <= 0);
                    domAttr.set(input, "tabIndex", -1);
                    if (!has("opera")) {
                        // Opera (at least <= 9) does not support tabIndex="-1"
                        assert.equal(domAttr.get(input, "tabIndex"), -1);
                    }
                    domAttr.set(input, "tabIndex", 0);
                    assert.equal(domAttr.get(input, "tabIndex"), 0);
                    domAttr.set(input, "tabIndex", 1);
                    assert.equal(domAttr.get(input, "tabIndex"), 1);
                },
                "removeTabindexFromDiv": function () {
                    var div = document.createElement("div");
                    domAttr.set(div, "tabIndex", 1);
                    assert.equal(domAttr.get(div, "tabIndex"), 1);
                    domAttr.remove(div, "tabIndex");
                    assert.isNull(domAttr.get(div, "tabIndex"));
                },
                "removeDisabledFromInput": function () {
                    var input = document.createElement("input");
                    domAttr.set(input, "disabled", true);
                    assert.isTrue(domAttr.get(input, "disabled"));
                    domAttr.remove(input, "disabled");
                    assert.isFalse(domAttr.get(input, "disabled"));
                },
                "removeTabindexFromInput": function () {
                    var input = document.createElement("input");
                    domAttr.set(input, "tabIndex", 1);
                    assert.equal(domAttr.get(input, "tabIndex"), 1);
                    domAttr.remove(input, "tabIndex");
                    assert.isNull(domAttr.get(input, "tabIndex"));
                },
                "setReadonlyInput": function () {
                    var input = document.createElement("input");
                    assert.isFalse(domAttr.get(input, "readonly"));
                    domAttr.set(input, "readonly", true);
                    assert.isTrue(domAttr.get(input, "readonly"));
                    domAttr.set(input, "readonly", false);
                    assert.isFalse(domAttr.get(input, "readonly"));
                },
                "attr_map": function () {
                    var input = document.createElement("input"),
                        input2 = document.createElement("input"),
                        ctr = 0,
                        callbackFired = false,
                        map = {
                            "tabIndex": 1,
                            "type": "text",
                            "onfocus": function (e) {
                                ctr++;
                                callbackFired = true;
                            }
                        };
                    domAttr.set(input, map);
                    domAttr.set(input2, {
                        "onfocus": function (e) {
                            callbackFired = true;
                        }
                    });
                    document.body.appendChild(input);
                    document.body.appendChild(input2);
                    assert.equal(domAttr.get(input, "tabIndex"), map.tabIndex, "tabIndex");
                    assert.equal(domAttr.get(input, "type"), map.type, "type");
                    assert.equal(ctr, 0, "onfocus ctr == 0");
                    var def = this.async(1000);

                    firstCallback = def.callback(function () {
                        if (callbackFired) {
                            console.log("one");
                            assert.equal(ctr, 1, "onfocus ctr == 1");
                            callbackFired = false;
                            input2.focus();
                        } else {
                            setTimeout(firstCallback, 50); //try again in a bit
                        }
                    });

                    secondCallback = def.callback(function () {
                        if (callbackFired) {
                            console.log("two");
                            callbackFired = false;
                            input.focus();
                        } else {
                            setTimeout(secondCallback, 50); //try again in a bit
                        }
                    });
                    thirdCallback = def.callback(function () {
                        if (callbackFired) {
                            console.log("three");
                            assert.equal(ctr, 2, "onfocus ctr == 2");
                            callbackFired = false;
                        } else {
                            setTimeout(thirdCallback, 50); //try again in a bit
                        }
                    });
                    callbackFired = false;
                    firstCallback();
                    input.focus();
                },
                "attr_reconnect": function () {
                    var input = document.createElement("input"),
                        input2 = document.createElement("input"),
                        callbackFired = false;
                    var ctr = 0;
                    domAttr.set(input, "type", "text");
                    domAttr.set(input, "onfocus", function (e) { ctr++; callbackFired = true});
                    domAttr.set(input, "onfocus", function (e) { ctr++; callbackFired = true });
                    domAttr.set(input, "onfocus", function (e) { ctr++; callbackFired = true });
                    domAttr.set(input2, "onfocus", function (e) { callbackFired = true });
                    document.body.appendChild(input);
                    document.body.appendChild(input2);
                    assert.equal(domAttr.get(input, "type"), "text");
                    assert.equal(ctr, 0);
                    var def = this.async(1000);

                    firstCallback = def.callback(function () {
                        if (callbackFired) {
                            assert.equal(ctr, 1, "onfocus ctr == 1");
                            callbackFired = false;
                            input2.focus();
                        } else {
                            setTimeout(firstCallback, 50); //try again in a bit
                        }
                    });

                    secondCallback = def.callback(function () {
                        if (callbackFired) {
                            callbackFired = false;
                            input.focus();
                        } else {
                            setTimeout(secondCallback, 50); //try again in a bit
                        }
                    });
                    thirdCallback = def.callback(function () {
                        if (callbackFired) {
                            assert.equal(ctr, 2, "onfocus ctr == 2");
                            callbackFired = false;
                        } else {
                            setTimeout(thirdCallback, 50); //try again in a bit
                        }
                    });

                    callbackFired = false;
                    firstCallback();
                    input.focus();
                },
                "attrSpecials": function () {
                    var node = document.createElement("div"),
                        styleMap = {
                            opacity: 0.5,
                            width: "30px",
                            border: "1px solid black"
                        },
                        propMap = {
                            innerHTML: "howdy!"
                        };
                    document.body.appendChild(node);
                    domAttr.set(node, { style: styleMap });
                    assert.equal(node.style.opacity, styleMap.opacity);
                    assert.equal(node.style.width, styleMap.width);
                    assert.equal(node.style.borderWidth, styleMap.border.split(" ")[0]);
                    domAttr.set(node, propMap);
                    assert.equal(node.innerHTML, propMap.innerHTML);
                    assert.equal(domAttr.get(node, "innerHTML"), propMap.innerHTML);
                    domAttr.set(node, "innerHTML", "<span>howdy!</span>");
                    assert.equal(node.firstChild.nodeType, 1);
                    assert.equal(node.firstChild.nodeName.toLowerCase(), "span");
                    assert.equal(node.innerHTML.toLowerCase(), "<span>howdy!</span>");
                    assert.equal(domAttr.get(node, "innerHTML").toLowerCase(), "<span>howdy!</span>");
                },
                "testLabelForAttr": function () {
                    // create label with no for attribute make sure requesting
                    // it as for and html for returns null
                    var label = document.createElement("label");
                    if (!has("ie")) {
                        // IE always assumes that "for" is present
                        assert.isNull(domAttr.get(label, "for"));
                        assert.isNull(domAttr.get(label, "htmlFor"));
                    }
                    // add a for attribute and test that can get by requesting for
                    domAttr.set(label, "for", "testId");
                    assert.equal(domAttr.get(label, "for"), "testId");
                    // add as htmlFor and make sure it is returned when requested as htmlFor
                    var label2 = document.createElement("label");
                    domAttr.set(label2, "htmlFor", "testId2");
                    assert.equal(domAttr.get(label2, "htmlFor"), "testId2");
                    // check than when requested as for or htmlFor attribute is found
                    assert.isTrue(domAttr.has(label, "for"));
                    assert.isTrue(domAttr.has(label2, "htmlfor"));
                    // test from markup

                    // make sure testing if has attribute using for or htmlFor
                    // both return null when no value set
                    if (!has("ie")) {
                        // IE always assumes that "for" is present
                        assert.isFalse(domAttr.has(labelNoFor, "for"));
                        assert.isFalse(domAttr.has(labelNoFor, "htmlFor"));
                    }
                    // when markup includes for make certain testing if has attribute
                    // using for or htmlFor returns true
                    assert.isTrue(domAttr.has(labelWithFor, "for"));
                    assert.isTrue(domAttr.has(labelWithFor, "htmlFor"));
                    // when markup include for attrib make sure can retrieve using for or htmlFor
                    assert.equal(domAttr.get(labelWithFor, "for"), "input-with-label");
                    assert.equal(domAttr.get(labelWithFor, "htmlFor"), "input-with-label");
                },
                "attrInputTextValue": function () {
                    assert.equal(inputTextValue.value, "123");
                    assert.equal(domAttr.get("input-text-value", "value"), "123");
                    domAttr.set("input-text-value", "value", "abc");
                    assert.equal(inputTextValue.value, "abc");
                    assert.equal(domAttr.get("input-text-value", "value"), "abc");
                    inputTextValue.value = "xyz";
                    assert.equal(inputTextValue.value, "xyz");
                    assert.equal(domAttr.get("input-text-value", "value"), "xyz");
                    inputTextValue.value = "123"; // fixes initialization problem when the test is reloaded
                },
                "testInputDisabled": function () {
                    assert.isFalse(domAttr.get("input-no-disabled", "disabled"));
                    assert.isTrue(domAttr.get("input-with-disabled", "disabled"));
                    assert.isTrue(domAttr.get("input-with-disabled-true", "disabled"));
                }
            };
        })()
    });
});