define([
    'intern!object',
    'intern/chai!assert',
    '../../../query',
    '../../../_base/query'
], function (registerSuite, assert, query, baseQuery) {
    registerSuite({
        name: 'dojo/_base/query',

        "delegates to dojo/query": function () {
            assert.equal(query, baseQuery);
        }
    });
});
