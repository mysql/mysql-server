define("dojox/calendar/DecorationRenderer", [
	"dojo/_base/declare",
	"dojo/dom-class",
	"dijit/_WidgetBase",
	"dijit/_TemplatedMixin"],

	function(declare, domClass, _WidgetBase, _TemplatedMixin, template){

	return declare("dojox.calendar.DecorationRenderer", [_WidgetBase, _TemplatedMixin], {

		// summary:
		//		The default item vertical renderer.

		templateString: "<div class=\"dojoxCalendarDecoration\"></div>",

		_setItemAttr: function(value){
			if(value == null){
				if(this.item && this.item.cssClass){
					domClass.remove(this.domNode, this.item.cssClass);
				}
				this.item = null;
			}else{
				if(this.item != null){
					if(this.item.cssClass != value.cssClass){
						if(this.item.cssClass){
							domClass.remove(this.domNode, this.item.cssClass);
						}
					}
					this.item = lang.mixin(this.item, value);
					if(value.cssClass){
						domClass.add(this.domNode, value.cssClass);
					}
				}else{
					this.item = value;
					if(value.cssClass){
						domClass.add(this.domNode, value.cssClass);
					}
				}
			}
		},

		postCreate: function() {
			this.inherited(arguments);
			this._applyAttributes();
		}


	});
});
