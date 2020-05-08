define([
    'intern!object',
    'intern/chai!assert',
    '../../../_base/kernel',
    '../../../sniff',
    '../../../_base/sniff',
], function (registerSuite, assert, kernel, has) {
    registerSuite({
        name: 'dojo/_base/sniff',

        "direct delegates": function () {
            assert.isTrue(kernel.isBrowser == has("host-browser"));
            assert.equal(kernel.isFF, has("ff"));
            assert.equal(kernel.isIE, has("ie"));
            assert.equal(kernel.isKhtml, has("khtml"));
            assert.equal(kernel.isWebKit, has("webkit"));
            assert.equal(kernel.isMozilla, has("mozilla"));
            assert.equal(kernel.isMoz, has("mozilla"));
            assert.equal(kernel.isOpera, has("opera"));
            assert.equal(kernel.isSafari, has("safari"));
            assert.equal(kernel.isChrome, has("chrome"));
            assert.equal(kernel.isMac, has("mac"));
            assert.equal(kernel.isIos, has("ios"));
            assert.equal(kernel.isAndroid, has("android"));
            assert.equal(kernel.isWii, has("wii"));
            assert.equal(kernel.isQuirks, has("quirks"));
            assert.equal(kernel.isAir, has("air"));
        }
    });
});
