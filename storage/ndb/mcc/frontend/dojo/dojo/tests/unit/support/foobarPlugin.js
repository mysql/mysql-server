define(["dojo/has"], function(has) {
	/* Loads a string based on the value of defined features */
	var data = has("foo") ? "foo" : has("bar") ? "bar" : "undefined";
	return {
		load: function(id, parentRequire, loaded) {
			loaded(data);
		}
	}
});