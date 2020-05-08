// summary:
//		Test whether subworkers work.

var dojoConfig = {
	baseUrl: "../../../../../",
	async: true,
	packages: [{
		name: "dojo", location: "dojo"
	}]
};

importScripts("../../../../dojo.js", "console.js");

try{
	require(["dojo/testsDOH/_base/loader/hostenv_webworkers/strings"], function(strings){
		self.postMessage({
			type: "testResult",
			test: "subworkers are working",
			value: true
		});
	});
}catch(e){
	self.postMessage({
		type: "testResult",
		test: "subworkers are working",
		value: false
	});
}
