// Note: the provided module IDs must equate to the relative path from dojo to the module. In this case, the modules are
// in <dojo path>/tests/functional/_base/loader.

dojo.provide('dojo.tests._base.loader.declareStepsOnProvide');
dojo.provide('dojo.tests._base.loader.declareStepsOnProvide1');

dojo.declare('dojo.tests._base.loader.declareStepsOnProvide', [], {
	status: function () {
		return 'OK';
	}
});

dojo.declare('dojo.tests._base.loader.declareStepsOnProvide1', [], {
	status: function () {
		return 'OK-1';
	}
});
