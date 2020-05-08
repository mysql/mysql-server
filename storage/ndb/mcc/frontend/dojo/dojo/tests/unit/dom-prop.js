define([
   'intern!object',
   'intern/chai!assert',
   'sinon',
   '../../dom-prop',
   '../../dom-style',
   '../../_base/connect'
], function (registerSuite, assert, sinon, domProp, domStyle, connect) {
    var baseId = "dom-prop",
        uniqueId = 0;

    function getId() {
        return baseId + uniqueId++;
    }

    var container;

    registerSuite({
        name: 'dojo/dom-prop',
        setup: function () {
            container = document.createElement("div");
            document.body.appendChild(container);
        },
        teardown: function () {
            document.body.removeChild(container);
        },
        "names": {
            "maps values correctly": function () {
                //arrange

                //act

                //assert
                assert.equal(domProp.names["class"], "className");
                assert.equal(domProp.names["for"], "htmlFor");
                assert.equal(domProp.names["tabindex"], "tabIndex");
                assert.equal(domProp.names["readonly"], "readOnly");
                assert.equal(domProp.names["colspan"], "colSpan");
                assert.equal(domProp.names["frameborder"], "frameBorder");
                assert.equal(domProp.names["rowspan"], "rowSpan");
                assert.equal(domProp.names["textcontent"], "textContent");
                assert.equal(domProp.names["valuetype"], "valueType");

            }
        },
        "get()": {
            "node + string": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyName = "name",
                    propertyValue = "the value";

                node[propertyName] = propertyValue;

                //act
                var result = domProp.get(node, propertyName);

                //assert
                assert.equal(result, propertyValue);

            },
            "string + string": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyName = "name",
                    propertyValue = "the value",
                    id = getId();

                node[propertyName] = propertyValue;
                node.id = id;
                container.appendChild(node);

                //act
                var result = domProp.get(id, propertyName);

                //assert
                assert.equal(result, propertyValue);
            },
            "node + 'textContent'": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyName = "textContent",
                    propertyValue = "the value";

                node[propertyName] = propertyValue;

                //act
                var result = domProp.get(node, propertyName);

                //assert
                assert.equal(result, propertyValue);
            },
            "attribute name is case-insensitive": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyName = "name".toUpperCase(),
                    propertyValue = "the value";

                node[propertyName] = propertyValue;

                //act
                var result = domProp.get(node, propertyName);

                //assert
                assert.equal(result, propertyValue);
            },
            "getting attribute that is aliased in JavaScript gets correct attribute": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyValue = 42,
                    attributes =
                    [
                        { requested: "class", expected: "className" },
                        { requested: "for", expected: "htmlFor" },
                        { requested: "tabindex", expected: "tabIndex" },
                        { requested: "readonly", expected: "readOnly" },
                        { requested: "colspan", expected: "colSpan" },
                        { requested: "frameborder", expected: "frameBorder" },
                        { requested: "rowspan", expected: "rowSpan" },
                        { requested: "textcontent", expected: "textContent" },
                        { requested: "valuetype", expected: "valueType" },
                    ];

                for (var i in attributes) {
                    node[attributes[i].expected] = propertyValue;

                    //act
                    var result = domProp.get(node, attributes[i].requested);

                    //assert
                    assert.equal(result, propertyValue);
                }
            }
        },
        "set()": {
            "node + string + string": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyName = "name",
                    propertyValue = "the value";

                //act
                domProp.set(node, propertyName, propertyValue);

                //assert
                assert.equal(node[propertyName], propertyValue);

            },
            "string + string + string": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyName = "name",
                    propertyValue = "the value",
                    id = getId();

                node.id = id;
                container.appendChild(node);

                //act
                domProp.set(id, propertyName, propertyValue);

                //assert
                assert.equal(node[propertyName], propertyValue);
            },
            "setting attribute that is aliased in JavaScript sets correct attribute": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyValue = 42,
                    propertyNames =
                    [
                        { requested: "class", expected: "className" },
                        { requested: "for", expected: "htmlFor" },
                        { requested: "tabindex", expected: "tabIndex" },
                        { requested: "readonly", expected: "readOnly" },
                        { requested: "colspan", expected: "colSpan" },
                        { requested: "frameborder", expected: "frameBorder" },
                        { requested: "rowspan", expected: "rowSpan" },
                        { requested: "textcontent", expected: "textContent" },
                        { requested: "valuetype", expected: "valueType" },
                    ];

                for (var i in propertyNames) {

                    //act
                    domProp.set(node, propertyNames[i].requested, propertyValue);

                    //assert
                    assert.equal(node[propertyNames[i].expected], propertyValue);
                }
            },
            "node + map": function () {
                //arrange
                var node = document.createElement("div"),
                    map =
                    {
                        name: "the name",
                        id: getId()
                    };

                //act
                domProp.set(node, map);

                //assert
                for (var i in map) {
                    assert.equal(node[i], map[i]);
                }

            },
            "node + 'style' + any delegates to dojo/dom-style::set()": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyValue = {
                        width: "100px"
                    },
                    mock = sinon.spy(domStyle, "set");

                //act
                domProp.set(node, "style", propertyValue);

                //assert
                assert.isTrue(mock.calledWith(node, propertyValue));

                mock.restore();

            },
            "node + 'innerHTML' + string": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyValue = "<h1>Foo</h1>";

                //act
                domProp.set(node, "innerHTML", propertyValue);

                //assert
                assert.equal(node.innerHTML, propertyValue);

            },
            "<tag whose innerHTML is readonly in IE/> + 'innerHTML' + string": function () {
                //arrange
                var map =
                    [
                        { tag: "table", markup: "<tbody><tr><td>Foo</td></tr></tbody>" },
                        { tag: "tbody", markup: "<tr><td>Bar</td></tr>" },
                        { tag: "tfoot", markup: "<tr><td>Baz</td></tr>" },
                        { tag: "thead", markup: "<tr><td>Buz</td></tr>" },
                        { tag: "tr", markup: "<td>quux</td>" },
                        { tag: "title", markup: "the value" }
                    ];

                for (var i in map) {
                    var node = document.createElement(map[i].tag);

                    //act
                    domProp.set(node, "innerHTML", map[i].markup);

                    //assert
                    assert.equal(node.innerHTML, map[i].markup);
                }

            },
            "node + 'textContent' + string": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyValue = "Foo";

                //act
                domProp.set(node, "textContent", propertyValue);

                //assert
                assert.equal(node.textContent, propertyValue);
            },
            "if property is already assigned as a function and 'null' is set, then function removed": function () {
                //arrange
                var node = document.createElement("div"),
                    oldPropertyValue = function () { },
                    propertyName = "foofunction";

                node[propertyName] = oldPropertyValue;

                //act
                domProp.set(node, propertyName, null);


                //assert
                assert.isNull(node[propertyName]);

            },
            "if property is already assigned as a function, then dojo/_base/connect::disconnect() called with correct args": function () {
                //arrange
                var node = document.createElement("div"),
                    oldPropertyValue = function () { },
                    newPropertyValue = function () { },
                    propertyName = "foofunction",
                    mock = sinon.spy(connect, "disconnect");

                domProp.set(node, propertyName, oldPropertyValue);

                //act
                domProp.set(node, propertyName, newPropertyValue);

                //assert
                assert.isTrue(mock.called);

                mock.restore();

            },
            "node + string + function": function () {
                //arrange
                var node = document.createElement("div"),
                    propertyValue = function () { },
                    propertyName = "foofunction",
                    mock = sinon.spy(connect, "connect");

                //act
                domProp.set(node, propertyName, propertyValue);

                //assert
                assert.isTrue(mock.calledWith(node, propertyName, propertyValue));

                mock.restore();
            }

        }
    });
});
