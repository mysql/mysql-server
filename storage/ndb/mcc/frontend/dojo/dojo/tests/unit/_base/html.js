define([
    'intern!object',
    'intern/chai!assert',
  '../../../_base/html',
  '../../../dom-attr',
  '../../../dom',
  '../../../dom-style',
  '../../../dom-prop',
  '../../../dom-class',
  '../../../dom-construct',
  '../../../dom-geometry',
], function (registerSuite, assert,
  dojo, domAttr, dom, style, prop, cls, ctr,
  geom) {
    registerSuite({
        name: 'dojo/_base/html',

        "direct delegates": function () {
            //mix-in dom
            assert.equal(dojo.byId, dom.byId);
            assert.equal(dojo.isDescendant, dom.isDescendant);
            assert.equal(dojo.setSelectable, dom.setSelectable);

            // mix-in dom-class
            assert.equal(dojo.hasClass, cls.contains);
            assert.equal(dojo.addClass, cls.add);
            assert.equal(dojo.removeClass, cls.remove);
            assert.equal(dojo.toggleClass, cls.toggle);
            assert.equal(dojo.replaceClass, cls.replace);

            // mix-in dom-construct
            assert.equal(dojo._toDom, ctr.toDom);
            assert.equal(dojo.toDom, ctr.toDom);
            assert.equal(dojo.place, ctr.place);
            assert.equal(dojo.create, ctr.create);

            // mix-in dom-geometry
            assert.equal(dojo._getPadExtents, geom.getPadExtents);
            assert.equal(dojo.getPadExtents, geom.getPadExtents);
            assert.equal(dojo._getBorderExtents, geom.getBorderExtents);
            assert.equal(dojo.getBorderExtents, geom.getBorderExtents);
            assert.equal(dojo._getPadBorderExtents, geom.getPadBorderExtents);
            assert.equal(dojo.getPadBorderExtents, geom.getPadBorderExtents);
            assert.equal(dojo._getMarginExtents, geom.getMarginExtents);
            assert.equal(dojo.getMarginExtents, geom.getMarginExtents);
            assert.equal(dojo._getMarginSize, dojo.getMarginSize, geom.getMarginSize);
            assert.equal(dojo._getMarginBox, geom.getMarginBox);
            assert.equal(dojo.getMarginBox, geom.getMarginBox);
            assert.equal(dojo.setMarginBox, geom.setMarginBox);
            assert.equal(dojo._getContentBox, geom.getContentBox);
            assert.equal(dojo.getContentBox, geom.getContentBox);
            assert.equal(dojo.setContentSize, geom.setContentSize);
            assert.equal(dojo.isBodyLtr, geom.isBodyLtr);
            assert.equal(dojo._isBodyLtr, geom.isBodyLtr);
            assert.equal(dojo.docScroll, geom.docScroll);
            assert.equal(dojo._docScroll, geom.docScroll);
            assert.equal(dojo._getIeDocumentElementOffset, geom.getIeDocumentElementOffset);
            assert.equal(dojo.getIeDocumentElementOffset, geom.getIeDocumentElementOffset);
            assert.equal(dojo.fixIeBiDiScrollLeft, geom.fixIeBiDiScrollLeft);
            assert.equal(dojo._fixIeBiDiScrollLeft, geom.fixIeBiDiScrollLeft);
            assert.equal(dojo.position, geom.position);

            // mix-in dom-prop
            assert.equal(dojo.getProp, prop.get);
            assert.equal(dojo.setProp, prop.set);

            // mix-in dom-style
            assert.equal(dojo.getStyle, style.get);
            assert.equal(dojo.setStyle, style.set);
            assert.equal(dojo.getComputedStyle, style.getComputedStyle);
            assert.equal(dojo.__toPixelValue, style.toPixelValue);
            assert.equal(dojo.toPixelValue, style.toPixelValue);
        },

        "dojo.attr": (function () {
            var node = document.createElement("div");
            var origDomAttrGet;
            var origDomAttrSet;

            return {
                setup: function () {
                    origDomAttrGet = domAttr.get;
                    origDomAttrSet = domAttr.set;
                },

                teardown: function () {
                    domAttr.get = origDomAttrGet;
                    domAttr.set = origDomAttrSet;
                },

                "node + string => dom-attr::get": function () {
                    //arrange
                    var result = {};
                    var attribute = "the attribute";

                    domAttr.get = function (node, attribute) {
                        result.node = node;
                        result.attribute = attribute;
                    }

                    //act
                    dojo.attr(node, attribute);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.attribute, attribute);

                },
                "node + dictionary => dom-attr::set": function () {
                    //arrange
                    var result = {};
                    var dictionary = {
                        foo: "bar",
                        baz: "quux"
                    };

                    domAttr.set = function (node, dictionary) {
                        result.node = node;
                        result.dictionary = dictionary;
                    }

                    //act
                    dojo.attr(node, dictionary);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.dictionary, dictionary);
                },
                "node + string + any => dom-attr::set": function () {
                    //arrange
                    var result = {};
                    var attribute = "the attribute";
                    var value = "the value";

                    domAttr.set = function (node, attribute, value) {
                        result.node = node;
                        result.attribute = attribute;
                        result.value = value;
                    }

                    //act
                    dojo.attr(node, attribute, value);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.attribute, attribute);
                    assert.equal(result.value, value);
                }
            };
        })(),

        "dojo.empty": (function () {
            var origCtrEmpty;
            var node = document.createElement("div");

            return {
                setup: function () {
                    origCtrEmpty = ctr.empty;
                },
                teardown: function () {
                    ctr.empty = origCtrEmpty;
                },

                "deletes to dom-construct::empty": function () {
                    //arrange
                    var result = {};
                    ctr.empty = function (node) {
                        result.node = node;
                    }

                    //act
                    dojo.empty(node);

                    //assert
                    assert.equal(result.node, node);

                }
            };
        })(),

        "dojo.destroy & dojo._destroyElement": (function () {
            var origCtrDestroy;
            var node = document.createElement("div");

            return {
                setup: function () {
                    origCtrDestroy = ctr.destroy;
                },
                teardown: function () {
                    ctr.destroy = origCtrDestroy;
                },
                "dojo.destroy => dom-geometry::destroy": function () {
                    //arrange
                    var result = {};
                    ctr.destroy = function (node) {
                        result.node = node;
                    }

                    //act
                    dojo.destroy(node);

                    //assert
                    assert.equal(result.node, node);

                },
                "dojo._destroyElement => dom-geometry::destroy": function () {
                    //arrange
                    var result = {};
                    ctr.destroy = function (node) {
                        result.node = node;
                    }

                    //act
                    dojo.destroy(node);

                    //assert
                    assert.equal(result.node, node);

                }
            };
        })(),

        "dojo.marginBox": (function () {
            var origGeomSetMarginBox;
            var origGeomGetMarginBox;
            var node = document.createElement("div");

            return {
                setup: function () {
                    origGeomSetMarginBox = geom.setMarginBox;
                    origGeomGetMarginBox = geom.getMarginBox;
                },
                teardown: function () {
                    geom.setMarginBox = origGeomSetMarginBox;
                    geom.getMarginBox = origGeomGetMarginBox;
                },
                "node + dictionary => dom-geometry::setMarginBox": function () {
                    //arrange
                    var box = {
                        foo: "bar",
                        baz: "quux"
                    }

                    var result = {};
                    geom.setMarginBox = function (node, box) {
                        result.node = node;
                        result.box = box;
                    }
                    //act
                    dojo.marginBox(node, box);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.box, box);
                },
                "node=> dom-geometry::getMarginBox": function () {
                    //arrange
                    var result = {};
                    geom.getMarginBox = function (node) {
                        result.node = node;
                    }
                    //act
                    dojo.marginBox(node);

                    //assert
                    assert.equal(result.node, node);
                }
            };
        })(),

        "dojo.contentBox": (function () {
            var origGeomSetContentSize;
            var origGeomGetContentBox;
            var node = document.createElement("div");

            return {
                setup: function () {
                    origGeomSetContentSize = geom.setContentSize;
                    origGeomSetContentSize = geom.getContentBox;
                },
                teardown: function () {
                    geom.setContentSize = origGeomSetContentSize;
                    geom.getContentBox = origGeomSetContentSize;
                },
                "node + dictionary => dom-geometry::setContentBox": function () {
                    //arrange
                    var box = {
                        foo: "bar",
                        baz: "quux"
                    }

                    var result = {};
                    geom.setContentSize = function (node, box) {
                        result.node = node;
                        result.box = box;
                    }
                    //act
                    dojo.contentBox(node, box);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.box, box);
                },
                "node=> dom-geometry::getContentBox": function () {
                    //arrange
                    var result = {};
                    geom.getContentBox = function (node) {
                        result.node = node;
                    }
                    //act
                    dojo.contentBox(node);

                    //assert
                    assert.equal(result.node, node);
                }
            };
        })(),

        "dojo.coords": (function () {
            var node = document.createElement("div");
            var origStyleGetComputedStyle;
            var origGeomGetMarginBox;
            var origDojoDeprecated;

            return {
                setup: function () {
                    origStyleGetComputedStyle = style.getComputedStyle;
                    origGeomGetMarginBox = geom.getMarginBox;
                    origGeomPosition = geom.position;
                    origDojoDeprecated = dojo.deprecated;
                },
                teardown: function () {
                    style.getComputedStyle = origStyleGetComputedStyle;
                    geom.getMarginBox = origGeomGetMarginBox;
                    geom.position = origGeomPosition;
                    dojo.deprecated = origDojoDeprecated;
                },

                "calls dojo.deprecated properly": function () {
                    //arrange
                    var result = {};
                    dojo.deprecated = function (behaviour, extra) {
                        result.behaviour = behaviour;
                        result.extra = extra;
                    }

                    //act
                    dojo.coords(node);

                    //assert
                    assert.equal(result.behaviour, "dojo.coords()");
                    assert.equal(result.extra, "Use dojo.position() or dojo.marginBox().");

                },

                "node + boolean": function () {
                    //arrange
                    var includeScroll = true;

                    var styleGetComputedStyleReturn = {
                        foo: "bar",
                        baz: "buzz"
                    };
                    var geomGetMarginBoxReturn = {
                        getMarginBoxFoo: "bar",
                        getMarginBoxBaz: "buzz"
                    };
                    var geomGetPositionReturn = {
                        x: 42,
                        y: 27
                    };
                    var styleGetComputedStyleArgs = {};
                    var geomGetMarginBoxArgs = {};
                    var geomPositionArgs = {};

                    style.getComputedStyle = function (node) {
                        styleGetComputedStyleArgs.node = node;
                        return styleGetComputedStyleReturn;
                    }

                    geom.getMarginBox = function (node, style) {
                        geomGetMarginBoxArgs.node = node;
                        geomGetMarginBoxArgs.style = style;
                        return geomGetMarginBoxReturn;
                    }
                    geom.position = function (node, includeScroll) {
                        geomPositionArgs.node = node;
                        geomPositionArgs.includeScroll = includeScroll;
                        return geomGetPositionReturn;
                    }

                    //act
                    var result = dojo.coords(node, includeScroll);

                    //assert
                    assert.equal(styleGetComputedStyleArgs.node, node);
                    assert.equal(geomGetMarginBoxArgs.node, node);
                    assert.equal(geomGetMarginBoxArgs.style, styleGetComputedStyleReturn);
                    assert.equal(geomPositionArgs.node, node);
                    assert.equal(geomPositionArgs.includeScroll, includeScroll);
                    assert.equal(result.getMarginBoxFoo, geomGetMarginBoxReturn.getMarginBoxFoo);
                    assert.equal(result.getMarginBoxBaz, geomGetMarginBoxReturn.getMarginBoxBaz);
                    assert.equal(result.x, geomGetPositionReturn.x);
                    assert.equal(result.y, geomGetPositionReturn.y);
                }
            };
        })(),
        "dojo.prop": (function () {
            var node = document.createElement("node");
            var origPropGet;
            var origPropSet;

            return {
                setup: function () {
                    origPropGet = prop.get;
                    origPropSet = prop.set;
                },
                teardown: function () {
                    prop.get = origPropGet;
                    prop.set = origPropSet;
                },
                "node + string => dojo-prop::get": function () {
                    //arrange
                    var property = "the property";
                    var result = {};
                    prop.get = function (node, property) {
                        result.node = node;
                        result.property = property;
                    };

                    //act
                    dojo.prop(node, property);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.property, property);

                },
                "node + dictionary => dojo-prop::set": function () {
                    //arrange
                    var property = {
                        foo: "bar",
                        baz: "buzz"
                    };
                    var result = {};
                    prop.set = function (node, property) {
                        result.node = node;
                        result.property = property;
                    };

                    //act
                    dojo.prop(node, property);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.property, property);
                },
                "node + string + string": function () {
                    //arrange
                    var property = "the property";
                    var value = "the value";
                    var result = {};
                    prop.set = function (node, property, value) {
                        result.node = node;
                        result.property = property;
                        result.value = value;
                    };

                    //act
                    dojo.prop(node, property, value);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.property, property);
                    assert.equal(result.value, value);
                }
            };
        })(),
        "dojo.style": (function () {
            var node = document.createElement("div");
            var origStyleGet;
            var origStyleSet;
            return {
                setup: function () {
                    origStyleGet = style.get;
                    origStyleSet = style.set;
                },
                teardown: function () {
                    style.get = origStyleGet;
                    style.set = origStyleSet;
                },
                "node => dom-style::get": function () {
                    //arrange

                    var args = {};
                    var returnValue = {
                        foo: "bar"
                    };
                    style.get = function (node) {
                        args.node = node;
                        return returnValue;
                    }

                    //act
                    var result = dojo.style(node);

                    //assert
                    assert.equal(args.node, node);
                    assert.equal(result, returnValue);

                },
                "node + string => dom-style::get": function () {
                    //arrange
                    var name = "the property";
                    var args = {};
                    var returnValue = {
                        foo: "bar"
                    };
                    style.get = function (node, name) {
                        args.node = node;
                        args.name = name;
                        return returnValue;
                    }

                    //act
                    var result = dojo.style(node, name);

                    //assert
                    assert.equal(args.node, node);
                    assert.equal(args.name, name);
                    assert.equal(result, returnValue);
                },
                "node + dictionary => dom-style::set": function () {
                    //arrange
                    var dictionary = {
                        foo: "bar",
                        baz: "buzz"
                    };
                    var result = {};
                    style.set = function (node, dictionary) {
                        result.node = node;
                        result.dictionary = dictionary;
                    }

                    //act
                    dojo.style(node, dictionary);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.dictionary, dictionary);

                },
                "node + string + string => dom-style::set": function () {
                    //arrange
                    var name = "the name";
                    var value = "the value";

                    var result = {};
                    style.set = function (node, name, value) {
                        result.node = node;
                        result.name = name;
                        result.value = value;
                    }

                    //act
                    dojo.style(node, name, value);

                    //assert
                    assert.equal(result.node, node);
                    assert.equal(result.name, name);
                    assert.equal(result.value, value);
                }
            };
        })()
    });
});
