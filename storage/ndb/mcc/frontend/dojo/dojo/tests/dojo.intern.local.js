define([
	'./dojo.intern'
], function (intern) {
	intern.tunnel = 'NullTunnel';
	intern.tunnelOptions = {
		hostname: 'localhost',
		port: 4444
	};

	intern.environments = [
		{ browserName: 'firefox' },
		{ browserName: 'chrome' }
	];

	return intern;
});
