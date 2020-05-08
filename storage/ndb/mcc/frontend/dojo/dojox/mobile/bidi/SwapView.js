define([
	"dojo/_base/declare"
], function(declare){

	// module:
	//		dojox/mobile/bidi/SwapView

	return declare(null, {
		
		// callParentFunction: Boolean
		//		Boolean value indicate whether to call the parent version of the function or the child one.
		//		Used to support mirroring.
		_callParentFunction: false,
		
		nextView: function(/*DomNode*/node){
			//dojox.mobile mirroring support
			if(this.isLeftToRight() || this._callParentFunction){
				this._callParentFunction = false;
				return this.inherited(arguments);
			}else{
				this._callParentFunction = true;
				return this.previousView(node);
			}
		},
		previousView: function(/*DomNode*/node){
			//dojox.mobile mirroring support
			if(this.isLeftToRight() || this._callParentFunction){
				this._callParentFunction = false;
				return this.inherited(arguments);
			}else{
				this._callParentFunction = true;
				return this.nextView(node);
			}
		}
		
		
	});
});
