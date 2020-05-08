define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../../_base/NodeList',
    'dojo',
    '../../../query',
    '../../../on',
    '../../../dom-construct',
    '../../../dom-attr',
    '../../../string',
    '../../../parser',
    '../../../_base/array'
], function (registerSuite, assert, sinon, NodeList, dojo, query, on, domConstruct, domAttr) {
    registerSuite({
        name: 'dojo/_base/NodeList',

        "connect()": (function () {
            return {
                "delegates to dojo.connect for each node": function () {
                    //arrange
                    var nodes =
                        [
                            document.createElement("div"),
                            document.createElement("div")
                        ],
                        nodeList = new NodeList(nodes),
                        origDojoConnect = dojo.connect,
                        callback = function () { },
                        event = "onClick",
                        capturedArguments = [],
                        capturedThis = [];

                    dojo.connect = function () {
                        capturedArguments.push(arguments);
                        capturedThis = this;
                    }

                    //act
                    nodeList.connect(event, callback);

                    //assert
                    for (var i in nodes) {
                        assert.propertyVal(capturedArguments,
                            { 0: event, 1: callback });
                        assert.propertyVal(capturedThis, nodes[i]);
                    }

                    dojo.connect = origDojoConnect;

                }
            };
        })(),
        "coords()": (function () {
            var coords =
                [
                    {
                        l: 1,
                        t: 2,
                        w: 3,
                        h: 4
                    }, {
                        l: 10,
                        t: 11,
                        w: 12,
                        h: 13
                    }
                ],
                nodes,
                nodeList,
                container;

            return {
                setup: function () {
                    nodes = [];

                    container = document.createElement("div");
                    for (var i in coords) {
                        var node = document.createElement("div");
                        nodes.push(node);
                        container.appendChild(node);
                        node.style.position = "absolute";
                        node.style.left = coords[i].l + "px";
                        node.style.top = coords[i].t + "px";
                        node.style.width = coords[i].w + "px";
                        node.style.height = coords[i].h + "px";
                    }

                    nodeList = new NodeList(nodes);

                    document.body.appendChild(container);
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "returns array of the nodelist's nodes' coordinates": function () {
                    //arrange


                    //act
                    var result = nodeList.coords();

                    //assert
                    for (var i in coords) {
                        for (var j in coords[i]) {
                            assert.propertyVal(result[i], coords[i][j]);
                        }
                    }

                }
            };
        })(),
        "events": (function () {
            var events = [
                "blur", "focus", "change", "click", "error", "keydown", "keypress",
                "keyup", "load", "mousedown", "mouseenter", "mouseleave", "mousemove",
                "mouseout", "mouseover", "mouseup", "submit"
            ];
            return {
                "delegates wiring of event handlers for each to NodeList::connect with correct arguments": function () {

                    //arrange
                    var nodes =
                        [
                            document.createElement("input"),
                            document.createElement("div")
                        ],
                        nodeList = new NodeList(nodes),
                        capturedArgs = [],
                        mock = sinon.stub(nodeList, "connect"),
                        results = [];

                    for (var i in events) {
                        mock.withArgs("on" + events[i], events[i] + "0", events[i] + "1")
                            .returns(events[i] + "return");
                    }

                    //act
                    for (var i in events) {
                        results.push(nodeList["on" + events[i]](events[i] + "0", events[i] + "1"));
                    }

                    //assert
                    for (var i in events) {
                        assert.isTrue(mock.calledWith("on" + events[i], events[i] + "0", events[i] + "1"));
                        assert.include(results, events[i] + "return");
                    }

                }
            };
        })(),
        "validation tests": (function () {
            var firstSubContainer,
                secondSubContainer,
                thirdSubContainer,
                container,
                idIndex = 0,
                baseId = "_base/NodeList",
                sq100Id = baseId + idIndex++

            var tn;
            var c, t, s, fourElementNL;

            var listen = on; //alias of dojo/on

            function verify(/*dojo.NodeList*/nl, /*Array*/ids) {
                for (var i = 0, node; node = nl[i]; i++) {
                    assert.equal(node.id, ids[i]);
                }
                //Make sure lengths are equal.
                assert.equal(i, ids.length);
            }

            return {
                setup: function () {
                    container = document.createElement("div");

                    firstSubContainer = document.createElement("div");

                    var sq100 = document.createElement("div");
                    firstSubContainer.appendChild(sq100);
                    sq100.id = sq100Id;
                    sq100.style.width = "120px";
                    sq100.style.height = "130px";

                    secondSubContainer = document.createElement("div");

                    var frag = domConstruct.toDom(
                        '<h1>testing dojo.NodeList</h1>' +
                        '<div id="sq100-NodeList" style="position:absolute;left:100px;top:100px;width:100px;height:100px;">' +
                        '    100px square, abs' +
                        '</div>' +
                        '<div id="t-NodeList">' +
                        '    <span id="c1-NodeList">c1</span>' +
                        '</div>');


                    domConstruct.place(frag, secondSubContainer);

                    thirdSubContainer = document.createElement("div");
                    var frag = domConstruct.toDom(
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>' +
                        '<div></div>');
                    domConstruct.place(frag, thirdSubContainer);

                    container.appendChild(firstSubContainer);
                    container.appendChild(secondSubContainer);
                    container.appendChild(thirdSubContainer);
                    document.body.appendChild(container);
                    c = dojo.byId("c1-NodeList");
                    t = dojo.byId("t-NodeList");
                    s = dojo.byId("sq100-NodeList");
                    fourElementNL = new NodeList(c, t, c, t);
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "connect": function () {
                    var ih = "<div>" +
                            "    <span></span>" +
                            "</div>" +
                            "<span class='thud'>" +
                            "    <button>blah</button>" +
                            "</span>";

                    tn = document.createElement("div");
                    tn.innerHTML = ih;
                    document.body.appendChild(tn);

                    var ctr = 0;
                    var nl = query("button", tn).connect("onclick", function () {
                        ctr++;
                    });
                    nl[0].click();
                    assert.equal(1, ctr);
                    nl[0].click();
                    nl[0].click();
                    assert.equal(3, ctr);
                },
                "coords": function () {
                    var tnl = new NodeList(dojo.byId(sq100Id));
                    assert.isTrue(dojo.isArrayLike(tnl));
                    assert.equal(120, tnl.coords()[0].w, 120);
                    assert.equal(130, tnl.coords()[0].h, 130);
                    assert.equal(query("body *").coords().length, document.body.getElementsByTagName("*").length);
                    assert.equal(query("body *").position().length, document.body.getElementsByTagName("*").length);
                },

                // constructor tests
                "ctor": function () {
                    var nl = new NodeList();
                    nl.push(c);
                    assert.equal(nl.length, 1);
                },
                "ctorArgs": function () {
                    var nl = new NodeList(4);
                    nl.push(c);
                    assert.equal(nl.length, 5);
                },
                "ctorArgs2": function () {
                    var nl = new NodeList(c, t);
                    assert.equal(nl.length, 2);
                    assert.equal(nl[0], c);
                    assert.equal(nl[1], t);
                },
                // iteration and array tests
                "forEach": function () {
                    var lastItem;
                    var nl = new NodeList(c, t);
                    nl.forEach(function (i) { lastItem = i; });
                    assert.equal(lastItem, t);

                    var r = nl.forEach(function (i, idx, arr) {
                        assert.instanceOf(arr, NodeList);
                        assert.equal(arr.length, 2);
                    });
                    assert.instanceOf(r, NodeList);
                    assert.equal(nl, r);
                },

                "indexOf": function () {
                    assert.equal(fourElementNL.indexOf(c), 0);
                    assert.equal(fourElementNL.indexOf(t), 1);
                    assert.equal(fourElementNL.indexOf(null), -1);
                },

                "lastIndexOf": function () {
                    assert.equal(fourElementNL.lastIndexOf(c), 2);
                    assert.equal(fourElementNL.lastIndexOf(t), 3);
                    assert.equal(fourElementNL.lastIndexOf(null), -1);
                },

                "every": function () {
                    var ctr = 0;
                    var ret = fourElementNL.every(function () {
                        ctr++;
                        return true;
                    });
                    assert.equal(ctr, 4);
                    assert.isTrue(ret);

                    ctr = 0;
                    var ret = fourElementNL.every(function () {
                        ctr++;
                        return false;
                    });
                    assert.equal(ctr, 1);
                    assert.isFalse(ret);
                },

                "some": function () {
                    var ret = fourElementNL.some(function () {
                        return true;
                    });
                    assert.isTrue(ret);

                    var ret = fourElementNL.some(function (i) {
                        return (i.id == "t-NodeList");
                    });
                    assert.isTrue(ret);
                },

                "map": function () {
                    var ret = fourElementNL.map(function () {
                        return true;
                    });

                    for (var i = 0; i < ret.length; i++) {
                        assert.equal(ret[i], true);
                    }

                    verify(ret.end(), ["c1-NodeList", "t-NodeList", "c1-NodeList", "t-NodeList"]);

                    var cnt = 0;
                    var ret = fourElementNL.map(function () {
                        return cnt++;
                    });
                    // assert.equal([0, 1, 2, 3], ret);

                    assert.instanceOf(ret, NodeList);

                    // make sure that map() returns a NodeList
                    var sum = 0;
                    fourElementNL.map(function () { return 2; }).forEach(function (x) { sum += x; });
                    assert.equal(sum, 8);
                },

                "slice": function () {
                    var pnl = new NodeList(t, t, c);
                    assert.equal(pnl.slice(1).length, 2);
                    assert.equal(pnl.length, 3);
                    assert.equal(pnl.slice(-1)[0], c);
                    assert.equal(pnl.slice(-2).length, 2);
                    verify(pnl.slice(1).end(), ["t-NodeList", "t-NodeList", "c1-NodeList"]);
                },

                "splice": function () {
                    var pnl = new NodeList(t, t, c);

                    assert.equal(pnl.splice(1).length, 2);
                    assert.equal(pnl.length, 1);
                    pnl = new NodeList(t, t, c);
                    assert.equal(pnl.splice(-1)[0], c);
                    assert.equal(pnl.length, 2);
                    pnl = new NodeList(t, t, c);
                    assert.equal(pnl.splice(-2).length, 2);
                },

                "spliceInsert": function () {
                    // insert 1
                    var pnl = new NodeList(t, t, c);
                    pnl.splice(0, 0, c);
                    assert.equal(pnl.length, 4);
                    assert.equal(pnl[0], c);

                    // insert multiple
                    pnl = new NodeList(t, t, c);
                    pnl.splice(0, 0, c, s);
                    assert.equal(pnl.length, 5);
                    assert.equal(pnl[0], c);
                    assert.equal(pnl[1], s);
                    assert.equal(pnl[2], t);

                    // insert multiple at offset
                    pnl = new NodeList(t, t, c);
                    pnl.splice(1, 0, c, s);
                    assert.equal(pnl.length, 5);
                    assert.equal(pnl[0], t);
                    assert.equal(pnl[1], c);
                    assert.equal(pnl[2], s);
                    assert.equal(pnl[3], t);
                },

                "spliceDel": function () {
                    // clobbery 1
                    var pnl = new NodeList(c, t, s);
                    pnl.splice(0, 1);
                    assert.equal(pnl.length, 2);
                    assert.equal(pnl[0], t);

                    // clobber multiple
                    pnl = new NodeList(c, t, s);
                    pnl.splice(0, 2);
                    assert.equal(pnl.length, 1);
                    assert.equal(pnl[0], s);

                    // ...at an offset
                    pnl = new NodeList(c, t, s);
                    pnl.splice(1, 1);
                    assert.equal(pnl.length, 2);
                    assert.equal(pnl[0], c);
                    assert.equal(pnl[1], s);

                },

                "spliceInsertDel": function () {
                    // clobbery 1
                    var pnl = new NodeList(c, t, s);
                    pnl.splice(1, 1, s);
                    assert.equal(pnl.length, 3);
                    assert.deepEqual(pnl, new NodeList(c, s, s));

                    pnl = new NodeList(c, t, s);
                    pnl.splice(1, 2, s);
                    assert.equal(pnl.length, 2);
                    assert.deepEqual(pnl, new NodeList(c, s));
                },

                // sub-search
                "queryTest": function () {
                    var pnl = new NodeList(t);
                    assert.equal(pnl.query("span")[0], c);
                    if (name != "t-NodeList") {
                        // this gets messed up by new DOM nodes the second time around
                        assert.equal(query(":last-child", secondSubContainer)[0], t);
                        assert.equal(query(":last-child", secondSubContainer)[1], c);
                        assert.equal(pnl.query().length, 1);
                        verify(pnl.query("span").end(), ["t-NodeList"]);
                    }
                },

                "filter": function () {
                    if (name != "t-NodeList") {
                        // this gets messed up by new DOM nodes the second time around
                        assert.equal(c, query(":first-child", secondSubContainer).filter(":last-child")[0]);
                        assert.equal(query("*", container).filter(function (n) { return (n.nodeName.toLowerCase() == "span"); }).length, 1);

                        var filterObj = {
                            filterFunc: function (n) {
                                return (n.nodeName.toLowerCase() == "span");
                            }
                        };
                        assert.equal(query("*", container).filter(filterObj.filterFunc).length, 1);
                        assert.equal(query("*", container).filter(filterObj.filterFunc, filterObj).length, 1);
                        verify((new NodeList(t)).filter("span").end(), ["t-NodeList"]);
                    }
                },

                // layout DOM functions
                "position": function () {
                    var tnl = new NodeList(dojo.byId('sq100-NodeList'));
                    assert.isTrue(dojo.isArrayLike(tnl));
                    assert.equal(tnl.position()[0].w, 100);
                    assert.equal(tnl.position()[0].h, 100);
                    assert.equal(query("body *").position().length, document.body.getElementsByTagName("*").length);
                },

                "styleGet": function () {
                    // test getting
                    var tnl = new NodeList(s);
                    assert.equal(tnl.style("opacity")[0], 1);
                    tnl.push(t);
                    dojo.style(t, "opacity", 0.5);
                    assert.equal(tnl.style("opacity").slice(-1)[0], 0.5);
                    tnl.style("opacity", 1);
                },

                "styleSet": function () {
                    // test setting
                    var tnl = new NodeList(s, t);
                    tnl.style("opacity", 0.5);
                    assert.equal(dojo.style(tnl[0], "opacity"), 0.5);
                    assert.equal(dojo.style(tnl[1], "opacity"), 0.5);
                    // reset
                    tnl.style("opacity", 1);
                },

                "style": function () {
                    var tnl = new NodeList(s, t);
                    tnl.style("opacity", 1);
                    assert.equal(tnl.style("opacity")[0], 1);
                    dojo.style(t, "opacity", 0.5);
                    assert.equal(tnl.style("opacity")[0], 1.0);
                    assert.equal(tnl.style("opacity")[1], 0.5);
                    // reset things
                    tnl.style("opacity", 1);
                },

                "addRemoveClass": function () {
                    var tnl = new NodeList(s, t);
                    tnl.addClass("a");
                    assert.equal(s.className, "a");
                    assert.equal(t.className, "a");
                    tnl.addClass("a b");
                    assert.equal(s.className, "a b");
                    assert.equal(t.className, "a b");
                    tnl.addClass(["a", "c"]);
                    assert.equal(s.className, "a b c");
                    assert.equal(t.className ,"a b c");
                    tnl.removeClass();
                    assert.equal(s.className, "");
                    assert.equal(t.className, "");
                    tnl.addClass("    a");
                    assert.equal(s.className, "a");
                    assert.equal(t.className, "a");
                    tnl.addClass(" a  b ");
                    assert.equal(s.className, "a b");
                    assert.equal(t.className, "a b");
                    tnl.addClass(" c  b a ");
                    assert.equal(s.className, "a b c");
                    assert.equal(t.className, "a b c");
                    tnl.removeClass(" b");
                    assert.equal(s.className, "a c");
                    assert.equal(t.className, "a c");
                    tnl.removeClass("a b ");
                    assert.equal(s.className, "c");
                    assert.equal(t.className, "c");
                    tnl.removeClass(["a", "c"]);
                    assert.equal(s.className, "");
                    assert.equal(t.className, "");
                    tnl.addClass("a b c");
                    tnl.replaceClass("d e", "a b");
                    assert.equal(s.className, "c d e", "class is c d e after replacing a b with d e");
                    assert.equal(t.className, "c d e", "class is c d e after replacing a b with d e");
                    tnl.replaceClass("f", "d");
                    assert.equal(s.className, "c e f", "class is c e f after replacing d with f");
                    assert.equal(t.className, "c e f", "class is c e f after replacing d with f");
                    tnl.replaceClass("d");
                    assert.equal(s.className, "d");
                    assert.equal(t.className, "d");
                    tnl.removeClass();
                    assert.equal(s.className, "", "empty class");
                    assert.equal(t.className, "", "empty class");
                },

                "concat": function () {
                    if (name != "t") {
                        // this isn't supported in the new query method
                        var spans = query("span", secondSubContainer);
                        var divs = query("div", secondSubContainer);
                        var cat = spans.concat(divs);
                        console.debug(cat);
                        assert.isTrue(cat.constructor == NodeList || cat.constructor == NodeList);
                        assert.equal(cat.length, (divs.length + spans.length));
                        verify(cat.end(), ["c1-NodeList"]);
                    }
                },

                "concat2": function () {
                    var spans = query("span", secondSubContainer);
                    var divs = query("div", secondSubContainer);
                    assert.instanceOf(spans.concat([]), NodeList);
                },

                "concat3": function () {
                    var spans = query("span", secondSubContainer);
                    var divs = query("div", secondSubContainer);
                    var cat = spans.concat(divs);

                    assert.instanceOf(cat, NodeList);
                },
                "concat4": function () {
                    var res = (new dojo.NodeList()).concat([]);
                    assert.equal(res.length, 0);
                },
                "place": function () {
                    var ih = "<div><span></span></div><span class='thud'><b>blah</b></span>";

                    var tn = document.createElement("div");
                    tn.innerHTML = ih;
                    dojo.body().appendChild(tn);
                    var nl = query("b", tn).place(tn, "first");
                    assert.instanceOf(nl,NodeList);
                    assert.equal(nl.length, 1);
                    assert.equal(nl[0].nodeName.toLowerCase(), "b");
                    assert.equal(nl[0].parentNode, tn);
                    assert.equal(nl[0], tn.firstChild);
                },

                "orphan": function () {
                    var ih = "<div><span></span></div><span class='thud'><b>blah</b></span>";

                    var tn = document.createElement("div");
                    tn.innerHTML = ih;
                    dojo.body().appendChild(tn);
                    var nl = query("span", tn).orphan();
                    assert.instanceOf(nl, NodeList);

                    assert.equal(nl.length, 2);
                    assert.equal(tn.getElementsByTagName("*").length, 1);

                    tn.innerHTML = ih;
                    var nl = query("*", tn).orphan("b");
                    assert.equal(nl.length, 1);
                    assert.equal(nl[0].innerHTML, "blah");
                },

                "adopt": function () {
                    var div = query(dojo.create("div"));
                    div.adopt(dojo.create("span"));
                    div.adopt(dojo.create("em"), "first");
                    assert.equal(2, query("*", div[0]).length, 2);
                    assert.equal(div[0].firstChild.tagName.toLowerCase(), "em");
                    assert.equal(div[0].lastChild.tagName.toLowerCase(), "span");
                },

                "addContent": function () {
                    //text content
                    var tn = document.createElement("div");
                    var nl = query(tn).addContent("some text content");

                    assert.equal(nl[0].childNodes.length, 1);
                    assert.equal(nl[0].firstChild.nodeValue, "some text content");

                    //move a node
                    var mNode = document.createElement("span");
                    mNode.id = "addContent1";
                    mNode.innerHTML = "hello";
                    dojo.body().appendChild(mNode);
                    assert.isNotNull(dojo.byId("addContent1"));

                    nl.addContent(mNode);
                    assert.isNull(dojo.byId("addContent1"));
                    assert.equal(nl[0].lastChild.id, "addContent1");

                    //put in multiple content/clone node
                    tn.innerHTML = '<select><option name="second"  value="second" selected>second</option></select>';
                    nl = query("select", tn).addContent('<option name="first" value="first">first</option>', "first");
                    nl.forEach(function (node) {
                        assert.equal(node.options[0].value, "first");
                        assert.isFalse(node.options[0].selected);
                    });

                    //Some divs to use for addContent template actions.
                    var templs = domConstruct.toDom('<div class="multitemplate"></div><div class="multitemplate"></div>');
                    domConstruct.place(templs, document.body);
                    templs = query(".multitemplate");

                    //templateFunc test
                    templs.addContent({
                        template: '<b>[name]</b>',
                        templateFunc: function (str, obj) { return str.replace(/\[name\]/g, obj.name); },
                        name: "bar"
                    });

                    var bolds = templs.query("b");
                    assert.equal(bolds.length, 2);
                    bolds.forEach(function (node) {
                        assert.equal(node.innerHTML, "bar");
                    });

                    //template with dojo.string.substitute used.
                    templs.addContent({
                        template: "<p>${name}</p>",
                        name: "baz"
                    });

                    var ps = templs.query("p");
                    assert.equal(ps.length, 2);
                    ps.forEach(function (node) {
                        assert.equal(node.innerHTML, "baz");
                    });

                    //Try a dojo.declared thing.
                    dojo.declare("dojo.testsDOH.Mini", null, {
                        constructor: function (args, node) {
                            dojo.mixin(this, args);
                            node.innerHTML = this.name;
                            this.domNode = node;
                        },
                        name: ""
                    });

                    templs.addContent({
                        template: '<i dojoType="dojo.testsDOH.Mini">cool</i>',
                        parse: true
                    });


                    var declaredNodes = templs.query("[dojoType]");

                    assert.equal(declaredNodes.length, 2);
                    dojo.forEach(declaredNodes, function (node) {
                        assert.equal(node.innerHTML, "cool");
                    });

                    //Get rid of the junk used for template testing.
                    templs.orphan();
                },


                "on": function () {
                    var ctr = 0;
                    dojo.body().appendChild(tn);
                    var nl = query("button", tn);
                    var handle = nl.on("click", function () {
                        ctr++;
                    });
                    nl[0].click();
                    assert.equal(ctr, 1);
                    var inButton = nl[0].appendChild(document.createElement("span"));
                    listen.emit(nl[0], "click", {
                    });
                    listen.emit(inButton, "click", {
                        bubbles: true
                    });
                    listen.emit(inButton, "click", {
                        bubbles: false
                    });
                    assert.equal(ctr, 3);
                    handle.remove();
                    listen.emit(nl[0], "click", {
                    });
                    assert.equal(ctr, 3);
                },
                "onDelegate": function () {
                    var ctr = 0;
                    dojo.body().appendChild(tn);
                    var nl = query(".thud", tn);
                    var bl = query("button", tn);
                    var handle = nl.on("button:click", function () {
                        assert.equal(bl[0], this);
                        ctr++;
                    });
                    assert.equal(ctr, 0);
                    listen.emit(nl[0], "click", {
                    });
                    listen.emit(bl[0], "click", {
                        bubbles: true
                    });
                    assert.equal(ctr, 1);
                    handle.remove();
                    listen.emit(bl[0], "click", {
                        bubbles: true
                    });
                    assert.equal(ctr, 1);
                    // listen and on should behave the same
                    query(tn).on(".thud:click, .thud button:custom", function () {
                        ctr++;
                    });
                    listen.emit(bl[0], "click", {
                        bubbles: true
                    });
                    assert.equal(ctr, 2);
                    listen.emit(bl[0], "click", {
                        bubbles: false
                    });
                    assert.equal(ctr, 2);
                    listen.emit(bl[0], "custom", {
                        bubbles: true
                    });
                    assert.equal(ctr, 3);
                    listen.emit(bl[0], "mouseout", {
                        bubbles: true
                    });
                    assert.equal(ctr, 3);
                    bl[0].click();
                    assert.equal(ctr, 4);
                },

                "at": function () {
                    var divs = query("div", thirdSubContainer);
                    var at0 = divs.at(0);
                    assert.equal( at0[0], divs[0]);
                    if (name != "t-NodeList") {
                        // this gets messed up by new DOM nodes the second time around

                        var at1 = divs.at(1, 3, 5);
                        assert.equal(at1[0], divs[1]);
                        assert.equal(at1[1], divs[3]);
                        assert.equal(at1[2], divs[5]);

                        var at2 = divs.at(3, 6, 9);
                        assert.equal(at2.length, 2);

                        var at3 = divs.at(3, 6).at(1);
                        assert.equal(at3[0], divs[6]);

                        var ending = divs.at(0).end();
                        assert.equal(ending, divs);

                        var at4 = divs.at(-1);
                        assert.equal(at4[0], divs[divs.length - 1]);

                        var at5 = divs.at(1, -1);
                        assert.equal(divs[1], at5[0]);
                        assert.equal(divs[divs.length - 1], at5[1]);
                    }

                },

                "attr": function () {
                    var divs = query("div");
                    var ids = divs.attr("id");
                },

                "_adaptAsForEach": function () {
                    var passes = false;
                    var count = 0;
                    var i = {
                        setTrue: function (node) {
                            count++;
                            passes = true;
                        }
                    };
                    NodeList.prototype.setTrue = NodeList._adaptAsForEach(i.setTrue, i);
                    var divs = query("div").setTrue();
                    assert.isTrue(passes);
                    assert.equal(divs.length, count);
                },

                "instantiate": function () {
                    //Insert some divs to use for test
                    domConstruct.place('<p id="thinger">Hi</p><p id="thinger2">Hi</p>', document.body);

                    var test = 0;
                    dojo.declare("testsDOH._base.NodeList.some.Thing", null, {
                        foo: "baz",
                        constructor: function (props, node) {
                            dojo.mixin(this, props);
                            assert.equal(this.foo, "bar", test++);
                        }
                    });

                    query("#thinger").instantiate(testsDOH._base.NodeList.some.Thing, {
                        foo: "bar"
                    });

                    query("#thinger2").instantiate("testsDOH._base.NodeList.some.Thing", {
                        foo: "bar"
                    });

                    assert.equal(test, 2);

                    //clean up the divs inserted for the test.
                    query("#thinger, #thinger2").orphan();
                },

                "removeAttr": function () {
                                        // buildup
                    domConstruct.place('<p id="attr" title="Foobar">Hi</p>', document.body);

                    var n = query("#attr");

                    assert.isTrue(dojo.hasAttr(n[0], "title"));

                    var t = n.attr("title");
                    assert.equal(t, "Foobar");

                    n.removeAttr("title");

                    t = domAttr.has(n[0], "title");
                    assert.isFalse(t);

                    // cleanup
                    n.orphan();
                }
            };
        })()
    });
});
