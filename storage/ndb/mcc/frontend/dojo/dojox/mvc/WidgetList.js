define([
	"require",
	"dojo/_base/array",
	"dojo/_base/lang",
	"dojo/_base/declare",
	"dijit/_Container",
	"dijit/_WidgetBase",
	"./Templated"
], function(require, array, lang, declare, _Container, _WidgetBase, Templated){
	var childTypeAttr = "data-mvc-child-type",
	 childMixinsAttr = "data-mvc-child-mixins",
	 childParamsAttr = "data-mvc-child-props",
	 childBindingsAttr = "data-mvc-child-bindings",
	 undef;

	function evalParams(params){
		return eval("({" + params + "})");
	}

	function unwatchElements(/*dojox/mvc/WidgetList*/ w){
		for(var h = null; h = (w._handles || []).pop();){
			h.unwatch();
		}
	}

	function flatten(/*String[][]*/ a){
		var flattened = [];
		array.forEach(a, function(item){
			[].push.apply(flattened, item);
		});
		return flattened;
	}

	function loadModules(/*dojo/Stateful[]*/ items, /*Function*/ callback){
		// summary:
		//		Load modules associated with an array of data.
		// items: dojo/Stateful[]
		//		The array of data.
		// callback: Function
		//		Then callback called when the modules have been loaded.

		if(this.childClz){
			callback(this.childClz);
		}else if(this.childType){
			var typesForItems = !lang.isFunction(this.childType) && !lang.isFunction(this.childMixins) ? [[this.childType].concat(this.childMixins && this.childMixins.split(",") || [])] :
			 array.map(items, function(item){
				var type = lang.isFunction(this.childType) ? this.childType.call(item, this) : this.childType,
				 mixins = lang.isFunction(this.childMixins) ? this.childMixins.call(item, this) : this.childMixins;
				return type ? [type].concat(lang.isArray(mixins) ? mixins : mixins ? mixins.split(",") : []) : ["dojox/mvc/Templated"];
			}, this);
			require(array.filter(array.map(flatten(typesForItems), function(type){ return lang.getObject(type) ? undef : type; }), function(type){ return type !== undef; }), function(){
				callback.apply(this, array.map(typesForItems, function(types){
					var clzList = array.map(types, function(type){ return lang.getObject(type) || require(type); });
					return clzList.length > 1 ? declare(clzList, {}) : clzList[0];
				}));
			});
		}else{
			callback(Templated);
		}
	}

	var WidgetList = declare("dojox.mvc.WidgetList", [_WidgetBase, _Container], {
		// summary:
		//		A widget that creates child widgets repeatedly based on the children attribute (the repeated data) and childType/childMixins/childParams attributes (determines how to create each child widget).
		// example:
		//		Create multiple instances of dijit/TextBox based on the data in array.
		//		The text box refers to First property in the array item.
		// |		<div data-dojo-type="dojox/mvc/WidgetList"
		// |		 data-dojo-props="children: array"
		// |		 data-mvc-child-type="dijit/form/TextBox"
		// |		 data-mvc-child-props="value: at(this.target, 'First')"></div>
		// example:
		//		Create multiple instances of widgets-in-template based on the HTML written in `<script type="dojox/mvc/InlineTemplate">`.
		//		The label refers to Serial property in the array item, and the text box refers to First property in the array item.
		// |		<div data-dojo-type="dojox/mvc/WidgetList"
		// |		 data-dojo-mixins="dojox/mvc/_InlineTemplateMixin"
		// |		 data-dojo-props="children: array">
		// |			<script type="dojox/mvc/InlineTemplate">
		// |				<div>
		// |					<span data-dojo-type="dijit/_WidgetBase"
		// |					 data-dojo-props="_setValueAttr: {node: 'domNode', type: 'innerText'}, value: at('rel:', 'Serial')"></span>: 
		// |					<span data-dojo-type="dijit/form/TextBox"
		// |					 data-dojo-props="value: at('rel:', 'First')"></span>
		// |				</div>
		// |			</script>
		// |		</div>
		// example:
		//		Programmatically create multiple instances of widgets-in-template based on the HTML stored in childTemplate.
		//		(childTemplate may come from dojo/text)
		//		Also programmatically establish data binding at child widget's startup phase.
		//		The label refers to Serial property in the array item, and the text box refers to First property in the array item.
		// |		var childTemplate = '<div>'
		// |		 + '<span data-dojo-type="dijit/_WidgetBase"'
		// |		 + ' data-dojo-attach-point="labelNode"'
		// |		 + ' data-dojo-props="_setValueAttr: {node: \'domNode\', type: \'innerText\'}"></span>'
		// |		 + '<span data-dojo-type="dijit/form/TextBox"'
		// |		 + ' data-dojo-attach-point="inputNode"></span>'
		// |		 + '</div>';
		// |		(new WidgetList({
		// |			children: array,
		// |			childParams: {
		// |				startup: function(){
		// |					this.labelNode.set("value", at("rel:", "Serial"));
		// |					this.inputNode.set("value", at("rel:", "First"));
		// |					this.inherited("startup", arguments);
		// |				}
		// |			},
		// |			templateString: childTemplate
		// |		}, dom.byId("programmaticRepeat"))).startup();
		// example:
		//		Using the same childTemplate above, establish data binding for child widgets based on the declaration in childBindings.
		//		(childBindings may come from dojo/text, by eval()'ing the text)
		// |		var childBindings = {
		// |			labelNode: {value: at("rel:", "Serial")},
		// |			inputNode: {value: at("rel:", "First")}
		// |		};
		// |		(new WidgetList({
		// |			children: array,
		// |			templateString: childTemplate,
		// |			childBindings: childBindings
		// |		}, dom.byId("programmaticRepeatWithSeparateBindingDeclaration"))).startup();

		// childClz: Function
		//		The class of the child widget. Takes precedence over childType/childMixins.
		childClz: null,

		// childType: String|Function
		//		The module ID of child widget, or a function that takes child data as the argument and returns the module ID of child widget. childClz takes precedence over this/childMixins.
		//		Can be specified via data-mvc-child-type attribute of widget declaration.
		childType: "",

		// childMixins: String|String[]|Function
		//		The list of module IDs (separated by comma), or a function that takes child data as the argument and returns it, of the classes that will be mixed into child widget. childClz takes precedence over childType/this.
		//		Can be specified via data-mvc-child-mixins attribute of widget declaration.
		childMixins: "",

		// childParams: Object|Function
		//		The mixin properties for child widget.
		//		Can be specified via data-mvc-child-props attribute of widget declaration.
		//		"this" in data-mvc-child-props will have the following properties:
		//
		//		- parent - This widget's instance.
		//		- target - The data item in children.
		childParams: null,

		// childBindings: Object|Function
		//		Data bindings for child widget.
		childBindings: null,

		// children: dojox/mvc/StatefulArray
		//		The array of data model that is used to render child nodes.
		children: null,

		/*=====
		// templateString: String
		//		The template string for each child items. templateString in child widgets take precedence over this.
		templateString: "",
		=====*/

		// partialRebuild: Boolean
		//		If true, only rebuild repeat items for changed elements. Otherwise, rebuild everything if there is a change in children.
		partialRebuild: false,

		// _relTargetProp: String
		//		The name of the property that is used by child widgets for relative data binding.
		_relTargetProp : "children",

		postMixInProperties: function(){
			this.inherited(arguments);
			if(this[childTypeAttr]){
				this.childType = this[childTypeAttr];
			}
			if(this[childMixinsAttr]){
				this.childMixins = this[childMixinsAttr];
			}
		},

		startup: function(){
			this.inherited(arguments);
			this._setChildrenAttr(this.children);
		},

		_setChildrenAttr: function(/*dojo/Stateful*/ value){
			// summary:
			//		Handler for calls to set("children", val).

			var children = this.children;
			this._set("children", value);
			if(this._started && (!this._builtOnce || children != value)){
				this._builtOnce = true;
				this._buildChildren(value);
				if(lang.isArray(value)){
					var _self = this;
					value.watch !== {}.watch && (this._handles = this._handles || []).push(value.watch(function(name, old, current){
						if(!isNaN(name)){
							var w = _self.getChildren()[name - 0];
							w && w.set(w._relTargetProp || "target", current);
						}
					}));
				}
			}
		},

		_buildChildren: function(/*dojox/mvc/StatefulArray*/ children){
			// summary:
			//		Create child widgets upon children and inserts them into the container node.

			unwatchElements(this);
			for(var cw = this.getChildren(), w = null; w = cw.pop();){ this.removeChild(w); w.destroy(); }
			if(!lang.isArray(children)){ return; }

			var _self = this,
			 seq = this._buildChildrenSeq = (this._buildChildrenSeq || 0) + 1,
			 initial = {idx: 0, removals: [], adds: [].concat(children)},
			 changes = [initial];

			function loadedModule(/*Object*/ change){
				// summary:
				//		The callback function called when modules associated with an array splice have been loaded.
				// description:
				//		Looks through the queued array splices and process queue entries whose modules have been loaded, by removing/adding child widgets upon the array splice.

				if(this._beingDestroyed || this._buildChildrenSeq > seq){ return; } // If this _WidgetList is being destroyed, or newer _buildChildren call comes during lazy loading, bail

				// Associate an object associated with an array splice with the module loaded
				var list = [].slice.call(arguments, 1);
				change.clz = lang.isFunction(this.childType) || lang.isFunction(this.childMixins) ? list : list[0];

				// Looks through the queued array splices
				for(var item = null; item = changes.shift();){
					// The modules for the array splice have not been loaded, bail
					if(!item.clz){
						changes.unshift(item);
						break;
					}

					// Remove child widgets upon the array removals
					for(var i = 0, l = (item.removals || []).length; i < l; ++i){
						this.removeChild(item.idx);
					}

					// Create/add child widgets upon the array adds
					array.forEach(array.map(item.adds, function(child, idx){
						var params = {
							ownerDocument: this.ownerDocument,
							parent: this,
							indexAtStartup: item.idx + idx // Won't be updated even if there are removals/adds of repeat items after startup
						}, childClz = lang.isArray(item.clz) ? item.clz[idx] : item.clz;
						params[(lang.isFunction(this.childParams) && this.childParams.call(params, this) || this.childParams || this[childParamsAttr] && evalParams.call(params, this[childParamsAttr]) || {})._relTargetProp || childClz.prototype._relTargetProp || "target"] = child;

						var childParams = this.childParams || this[childParamsAttr] && evalParams.call(params, this[childParamsAttr]),
						 childBindings = this.childBindings || this[childBindingsAttr] && evalParams.call(params, this[childBindingsAttr]);
						if(this.templateString && !params.templateString && !childClz.prototype.templateString){ params.templateString = this.templateString; }
						if(childBindings && !params.bindings && !childClz.prototype.bindings){ params.bindings = childBindings; }
						return new childClz(lang.delegate(lang.isFunction(childParams) ? childParams.call(params, this) : childParams, params));
					}, this), function(child, idx){
						this.addChild(child, item.idx + idx);
					}, this);
				}
			}

			lang.isFunction(children.watchElements) && (this._handles = this._handles || []).push(children.watchElements(function(idx, removals, adds){
				if(!removals || !adds || !_self.partialRebuild){
					// If the entire array is changed, or this WidgetList should rebuild the whole child widgets with every change in array, rebuild the whole
					_self._buildChildren(children);
				}else{
					// Otherwise queue the array splice and load modules associated with the additions
					var change = {idx: idx, removals: removals, adds: adds};
					changes.push(change);
					loadModules.call(_self, adds, lang.hitch(_self, loadedModule, change));
				}
			}));

			// Load modules associated with the initial data
			loadModules.call(this, children, lang.hitch(this, loadedModule, initial));
		},

		destroy: function(){
			unwatchElements(this);
			this.inherited(arguments);
		}
	});

	WidgetList.prototype[childTypeAttr] = WidgetList.prototype[childMixinsAttr] = WidgetList.prototype[childParamsAttr] = WidgetList.prototype[childBindingsAttr] = ""; // Let parser treat these attributes as string
	return WidgetList;
});
