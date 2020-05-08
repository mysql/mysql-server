define(["dojo/dom", "dojo/_base/connect", "dijit/registry"],
function(dom, connect, registry){

	var _connectResults = []; // events connect results

	return {
		// simple view init
		init: function(){
			currentModel = this.loadedModels.names;
			var connectResult;

			var self = this;
			connectResult = connect.connect(dom.byId('testRTL'), "click", function(e){
				console.log("testRTL called. ");
				//first set the dir="rtl" on the app root
				self.app.domNode.parentNode.dir = "rtl";
				//next unload all chldren
				var params = {};
				var footer = self.app.children.domOrderByConstraint_footer;
				var header = footer.children.domOrderByConstraint_footer_header;
				var right = header.children.domOrderByConstraint_footer_header_right;
				var left = right.children.domOrderByConstraint_footer_header_right_left;
				var center = left.children.domOrderByConstraint_footer_header_right_left_center;
				params.view = header;
				params.parent = footer;
				//params.viewId = view.id;
				self.app.emit("unload-view", params);
			});
			_connectResults.push(connectResult);

			connectResult = connect.connect(dom.byId('testRTL2'), "click", function(e){
				console.log("testRTL called. ");
				var params = {};
				var footer = self.app.children.domOrderByConstraint_footer;
				//first set the dir="rtl" on the footer
				footer.domNode.parentNode.dir = "rtl";

				//next unload all chldren
				var header = footer.children.domOrderByConstraint_footer_header;
				var right = header.children.domOrderByConstraint_footer_header_right;
				var left = right.children.domOrderByConstraint_footer_header_right_left;
				var center = left.children.domOrderByConstraint_footer_header_right_left_center;
				params.view = header;
				params.parent = footer;
				//params.viewId = view.id;
				self.app.emit("unload-view", params);
			});
			_connectResults.push(connectResult);

			connectResult = connect.connect(dom.byId('testLTR'), "click", function(e){
				console.log("testLTR called. ");
				//first set the dir="rtl" on the app root
				self.app.domNode.parentNode.dir = "ltr";
				//next unload all chldren
				var params = {};
				var footer = self.app.children.domOrderByConstraint_footer;
				var header = footer.children.domOrderByConstraint_footer_header;
				var right = header.children.domOrderByConstraint_footer_header_right;
				var left = right.children.domOrderByConstraint_footer_header_right_left;
				var center = left.children.domOrderByConstraint_footer_header_right_left_center;
				params.view = header;
				params.parent = footer;
				//params.viewId = view.id;
				self.app.emit("unload-view", params);
			});
			_connectResults.push(connectResult);

			connectResult = connect.connect(dom.byId('testLTR2'), "click", function(e){
				console.log("testLTR called. ");
				var params = {};
				var footer = self.app.children.domOrderByConstraint_footer;
				//first set the dir="ltr" on the footer
				footer.domNode.parentNode.dir = "ltr";

				//next unload all chldren
				var header = footer.children.domOrderByConstraint_footer_header;
				var right = header.children.domOrderByConstraint_footer_header_right;
				var left = right.children.domOrderByConstraint_footer_header_right_left;
				var center = left.children.domOrderByConstraint_footer_header_right_left_center;
				params.view = header;
				params.parent = footer;
				//params.viewId = view.id;
				self.app.emit("unload-view", params);
			});
			_connectResults.push(connectResult);

			connectResult = connect.connect(dom.byId('testTrans'), "click", function(e){
				console.log("testTrans called. ");
				var params = {};
				var transOpts = {
					title : "footer,header,right,left,center2",
					target : "footer,header,right,left,center2",
					url : "#footer,header,right,left,center2", // this is optional if not set it will be created from target
					params : params
				};
				self.app.transitionToView(e.target,transOpts,e);
			});
			_connectResults.push(connectResult);

		},

		// simple view destroy
		destroy: function(){
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		}
	};
});
