define(["dojo/dom", "dojo/_base/connect", "dijit/registry"],
function(dom, connect, registry){

	var _connectResults = []; // events connect results
	var currentModel = null;

	var setFromModel = function (){
	//	registry.byId("firstInput1").set('value', currentModel[0].First);
	//	registry.byId("lastInput1").set('value', currentModel[0].Last);
	//	registry.byId("emailInput1").set('value', currentModel[0].Email);
	//	registry.byId("shiptostreetInput1").set('value', currentModel[0].ShipTo.Street);
	//	registry.byId("shiptocityInput1").set('value', currentModel[0].ShipTo.City);
	//	registry.byId("shiptostateInput1").set('value', currentModel[0].ShipTo.State);
	//	registry.byId("shiptozipInput1").set('value', currentModel[0].ShipTo.Zip);
	//	registry.byId("billtostreetInput1").set('value', currentModel[0].BillTo.Street);
	//	registry.byId("billtocityInput1").set('value', currentModel[0].BillTo.City);
	//	registry.byId("billtostateInput1").set('value', currentModel[0].BillTo.State);
	//	registry.byId("billtozipInput1").set('value', currentModel[0].BillTo.Zip);
	};

	return {
		// simple view init
		init: function(){
			currentModel = this.loadedModels.names;
			var connectResult;

			var self = this;
			connectResult = connect.connect(dom.byId('2testRTL'), "click", function(e){
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

			connectResult = connect.connect(dom.byId('2testRTL2'), "click", function(e){
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

			connectResult = connect.connect(dom.byId('2testLTR'), "click", function(e){
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

			connectResult = connect.connect(dom.byId('2testLTR2'), "click", function(e){
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
