define(["require", "dojo/when", "dojo/on", "dojo/_base/declare", "dojo/_base/lang", "dojo/Deferred",
		"dijit/Destroyable", "dijit/_TemplatedMixin", "dijit/_WidgetsInTemplateMixin", "./ViewBase", "./utils/nls"],
	function(require, when, on, declare, lang, Deferred, Destroyable, _TemplatedMixin, _WidgetsInTemplateMixin, ViewBase, nls){

	return declare("dojox.app.View", [_TemplatedMixin, _WidgetsInTemplateMixin, Destroyable, ViewBase], {
		// summary:
		//		View class inheriting from ViewBase adding templating & globalization capabilities.
		constructor: function(params){
			// summary:
			//		Constructs a View instance either from a configuration or programmatically.
			//
			// example:
			//		|	use configuration file
			//		|
			// 		|	// load view controller from views/simple.js by default
			//		|	"simple":{
			//		|		"template": "myapp/views/simple.html",
			//		|		"nls": "myapp/nls/simple"
			//		|		"dependencies":["dojox/mobile/TextBox"]
			//		|	}
			//		|
			//		|	"home":{
			//		|		"template": "myapp/views/home.html", // no controller set to not use a view controller
			//		|		"dependencies":["dojox/mobile/TextBox"]
			//		|	}
			//		|	"main":{
			//		|		"template": "myapp/views/main.html",
			//		|		"controller": "myapp/views/main.js", // identify load view controller from views/main.js
			//		|		"dependencies":["dojox/mobile/TextBox"]
			//		|	}
			//
			// example:
			//		|	var viewObj = new View({
			//		|		app: this.app,
			//		|		id: this.id,
			//		|		name: this.name,
			//		|		parent: this,
			//		|		templateString: this.templateString,
			//		|		template: this.template, 
			//		|		controller: this.controller
			//		|	});
			//		|	viewObj.start(); // start view
			//
			// params:
			//		view parameters, include:
			//
			//		- app: the app
			//		- id: view id
			//		- name: view name
			//		- template: view template identifier. If templateString is not empty, this parameter is ignored.
			//		- templateString: view template string
			//		- controller: view controller module identifier
			//		- parent: parent view
			//		- children: children views
			//		- nls: nls definition module identifier
		},

		// _TemplatedMixin requires a connect method if data-dojo-attach-* are used
		connect: function(obj, event, method){
			return this.own(on(obj, event, lang.hitch(this, method)))[0]; // handle
		},

		_loadTemplate: function(){
			// summary:
			//		load view HTML template and dependencies.
			// tags:
			//		private
			//

			if(this.templateString){
				return true;
			}else{
				var tpl = this.template;
				var deps = this.dependencies?this.dependencies:[];
				if(tpl){
					if(tpl.indexOf("./") == 0){
						tpl = "app/"+tpl;
					}
					deps = deps.concat(["dojo/text!"+tpl]);
				}
				var def = new Deferred();
				if(deps.length > 0){
					var requireSignal;
					try{
						requireSignal = require.on ? require.on("error", lang.hitch(this, function(error){
							if(def.isResolved() || def.isRejected()){
								return;
							}
							if(error.info[0] && error.info[0].indexOf(this.template) >= 0 ){
								def.resolve(false);
								if(requireSignal){
									requireSignal.remove();
								}
							}
						})) :  null;
						require(deps, function(){
							def.resolve.call(def, arguments);
							if(requireSignal){
								requireSignal.remove();
							}
						});
					}catch(e){
						def.resolve(false);
						if(requireSignal){
							requireSignal.remove();
						}
					}
				}else{
					def.resolve(true);
				}
				var loadViewDeferred = new Deferred();
				when(def, lang.hitch(this, function(){
					this.templateString = this.template ? arguments[0][arguments[0].length - 1] : "<div></div>";
					loadViewDeferred.resolve(this);
				}));
				return loadViewDeferred;
			}
		},

		// start view
		load: function(){
			var tplDef = new Deferred();
			var defDef = this.inherited(arguments);
			var nlsDef = nls(this);
			// when parent loading is done (controller), proceed with template
			// (for data-dojo-* to work we need to wait for controller to be here, this is also
			// useful when the controller is used as a layer for the view)
			when(defDef, lang.hitch(this, function(){
				when(nlsDef, lang.hitch(this, function(nls){
					// we inherit from the parent NLS
					this.nls = lang.mixin({}, this.parent.nls);
					if(nls){
						// make sure template can access nls doing ${nls.myprop}
						lang.mixin(this.nls, nls);
					}
					when(this._loadTemplate(), function(value){
						tplDef.resolve(value);
					});
				}));
			}));
			return tplDef;
		},

		_startup: function(){
			// summary:
			//		startup widgets in view template.
			// tags:
			//		private
			this.buildRendering();
			this.inherited(arguments);
		}
	});
});
