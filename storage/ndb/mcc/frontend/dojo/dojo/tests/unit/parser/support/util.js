define([
	'../../../../_base/kernel',
	'dojo/string'
], function (dojo, string) {
	return {
		fixScope: function (snippet) {
			return string.substitute(snippet, {
				dojo: dojo._scopeName
			});
		}
	};
});
