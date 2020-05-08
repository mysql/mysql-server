define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    'require',
    '../../../errors/create',
    '../../../errors/RequestError',
    '../../../_base/declare'
], function (registerSuite, assert, sinon, require, create, RequestError, declare) {
    registerSuite({
        name: 'dojo/errors/RequestError',

        "returns correct result": function () {
            //arrange
            var response= {foo: "bar"};

            //act
            var result = new RequestError("foo", response);


            //assert
            assert.equal(result.name, "RequestError");
            assert.equal(result.response, response);
        }
    });
});
