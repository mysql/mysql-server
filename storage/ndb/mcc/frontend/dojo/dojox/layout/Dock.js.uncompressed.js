define("dojox/layout/Dock", [
	"dojo/_base/lang", "dojo/_base/window", "dojo/_base/declare",
	"dojo/_base/fx", "dojo/on", "dojo/_base/array", "dojo/_base/sniff",
	"dojo/window", "dojo/dom", "dojo/dom-class", "dojo/dom-geometry", "dojo/dom-construct",
	"dijit/_TemplatedMixin", "dijit/_WidgetBase"
], function(
	lang, winUtil, declare, fx, on, arrayUtil, 
	has, windowLib, dom, domClass, domGeom, domConstruct, _TemplatedMixin, _WidgetBase
){

//TODO: don't want to rely on kernel just to make something as experimental	
//kernel.experimental("dojox.layout.Dock");

var Dock = declare("dojox.layout.Dock",[_WidgetBase, _TemplatedMixin],{
	// summary:
	//		A widget that attaches to a node and keeps track of incoming / outgoing FloatingPanes
	//		and handles layout

	templateString: '<div class="dojoxDock"><ul data-dojo-attach-point="containerNode" class="dojoxDockList"></ul></div>',

	// _docked: [private] Array
	//		array of panes currently in our dock
	_docked: [],
	
	_inPositioning: false,
	
	autoPosition: false,
	
	addNode: function(refNode){
		// summary:
		//		Insert a dockNode reference into the dock
		
		var div = domConstruct.create('li', null, this.containerNode),
			node = new DockNode({
				title: refNode.title,
				paneRef: refNode
			}, div)
		;
		node.startup();
		return node;
	},

	startup: function(){
				
		if (this.id == "dojoxGlobalFloatingDock" || this.isFixedDock) {
			// attach window.onScroll, and a position like in presentation/dialog
			this.own(
				on(window, "resize", lang.hitch(this, "_positionDock")),
				on(window, "scroll", lang.hitch(this, "_positionDock"))
			);
			if(has("ie")){ // TODO: All versions of IE, or pre-IE9, or some feature to test?
				this.own(
					on(this.domNode, "resize", lang.hitch(this, "_positionDock"))
				);
			}
		}
		this._positionDock(null);
		this.inherited(arguments);

	},
	
	_positionDock: function(/* Event? */e){
		if(!this._inPositioning){
			if(this.autoPosition == "south"){
				// Give some time for scrollbars to appear/disappear
				this.defer(function() {
					this._inPositiononing = true;
					var viewport = windowLib.getBox();
					var s = this.domNode.style;
					s.left = viewport.l + "px";
					s.width = (viewport.w-2) + "px";
					s.top = (viewport.h + viewport.t) - this.domNode.offsetHeight + "px";
					this._inPositioning = false;
				}, 125);
			}
		}
	}


});

var DockNode = declare("dojox.layout._DockNode",[_WidgetBase, _TemplatedMixin],{
	// summary:
	//		dojox.layout._DockNode is a private widget used to keep track of
	//		which pane is docked.

	// title: String
	//		Shown in dock icon. should read parent iconSrc?
	title: "",

	// paneRef: Widget
	//		reference to the FloatingPane we reprasent in any given dock
	paneRef: null,

	templateString:
		'<li data-dojo-attach-event="onclick: restore" class="dojoxDockNode">'+
			'<span data-dojo-attach-point="restoreNode" class="dojoxDockRestoreButton" data-dojo-attach-event="onclick: restore"></span>'+
			'<span class="dojoxDockTitleNode" data-dojo-attach-point="titleNode">${title}</span>'+
		'</li>',

	restore: function(){
		// summary:
		//		remove this dock item from parent dock, and call show() on reffed floatingpane
		this.paneRef.show();
		this.paneRef.bringToTop();
		this.destroy();
	}
});

return Dock;
});

