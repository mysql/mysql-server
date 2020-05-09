define('testing/tests/functional/_base/loader/modules/idFactoryArity', function (require, exports, module) {
	var impliedDep = require('./impliedDep3');
	return {
		module: module,
		id: 'idFactoryArity',
		impliedDep: impliedDep.id
	};
});
