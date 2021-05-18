define("dojox/calendar/RendererManager", [
	"dojo/_base/declare",
	"dojo/_base/array",
	"dojo/_base/html",
	"dojo/_base/lang",
	"dojo/dom-class",
	"dojo/dom-style",
	"dojo/Stateful",
	"dojo/Evented"],

	function(
		declare,
		arr,
		html,
		lang,
		domClass,
		domStyle,
		Stateful,
		Evented,
		when){

	return declare("dojox.calendar.RendererManager", [Stateful, Evented], {

		// summary:
		//		This mixin contains the store management.

		// owner: Object
		//	The owner of the store manager: a view or a calendar widget.
		owner: null,

		// rendererPool: [protected] Array
		//		The stack of recycled renderers available.
		rendererPool: null,

		// rendererList: [protected] Array
		//		The list of used renderers
		rendererList: null,

		// itemToRenderer: [protected] Object
		//		The associated array item to renderer list.
		itemToRenderer: null,

		constructor: function(/*Object*/ args){
			args = args || {};

			this.rendererPool = [];
			this.rendererList = [];
			this.itemToRenderer = {};
		},

		destroy: function(){
			while(this.rendererList.length > 0){
				this.destroyRenderer(this.rendererList.pop());
			}
			for(var kind in this._rendererPool){
				var pool = this._rendererPool[kind];
				if(pool){
					while(pool.length > 0){
						this.destroyRenderer(pool.pop());
					}
				}
			}
		},

		recycleItemRenderers: function(remove){
			// summary:
			//		Recycles all the item renderers.
			// remove: Boolean
			//		Whether remove the DOM node from it parent.
			// tags:
			//		protected
			while(this.rendererList.length>0){
				var ir = this.rendererList.pop();
				this.recycleRenderer(ir, remove);
			}
			this.itemToRenderer = {};
		},

		getRenderers: function(item){
			// summary:
			//		Returns the renderers that are currently used to displayed the speficied item.
			//		Returns an array of objects that contains two properties:
			//		- container: The DOM node that contains the renderer.
			//		- renderer: The dojox.calendar._RendererMixin instance.
			//		Do not keep references on the renderers are they are recycled and reused for other items.
			// item: Object
			//		The data or render item.
			// returns: Object[]
			if(item == null || item.id == null){
				return null;
			}
			var list = this.itemToRenderer[item.id];
			return list == null ? null : list.concat();
		},

		createRenderer: function(item, kind, rendererClass, cssClass){
			// summary:
			//		Creates an item renderer of the specified kind. A renderer is an object with the "container" and "instance" properties.
			// item: Object
			//		The data item.
			// kind: String
			//		The kind of renderer.
			// rendererClass: Object
			//		The class to instantiate to create the renderer.
			// returns: Object
			// tags:
			//		protected

			if(item != null && kind != null && rendererClass != null){

				var res=null, renderer=null;

				var pool = this.rendererPool[kind];

				if(pool != null){
					res = pool.shift();
				}

				if (res == null){

					renderer = new rendererClass;

					res = {
						renderer: renderer,
						container: renderer.domNode,
						kind: kind
					};

					this.emit("rendererCreated", {renderer:res, source:this.owner, item:item});

				} else {
					renderer = res.renderer;

					this.emit("rendererReused", {renderer:renderer, source:this.owner, item:item});
				}

				renderer.owner = this.owner;
				renderer.set("rendererKind", kind);
				renderer.set("item", item);

				var list = this.itemToRenderer[item.id];
				if (list == null) {
					this.itemToRenderer[item.id] = list = [];
				}
				list.push(res);

				this.rendererList.push(res);
				return res;
			}
			return null;
		},

		recycleRenderer: function(renderer, remove){
			// summary:
			//		Recycles the item renderer to be reused in the future.
			// renderer: dojox/calendar/_RendererMixin
			//		The item renderer to recycle.
			// tags:
			//		protected

			this.emit("rendererRecycled", {renderer:renderer, source:this.owner});

			var pool = this.rendererPool[renderer.kind];

			if(pool == null){
				this.rendererPool[renderer.kind] = [renderer];
			}else{
				pool.push(renderer);
			}

			if(remove){
				renderer.container.parentNode.removeChild(renderer.container);
			}

			domStyle.set(renderer.container, "display", "none");

			renderer.renderer.owner = null;
			renderer.renderer.set("item", null);
		},

		destroyRenderer: function(renderer){
			// summary:
			//		Destroys the item renderer.
			// renderer: dojox/calendar/_RendererMixin
			//		The item renderer to destroy.
			// tags:
			//		protected
			this.emit("rendererDestroyed", {renderer:renderer, source:this.owner});

			var ir = renderer.renderer;

			if(ir["destroy"]){
				ir.destroy();
			}

			html.destroy(renderer.container);
		},

		destroyRenderersByKind: function(kind){
			// tags:
			//		private

			var list = [];
			for(var i=0;i<this.rendererList.length;i++){
				var ir = this.rendererList[i];
				if(ir.kind == kind){
					this.destroyRenderer(ir);
				}else{
					list.push(ir);
				}
			}

			this.rendererList = list;

			var pool = this.rendererPool[kind];
			if(pool){
				while(pool.length > 0){
					this.destroyRenderer(pool.pop());
				}
			}

		}
	});

});
