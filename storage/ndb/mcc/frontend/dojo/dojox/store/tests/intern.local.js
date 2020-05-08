define([
	'./intern'
], function (intern) {
	intern.useSauceConnect = false;
	intern.webdriver = {
		host: 'localhost',
		port: 4444
	};

	intern.environments = [
		{ browserName: 'firefox' },
		{ browserName: 'chrome' }
	];

	return intern;
});
