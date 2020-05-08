define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../../_base/window',
    '../../../_base/kernel',
    '../../../_base/lang',
    '../../../_base/sniff'
], function (registerSuite, assert, sinon, window, kernel, lang, sniff) {
    registerSuite({
        name: 'dojo/_base/window',
        "direct delegation": function () {
            assert.equal(kernel.global, window.global);
            assert.equal(kernel.global["document"], window.doc);
        },
        "body()": {
            "undefined": function () {
                //arrange

                //act
                var result = window.body();

                //assert
                assert.equal(result, kernel.doc.body);
            },
            "document": function () {
                //arrange
                var iframe = document.createElement("iframe");
                document.body.appendChild(iframe);

                //act
                var result = window.body(iframe.contentDocument);


                //assert
                assert.equal(result, iframe.contentDocument.body);

                document.body.removeChild(iframe);
            }
        },
        "setContext()": {
            "object + document": function () {
                //arrange
                var object = { foo: "bar" },
                    iframe = document.createElement("iframe"),
                    origGlobal = kernel.global,
                    origDoc = kernel.doc;

                document.body.appendChild(iframe);

                var doc = iframe.contentDocument;

                //act
                window.setContext(object, doc);

                //assert
                assert.equal(doc, kernel.doc);
                assert.equal(doc, window.doc);
                assert.equal(object, kernel.global);
                assert.equal(object, window.global);

                kernel.global = window.global = origGlobal;
                kernel.doc = window.doc = origDoc;

            }
        },
        "withGlobal()": {
            "object + function + object + array": function () {
                //arrange
                var globalObject = { document: { foo: "bar" } },
                    callback = function () { },
                    thisObject = { baz: "buz" },
                    cbArguments = [42, 27],
                    mock = sinon.spy(window, "withDoc"),
                    oldGlobal = kernel.global;

                //act
                window.withGlobal(globalObject, callback, thisObject, cbArguments);

                //assert
                assert.isTrue(mock.calledWith(globalObject.document,
                    callback, thisObject, cbArguments));
                assert.equal(kernel.global, oldGlobal);

                mock.restore();

            }
        },
        "withDoc()": {
            "document + function + object + array": function () {
                //arrange
                var iframe = document.createElement("iframe"),
                    caughtArgs,
                    caughtThis,
                    expectedReturn = "expected return",
                    callback = function () {
                        caughtThis = this;
                        caughtArgs = arguments;

                        return expectedReturn;
                    },
                    thisObject = { baz: "buz" },
                    cbArguments = [42, 27],
                    oldGlobal = kernel.global;

                document.body.appendChild(iframe);
                var doc = iframe.contentDocument;

                //act
                var result = window.withDoc(doc, callback, thisObject, cbArguments);

                //assert
                assert.equal(result, expectedReturn);
                for (var i in cbArguments) {
                    assert.equal(caughtArgs[i], cbArguments[i]);
                }
                assert.equal(caughtThis, thisObject);

            },
            "document + string + object + array": function () {
                //arrange
                var iframe = document.createElement("iframe"),
                    caughtArgs,
                    caughtThis,
                    expectedReturn = "expected return",
                    callback = function () {
                        caughtThis = this;
                        caughtArgs = arguments;

                        return expectedReturn;
                    },
                    functionName = "theFunction",
                    thisObject = { baz: "buz" },
                    cbArguments = [42, 27],
                    oldDoc = kernel.doc;

                thisObject[functionName] = callback;

                document.body.appendChild(iframe);
                var doc = iframe.contentDocument;

                //act
                var result = window.withDoc(doc, functionName, thisObject, cbArguments);

                //assert
                assert.equal(result, expectedReturn);
                for (var i in cbArguments) {
                    assert.equal(caughtArgs[i], cbArguments[i]);
                }
                assert.equal(caughtThis, thisObject);
                assert.equal(kernel.doc, oldDoc);
            },
            "document + function + object": function () {
                //arrange
                var iframe = document.createElement("iframe"),
                    caughtArgs,
                    caughtThis,
                    expectedReturn = "expected return",
                    callback = function () {
                        caughtThis = this;
                        caughtArgs = arguments;

                        return expectedReturn;
                    },
                    functionName = "theFunction",
                    thisObject = { baz: "buz" },
                    oldDoc = kernel.doc;

                thisObject[functionName] = callback;

                document.body.appendChild(iframe);
                var doc = iframe.contentDocument;

                //act
                var result = window.withDoc(doc, functionName, thisObject);

                //assert
                assert.equal(result, expectedReturn);
                assert.deepEqual(caughtArgs.length, 0);
                assert.equal(caughtThis, thisObject);
                assert.equal(kernel.doc, oldDoc);
            }
        }
    });
});

