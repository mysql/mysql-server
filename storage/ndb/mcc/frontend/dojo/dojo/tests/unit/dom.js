define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../dom',
    '../../sniff',
    '../../dom-construct',
    '../../Deferred'
], function (registerSuite, assert, sinon, dom, has, domConstruct, Deferred) {

    var baseId = "dojo_dom",
        uniqueId = 0;

    function getId() {
        return baseId + uniqueId++;
    }

    registerSuite({
        name: 'dojo/dom',

        "byId()": (function () {
            var container,
                iframe,
                node,
                nodeId,
                iframeChild,
                iframeChildId;


            return {
                setup: function () {
                    container = document.createElement("div");
                    iframe = document.createElement("iframe");
                    node = document.createElement("span");
                    nodeId = getId();
                    document.body.appendChild(container);
                    container.appendChild(iframe);
                    container.appendChild(node);

                    iframeChild = iframe.contentDocument.createElement("div");
                    iframeChildId = getId();

                    //make async because FF seems to need a bit to setup the iframe's contentDocument after adding to the page
                    var dfd = new Deferred();
                    function placeContent() {
                        if (iframe.contentDocument && iframe.contentDocument.body) {
                            iframe.contentDocument.body.appendChild(iframeChild);

                            node.id = nodeId;
                            iframeChild.id = iframeChildId;
                            dfd.resolve();
                        } else {
                            setTimeout(placeContent, 0);
                        }
                    }
                    setTimeout(placeContent, 0);

                    return dfd.promise;
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "node": function () {
                    //arrange

                    //act
                    var result = dom.byId(node);

                    //assert
                    assert.equal(result, node);

                },
                "string": function () {
                    //arrange

                    //act
                    var result = dom.byId(nodeId);

                    //assert
                    assert.equal(result, node);
                },
                "string + document": function () {
                    //arrange

                    //act
                    var result = dom.byId(iframeChildId, iframe.contentDocument);

                    //assert
                    assert.equal(result, iframeChild);
                },
                "non-existent node returns null": function () {
                    //arrange

                    //act
                    var result = dom.byId(getId());

                    //assert
                    assert.isNull(result);
                }
            }
        })(),

        "isDescendant()": (function () {
            var container,
                node,
                containerId,
                nodeId;

            return {
                setup: function () {
                    container = document.createElement("div");
                    node = document.createElement("div");

                    document.body.appendChild(container);
                    container.appendChild(node);

                    containerId = getId();
                    nodeId = getId();

                    container.id = containerId;
                    node.id = nodeId;
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "node + parent-node": function () {
                    //arrange

                    //act
                    var result = dom.isDescendant(node, container);

                    //assert
                    assert.isTrue(result);

                },
                "node + parent-node-id": function () {
                    //arrange

                    //act
                    var result = dom.isDescendant(node, containerId);

                    //assert
                    assert.isTrue(result);
                },
                "string + parent-node": function () {
                    //arrange

                    //act
                    var result = dom.isDescendant(nodeId, container);

                    //assert
                    assert.isTrue(result);
                },
                "node + not-ancestor": function () {
                    //arrange

                    //act
                    var result = dom.isDescendant(container, node);

                    //assert
                    assert.isFalse(result);
                },
                "node + grandparent-node": function () {
                    //arrange

                    //act
                    var result = dom.isDescendant(node, container.parentNode);

                    //assert
                    assert.isTrue(result);

                }
            }
        })(),

        "setSelectable()": (function () {
            var container,
                node,
                child,
                nodeId;

            return {
                setup: function () {
                    nodeId = getId();
                    container = document.createElement("input");
                    node = document.createElement("input");
                    child = document.createElement("input");

                    node.id = nodeId;

                    document.body.appendChild(container);
                    container.appendChild(node);
                    node.appendChild(child);
                },
                teardown: function () {
                    document.body.removeChild(container);
                },
                "node + true": function () {
                    //arrange
                    var cssUserSelect = has("css-user-select");

                    if (cssUserSelect) {
                        node.style[cssUserSelect] = "none";
                    } else {
                        node.removeAttribute("unselectable");
                    }

                    //act
                    dom.setSelectable(node, true);

                    //assert

                    if (cssUserSelect) {
                        assert.equal(node.style[cssUserSelect], "");
                    } else {
                        assert.isFalse(node.hasAttribute("unselectable"));
                    }

                },
                "string + true": function () {
                    //arrange
                    var cssUserSelect = has("css-user-select");

                    if (cssUserSelect) {
                        node.style[cssUserSelect] = "none";
                    } else {
                        node.removeAttribute("unselectable");
                    }

                    //act
                    dom.setSelectable(nodeId, true);

                    //assert

                    if (cssUserSelect) {
                        assert.equal(node.style[cssUserSelect], "");
                    } else {
                        assert.isFalse(node.hasAttribute("unselectable"));
                    }

                },
                "node + false": function () {
                    //arrange
                    var cssUserSelect = has("css-user-select");

                    if (cssUserSelect) {
                        node.style[cssUserSelect] = "";
                    } else {
                        node.setAttribute("unselectable");
                    }

                    //act
                    dom.setSelectable(nodeId, false);

                    //assert

                    if (cssUserSelect) {
                        assert.equal(node.style[cssUserSelect], "none");
                    } else {
                        assert.isTrue(node.hasAttribute("unselectable"));
                    }

                },
                "validation tests": (function () {
                    var container,
                        node,
                        child,
                        nodeId,
                        iframeId,
                        iframe,
                        iframeChildId;

                    function getIframeDocument(/*DOMNode*/iframeNode) {
                        //summary: Returns the document object associated with the iframe DOM Node argument.
                        var doc = iframeNode.contentDocument || // W3
                            (
                                (iframeNode.contentWindow) && (iframeNode.contentWindow.document)
                            ) ||  // IE
                            (
                                (iframeNode.name) && (document.frames[iframeNode.name]) &&
                                (documendoh.frames[iframeNode.name].document)
                            ) || null;
                        return doc;
                    }

                    return {
                        setup: function () {
                            container = document.createElement("div");
                            iframe = document.createElement("iframe");
                            node = document.createElement("div");
                            child = document.createElement("div");

                            iframeId = getId();
                            iframe.id = iframeId;
                            iframe.name = iframeId;

                            nodeId = getId();
                            node.id = nodeId;

                            iframeChildId = getId();
                            var iframeContent = domConstruct.toDom("<div id='" + iframeChildId +"'></div>");

                            document.body.appendChild(container);
                            container.appendChild(iframe);
                            container.appendChild(node);
                            node.appendChild(child);

                            //make async because FF seems to need a bit to setup the iframe's contentDocument after adding to the page
                            var dfd = new Deferred();
                            function placeContent() {
                                if (iframe.contentDocument && iframe.contentDocument.body) {
                                    domConstruct.place(iframeContent, iframe.contentDocument.body);
                                    dfd.resolve();
                                } else {
                                    setTimeout(placeContent, 0);
                                }
                            }
                            setTimeout(placeContent, 0);

                            return dfd.promise;
                        },
                        teardown: function () {
                            document.body.removeChild(container);
                        },
                        "nonExistentId": function () {
                            assert.isNull(dom.byId('nonExistentId'));
                        },
                        "null": function () {
                            assert.isNull(dom.byId(null));
                        },
                        "empty string": function () {
                            assert.isNull(dom.byId(""));
                        },
                        "undefined": function () {
                            assert.isNull(dom.byId(undefined));
                        },
                        "isDescendant": function () {
                            assert.isTrue(dom.isDescendant(nodeId, document.body));
                            assert.isTrue(dom.isDescendant(nodeId, document));
                            assert.isTrue(dom.isDescendant(nodeId, nodeId));
                            assert.isTrue(dom.isDescendant(dom.byId(nodeId), nodeId));
                            assert.isFalse(dom.isDescendant(nodeId, dom.byId(nodeId).firstChild));
                            assert.isTrue(dom.isDescendant(dom.byId(nodeId).firstChild, nodeId));
                        },
                        "isDescendantIframe": function () {
                            var subDiv = getIframeDocument(iframe).getElementById(iframeChildId);
                            assert.isTrue(dom.isDescendant(subDiv, subDiv));
                            assert.isTrue(dom.isDescendant(subDiv, subDiv.parentNode));
                            assert.isFalse(dom.isDescendant(subDiv.parentNode, subDiv));

                        }
                    }
                })()
            }
        })()
    });
});