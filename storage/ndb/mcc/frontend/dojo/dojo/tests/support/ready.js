define([
	'intern/dojo/node!leadfoot/helpers/pollUntil'
], function (pollUntil) {
	function ready(remote, url, timeout) {
		return remote
			.get(url)
			.then(pollUntil(
				'return typeof ready !== "undefined" && ready ? true : undefined;',
				[],
				typeof timeout === 'undefined' ? 5000 : timeout
			));
	}

	return ready;
});
