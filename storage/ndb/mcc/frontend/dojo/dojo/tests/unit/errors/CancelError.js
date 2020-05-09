define([
    'intern!object',
    'intern/chai!assert',
    'sinon',
    'require',
    '../../../errors/create',
    '../../../errors/CancelError',
    '../../../_base/declare'
], function (registerSuite, assert, sinon, require, create, CancelError, declare) {
    registerSuite({
        name: 'dojo/errors/CancelError',

        "returns correct result": function () {
            //arrange

            //act


            //assert
            assert.equal(CancelError.prototype.name, "CancelError");
            assert.equal(CancelError.prototype.dojoType, "cancel");
        }
    });
});
