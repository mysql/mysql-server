// summary:
//		Test the use of dojo/on without access to dom in a webworker.

var dojoConfig = {
	baseUrl: "../../../../../",
	async: true,
	packages: [{
		name: "dojo", location: "dojo"
	}]
};

importScripts("../../../../dojo.js", "console.js");

try{
	require(["dojo/on"], function(on){
		on(self, "message", function(message){
			if(message.data.type === "gotMessage"){
				self.postMessage({
					type: "testResult",
					test: "dojo/on in a worker is working",
					value: true
				});
			}else{
				self.postMessage({
					type: "testResult",
					test: "dojo/on in a worker is working",
					value: false
				});
			}
		});

		self.postMessage({
			type: "requestMessage"
		});
	});
}catch(e){
	self.postMessage({
		type: "testResult",
		test: "dojo/on in a worker is working",
		value: false
	});
}
