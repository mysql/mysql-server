define("dojox/dtl/_Templated", [
	"dojo/aspect",
	"dojo/_base/declare",
	"./_base",
	"dijit/_TemplatedMixin",
	"dojo/dom-construct",
	"dojo/cache",
	"dojo/_base/array",
	"dojo/string",
	"dojo/parser"
], function(aspect,declare,dd,TemplatedMixin, domConstruct,Cache,Array,dString,Parser){

	return declare("dojox.dtl._Templated", TemplatedMixin, {
		// summary:
		//		The base-class for DTL-templated widgets.

		_dijitTemplateCompat: false,
		
		buildRendering: function(){
			var node;

			if(this.domNode && !this._template){
				return;
			}

			if(!this._template){
				var t = this.getCachedTemplate(
					this.templatePath,
					this.templateString,
					this._skipNodeCache
				);
				if(t instanceof dd.Template) {
					this._template = t;
				}else{
					node = t.cloneNode(true);
				}
			}
			if(!node){
				var context = new dd._Context(this);
				if(!this._created){
					delete context._getter;
				}
				var nodes = domConstruct.toDom(
					this._template.render(context)
				);
				// TODO: is it really necessary to look for the first node?
				if(nodes.nodeType !== 1 && nodes.nodeType !== 3){
					// nodes.nodeType === 11
					// the node is a document fragment
					for(var i = 0, l = nodes.childNodes.length; i < l; ++i){
						node = nodes.childNodes[i];
						if(node.nodeType == 1){
							break;
						}
					}
				}else{
					// the node is an element or a text
					node = nodes;
				}
			}
			this._attachTemplateNodes(node);
			if(this.widgetsInTemplate){
				//Make sure dojoType is used for parsing widgets in template.
				//The Parser.query could be changed from multiversion support.
				var parser = Parser, qry, attr;
				if(parser._query != "[dojoType]"){
					qry = parser._query;
					attr = parser._attrName;
					parser._query = "[dojoType]";
					parser._attrName = "dojoType";
				}

				//Store widgets that we need to start at a later point in time
				var cw = (this._startupWidgets = Parser.parse(node, {
					noStart: !this._earlyTemplatedStartup,
					inherited: {dir: this.dir, lang: this.lang}
				}));

				//Restore the query.
				if(qry){
					parser._query = qry;
					parser._attrName = attr;
				}

				// Hook up attach points and events for nodes that were converted to widgets
				for(var i = 0; i < cw.length; i++){
					this._processTemplateNode(cw[i], function(n,p){
						return n[p];
					}, function(widget, type, callback){
						// function to do data-dojo-attach-event to a widget
						if(type in widget){
							// back-compat, remove for 2.0
							return aspect.after(widget, type, callback, true);
						}else{
							// 1.x may never hit this branch, but it's the default for 2.0
							return widget.on(type, callback, true);
						}
					});
				}
			}

			if(this.domNode){
				domConstruct.place(node, this.domNode, "before");
				this.destroyDescendants();
				domConstruct.destroy(this.domNode);
			}
			this.domNode = node;

			this._fillContent(this.srcNodeRef);
		},

		_processTemplateNode: function(/*DOMNode|Widget*/ baseNode, getAttrFunc, attachFunc){
			// Override _AttachMixin._processNode to skip nodes with data-dojo-type set.   They are handled separately
			// in the buildRendering() code above.

			if(this.widgetsInTemplate && (getAttrFunc(baseNode, "dojoType") || getAttrFunc(baseNode, "data-dojo-type"))){
				return true;
			}

			this.inherited(arguments);
		},

		_templateCache: {},
		getCachedTemplate: function(templatePath, templateString, alwaysUseString){
			// summary:
			//		Layer for dijit._Templated.getCachedTemplate
			var tmplts = this._templateCache;
			var key = templateString || templatePath;
			if(tmplts[key]){
				return tmplts[key];
			}

			templateString = dString.trim(templateString || Cache(templatePath, {sanitize: true}));

			if(	this._dijitTemplateCompat &&
				(alwaysUseString || templateString.match(/\$\{([^\}]+)\}/g))
			){
				templateString = this._stringRepl(templateString);
			}

			// If we always use a string, or find no variables, just store it as a node
			if(alwaysUseString || !templateString.match(/\{[{%]([^\}]+)[%}]\}/g)){
				return tmplts[key] = domConstruct.toDom(templateString);
			}else{
				return tmplts[key] = new dd.Template(templateString);
			}
		},
		render: function(){
			// summary:
			//		Renders the widget.
			this.buildRendering();
		},
		startup: function(){
			Array.forEach(this._startupWidgets, function(w){
				if(w && !w._started && w.startup){
					w.startup();
				}
			});
			this.inherited(arguments);
		}
	});
});
