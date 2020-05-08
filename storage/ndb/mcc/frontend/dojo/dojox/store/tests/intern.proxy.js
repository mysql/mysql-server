define([
	'./intern'
], function (config) {
	config.excludeInstrumentation = /^.*/;

	return config;
});
