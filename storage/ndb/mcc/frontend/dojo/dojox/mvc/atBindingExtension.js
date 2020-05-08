define([
	"dojo/aspect",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dijit/_WidgetBase",
	"./_atBindingMixin",
	"dijit/registry"
], function(aspect, array, lang, _WidgetBase, _atBindingMixin){
	return function(/*dijit/_WidgetBase...*/ w){
		// summary:
		//		Monkey-patch the given widget so that they looks at dojox/mvc/at set in them and start data binding specified there.
		// w: dijit/_WidgetBase...
		//		The widget.

		array.forEach(arguments, function(w){
			if(w.dataBindAttr){
				console.warn("Detected a widget or a widget class that has already been applied data binding extension. Skipping...");
				return;
			}

			// Apply the at binding mixin
			lang._mixin(w, _atBindingMixin.mixin);

			// Monkey patch widget.postscript to get the list of dojox/mvc/at handles before startup
			aspect.before(w, "postscript", function(/*Object?*/ params, /*DomNode|String*/ srcNodeRef){
				this._dbpostscript(params, srcNodeRef);
			});

			// Monkey patch widget.startup to get data binds set up
			aspect.before(w, "startup", function(){
				if(this._started){
					return;
				}
				this._startAtWatchHandles();
			});

			// Monkey patch widget.destroy to remove watches setup in _DataBindingMixin
			aspect.before(w, "destroy", function(){
				this._stopAtWatchHandles();
			});

			// Monkey patch widget.set to establish data binding if a dojox/mvc/at handle comes
			aspect.around(w, "set", function(oldWidgetBaseSet){
				return function(/*String*/ name, /*Anything*/ value){
					if(name == _atBindingMixin.prototype.dataBindAttr){
						return this._setBind(value);
					}else if((value || {}).atsignature == "dojox.mvc.at"){
						return this._setAtWatchHandle(name, value);
					}
					return oldWidgetBaseSet.apply(this, lang._toArray(arguments));
				};
			});
		});

		return arguments; // dijit/_WidgetBase...
	};
});
