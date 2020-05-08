define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../NodeList',
    '../../_base/lang',
    '../../dom-construct',
    '../../query',
    '../../NodeList-traverse'
], function (registerSuite, assert, sinon, NodeList, lang, domConstruct, query) {
    registerSuite({
        name: 'dojo/NodeList-traverse',

        "children()": {
            "returns children of nodes": function () {
                //arrange
                var containers =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    children =
                    [
                        [
                            document.createElement("span"),
                            document.createElement("span")
                        ], [
                            document.createElement("input"),
                            document.createElement("input")
                        ]
                    ],
                    allChildren = children[0].concat(children[1]),
                    nodeList = new NodeList(containers);

                for (var i in containers) {
                    for (var j in children[i]) {
                        containers[i].appendChild(children[i][j]);
                    }
                    document.body.appendChild(containers[i]);
                }

                //act
                var result = nodeList.children();

                //assert
                for (var i = 0; i < result.length; i++) {
                    assert.propertyVal(allChildren, result[i]);
                }

                for (var i in containers) {
                    document.body.removeChild(containers[i]);
                }

            },
            "filters children with query, if present": function () {
                //arrange
                var containers =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    children =
                    [
                        [
                            document.createElement("span"),
                            document.createElement("span")
                        ], [
                            document.createElement("input"),
                            document.createElement("input")
                        ]
                    ],
                    query = "span",
                    allChildren = children[0].concat(children[1]),
                    nodeList = new NodeList(containers);

                for (var i in containers) {
                    for (var j in children[i]) {
                        containers[i].appendChild(children[i][j]);
                    }
                    document.body.appendChild(containers[i]);
                }

                //act
                var result = nodeList.children(query);

                //assert
                for (var i = 0; i < result.length; i++) {
                    assert.equal(result[i].nodeName.toLowerCase(), "span");
                }
            }
        },

        "closest()": (function () {
            var root,
                containers,
                nodes,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers = [
                        document.createElement("span"),
                        document.createElement("span")
                    ];
                    nodes = [
                        [
                            document.createElement("b"),
                            document.createElement("b")
                        ], [
                            document.createElement("i"),
                            document.createElement("i")
                        ]
                    ];

                    nodeList = new NodeList(nodes[0].concat(nodes[1]));

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);

                        for (var j in nodes[i]) {
                            containers[i].appendChild(nodes[i][j]);
                        }
                    }


                },
                teardown: function () {
                    document.body.removeChild(root);
                },
                "returns closest parent that matches query": function () {
                    //arrange

                    //act
                    var result = nodeList.closest("span");

                    //assert
                    assert.equal(result.length, containers.length);
                    for (var i in containers) {
                        assert.propertyVal(result, containers[i]);
                    }

                },
                "returns empty NodeList if no parent matches query": function () {
                    //arrange

                    //act
                    var result = nodeList.closest("h1");

                    //assert
                    assert.equal(result.length, 0);
                }
            };
        })(),

        "parent()": (function () {
            var containers,
                expected,
                nodes,
                root,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers =
                        [
                            document.createElement("h1"),
                            document.createElement("h1")
                        ];
                    expected =
                        [
                            document.createElement("span"),
                            document.createElement("i")
                        ];
                    nodes =
                        [
                            document.createElement("b"),
                            document.createElement("b")
                        ];

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        containers[i].appendChild(expected[i]);
                        expected[i].appendChild(nodes[i]);
                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },
                "returns parents of nodes in NodeList": function () {
                    //arrange

                    //act
                    var result = nodeList.parent();

                    //assert
                    assert.equal(result.length, expected.length);
                    for (var i in expected) {
                        assert.propertyVal(result, expected[i]);
                    }
                },
                "filters returned parents by query, if present": function () {
                    //arrange

                    //act
                    var result = nodeList.parent("i");

                    //assert
                    assert.equal(result.length, 1);
                    assert.equal(result[0], expected[1]);
                }

            };
        })(),

        "parents()": (function () {
            var containers,
                expected,
                nodes,
                root,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers =
                        [
                            document.createElement("h1"),
                            document.createElement("h1")
                        ];
                    expected =
                        [
                            document.createElement("span"),
                            document.createElement("i")
                        ];
                    nodes =
                        [
                            document.createElement("b"),
                            document.createElement("b")
                        ];

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        containers[i].appendChild(expected[i]);
                        expected[i].appendChild(nodes[i]);
                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },
                "returns parents of nodes in NodeList": function () {
                    //arrange

                    //act
                    var result = nodeList.parents();

                    //assert
                    assert.equal(result.length, expected.length + containers.length + 3 /*root, body, html*/);
                    for (var i in expected) {
                        assert.propertyVal(result, expected[i]);
                    }
                    for (var i in containers) {
                        assert.propertyVal(result, containers[i]);
                    }
                    assert.propertyVal(result, root);
                    assert.propertyVal(result, document.body);
                    assert.propertyVal(result, document.body.parentElement);
                },
                "filters returned parents by query, if present": function () {
                    //arrange

                    //act
                    var result = nodeList.parents("i");

                    //assert
                    assert.equal(result.length, 1);
                    assert.propertyVal(result[0], expected[1]);

                }

            };
        })(),

        "siblings()": (function () {
            var containers,
                expected,
                nodes,
                root,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers =
                        [
                            document.createElement("h1"),
                            document.createElement("h1")
                        ];
                    expected =
                        [
                            document.createElement("span"),
                            document.createElement("i")
                        ];
                    nodes =
                        [
                            document.createElement("b"),
                            document.createElement("b")
                        ];

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        containers[i].appendChild(nodes[i]);
                        containers[i].appendChild(expected[i]);
                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },
                "returns siblings of nodes in NodeList": function () {
                    //arrange

                    //act
                    var result = nodeList.siblings();

                    //assert
                    assert.equal(result.length, expected.length);
                    for (var i in expected) {
                        assert.propertyVal(result, expected[i]);
                    }
                    for (var i in containers) {
                        assert.propertyVal(result, containers[i]);
                    }
                },
                "filters returned siblings by query, if present": function () {
                    //arrange

                    //act
                    var result = nodeList.siblings("i");

                    //assert
                    assert.equal(result.length, 1);
                    assert.propertyVal(result[0], expected[1]);

                }
            };
        })(),

        "next()": (function () {
            var root,
                containers,
                nodes,
                expected,
                notExpected,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers = [
                        document.createElement("h1"),
                        document.createElement("h1")
                    ];
                    nodes = [
                        document.createElement("span"),
                        document.createElement("span")
                    ];
                    expected = [
                        document.createElement("i"),
                        document.createElement("b")
                    ];
                    notExpected = [
                        document.createElement("button"),
                        document.createElement("button")
                    ];

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        containers[i].appendChild(nodes[i]);
                        containers[i].appendChild(expected[i]);
                        containers[i].appendChild(notExpected[i]);
                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },

                "returns NodeList consisting of next siblings of nodes in this NodeList": function () {
                    //arrange

                    //act
                    var result = nodeList.next();

                    //assert
                    assert.equal(result.length, expected.length);
                    for (var i in expected) {
                        assert.propertyVal(result, expected[i]);
                    }

                },
                "filters returned nodes by query, if provided": function () {
                    //arrange

                    //act
                    var result = nodeList.next("b");

                    //assert
                    assert.equal(result.length, 1);
                    assert.propertyVal(result[0], expected[1]);
                }
            };
        })(),

        "nextAll()": (function () {
            var root,
                containers,
                previousSiblings,
                nodes,
                nextSiblings,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers = [
                        document.createElement("div"),
                        document.createElement("div")
                    ];
                    previousSiblings = [
                        document.createElement("h1"),
                        document.createElement("h1")
                    ];
                    nodes = [
                        document.createElement("table"),
                        document.createElement("table")
                    ];
                    nextSiblings = [
                        [
                            document.createElement("button"),
                            document.createElement("button")
                        ], [
                            document.createElement("b"),
                            document.createElement("b")
                        ]
                    ];
                    nodeList = new NodeList(nodes);

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        containers[i].appendChild(previousSiblings[i]);
                        containers[i].appendChild(nodes[i]);
                        for (var j in nextSiblings[i]) {
                            containers[i].appendChild(nextSiblings[i][j]);
                        }
                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },
                "returns all siblings after nodes in nodelist": function () {
                    //arrange

                    //act
                    var result = nodeList.nextAll();

                    //assert

                    var allSiblings = nextSiblings[0].concat(nextSiblings[1]);
                    assert.equal(result.length, allSiblings.length);
                    for (var i in allSiblings) {
                        assert.propertyVal(result, allSiblings[i]);
                    }

                },
                "filters returned values by query, if provided": function () {
                    //arrange

                    //act
                    var result = nodeList.nextAll("button");

                    //assert
                    assert.equal(result.length, nextSiblings[0].length);
                    for (var i in nextSiblings[0]) {
                        assert.propertyVal(result, nextSiblings[0][i]);
                    }
                }
            };
        })(),

        "prev()": (function () {
            var root,
                containers,
                nodes,
                expected,
                notExpected,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers = [
                        document.createElement("h1"),
                        document.createElement("h1")
                    ];
                    nodes = [
                        document.createElement("span"),
                        document.createElement("span")
                    ];
                    expected = [
                        document.createElement("i"),
                        document.createElement("b")
                    ];
                    notExpected = [
                        document.createElement("button"),
                        document.createElement("button")
                    ];

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        containers[i].appendChild(notExpected[i]);
                        containers[i].appendChild(expected[i]);
                        containers[i].appendChild(nodes[i]);

                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },

                "returns NodeList consisting of previous siblings of nodes in this NodeList": function () {
                    //arrange

                    //act
                    var result = nodeList.prev();

                    //assert
                    assert.equal(result.length, expected.length);
                    for (var i in expected) {
                        assert.propertyVal(result, expected[i]);
                    }

                },
                "filters returned nodes by query, if provided": function () {
                    //arrange

                    //act
                    var result = nodeList.prev("b");

                    //assert
                    assert.equal(result.length, 1);
                    assert.propertyVal(result[0], expected[1]);
                }
            };
        })(),

        "prevAll()": (function () {
            var root,
                containers,
                previousSiblings,
                nodes,
                nextSiblings,
                nodeList;

            return {
                setup: function () {
                    root = document.createElement("div");
                    containers = [
                        document.createElement("div"),
                        document.createElement("div")
                    ];
                    nextSiblings = [
                        document.createElement("h1"),
                        document.createElement("h1")
                    ];
                    nodes = [
                        document.createElement("table"),
                        document.createElement("table")
                    ];
                    previousSiblings = [
                        [
                            document.createElement("button"),
                            document.createElement("button")
                        ], [
                            document.createElement("b"),
                            document.createElement("b")
                        ]
                    ];
                    nodeList = new NodeList(nodes);

                    document.body.appendChild(root);
                    for (var i in containers) {
                        root.appendChild(containers[i]);
                        for (var j in previousSiblings[i]) {
                            containers[i].appendChild(previousSiblings[i][j]);
                        }
                        containers[i].appendChild(nodes[i]);
                        containers[i].appendChild(nextSiblings[i]);

                    }

                    nodeList = new NodeList(nodes);
                },
                teardown: function () {
                    document.body.removeChild(root);
                },
                "returns all siblings before nodes in nodelist": function () {
                    //arrange

                    //act
                    var result = nodeList.prevAll();

                    //assert

                    var allSiblings = previousSiblings[0].concat(previousSiblings[1]);
                    assert.equal(result.length, allSiblings.length);
                    for (var i in allSiblings) {
                        assert.propertyVal(result, allSiblings[i]);
                    }

                },
                "filters returned values by query, if provided": function () {
                    //arrange

                    //act
                    var result = nodeList.prevAll("button");

                    //assert
                    assert.equal(result.length, previousSiblings[0].length);
                    for (var i in previousSiblings[0]) {
                        assert.propertyVal(result, previousSiblings[0][i]);
                    }
                }
            };
        })(),

        "andSelf()": {
            "returns concatenation of nodeList's parent to itself": function () {
                //arrange
                var nodes =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    parentNodes =
                    [
                        document.createElement("span"),
                        document.createElement("span")
                    ],
                    nodeList = new NodeList(nodes),
                    parentNodeList = new NodeList(parentNodes);

                nodeList._parent = parentNodeList;

                //act
                var result = nodeList.andSelf();

                //assert
                var allNodes = nodes.concat(parentNodes);
                assert.equal(result.length, allNodes.length);
                for (var i in allNodes) {
                    assert.propertyVal(result, allNodes[i]);
                }
            }
        },

        "first()": {
            "returns first node from the NodeList": function () {
                //arrange
                var expected = document.createElement("div"),
                    notExpected =
                    [
                        document.createElement("span"),
                        document.createElement("button")
                    ],
                    nodes = [expected].concat(notExpected),
                    nodeList = new NodeList(nodes);

                //act
                var result = nodeList.first();
                console.dir(result);
                //assert
                assert.equal(result.length, 1);
                assert.equal(result[0], expected);

            }
        },

        "last()": {
            "returns last node from the NodeList": function () {
                //arrange
                var expected = document.createElement("div"),
                    notExpected =
                    [
                        document.createElement("span"),
                        document.createElement("button")
                    ],
                    nodes = notExpected.concat([expected]),
                    nodeList = new NodeList(nodes);

                //act
                var result = nodeList.last();
                console.dir(result);
                //assert
                assert.equal(result.length, 1);
                assert.equal(result[0], expected);

            }
        },

        "even()": {
            "returns even entries from the nodeList": function () {
                //arrange
                var odd =
                    [
                        document.createElement("div"),
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    even =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    nodes = [],
                    nodeList;

                for (var i in odd) {
                    nodes.push(odd[i]);
                    if (even[i]) {
                        nodes.push(even[i]);
                    }
                }

                nodeList = new NodeList(nodes);

                //act
                var result = nodeList.even();

                //assert
                assert.equal(result.length, even.length);
                for (var i in even) {
                    assert.propertyVal(result, even[i]);
                }

            }
        },

        "odd()": {
            "returns odd entries from the nodeList": function () {
                //arrange
                var odd =
                    [
                        document.createElement("div"),
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    even =
                    [
                        document.createElement("div"),
                        document.createElement("div")
                    ],
                    nodes = [],
                    nodeList;

                for (var i in odd) {
                    nodes.push(odd[i]);
                    if (even[i]) {
                        nodes.push(even[i]);
                    }
                }

                nodeList = new NodeList(nodes);

                //act
                var result = nodeList.odd();

                //assert
                assert.equal(result.length, odd.length);
                for (var i in odd) {
                    assert.propertyVal(result, odd[i]);
                }

            }
        },

        "validation tests": (function () {
            function verify(/*dojo.NodeList*/nl, /*Array*/ids, /*String*/ comment) {
                comment = comment || "verify";
                for (var i = 0, node; (node = nl[i]) ; i++) {
                    assert.isTrue(ids[i] == node.id || ids[i] == node, comment + " " + i);
                }
                //Make sure lengths are equal.
                assert.equal(i, ids.length, comment + " length");
            }

            var container,
                divs;

            return {
                setup: function () {
                    container = document.createElement("div");
                    container = domConstruct.toDom(
                        '<div>' +
                        '    <h1 id="firstH1">testing dojo.NodeList-traverse</h1>' +
                        '    <div id="sq100" class="testDiv">' +
                        '        100px square, abs' +
                        '    </div>' +
                        '    <div id="t" class="testDiv">' +
                        '        <span id="c1">c1</span>' +
                        '    </div>' +
                        '    <div id="third" class="third testDiv">' +
                        '        <!-- This is the third top level div -->' +
                        '        <span id="crass">Crass, baby</span>' +
                        '            The third div' +
                        '            <span id="classy" class="classy">Classy, baby</span>' +
                        '            The third div, again' +
                        '            <!-- Another comment -->' +
                        '        <span id="yeah">Yeah, baby</span>' +
                        '    </div>' +
                        '    <div id="level1" class="foo">' +
                        '        <div id="level2" class="bar">' +
                        '            <div id="level3" class="foo">' +
                        '                <div id="level4" class="bar">' +
                        '                    <div id="level5" class="bar">' +
                        '                        <div id="level6" class="bang">foo bar bar bang</div>' +
                        '                    </div>' +
                        '                </div>' +
                        '            </div>' +
                        '        </div>' +
                        '    </div>' +
                        '</div>');

                    document.body.appendChild(container);

                    divs = query("div.testDiv");
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                children: function () {
                    verify(divs.last().children(), ["crass", "classy", "yeah"]);
                },

                closest: function () {
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

                parent: function () {
                    verify(query("#classy").parent(), ["third"]);
                },

                parents: function () {
                    var classy = query("#classy");
                    verify(classy.parents(), ["third", container, document.body, document.body.parentElement]);
                    verify(classy.parents(".third"), ["third"]);
                    verify(classy.parents("body"), [document.body]);
                },

                siblings: function () {
                    verify(query("#classy").siblings(), ["crass", "yeah"]);
                },

                next: function () {
                    verify(query("#crass").next(), ["classy"]);
                },

                nextAll: function () {
                    verify(query("#crass").nextAll(), ["classy", "yeah"]);
                    verify(query("#crass").nextAll("#yeah"), ["yeah"]);
                },

                prev: function () {
                    verify(query("#classy").prev(), ["crass"]);
                },

                prevAll: function () {
                    verify(query("#yeah").prevAll(), ["classy", "crass"]);
                    verify(query("#yeah").prevAll("#crass"), ["crass"]);
                },

                andSelf: function () {
                    verify(query("#yeah").prevAll().andSelf(), ["classy", "crass", "yeah"]);
                },

                first: function () {
                    verify(divs.first(), ["sq100"]);
                },

                last: function () {
                    verify(divs.last(), ["third"]);
                },

                even: function () {
                    var even = divs.even();
                    verify(even, ["t"]);
                    verify(even.end(), ["sq100", "t", "third"]);
                },

                odd: function () {
                    var odd = divs.odd();
                    verify(odd, ["sq100", "third"]);
                    verify(odd.end(), ["sq100", "t", "third"]);
                }
            }
        })()
    });
});
