//>>built
define("dojo/dnd/AutoSource", [ "./Source" ], function(Source){
	/*===== Source = dojo.dnd.Source =====*/
	return dojo.declare("dojo.dnd.AutoSource", Source, {
		// summary:
		//		a source that syncs its DnD nodes by default

		constructor: function(node, params){
			// summary:
			//		constructor of the AutoSource --- see the Source constructor for details
			this.autoSync = true;
		}
	});
});
