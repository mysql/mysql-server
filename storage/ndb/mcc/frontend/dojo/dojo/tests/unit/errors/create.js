define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    '../../../errors/create',
    '../../../_base/lang'
], function (registerSuite, assert, sinon, create, lang) {
    registerSuite({
        name: 'dojo/errors/create',

        "prototype properties": function () {
            //arrange
            var name = "the name",
                props = {
                    foo: "bar",
                    baz: "buz"
                },
                expectedPrototype = { qux: 42 },
                base = new Error(),
                mock = sinon.stub(lang, "delegate").returns(expectedPrototype);

            //act
            var result = create(name, null, base, props);

            //assert
            assert.isTrue(mock.calledWith(base.prototype, props));
            assert.equal(result.prototype, expectedPrototype);
            assert.equal(result.prototype.name, name);
            assert.equal(result.prototype.constructor, result);

            mock.restore();
        },
        "string + null + Error + props captures error properties": function () {
            if (Error.captureStackTrace) {
                //arrange
                var message = "the message",
                    ErrorClass = create("foo", null, Error);

                //act
                var result = new ErrorClass(message);

                //assert
                assert.isDefined(result.stack);
                assert.equal(result.message, message);
                assert.instanceOf(result, Error);
            } else {
                this.skip("Only valid on clients that support Error.captureStackTrace");
            }
        },
        "string + null + non-Error func + props calls non-Error func": function () {
            //arrange
            var caughtThis,
                caughtArgs,
                mock = function () {
                    caughtThis = this;
                    caughtArgs = arguments;
                },
                message = "the message",
                ErrorClass = create("", null, mock);

            //act
            var result = new ErrorClass(message);

            //assert
            assert.equal(caughtArgs[0], message);
            assert.equal(caughtThis, result);

        },
        "string + function + Error + props calls function": function () {
            //arrange
            var caughtThis,
                caughtArgs,
                mock = function () {
                    caughtThis = this;
                    caughtArgs = arguments;
                },
                message = "the message",
                ErrorClass = create("", mock, Error);

            //act
            var result = new ErrorClass(message);

            //assert
            assert.equal(caughtArgs[0], message);
            assert.equal(caughtThis, result);
        },
        "validation tests": (function () {
            var TestError = create("TestError", function (message, foo) {
                this.foo = foo;
            });

            var OtherError = create("OtherError", function (message, foo, bar) {
                this.bar = bar;
            }, TestError, {
                getBar: function () {
                    return this.bar;
                }
            });

            var testError = new TestError("hello", "asdf"),
                otherError = new OtherError("goodbye", "qwerty", "blah");

            return {
                "TestError": function () {
                    assert.instanceOf(testError, Error, "testError should be an instance of Error");
                    assert.instanceOf(testError, TestError, "testError should be an instance of TestError");
                    assert.notInstanceOf(testError, OtherError, "testError should not be an instance of OtherError");
                    assert.notProperty(testError, "getBar", "testError should not have a 'getBar' property");
                    assert.equal(testError.message, "hello", "testError's message property should be 'hello'");
                    if ((new Error()).stack) {
                        assert.isTrue(!!testError.stack, "custom error should have stack set");
                    }
                },
                "OtherError": function () {
                    assert.instanceOf(otherError, Error, "otherError should be an instance of Error");
                    assert.instanceOf(otherError, TestError, "otherError should be an instance of TestError");
                    assert.instanceOf(otherError, OtherError, "otherError should be an instance of OtherError");
                    assert.property(otherError, "getBar", "otherError should have a 'getBar' property");
                    assert.isFalse(otherError.hasOwnProperty("getBar"), "otherError should not have a 'getBar' own property");
                    assert.equal(otherError.getBar(), "blah", "otherError should return 'blah' from getBar()");
                    assert.equal(otherError.message, "goodbye", "otherError's message property should be 'goodbye'");
                }
            }
        })()
    });
});
