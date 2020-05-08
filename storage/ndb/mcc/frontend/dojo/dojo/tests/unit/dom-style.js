define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    'dojo/sniff',
    '../../dom-style',
    '../../dom-construct'
], function (registerSuite, assert, sinon, has, domStyle, domConstruct) {
    var baseId = "dom-style",
        uniqueId = 0;

    function getId() {
        return baseId + uniqueId++;
    }

    var container;

    registerSuite({
        name: 'dojo/dom-style',

        setup: function() {
            container = document.createElement("div");
            document.body.appendChild(container);
        },

        teardown: function() {
            document.body.removeChild(container);
        },

        "getComputedStyle()": {
                "gets expected result": function () {
                    //arrange
                    var node = document.createElement("div"),
                        map = {
                            position: "relative",
                            width: "100px",
                            height: "125px"
                        };

                    for (var i in map) {
                        node.style[i] = map[i];
                    }

                    container.appendChild(node);

                    //act
                    var result = domStyle.getComputedStyle(node);

                    //assert
                    for (var i in map) {
                        assert.equal(result[i], map[i]);
                    }
                },
                "does not throw on frame elements": function () {
                    var node = document.createElement("div"),
                        frame = document.createElement("iframe"),
                        frameDoc;

                    node.appendChild(frame);
                    container.appendChild(node);

                    domStyle.set(frame, "display", "none");

                    frameDoc = frame.contentWindow.document

                    frameDoc.open();
                    frameDoc.write("<!doctype html><html><body><div>Test</div></body></html>");
                    frameDoc.close();

                    assert.isNotNull(domStyle.getComputedStyle(frameDoc.body));
                }
            },
        "toPixelValue()": {
                "node + undefined": function () {
                    //arrange
                    var node = document.createElement("div");

                    container.appendChild(node);

                    //act
                    var result = domStyle.toPixelValue(node, undefined);

                    //assert
                    assert.equal(result, 0);

                },
                "node + 'medium'": function () {
                    //arrange
                    if (!has("ie")) {
                        this.skip("IE Only");
                    }

                    var node = document.createElement("div");

                    //act
                    var result = domStyle.toPixelValue(node, "medium");

                    //assert
                    assert.equal(result, 4);

                },
                "node + '##px'": function () {
                    //arrange
                    var node = document.createElement("div"),
                        value = 42;

                    //act
                    var result = domStyle.toPixelValue(node, value + "px");

                    //assert
                    assert.equal(result, value);
                }
            },
        "get()": {
                "string + string": function () {
                    //arrange
                    var node = document.createElement("div"),
                        id = getId(),
                        style = "width",
                        value = 42;

                    container.appendChild(node);
                    node.id = id;
                    node.style[style] = value + "px";

                    //act
                    var result = domStyle.get(node, style);

                    //assert
                    assert.equal(result, value);

                },
                "node + string": function () {
                    //arrange
                    var node = document.createElement("div"),
                        style = "width",
                        value = 42;

                    container.appendChild(node);
                    node.style[style] = value + "px";

                    //act
                    var result = domStyle.get(node, style);

                    //assert
                    assert.equal(result, value);
                },
                "node + 'opacity'": function () {
                    if (has("ie") < 9 || (has("ie") < 10 && has("quirks"))) {
                        this.skip("skipped for IE 8- && IE 9, 10 in quirks")
                    } else {
                        //arrange
                        var node = document.createElement("div"),
                            style = "opacity",
                            value = 0.3;

                        container.appendChild(node);
                        node.style[style] = value;

                        //act
                        var result = domStyle.get(node, style);

                        //assert
                        assert.equal(parseFloat(result).toFixed(4), value.toFixed(4));
                    }
                },
                "node + alias of float": function () {
                    //arrange
                    var styles =
                        [
                            "cssFloat",
                            "styleFloat",
                            "float"
                        ],
                        node = document.createElement("div"),
                        value = 'right';

                    container.appendChild(node);
                    node.style.cssFloat = value;

                    for (var i in styles) {
                        //act
                        var result = domStyle.get(node, styles[i]);

                        //assert
                        assert.equal(result, value);
                    }

                },
                "node + height when height:'auto'": function () {
                    //arrange
                    var node = document.createElement("div"),
                        style = "width",
                        value = "auto";

                    container.appendChild(node);
                    node.style[style] = value;

                    //act
                    var result = domStyle.get(node, style);

                    //assert
                    assert.notEqual(result, value);
                    assert.isNumber(result);
                },
                "node + width when width:'auto'": function () {
                    //arrange
                    var node = document.createElement("div"),
                        style = "height",
                        value = "auto";

                    container.appendChild(node);
                    node.style[style] = value;

                    //act
                    var result = domStyle.get(node, style);

                    //assert
                    assert.notEqual(result, value);
                    assert.isNumber(result);
                },
                "node + 'fontWeight' when fontWeight = 400": function () {
                    //arrange
                    if (has("ie") || has("trident")) {
                        var node = document.createElement("div"),
                            style = "fontweight",
                            value = 400;

                        container.appendChild(node);
                        node.style[style] = value;

                        //act
                        var result = domStyle.get(node, style);

                        //assert
                        assert.equal(result, "normal");
                    } else {
                        this.skip("for has('ie') and has('trident') only");
                    }
                },
                "node + 'fontWeight' when fontWeight = 700": function () {
                    //arrange
                    if (!has("ie") && !has("trident")) {
                        this.skip("for has('ie') and has('trident') only");
                    } else {
                        var node = document.createElement("div"),
                            style = "fontweight",
                            value = 700;

                        container.appendChild(node);
                        node.style[style] = value;

                        //act
                        var result = domStyle.get(node, style);

                        //assert
                        assert.equal(result, "bold");
                    }
                },
                "node + style with measurement returned as number of px": function () {
                    //arrange
                    var styles =
                        [
                            "margin",
                            "padding",
                            "left",
                            "top",
                            "width",
                            "height",
                            "max-width",
                            "min-width",
                            "offset-width"
                        ],
                        node = document.createElement("div"),
                        value = 10;

                    container.appendChild(node);

                    for (var i in styles) {
                        node.style[styles[i]] = value + "px";

                        //act
                        var result = domStyle.get(node, styles[i]);

                        //assert
                        assert.equal(result, value);

                    }

                },
                "node": function () {
                    //arrange
                    var node = document.createElement("div"),
                        map = {
                            position: "relative",
                            width: "100px",
                            height: "125px"
                        };

                    for (var i in map) {
                        node.style[i] = map[i];
                    }

                    container.appendChild(node);

                    //act
                    var result = domStyle.get(node);

                    //assert
                    for (var i in map) {
                        assert.equal(result[i], map[i]);
                    }
                }
            },
        "set()": {
                "string + string + string": function () {
                    //arrange
                    var node = document.createElement("div"),
                        id = getId(),
                        style = "width",
                        value = "10px";

                    container.appendChild(node);

                    node.id = id;

                    //act
                    domStyle.set(id, style, value);

                    //assert
                    assert.equal(node.style[style], value);
                },
                "node + string + string": function () {
                    //arrange
                    var node = document.createElement("div"),
                        style = "width",
                        value = "10px";

                    container.appendChild(node);

                    //act
                    domStyle.set(node, style, value);

                    //assert
                    assert.equal(node.style[style], value);
                },
                "node + map": function () {

                    //arrange
                    var node = document.createElement("div"),
                        map = {
                            position: "relative",
                            width: "100px",
                            height: "125px"
                        };

                    container.appendChild(node);

                    //act
                    domStyle.set(node, map);

                    //assert
                    for (var i in map) {
                        assert.equal(node.style[i], map[i]);
                    }
                },
                "node + aliases of float": function () {
                    //arrange
                    var styles =
                        [
                            "cssFloat",
                            "styleFloat",
                            "float"
                        ],
                        node = document.createElement("div"),
                        value = "right";

                    container.appendChild(node);
                    node.style.float = value;

                    for (var i in styles) {
                        //act
                        domStyle.set(node, styles[i], value);

                        //assert
                        assert.equal(node.style.cssFloat, value);

                        node.style.cssFloat = "none";
                    }
                },
                "node + opacity + number": function() {
                    //arrange
                    var node = document.createElement("div"),
                        style = "opacity",
                        value = 0.3

                    container.appendChild(node);

                    //act
                    domStyle.set(node, style, value);

                    //assert
                    assert.equal(node.style[style], value);
                },
                "node": function () {
                    //arrange
                    var node = document.createElement("div"),
                        map = {
                            position: "relative",
                            width: "100px",
                            height: "125px"
                        };

                    for (var i in map) {
                        node.style[i] = map[i];
                    }

                    container.appendChild(node);

                    //act
                    var result = domStyle.set(node);

                    //assert
                    for (var i in map) {
                        assert.equal(result[i], map[i]);
                    }
                }
            },
        "validation tests": (function () {
            var container,
                node,
                trow;

            return {
                setup: function () {
                    container = document.createElement("div");
                    node = domConstruct.toDom('<div style="padding: 1px 2px 3px 4px;"></div>');
                    var table = domConstruct.toDom('<table><tbody><tr id="trow"><td>Col A</td><td>Col B</td></tr></tbody></table>');

                    document.body.appendChild(container);
                    container.appendChild(node);
                    container.appendChild(table);

                    trow = document.getElementById("trow");
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "getComputedStyle": function () {
                    var s = domStyle.getComputedStyle(node);
                    assert.isTrue(s !== null);
                    // Create a node on the fly,
                    // IE < 9 has issue with currentStyle when creating elements on the fly.
                    node = document.createElement('div');
                    domStyle.set(node, 'nodeStyle');
                    s = domStyle.getComputedStyle(node);
                    assert.isTrue(s !== null);
                },
                "getWidth": function () {
                    // see http://bugs.dojotoolkit.org/ticket/17962
                    var rowWidth = domStyle.get(trow, "width");
                    assert.isTrue(rowWidth > 0, "width: " + rowWidth);
                }

            };
        })(),
    });
});
