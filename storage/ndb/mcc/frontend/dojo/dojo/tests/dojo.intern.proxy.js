define([
	'./dojo.intern'
], function (config) {
	config.excludeInstrumentation = /^.*/;

	return config;
});
