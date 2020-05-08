/*
 * Test module for dojo/node plugin that tries to detect AMD
 */

(function(undefined){

	var nodeamd = {};

	nodeamd.test = 'foo';

	// 'Improper' detection of AMD in a combined CommonJS/AMD modules, where the module thinks it is being loaded
	// by an AMD loader, when in fact it could be being loaded by a CommonJS module loader.  The dojo/node plugin
	// needs to 'hide' define from these types of modules.
	if (typeof define === 'function' && define.amd) {
		define('nodeamd', [], function () {
			return nodeamd;
		});
	}
	else if (typeof module !== 'undefined' && module.exports) {
		module.exports = nodeamd;
	}
	else if (typeof ender === 'undefined') {
		this['nodeamd'] = nodeamd;
	}

}).call(this);
