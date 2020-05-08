define(["dojo/dom"], function(dom){
	return {
		beforeActivate: function(previousView, data){
			dom.byId("label").innerHTML = this.nls[previousView.name] + (data ? ("-" + data) : "");

		}
	}
});