/*
 * Test module for dojo/node plugin that relies upon a 'dual' AMD/CommonJS module
 */

var noderequireamd = module.exports = exports;

noderequireamd.nodeamd = require('./nodeamd');
