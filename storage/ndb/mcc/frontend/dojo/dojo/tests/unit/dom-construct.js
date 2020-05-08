define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../dom-construct',
    '../../dom-attr',
    '../../has'
], function (registerSuite, assert, sinon, domConstruct, domAttr, has) {
    var baseId = "dom-construct",
        uniqueId = 0;

    function getId() {
        return baseId + uniqueId++;
    }

    registerSuite({
        name: 'dojo/dom-construct',

        "toDom": (function () {
            return {
                "returns expected node when one requested": function () {
                    //arrange
                    var rawHTML = "<div></div>";

                    //act
                    var result = domConstruct.toDom(rawHTML);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "div");

                },
                "returns expected result when tree requested": function () {
                    //arrange
                    var parent = document.createElement("div"),
                        child = document.createElement("span");

                    parent.appendChild(child);

                    //act
                    var result = domConstruct.toDom(parent.outerHTML);

                    //assert
                    assert.equal(result.outerHTML, result.outerHTML);
                },
                "returns expected result when forest requested": function () {
                    //arrange
                    var parent = document.createElement("div"),
                        children =
                        [
                            document.createElement("span"),
                            document.createElement("span")
                        ];

                    for (var i in children) {
                        parent.appendChild(children[i]);
                    }

                    //act
                    var result = domConstruct.toDom(parent.innerHTML);

                    //assert
                    assert.equal(result.outerHTML, result.innerHTML);
                },
                "able to create <option/> tag, which must be created in context of <select/>": function () {
                    //arrange
                    var node = "<option></option>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "option");

                },
                "able to create <tbody/> tag, which must be created in context of <table/>": function () {
                    //arrange
                    var node = "<tbody></tbody>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "tbody");

                },
                "able to create <thead/> tag, which must be created in context of <table/>": function () {
                    //arrange
                    var node = "<thead></thead>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "thead");

                },
                "able to create <tfoot/> tag, which must be created in context of <table/>": function () {
                    //arrange
                    var node = "<tfoot></tfoot>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "tfoot");

                },
                "able to create <tr/> tag, which must be created in context of <table><tbody/></table>": function () {
                    //arrange
                    var node = "<tr></tr>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "tr");

                },
                "able to create <td/> tag, which must be created in context of <table><tbody><tr/></tbody></table>": function () {
                    //arrange
                    var node = "<td></td>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "td");

                },
                "able to create <th/> tag, which must be created in context of <table><thead><tr/></thead></table>": function () {
                    //arrange
                    var node = "<th></th>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "th");

                },
                "able to create <legend/> tag, which must be created in context of <fieldset/>": function () {
                    //arrange
                    var node = "<legend></legend>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "legend");

                },
                "able to create <caption/> tag, which must be created in context of <table/>": function () {
                    //arrange
                    var node = "<caption></caption>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "caption");

                },
                "able to create <colgroup/> tag, which must be created in context of <table/>": function () {
                    //arrange
                    var node = "<colgroup></colgroup>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "colgroup");

                },
                "able to create <col/> tag, which must be created in context of <table><colgroup/></table>": function () {
                    //arrange
                    var node = "<col></col>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "col");

                },
                "able to create <li/> tag, which must be created in context of <ul/>": function () {
                    //arrange
                    var node = "<li></li>";

                    //act
                    var result = domConstruct.toDom(node);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), "li");

                },
            };
        })(),
        "place": (function () {
            return {
                "when first arg is rawHTML, then it is converted to DOM and placed": function () {
                    //arrange
                    var container = document.createElement("div"),
                        child = document.createElement("span"),
                        reference = document.createElement("h1");

                    container.appendChild(child);

                    //act
                    domConstruct.place(container.outerHTML, reference, "only");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, container.nodeName);
                    assert.equal(reference.firstElementChild.firstElementChild.nodeName, child.nodeName);
                },
                "when first arg is a node id, then the correct node is placed": function () {
                    //arrange
                    var node = document.createElement("div"),
                        reference = document.createElement("h1"),
                        nodeId = getId();

                    node.id = nodeId;
                    document.body.appendChild(node);

                    //act
                    domConstruct.place(nodeId, reference, "only");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                },
                "when first arg is a node, then it is placed": function () {
                    //arrange
                    var node = document.createElement("div"),
                        reference = document.createElement("h1");

                    document.body.appendChild(node);

                    //act
                    domConstruct.place(node, reference, "only");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                },
                "when second arg is a string, then the node with that id is used as reference": function () {
                    //arrange
                    var node = document.createElement("div"),
                        reference = document.createElement("h1"),
                        referenceId = getId();

                    document.body.appendChild(reference);
                    reference.id = referenceId;

                    //act
                    domConstruct.place(node, referenceId, "only");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                },
                "when second arg is a node, then it is used as the reference node": function () {
                    //arrange
                    var node = document.createElement("div"),
                        reference = document.createElement("h1");

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "only");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                },
                "when third argument is 'before', then the first arg is placed before the second": function () {
                    //arrange
                    var node = document.createElement("div"),
                        reference = document.createElement("h1");

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "before");

                    //assert
                    assert.equal(reference.previousSibling.nodeName, node.nodeName);
                },
                "when third argument is 'after', then the first arg is placed after the second": function () {
                    //arrange
                    var node = document.createElement("div"),
                        reference = document.createElement("h1");

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "after");

                    //assert
                    assert.equal(reference.nextSibling.nodeName, node.nodeName);
                },
                "when third argument is 'replace', then the first arg replaces the second": function () {
                    //arrange
                    var container = document.createElement("div"),
                        node = document.createElement("span"),
                        reference = document.createElement("h1");

                    document.body.appendChild(container);
                    container.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "replace");

                    //assert
                    assert.equal(container.firstElementChild.nodeName, node.nodeName);
                },
                "when third argument is 'only', then the first arg is placed as only content of second": function () {
                    //arrange
                    var node = document.createElement("span"),
                        reference = document.createElement("h1"),
                        children =
                        [
                            document.createElement("button"),
                            document.createElement("button"),
                            document.createElement("button")
                        ];

                    for (var i in children) {
                        reference.appendChild(children[i]);
                    }

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "only");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                    assert.equal(reference.children.length, 1);
                },
                "when third argument is 'first', then the first arg is placed as first child of second": function () {
                    //arrange
                    var node = document.createElement("span"),
                        reference = document.createElement("h1"),
                        children =
                        [
                            document.createElement("button"),
                            document.createElement("button"),
                            document.createElement("button")
                        ];

                    for (var i in children) {
                        reference.appendChild(children[i]);
                    }

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "first");

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                    assert.equal(reference.children.length, children.length + 1);
                },
                "when third argument is 'last', then the first arg is placed as last child of second": function () {
                    //arrange
                    var node = document.createElement("span"),
                        reference = document.createElement("h1"),
                        children =
                        [
                            document.createElement("button"),
                            document.createElement("button"),
                            document.createElement("button")
                        ];

                    for (var i in children) {
                        reference.appendChild(children[i]);
                    }

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, "last");

                    //assert
                    assert.equal(reference.lastElementChild.nodeName, node.nodeName);
                    assert.equal(reference.children.length, children.length + 1);
                },
                "when third argument is a number, then the first arg is placed as the correct child of second": function () {
                    //arrange
                    var node = document.createElement("span"),
                        reference = document.createElement("h1"),
                        children =
                        [
                            document.createElement("button"),
                            document.createElement("button"),
                            document.createElement("button")
                        ],
                        position = 2;

                    for (var i in children) {
                        reference.appendChild(children[i]);
                    }

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, position);

                    //assert
                    assert.equal(reference.children[position].nodeName, node.nodeName);
                    assert.equal(reference.children.length, children.length + 1);
                },
                "when third argument is a number and the reference node is empty, then the first arg is placed as the first child of second": function () {
                    //arrange
                    var node = document.createElement("span"),
                        reference = document.createElement("h1"),
                        position = 2;

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, position);

                    //assert
                    assert.equal(reference.firstElementChild.nodeName, node.nodeName);
                    assert.equal(reference.children.length, 1);
                },
                "when third argument is a number that is greater than the number of children in the reference, then the first arg is placed as the first child of second": function () {
                    //arrange
                    var node = document.createElement("span"),
                        reference = document.createElement("h1"),
                        children =
                        [
                            document.createElement("button"),
                            document.createElement("button"),
                            document.createElement("button")
                        ],
                        position = children + 42;

                    for (var i in children) {
                        reference.appendChild(children[i]);
                    }

                    document.body.appendChild(reference);

                    //act
                    domConstruct.place(node, reference, position);

                    //assert
                    assert.equal(reference.lastElementChild.nodeName, node.nodeName);
                    assert.equal(reference.children.length, children.length + 1);
                }
            };
        })(),
        "create": (function () {
            return {
                "when first arg is a string, then correct element type is created": function () {
                    //arrange
                    var tagType = "div";

                    //act
                    var result = domConstruct.create(tagType);

                    //assert
                    assert.equal(result.nodeName.toLowerCase(), tagType);

                },
                "when attributes provided, then dojo/dom-attr:set called with correct args": function () {
                    //arrange
                    var tag = document.createElement("div"),
                        attrs = { foo: "bar", baz: "buz" },
                        mock = sinon.spy(domAttr, "set");

                    //act
                    domConstruct.create(tag, attrs);

                    //assert
                    assert.isTrue(mock.calledWith(tag, attrs));

                    mock.restore();
                },
                "when reference node provided, then dojo/dom-construct::place() called with correct args": function () {
                    //arrange
                    var tag = document.createElement("div"),
                        reference = document.createElement("h1"),
                        position = "only",
                        mock = sinon.spy(domConstruct, "place");

                    //act
                    domConstruct.create(tag, null, reference, position);

                    //assert
                    assert.isTrue(mock.calledWith(tag, reference, position));

                    mock.restore();
                },
                "when reference node not part of global document, then new element created in correct context": function () {
                    //arrange
                    var iframe = document.createElement("iframe");

                    document.body.appendChild(iframe);
                    tagType = "div",
                    reference = iframe.contentDocument.createElement("h1"),
                    position = "only";

                    //act
                    var result = domConstruct.create(tagType, null, reference, position);

                    //assert
                    assert.equal(result.ownerDocument, iframe.contentDocument);
                }
            };
        })(),
        "empty": (function () {
            return {
                "when given a node's id, then the related node is emptied": function () {
                    //arrange
                    var container = document.createElement("div"),
                        child = document.createElement("button"),
                        id = getId();

                    container.id = id;
                    container.appendChild(child);
                    document.body.appendChild(container);

                    //act
                    domConstruct.empty(id);

                    //assert
                    assert.equal(container.children.length, 0);

                },
                "when given a node, then it is emptied": function () {
                    //arrange
                    var container = document.createElement("div"),
                        child = document.createElement("button");

                    container.appendChild(child);

                    //act
                    domConstruct.empty(container);

                    //assert
                    assert.equal(container.children.length, 0);
                },
                "when given an svg element, then it is emptied": function () {
                    //arrange
                    var container = document.createElement("div"),
                        svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");

                    svg.innerHTML = "<rect></rect>";

                    //act
                    domConstruct.empty(svg);

                    //assert
                    if (svg.children == null) {
                        // IE does not support children property on <svg>.
                        assert.equal(svg.innerHTML, "");
                    } else {
                        assert.equal(svg.children.length, 0);
                    }
                }
            };
        })(),
        "destroy": (function () {
            return {
                "when given a node's id, then it is removed": function () {
                    //arrange
                    var node = document.createElement("h1"),
                        container = document.createElement("div"),
                        id = getId();

                    node.id = id;
                    document.body.appendChild(container);
                    container.appendChild(node);

                    //act
                    domConstruct.destroy(id);

                    //assert
                    assert.equal(container.children.length, 0);
                },
                "when given a node, then it is removed": function () {
                    //arrange
                    var node = document.createElement("h1"),
                        container = document.createElement("div");

                    document.body.appendChild(container);
                    container.appendChild(node);

                    //act
                    domConstruct.destroy(node);

                    //assert
                    assert.equal(container.children.length, 0);
                }
            };
        })(),
        "validation tests": (function () {
            var TEST_POSITION = 2;
            var lastHtml = "<div id='last'><h1>First</h1></div>";
            var firstHtml = "<div id='first'><h1>First</h1></div>";
            var beforeHtml = "<div id='before'></div>";
            var afterHtml = "<div id='after'></div>";
            var replaceHtml = "<div id='replace'></div>";
            var onlyHtml = "<div id='only'><h1>first</h1></div>";

            var posHtml = "<div id='pos'><div>first</div><div>second</div><div>last</div></div>";

            var HTMLString = "<div id=\"test\">Test</div>";

            var nodes = {};
            var child;
            var fragment;
            var container;

            function clearTarget() {
                domConstruct.empty(container);
                child = domConstruct.toDom(HTMLString);
                nodes.last = domConstruct.toDom(lastHtml);
                nodes.first = domConstruct.toDom(firstHtml);
                nodes.before = domConstruct.toDom(beforeHtml);
                nodes.after = domConstruct.toDom(afterHtml);
                nodes.replace = domConstruct.toDom(replaceHtml);
                nodes.only = domConstruct.toDom(onlyHtml);
                nodes.pos = domConstruct.toDom(posHtml);
                container.appendChild(nodes.last);
                container.appendChild(nodes.first);
                container.appendChild(nodes.before);
                container.appendChild(nodes.after);
                container.appendChild(nodes.replace);
                container.appendChild(nodes.only);
                container.appendChild(nodes.pos);
                fragment = document.createDocumentFragment();
                fragment.appendChild(document.createElement("div"));
                fragment.appendChild(document.createElement("div"));
                fragment.appendChild(document.createElement("div"));
            }

            function elementsEqual(elementA, elementB) {
                return elementA.id === elementB.id &&
                    elementA.tagName === elementB.tagName &&
                    elementA.innerHTML === elementB.innerHTML;
            }

            return {
                setup: function () {
                    document.body.innerHTML = "";
                    container = document.createElement("div");
                    document.body.appendChild(container);
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                beforeEach: clearTarget,
                "last - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.last);
                    assert.isTrue(elementsEqual(child, nodes.last.lastChild));
                },
                "last - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "last");
                    assert.isTrue(elementsEqual(child, nodes.last.lastChild));
                },
                "last - place html string with fragment reference": function () {
                    domConstruct.place(HTMLString, fragment);
                    assert.isTrue(elementsEqual(child, fragment.lastChild));
                },
                "last - place node with node reference": function () {
                    domConstruct.place(child, nodes.last);
                    assert.equal(nodes.last.lastChild, child);
                },
                "last - place node with id reference": function () {
                    domConstruct.place(child, "last");
                    assert.equal(nodes.last.lastChild, child);
                },
                "last - place node with fragment reference": function () {
                    domConstruct.place(child, fragment);
                    assert.equal(fragment.lastChild, child);
                },
                "first - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.first, "first");
                    assert.isTrue(elementsEqual(child, nodes.first.firstChild));
                },
                "first - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "first", "first");
                    assert.isTrue(elementsEqual(child, nodes.first.firstChild));
                },
                "first - place html string with fragment reference": function () {
                    domConstruct.place(HTMLString, fragment, "first");
                    assert.isTrue(elementsEqual(child, fragment.firstChild));
                },
                "first - place node with node reference": function () {
                    domConstruct.place(child, nodes.first, "first");
                    assert.equal( nodes.first.firstChild, child);
                },
                "first - place node with id reference": function () {
                    domConstruct.place(child, "first", "first");
                    assert.equal(nodes.first.firstChild, child);
                },
                "first - place node with fragment reference": function () {
                    domConstruct.place(child, fragment, "first");
                    assert.equal(fragment.firstChild, child);
                },
                "before - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.before, "before");
                    assert.isTrue(elementsEqual(child, nodes.before.previousSibling));
                },
                "before - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "before", "before");
                    assert.isTrue(elementsEqual(child, nodes.before.previousSibling));
                },
                "before - place node with node reference": function () {
                    domConstruct.place(child, nodes.before, "before");
                    assert.equal(nodes.before.previousSibling, child);
                },
                "before - place node with id reference": function () {
                    domConstruct.place(child, "before", "before");
                    assert.equal(nodes.before.previousSibling, child);
                },
                "after - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.after, "after");
                    assert.isTrue(elementsEqual(child, nodes.after.nextSibling));
                },
                "after - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "after", "after");
                    assert.isTrue(elementsEqual(child, nodes.after.nextSibling));
                },
                "after - place node with node reference": function () {
                    domConstruct.place(child, nodes.after, "after");
                    assert.equal(nodes.after.nextSibling, child);
                },
                "after - place node with id reference": function () {
                    domConstruct.place(child, "after", "after");
                    assert.equal(nodes.after.nextSibling, child);
                },
                "replace - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.replace, "replace");
                    assert.equal(undefined, document.getElementById("replace"));
                    assert.isTrue(elementsEqual(child, document.getElementById('test')));
                },
                "replace - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "replace", "replace");
                    assert.equal(undefined, document.getElementById("replace"));
                    assert.isTrue(elementsEqual(child, document.getElementById('test')));
                },
                "replace - place node with node reference": function () {
                    domConstruct.place(child, nodes.replace, "replace");
                    assert.equal(document.getElementById("replace"), undefined);
                    assert.equal(child, document.getElementById('test'));
                },
                "replace - place node with id reference": function () {
                    domConstruct.place(child, "replace", "replace");
                    assert.equal(undefined, document.getElementById("replace"));
                    assert.equal(document.getElementById('test'), child);
                },
                "only - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.only, "only");
                    assert.equal( 1, nodes.only.children.length);
                    assert.isTrue(elementsEqual(child, nodes.only.firstChild));
                },
                "only - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "only", "only");
                    assert.equal(1, nodes.only.children.length);
                    assert.isTrue(elementsEqual(child, nodes.only.firstChild));
                },
                "only - place html string with fragment reference": function () {
                    domConstruct.place(HTMLString, fragment, "only");
                    assert.equal(1, fragment.childNodes.length);
                    assert.isTrue(elementsEqual(child, fragment.firstChild));
                },
                "only - place node with node reference": function () {
                    domConstruct.place(child, nodes.only, "only");
                    assert.equal(nodes.only.firstChild, child);
                    assert.equal(nodes.only.children.length, 1);
                },
                "only - place node with id reference": function () {
                    domConstruct.place(child, "only", "only");
                    assert.equal(nodes.only.firstChild, child);
                    assert.equal(nodes.only.children.length, 1);
                },
                "only - place node with fragment reference": function () {
                    domConstruct.place(child, fragment, "only");
                    assert.equal(1, fragment.childNodes.length);
                    assert.equal(fragment.firstChild, child);
                },
                "pos - place html string with node reference": function () {
                    domConstruct.place(HTMLString, nodes.pos, TEST_POSITION);
                    assert.isTrue(elementsEqual(child, nodes.pos.children[TEST_POSITION]));
                },
                "pos - place html string with id reference": function () {
                    domConstruct.place(HTMLString, "pos", TEST_POSITION);
                    assert.isTrue(elementsEqual(child, nodes.pos.children[TEST_POSITION]));
                },
                "pos - place html string with fragment reference": function () {
                    domConstruct.place(HTMLString, fragment, TEST_POSITION);
                    assert.isTrue(elementsEqual(child, fragment.childNodes[TEST_POSITION]));
                },
                "pos - place node with node reference": function () {
                    domConstruct.place(child, nodes.pos, TEST_POSITION);
                    assert.equal(nodes.pos.children[TEST_POSITION], child);
                },
                "pos - place node with id reference": function () {
                    domConstruct.place(child, "pos", TEST_POSITION);
                    assert.equal(nodes.pos.children[TEST_POSITION], child);
                },
                "pos - place node with fragment reference": function () {
                    domConstruct.place(child, fragment, TEST_POSITION);
                    assert.equal(fragment.childNodes[TEST_POSITION], child);
                }
            }
        })()
    });
});
