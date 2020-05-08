define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../NodeList',
    '../../dom-attr',
    '../../dom-construct',
    '../../query',
    '../../NodeList-manipulate',
    '../../NodeList-traverse'
], function (registerSuite, assert, sinon, NodeList, attr, construct, query) {
    registerSuite({
        name: 'dojo/NodeList-manipulate',

        // Test this private method because a lot of public methods defer to it.
        // Better to test all in one place so that the consumers can rely on it to "do the right thing"
        // instead of checking implementation details themselves
        "_placeMultiple": {
            "returns original node list": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList._placeMultiple(document.createElement("div"), "last");

                //assert
                assert.strictEqual(result, nodeList);

            },
            "string + string calls dojo/dom-construct::place with correct arguments": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes),
                    queryResults = [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    container = document.createElement("div"),
                    mock = sinon.spy(construct, "place"),
                    position = "last";

                for (var i in nodes) {
                    nodes[i].className = "class" + i;
                }

                for (var i in queryResults) {
                    container.appendChild(queryResults[i]);
                }

                document.body.appendChild(container);

                //act
                nodeList._placeMultiple("span", position);

                //assert

                //last node in nodelist placed with dojo/dom-construct::place into first query result node
                assert.deepEqual(mock.args[0], [nodes[1], queryResults[0], position]);
                //addiitional nodes in nodelist place into first query result before the last node
                assert.equal(nodes[1].previousSibling, nodes[0]);

                //last node in nodelist is cloned and that clone is placed with dojo/dom-construct::place into additional query result nodes
                assert.notEqual(mock.args[1][0], nodes[1]);
                assert.equal(mock.args[1][0].outerHTML, nodes[1].outerHTML);

                //addiitional nodes in nodelist place into first query result before the last node
                assert.notEqual(queryResults[1].children[1], nodes[1]);
                assert.equal(queryResults[1].children[1].outerHTML, nodes[1].outerHTML);

                mock.restore();

            },
            "Node + string calls dojo/dom-construct::place with correct arguments": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes),
                    queryNode = document.createElement("span"),
                    container = document.createElement("div"),
                    mock = sinon.spy(construct, "place"),
                    position = "last";

                for (var i in nodes) {
                    nodes[i].className = "class" + i;
                }

                container.appendChild(queryNode);

                document.body.appendChild(container);

                //act
                nodeList._placeMultiple(queryNode, position);

                //assert

                //last node in nodelist placed with dojo/dom-construct::place into queryNode
                assert.deepEqual(mock.args[0], [nodes[1], queryNode, position]);
                //addiitional nodes in nodelist place into queryNode before the last node
                assert.equal(nodes[1].previousSibling, nodes[0]);

                mock.restore();

            },
            "NodeList + string calls dojo/dom-construct::place with correct arguments": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes),
                    queryResults = [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    queryNodeList = new NodeList(queryResults),
                    container = document.createElement("div"),
                    mock = sinon.spy(construct, "place"),
                    position = "last";

                for (var i in nodes) {
                    nodes[i].className = "class" + i;
                }

                for (var i in queryResults) {
                    container.appendChild(queryResults[i]);
                }

                document.body.appendChild(container);

                //act
                nodeList._placeMultiple(queryNodeList, position);

                //assert

                //last node in nodelist placed with dojo/dom-construct::place into first query result node
                assert.deepEqual(mock.args[0], [nodes[1], queryResults[0], position]);
                //addiitional nodes in nodelist place into first query result before the last node
                assert.equal(nodes[1].previousSibling, nodes[0]);

                //last node in nodelist is cloned and that clone is placed with dojo/dom-construct::place into additional query result nodes
                assert.notEqual(mock.args[1][0], nodes[1]);
                assert.equal(mock.args[1][0].outerHTML, nodes[1].outerHTML);

                //addiitional nodes in nodelist place into first query result before the last node
                assert.notEqual(queryResults[1].children[1], nodes[1]);
                assert.equal(queryResults[1].children[1].outerHTML, nodes[1].outerHTML);

                mock.restore();
            }
        },
        "innerHTML()": {

            "when argument present => delegate to addContent": function () {
                //arrange
                var expected = "foo",
                    argument = "bar",
                    nodes = ["bar", "baz"],
                    nodeList = new NodeList(nodes),
                    mock = sinon.stub(nodeList, "addContent").returns(expected);

                //act
                var result = nodeList.innerHTML(argument);

                //assert
                assert.equal(result, expected);
                assert.deepEqual(mock.args[0], [argument, "only"]);

                mock.restore();

            },
            "when no argument, return first nodes innerHTML": function () {
                //arrange
                var node = document.createElement("div"),
                    otherNode = document.createElement("div"),
                    expected = "the expected result",
                    nodeList = new NodeList([node, otherNode]);

                node.innerHTML = expected;
                otherNode = "this is not the expected content";

                //act
                var result = nodeList.innerHTML();

                //assert
                assert.equal(result, expected);
            }
        },
        "text()": {
            "string => attr.set": function () {
                //arrange
                var nodes = [
                    document.createElement("div"),
                    document.createElement("div")
                ],
                nodeList = new NodeList(nodes),
                expected = "the expected content",
                mock = sinon.spy(attr, "set");

                //act
                var result = nodeList.text(expected);

                //assert
                for (var i in nodes) {
                    assert.deepEqual(mock.args[i], [nodes[i], "textContent", expected]);
                }
                assert.equal(result, nodeList);

                mock.restore();

            },
            "no args => attr.get": function () {
                //arrange
                var nodes = [
                    document.createElement("div"),
                    document.createElement("div")
                ],
                nodeList = new NodeList(nodes),
                expected = [
                    "the first expected content",
                    "the second expected content"
                ],
                mock = sinon.stub(attr, "get");

                for (var i in nodes) {
                    mock.withArgs(nodes[i]).returns(expected[i]);
                }

                //act
                var result = nodeList.text();

                //assert
                assert.equal(result, expected[0] + expected[1]);
                for (var i in nodes) {
                    assert.deepEqual(mock.args[i], [nodes[i], "textContent"]);
                }

                mock.restore();
            }

        },
        "val()": {
            "empty nodelist::val() = undefined": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList.val();

                //assert
                assert.isUndefined(result);

            },
            "nodeList([<select/>, ...]);:val() = value of first element": function () {
                //arrange
                var expected = "the value",
                    firstNode = document.createElement("select"),
                    nextNode = document.createElement("input"),
                    notSelectedOption = document.createElement("option"),
                    selectedOption = document.createElement("option"),
                    nodeList = new NodeList([firstNode, nextNode]);

                notSelectedOption.value = "not the expected value";
                selectedOption.value = expected;
                firstNode.add(notSelectedOption);
                firstNode.add(selectedOption);
                selectedOption.selected = true;

                //act
                var result = nodeList.val();

                //assert
                assert.equal(result, expected);

            },
            "nodeList([<select multiple/>, ...])::val() = array of selected options": function () {
                //arrange
                var expected = ["blue", "yellow"],
                    notExpected = "red",
                    node = document.createElement("select"),
                    notSelectedOption = document.createElement("option"),
                    firstSelectedOption = document.createElement("option"),
                    secondSelectedOption = document.createElement("option"),
                    nodeList = new NodeList([node]);

                node.multiple = true;
                notSelectedOption.value = notExpected;
                firstSelectedOption.value = expected[0];
                secondSelectedOption.value = expected[1];

                node.add(firstSelectedOption);
                node.add(notSelectedOption);
                node.add(secondSelectedOption);

                firstSelectedOption.selected = true;
                secondSelectedOption.selected = true;

                //act
                var result = nodeList.val();

                //assert
                assert.deepEqual(result, expected);

            },
            "nodeList([<input/>, <input/>])::val(string) => sets the value of the input elements": function () {
                //arrange
                var expected = "the value",
                    nodes = [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes);

                for (var i in nodes) {
                    nodes[i].value = "the old value";
                }

                //act
                var result = nodeList.val(expected);

                //assert
                for (var i in nodes) {
                    assert.equal(expected, nodes[i].value);
                }
                assert.equal(result, nodeList);

            },
            "nodeList([<input/>, <input/>])::val(array) => sets the value of the input elements": function () {
                //arrange
                var expected = [
                        "the first value",
                        "the second value"
                ],
                    nodes = [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes);

                for (var i in nodes) {
                    nodes[i].value = "the old value";
                }

                //act
                var result = nodeList.val(expected);

                //assert
                for (var i in nodes) {
                    assert.equal(expected[i], nodes[i].value);
                }
                assert.equal(result, nodeList);
            },
            "nodeList([<select/>])::val(string) => sets correct options to selected": function () {
                //arrange
                var expected = "the value",
                    node = document.createElement("select"),
                    notSelectedOption = document.createElement("option"),
                    selectedOption = document.createElement("option"),
                    nodeList = new NodeList([node]);

                notSelectedOption.value = "not the expected value";
                selectedOption.value = expected;

                node.add(notSelectedOption);
                node.add(selectedOption);

                //act
                var result = nodeList.val(expected);

                //assert
                assert.isTrue(selectedOption.selected);
                assert.isFalse(notSelectedOption.selected);
                assert.equal(result, nodeList);
            },
            "nodeList([<input type='checkbox'/>, <input type='checkbox'/>])::val(string) => selects correct elements": function () {
                //arrange
                var expected = "the value",
                    nodeToCheck = document.createElement("input"),
                    nodeToNotCheck = document.createElement("input"),
                    nodes = [nodeToNotCheck, nodeToCheck],
                    nodeList = new NodeList(nodes);

                for (var i in nodes) {
                    nodes[i].type = "checkbox";
                }

                nodeToCheck.value = expected;
                nodeToNotCheck.value = "not expected value";

                nodeToCheck.checked = false;
                nodeToNotCheck.checked = true;

                //act
                var result = nodeList.val(expected);

                //assert
                assert.isTrue(nodeToCheck.checked);
                assert.isFalse(nodeToNotCheck.checked);
                assert.equal(result, nodeList);

            },
            "nodeList([<input type='radio'/>])::val(string) => selects correct elements": function () {
                //arrange
                var expected = "the value",
                    nodeToCheck = document.createElement("input"),
                    nodeToNotCheck = document.createElement("input"),
                    nodes = [nodeToNotCheck, nodeToCheck],
                    nodeList = new NodeList(nodes);

                for (var i in nodes) {
                    nodes[i].type = "radio";
                    nodes[i].name = "theName" + i;
                }

                nodeToCheck.value = expected;
                nodeToNotCheck.value = "not expected value";

                nodeToCheck.checked = false;
                nodeToNotCheck.checked = true;

                //act
                var result = nodeList.val(expected);

                //assert
                assert.isTrue(nodeToCheck.checked);
                assert.isFalse(nodeToNotCheck.checked);
                assert.equal(result, nodeList);

            }
        },
        "append()": {
            "any => this.addContent(any, 'last')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "addContent").returns(expected);

                //act
                var result = nodeList.append(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "last"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "appendTo()": {
            "string => this._placeMultiple(string, 'last')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "_placeMultiple").returns(expected);

                //act
                var result = nodeList.appendTo(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "last"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "prepend()": {
            "string => this.prepend(string, 'first')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "addContent").returns(expected);

                //act
                var result = nodeList.prepend(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "first"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "prependTo()": {
            "string => this._placeMultiple(string, 'first')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "_placeMultiple").returns(expected);

                //act
                var result = nodeList.prependTo(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "first"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "after()": {
            "string => this.after(string, 'after')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "addContent").returns(expected);

                //act
                var result = nodeList.after(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "after"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "insertAfter()": {
            "string => this._placeMultiple(string, 'after')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "_placeMultiple").returns(expected);

                //act
                var result = nodeList.insertAfter(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "after"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "before()": {
            "string => this.after(string, 'before')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "addContent").returns(expected);

                //act
                var result = nodeList.before(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "before"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "insertBefore()": {
            "string => this._placeMultiple(string, 'before')": function () {
                //arrange
                var arg = { foo: "bar" },
                    expected = "the result",
                    nodeList = new NodeList(),
                    mock = sinon.stub(nodeList, "_placeMultiple").returns(expected);

                //act
                var result = nodeList.insertBefore(arg);

                //assert
                assert.deepEqual(mock.args[0], [arg, "before"]);
                assert.equal(result, expected);

                mock.restore();

            }
        },
        "remove()": {
            "remove is alias of NodeList.prototype.orphan": function () {
                //arrange
                var nodeList = new NodeList();

                //act

                //assert
                assert.strictEqual(nodeList.remove, NodeList.prototype.orphan);
            }
        },
        "wrap()": {
            "returns nodeList": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList.wrap("<div></div>");

                //assert
                assert.strictEqual(result, nodeList);

            },
            "inserts nodes at the lowest level of the wrapper": function () {
                //arrange
                var node = document.createElement("input"),
                    nodeList = new NodeList([node]),
                    container = document.createElement("div")

                container.appendChild(node);

                //act
                nodeList.wrap("<div><span><b></b></span></div>");

                //assert
                assert.equal(node.parentNode.nodeName.toLowerCase(), "b");
            },
            "wraps all nodes in nodelist": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div")

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }

                //act
                nodeList.wrap("<span></span>");

                //assert
                for (var i in nodes) {
                    assert.equal(nodes[i].parentNode.nodeName.toLowerCase(), "span");
                }
            }
        },
        "wrapAll()": {
            "returns original nodeList": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList.wrapAll("<div></div>");

                //assert
                assert.strictEqual(result, nodeList);

            },
            "wraps all nodes in the wrapper as direct children": function () {
                //arrange
                var nodes = [
                        document.createElement("input"),
                        document.createElement("input"),
                        document.createElement("input")],
                    nodeList = new NodeList(nodes),
                    containers = [
                        document.createElement("div"),
                        document.createElement("div"),
                        document.createElement("div")];

                for (var i in nodes) {
                    containers[i].appendChild(nodes[i]);
                }

                //act
                nodeList.wrapAll("<div></div>");

                //assert
                var parent = nodes[0].parentElement;
                for (var i in nodes) {
                    assert.strictEqual(parent, nodes[i].parentElement);
                }

            },
            "wrapper located in the same location as the first node": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("input"),
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes),
                    containers = [
                        document.createElement("div"),
                        document.createElement("div"),
                        document.createElement("div")];

                for (var i in nodes) {
                    containers[i].appendChild(nodes[i]);
                }

                //act
                nodeList.wrapAll("<div></div>");

                //assert
                var parent = nodes[0].parentElement;
                assert.strictEqual(parent.parentElement, containers[0]);
            }
        },
        "wrapInner()": {
            "returns the original node list": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList.wrapInner("<div></div>");

                //assert
                assert.strictEqual(result, nodeList);

            },
            "wraps the direct children with the wrapper": function () {
                //arrange
                var children =
                    [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    container = document.createElement("div"),
                    nodeList = new NodeList(container);

                for (var i in children) {
                    container.appendChild(children[i]);
                }

                //act
                nodeList.wrapInner("<span></span>");

                //assert
                for (var i in children) {
                    assert.equal(children[i].parentElement.nodeName.toLowerCase(), "span");
                }
            }
        },
        "replaceWith()": {
            "returns the original node list": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList.replaceWith("<div></div>");


                //assert
                assert.strictEqual(result, nodeList);


            },
            "content is passed through the _normalize method": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div"),
                    arg = "<div></div>",
                    expected = document.createElement("div"),
                    mock = sinon.stub(nodeList, "_normalize").returns(expected);

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }

                //act
                nodeList.replaceWith(arg);

                //assert
                assert.isTrue(mock.calledWith(arg, nodes[0]));

                mock.restore();
            },
            "calls _place method with correct arguments": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div"),
                    arg = "<div></div>",
                    expected = document.createElement("div"),
                    _normalizeMock = sinon.stub(nodeList, "_normalize").returns(arg),
                    mock = sinon.stub(nodeList, "_place");

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }

                //act
                nodeList.replaceWith(arg);

                //assert
                assert.isTrue(mock.calledWith(arg, nodes[0], "before", false));
                assert.isTrue(mock.calledWith(arg, nodes[1], "before", true));

                _normalizeMock.restore();
                mock.restore();

            },
            "removes original nodes": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div"),
                    arg = "<div></div>",
                    expected = document.createElement("div"),
                    _normalizeMock = sinon.stub(nodeList, "_normalize").returns(arg),
                    _placeMock = sinon.stub(nodeList, "_place");

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }

                //act
                nodeList.replaceWith(arg);

                //assert
                for (var i in nodes) {
                    assert.notEqual(container, nodes[i].parentElement);
                }

                _normalizeMock.restore();
                _placeMock.restore();
            }

        },
        "replaceAll()": {
            "returns the original node list": function () {
                //arrange
                var nodeList = new NodeList();

                //act
                var result = nodeList.replaceAll("div");


                //assert
                assert.strictEqual(result, nodeList);

            },
            "content is passed through the _normalize method": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div"),
                    queryNodes =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    queryNodeList = new NodeList(queryNodes),
                    expectedContent = document.createElement("b"),
                    _normalizeMock = sinon.stub(nodeList, "_normalize").returns(expectedContent),
                    _placeMock = sinon.spy(nodeList, "_place");

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }
                for (var i in queryNodes) {
                    container.appendChild(queryNodes[i]);
                }
                document.body.appendChild(container);

                //act
                nodeList.replaceAll("div div");

                //assert
                assert.isTrue(_normalizeMock.calledWith(nodeList, nodes[0]));

                _normalizeMock.restore();
                _placeMock.restore();
            },
            "calls _place method with correct arguments": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div"),
                    queryNodes =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    queryNodeList = new NodeList(queryNodes),
                    expectedContent = document.createElement("b"),
                    _normalizeMock = sinon.stub(nodeList, "_normalize").returns(expectedContent),
                    _placeMock = sinon.spy(nodeList, "_place");

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }
                for (var i in queryNodes) {
                    container.appendChild(queryNodes[i]);
                }
                document.body.appendChild(container);

                //act
                nodeList.replaceAll("div div");

                //assert
                assert.isTrue(_placeMock.calledWith(expectedContent, queryNodes[0], "before", false));
                assert.isTrue(_placeMock.calledWith(expectedContent, queryNodes[1], "before", true));

                _normalizeMock.restore();
                _placeMock.restore();
            },
            "removes original nodes": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    container = document.createElement("div"),
                    queryNodes =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    queryNodeList = new NodeList(queryNodes),
                    expectedContent = document.createElement("b"),
                    _normalizeMock = sinon.stub(nodeList, "_normalize").returns(expectedContent),
                    _placeMock = sinon.spy(nodeList, "_place");

                for (var i in nodes) {
                    container.appendChild(nodes[i]);
                }
                for (var i in queryNodes) {
                    container.appendChild(queryNodes[i]);
                }
                document.body.appendChild(container);

                //act
                nodeList.replaceAll("div div");

                //assert
                assert.isFalse(container.contains(queryNodes[0]));
                assert.isFalse(container.contains(queryNodes[1]));

                _normalizeMock.restore();
                _placeMock.restore();
            }
        },
        "clone()": {
            "returns node list populated with clones of originals": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("input"),
                        document.createElement("input")
                    ],
                    nodeList = new NodeList(nodes);

                for (var i in nodes) {
                    nodes[i].className = "nodeClass" + i;
                }

                //act
                var result = nodeList.clone();

                //assert
                assert.notStrictEqual(result, nodeList);
                for (var i in nodes) {
                    assert.notStrictEqual(result[i], nodes[i]);
                    assert.equal(result[i].outerHTML, nodes[i].outerHTML);
                }

            }
        },
        "validation tests": (function () {
            var container;
            function verify(/*dojo.NodeList*/nl, /*Array*/ids, /*String*/ comment) {
                comment = comment || "verify";
                for (var i = 0, node; (node = nl[i]) ; i++) {
                    assert.isTrue(node.id == ids[i] || node == ids[i], comment + " " + i)
                }
                //Make sure lengths are equal.
                assert.equal(i, ids.length, comment + " length");
            }

            var divs;

            return {
                setup: function () {
                    var html =
                        '<div>' +
                        '    <h1 id="firstH1">testing dojo.NodeList-traverse</h1>' +
                        '    <div id="sq100" class="testDiv">' +
                        '        100px square, abs' +
                        '    </div>' +
                        '    <div id="t" class="testDiv">' +
                        '        <span id="c1">c1</span>' +
                        '    </div>' +
                        '   <div id="third" class="third testDiv">' +
                        '       <!-- This is the third top level div -->' +
                        '       <span id="crass">Crass, baby</span>' +
                        '       The third div' +
                        '       <span id="classy" class="classy">Classy, baby</span>' +
                        '       The third div, again' +
                        '       <!-- Another comment -->' +
                        '       <span id="yeah">Yeah, baby</span>' +
                        '   </div>' +
                        '   <div id="level1" class="foo">' +
                        '       <div id="level2" class="bar">' +
                        '           <div id="level3" class="foo">' +
                        '               <div id="level4" class="bar">' +
                        '                   <div id="level5" class="bar">' +
                        '                       <div id="level6" class="bang">foo bar bar bang</div>' +
                        '                   </div>' +
                        '               </div>' +
                        '           </div>' +
                        '       </div>' +
                        '   </div>' +
                        '</div>';
                    container = construct.toDom(html);

                    document.body.appendChild(container);

                    divs = query("div.testDiv");
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "children": function () {
                    verify(divs.last().children(), ["crass", "classy", "yeah"]);
                },

                "closest": function () {
                    // test simple selector
                    var classy = query("#classy");
                    var closestDiv = classy.closest("div");
                    verify(closestDiv, ["third"], "closest('div')");
                    verify(closestDiv.end().closest(".classy"), ["classy"], "closestDiv.end().closest('.classy')");

                    // test descendant selector
                    var bang = query(".bang");
                    var closestFooBar = bang.closest(".foo > .bar");
                    verify(closestFooBar, ["level4"], ".foo > .bar");

                    // test descendant selector that doesn't match (".foo" alone matches nodes, but not
                    // ".bogus .foo")
                    var closestBogusFoo = bang.closest(".bogus .foo");
                    verify(closestBogusFoo, [], ".bogus .foo");

                    // positive test that scope argument works: .foo > .bar should match a scope
                    // of "level2" or above
                    closestFooBar = bang.closest(".foo > .bar", "level2");
                    verify(closestFooBar, ["level4"], ".foo > .bar query relative to level2");

                    // > .bar should match a scope of level3 or level1
                    var topBar = bang.closest("> .bar", "level3");
                    verify(topBar, ["level4"], "> .bar query relative to level3");

                    // negative test that scope argument works:  .foo > .bar relative to level3
                    // doesn't match since .foo is level3, rather than a child of level3
                    closestFooBar = bang.closest(".foo > .bar", "level3");
                    verify(closestFooBar, [], ".foo > .bar query relative to level3");

                    // complex test of multiple elements in NodeList
                    // Only some of the elements in query("div") have a ".foo" ancestor,
                    // and three of those elements have the *same* .foo ancestor, so
                    // closest(".foo") should result in list of just two elements
                    var closestFoo = query("div").closest(".foo");
                    verify(closestFoo, ["level1", "level3"], ".foo from div");

                },

                "parent": function () {
                    verify(query("#classy").parent(), ["third"]);
                },

                "parents": function () {
                    var classy = query("#classy");
                    verify(classy.parents(), ["third", container, document.body, document.body.parentElement]);
                    verify(classy.parents(".third"), ["third"]);
                    verify(classy.parents("body"), [document.body]);
                },

                "siblings": function () {
                    verify(query("#classy").siblings(), ["crass", "yeah"]);
                },

                "next": function () {
                    verify(query("#crass").next(), ["classy"]);
                },

                "nextAll": function () {
                    verify(query("#crass").nextAll(), ["classy", "yeah"]);
                    verify(query("#crass").nextAll("#yeah"), ["yeah"]);
                },

                "prev": function () {
                    verify(query("#classy").prev(), ["crass"]);
                },

                "prevAll": function () {
                    verify(query("#yeah").prevAll(), ["classy", "crass"]);
                    verify(query("#yeah").prevAll("#crass"), ["crass"]);
                },

                "andSelf": function () {
                    verify(query("#yeah").prevAll().andSelf(), ["classy", "crass", "yeah"]);
                },

                "first": function () {
                    verify(divs.first(), ["sq100"]);
                },

                "last": function () {
                    verify(divs.last(), ["third"]);
                },

                "even": function () {
                    var even = divs.even();
                    verify(even, ["t"]);
                    verify(even.end(), ["sq100", "t", "third"]);
                },

                "odd": function () {
                    var odd = divs.odd();
                    verify(odd, ["sq100", "third"]);
                    verify(odd.end(), ["sq100", "t", "third"]);
                }

            }
        })()
    });
});
