/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

/*
	This is an optimized version of Dojo, built for deployment and not for
	development. To get sources and documentation, please visit:

		http://dojotoolkit.org
*/

//>>built
require({cache:{
'dojo/uacss':function(){
define(["./dom-geometry", "./_base/lang", "./ready", "./_base/sniff", "./_base/window"],
	function(geometry, lang, ready, has, baseWindow){
	// module:
	//		dojo/uacss
	// summary:
	//		Applies pre-set CSS classes to the top-level HTML node, based on:
	//			- browser (ex: dj_ie)
	//			- browser version (ex: dj_ie6)
	//			- box model (ex: dj_contentBox)
	//			- text direction (ex: dijitRtl)
	//
	//		In addition, browser, browser version, and box model are
	//		combined with an RTL flag when browser text is RTL. ex: dj_ie-rtl.

	var
		html = baseWindow.doc.documentElement,
		ie = has("ie"),
		opera = has("opera"),
		maj = Math.floor,
		ff = has("ff"),
		boxModel = geometry.boxModel.replace(/-/,''),

		classes = {
			"dj_ie": ie,
			"dj_ie6": maj(ie) == 6,
			"dj_ie7": maj(ie) == 7,
			"dj_ie8": maj(ie) == 8,
			"dj_ie9": maj(ie) == 9,
			"dj_quirks": has("quirks"),
			"dj_iequirks": ie && has("quirks"),

			// NOTE: Opera not supported by dijit
			"dj_opera": opera,

			"dj_khtml": has("khtml"),

			"dj_webkit": has("webkit"),
			"dj_safari": has("safari"),
			"dj_chrome": has("chrome"),

			"dj_gecko": has("mozilla"),
			"dj_ff3": maj(ff) == 3
		}; // no dojo unsupported browsers

	classes["dj_" + boxModel] = true;

	// apply browser, browser version, and box model class names
	var classStr = "";
	for(var clz in classes){
		if(classes[clz]){
			classStr += clz + " ";
		}
	}
	html.className = lang.trim(html.className + " " + classStr);

	// If RTL mode, then add dj_rtl flag plus repeat existing classes with -rtl extension.
	// We can't run the code below until the <body> tag has loaded (so we can check for dir=rtl).
	// priority is 90 to run ahead of parser priority of 100
	ready(90, function(){
		if(!geometry.isBodyLtr()){
			var rtlClassStr = "dj_rtl dijitRtl " + classStr.replace(/ /g, "-rtl ");
			html.className = lang.trim(html.className + " " + rtlClassStr + "dj_rtl dijitRtl " + classStr.replace(/ /g, "-rtl "));
		}
	});
	return has;
});

},
'dojox/mobile/app/_Widget':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dijit/_WidgetBase"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app._Widget");
dojo.experimental("dojox.mobile.app._Widget");

dojo.require("dijit._WidgetBase");

dojo.declare("dojox.mobile.app._Widget", dijit._WidgetBase, {
	// summary:
	//		The base mobile app widget.

	getScroll: function(){
		// summary:
		//		Returns the scroll position.
		return {
			x: dojo.global.scrollX,
			y: dojo.global.scrollY
		};
	},

	connect: function(target, event, fn){
		if(event.toLowerCase() == "dblclick"
			|| event.toLowerCase() == "ondblclick"){

			if(dojo.global["Mojo"]){
				// Handle webOS tap event
				return this.connect(target, Mojo.Event.tap, fn);
			}
		}
		return this.inherited(arguments);
	}
});
});

},
'dojox/mobile/app/ImageThumbView':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dijit/_WidgetBase,dojo/string"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.ImageThumbView");
dojo.experimental("dojox.mobile.app.ImageThumbView");

dojo.require("dijit._WidgetBase");
dojo.require("dojo.string");

dojo.declare("dojox.mobile.app.ImageThumbView", dijit._WidgetBase, {
	// summary:
	//		An image thumbnail gallery

	// items: Array
	//		The data items from which the image urls are retrieved.
	//		If an item is a string, it is expected to be a URL. Otherwise
	//		by default it is expected to have a 'url' member.  This can
	//		be configured using the 'urlParam' attribute on this widget.
	items: [],

	// urlParam: String
	//		The paramter name used to retrieve an image url from a JSON object
	urlParam: "url",

	labelParam: null,

	itemTemplate: '<div class="mblThumbInner">' +
				'<div class="mblThumbOverlay"></div>' +
				'<div class="mblThumbMask">' +
					'<div class="mblThumbSrc" style="background-image:url(${url})"></div>' +
				'</div>' +
			'</div>',

	minPadding: 4,

	maxPerRow: 3,

	maxRows: -1,

	baseClass: "mblImageThumbView",

	thumbSize: "medium",

	animationEnabled: true,

	selectedIndex: -1,

	cache: null,

	cacheMustMatch: false,

	clickEvent: "onclick",

	cacheBust: false,

	disableHide: false,

	constructor: function(params, node){
	},

	postCreate: function(){

		this.inherited(arguments);
		var _this = this;

		var hoverCls = "mblThumbHover";

		this.addThumb = dojo.hitch(this, this.addThumb);
		this.handleImgLoad = dojo.hitch(this, this.handleImgLoad);
		this.hideCached = dojo.hitch(this, this.hideCached);

		this._onLoadImages = {};

		this.cache = [];
		this.visibleImages = [];

		this._cacheCounter = 0;

		this.connect(this.domNode, this.clickEvent, function(event){
			var itemNode = _this._getItemNodeFromEvent(event);

			if(itemNode && !itemNode._cached){
				_this.onSelect(itemNode._item, itemNode._index, _this.items);
				dojo.query(".selected", this.domNode).removeClass("selected");
				dojo.addClass(itemNode, "selected");
			}
		});

		dojo.addClass(this.domNode, this.thumbSize);

		this.resize();
		this.render();
	},

	onSelect: function(item, index, items){
		// summary:
		//		Dummy function that is triggered when an image is selected.
	},

	_setAnimationEnabledAttr: function(value){
		this.animationEnabled = value;
		dojo[value ? "addClass" : "removeClass"](this.domNode, "animated");
	},

	_setItemsAttr: function(items){
		this.items = items || [];

		var urls = {};
		var i;
		for(i = 0; i < this.items.length; i++){
			urls[this.items[i][this.urlParam]] = 1;
		}

		var clearedUrls = [];
		for(var url in this._onLoadImages){
			if(!urls[url] && this._onLoadImages[url]._conn){
				dojo.disconnect(this._onLoadImages[url]._conn);
				this._onLoadImages[url].src = null;
				clearedUrls.push(url);
			}
		}

		for(i = 0; i < clearedUrls.length; i++){
			delete this._onLoadImages[url];
		}

		this.render();
	},

	_getItemNode: function(node){
		while(node && !dojo.hasClass(node, "mblThumb") && node != this.domNode){
			node = node.parentNode;
		}

		return (node == this.domNode) ? null : node;
	},

	_getItemNodeFromEvent: function(event){
		if(event.touches && event.touches.length > 0){
			event = event.touches[0];
		}
		return this._getItemNode(event.target);
	},

	resize: function(){
		this._thumbSize = null;

		this._size = dojo.contentBox(this.domNode);

		this.disableHide = true;
		this.render();
		this.disableHide = false;
	},

	hideCached: function(){
		// summary:
		//		Hides all cached nodes, so that they're no invisible and overlaying
		//		other screen elements.
		for(var i = 0; i < this.cache.length; i++){
			if (this.cache[i]) {
				dojo.style(this.cache[i], "display", "none");
			}
		}
	},

	render: function(){
		var i;
		var url;
		var item;

		var thumb;
		while(this.visibleImages && this.visibleImages.length > 0){
			thumb = this.visibleImages.pop();
			this.cache.push(thumb);

			if (!this.disableHide) {
				dojo.addClass(thumb, "hidden");
			}
			thumb._cached = true;
		}

		if(this.cache && this.cache.length > 0){
			setTimeout(this.hideCached, 1000);
		}

		if(!this.items || this.items.length == 0){
			return;
		}

		for(i = 0; i < this.items.length; i++){
			item = this.items[i];
			url = (dojo.isString(item) ? item : item[this.urlParam]);

			this.addThumb(item, url, i);

			if(this.maxRows > 0 && (i + 1) / this.maxPerRow >= this.maxRows){
				break;
			}
		}

		if(!this._thumbSize){
			return;
		}

		var column = 0;
		var row = -1;

		var totalThumbWidth = this._thumbSize.w + (this.padding * 2);
		var totalThumbHeight = this._thumbSize.h + (this.padding * 2);

		var nodes = this.thumbNodes =
			dojo.query(".mblThumb", this.domNode);

		var pos = 0;
		nodes = this.visibleImages;
		for(i = 0; i < nodes.length; i++){
			if(nodes[i]._cached){
				continue;
			}

			if(pos % this.maxPerRow == 0){
				row ++;
			}
			column = pos % this.maxPerRow;

			this.place(
				nodes[i],
				(column * totalThumbWidth) + this.padding,	// x position
				(row * totalThumbHeight) + this.padding		// y position
			);

			if(!nodes[i]._loading){
				dojo.removeClass(nodes[i], "hidden");
			}

			if(pos == this.selectedIndex){
				dojo[pos == this.selectedIndex ? "addClass" : "removeClass"]
					(nodes[i], "selected");
			}
			pos++;
		}

		var numRows = Math.ceil(pos / this.maxPerRow);

		this._numRows = numRows;

		this.setContainerHeight((numRows * (this._thumbSize.h + this.padding * 2)));
	},

	setContainerHeight: function(amount){
		dojo.style(this.domNode, "height", amount + "px");
	},

	addThumb: function(item, url, index){

		var thumbDiv;
		var cacheHit = false;
		if(this.cache.length > 0){
			// Reuse a previously created node if possible
			var found = false;
			// Search for an image with the same url first
			for(var i = 0; i < this.cache.length; i++){
				if(this.cache[i]._url == url){
					thumbDiv = this.cache.splice(i, 1)[0];
					found = true;
					break
				}
			}

			// if no image with the same url is found, just take the last one
			if(!thumbDiv && !this.cacheMustMatch){
            	thumbDiv = this.cache.pop();
				dojo.removeClass(thumbDiv, "selected");
			} else {
				cacheHit = true;
			}
		}

		if(!thumbDiv){

			// Create a new thumb
			thumbDiv = dojo.create("div", {
				"class": "mblThumb hidden",
				innerHTML: dojo.string.substitute(this.itemTemplate, {
					url: url
				}, null, this)
			}, this.domNode);
		}

		if(this.labelParam) {
			var labelNode = dojo.query(".mblThumbLabel", thumbDiv)[0];
			if(!labelNode) {
				labelNode = dojo.create("div", {
					"class": "mblThumbLabel"
				}, thumbDiv);
			}
			labelNode.innerHTML = item[this.labelParam] || "";
		}

	    dojo.style(thumbDiv, "display", "");
		if (!this.disableHide) {
			dojo.addClass(thumbDiv, "hidden");
		}

		if (!cacheHit) {
			var loader = dojo.create("img", {});
			loader._thumbDiv = thumbDiv;
			loader._conn = dojo.connect(loader, "onload", this.handleImgLoad);
			loader._url = url;
			thumbDiv._loading = true;

			this._onLoadImages[url] = loader;
			if (loader) {
				loader.src = url;
			}
		}
		this.visibleImages.push(thumbDiv);

		thumbDiv._index = index;
		thumbDiv._item = item;
		thumbDiv._url = url;
		thumbDiv._cached = false;

		if(!this._thumbSize){
			this._thumbSize = dojo.marginBox(thumbDiv);

			if(this._thumbSize.h == 0){
				this._thumbSize.h = 100;
				this._thumbSize.w = 100;
			}

			if(this.labelParam){
				this._thumbSize.h += 8;
			}

			this.calcPadding();
		}
	},

	handleImgLoad: function(event){
		var img = event.target;
		dojo.disconnect(img._conn);
		dojo.removeClass(img._thumbDiv, "hidden");
		img._thumbDiv._loading = false;
		img._conn = null;

		var url = img._url;
		if(this.cacheBust){
			url += (url.indexOf("?") > -1 ? "&" : "?")
				+ "cacheBust=" + (new Date()).getTime() + "_" + (this._cacheCounter++);
		}

		dojo.query(".mblThumbSrc", img._thumbDiv)
				.style("backgroundImage", "url(" + url + ")");

		delete this._onLoadImages[img._url];
	},

	calcPadding: function(){
		var width = this._size.w;

		var thumbWidth = this._thumbSize.w;

		var imgBounds = thumbWidth + this.minPadding;

		this.maxPerRow = Math.floor(width / imgBounds);

		this.padding = Math.floor((width - (thumbWidth * this.maxPerRow)) / (this.maxPerRow * 2));
	},

	place: function(node, x, y){
		dojo.style(node, {
			"-webkit-transform" :"translate(" + x + "px," + y + "px)"
		});
	},

	destroy: function(){
		// Stop the loading of any more images

		var img;
		var counter = 0;
		for (var url in this._onLoadImages){
			img = this._onLoadImages[url];
			if (img) {
				img.src = null;
				counter++;
			}
		}

		this.inherited(arguments);
	}
});
});

},
'dojox/mobile/TransitionEvent':function(){
define("dojox/mobile/TransitionEvent", [
	"dojo/_base/declare",
	"dojo/_base/Deferred",
	"dojo/_base/lang",
	"dojo/on",
	"./transition"
], function(declare, Deferred, lang, on, transitDeferred){

	return declare("dojox.mobile.TransitionEvent", null, {
		constructor: function(target, transitionOptions, triggerEvent){
			this.transitionOptions=transitionOptions;	
			this.target = target;
			this.triggerEvent=triggerEvent||null;	
		},

		dispatch: function(){
			var opts = {bubbles:true, cancelable:true, detail: this.transitionOptions, triggerEvent: this.triggerEvent};	
			//console.log("Target: ", this.target, " opts: ", opts);

			var evt = on.emit(this.target,"startTransition", opts);
			//console.log('evt: ', evt);
			if(evt){
				Deferred.when(transitDeferred, lang.hitch(this, function(transition){
					Deferred.when(transition.call(this, evt), lang.hitch(this, function(results){
						this.endTransition(results);
					})); 
				}));
			}
		},

		endTransition: function(results){
			on.emit(this.target, "endTransition" , {detail: results.transitionOptions});
		}
	});
});

},
'dojox/mobile/ViewController':function(){
define([
	"dojo/_base/kernel",
	"dojo/_base/array",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojo/dom",
	"dojo/dom-class",
	"dojo/dom-construct",
//	"dojo/hash", // optionally prereq'ed
	"dojo/on",
	"dojo/ready",
	"dijit/registry",	// registry.byId
	"./ProgressIndicator",
	"./TransitionEvent"
], function(dojo, array, connect, declare, lang, win, dom, domClass, domConstruct, on, ready, registry, ProgressIndicator, TransitionEvent){

	// module:
	//		dojox/mobile/ViewController
	// summary:
	//		A singleton class that controlls view transition.

	var dm = lang.getObject("dojox.mobile", true);

	var Controller = declare("dojox.mobile.ViewController", null, {
		// summary:
		//		A singleton class that controlls view transition.
		// description:
		//		This class listens to the "startTransition" events and performs
		//		view transitions. If the transition destination is an external
		//		view specified with the url parameter, retrieves the view
		//		content and parses it to create a new target view.

		constructor: function(){
			this.viewMap={};
			this.currentView=null;
			this.defaultView=null;
			ready(lang.hitch(this, function(){
				on(win.body(), "startTransition", lang.hitch(this, "onStartTransition"));
			}));
		},

		findCurrentView: function(moveTo,src){
			// summary:
			//		Searches for the currently showing view.
			if(moveTo){
				var w = registry.byId(moveTo);
				if(w && w.getShowingView){ return w.getShowingView(); }
			}
			if(dm.currentView){
				return dm.currentView; //TODO:1.8 may not return an expected result especially when views are nested
			}
			//TODO:1.8 probably never reaches here
			w = src;
			while(true){
				w = w.getParent();
				if(!w){ return null; }
				if(domClass.contains(w.domNode, "mblView")){ break; }
			}
			return w;
		},

		onStartTransition: function(evt){
			// summary:
			//		A handler that performs view transition.

			evt.preventDefault();
			if(!evt.detail || (evt.detail && !evt.detail.moveTo && !evt.detail.href && !evt.detail.url && !evt.detail.scene)){ return; }
			var w = this.findCurrentView(evt.detail.moveTo, (evt.target && evt.target.id)?registry.byId(evt.target.id):registry.byId(evt.target)); // the current view widget
			if(!w || (evt.detail && evt.detail.moveTo && w === registry.byId(evt.detail.moveTo))){ return; }
			if(evt.detail.href){
				var t = registry.byId(evt.target.id).hrefTarget;
				if(t){
					dm.openWindow(evt.detail.href, t);
				}else{
					w.performTransition(null, evt.detail.transitionDir, evt.detail.transition, evt.target, function(){location.href = evt.detail.href;});
				}
				return;
			} else if(evt.detail.scene){
				connect.publish("/dojox/mobile/app/pushScene", [evt.detail.scene]);
				return;
			}
			var moveTo = evt.detail.moveTo;
			if(evt.detail.url){
				var id;
				if(dm._viewMap && dm._viewMap[evt.detail.url]){
					// external view has already been loaded
					id = dm._viewMap[evt.detail.url];
				}else{
					// get the specified external view and append it to the <body>
					var text = this._text;
					if(!text){
						if(registry.byId(evt.target.id).sync){
							// We do not add explicit dependency on dojo/_base/xhr to this module
							// to be able to create a build that does not contain dojo/_base/xhr.
							// User applications that do sync loading here need to explicitly
							// require dojo/_base/xhr up front.
							dojo.xhrGet({url:evt.detail.url, sync:true, load:function(result){
								text = lang.trim(result);
							}});
						}else{
							var s = "dojo/_base/xhr"; // assign to a variable so as not to be picked up by the build tool
							require([s], lang.hitch(this, function(xhr){
								var prog = ProgressIndicator.getInstance();
								win.body().appendChild(prog.domNode);
								prog.start();
								var obj = xhr.get({
									url: evt.detail.url,
									handleAs: "text"
								});
								obj.addCallback(lang.hitch(this, function(response, ioArgs){
									prog.stop();
									if(response){
										this._text = response;
										new TransitionEvent(evt.target, {
												transition: evt.detail.transition,
											 	transitionDir: evt.detail.transitionDir,
											 	moveTo: moveTo,
											 	href: evt.detail.href,
											 	url: evt.detail.url,
											 	scene: evt.detail.scene},
											 		evt.detail)
											 			.dispatch();
									}
								}));
								obj.addErrback(function(error){
									prog.stop();
									console.log("Failed to load "+evt.detail.url+"\n"+(error.description||error));
								});
							}));
							return;
						}
					}
					this._text = null;
					id = this._parse(text, registry.byId(evt.target.id).urlTarget);
					if(!dm._viewMap){
						dm._viewMap = [];
					}
					dm._viewMap[evt.detail.url] = id;
				}
				moveTo = id;
				w = this.findCurrentView(moveTo,registry.byId(evt.target.id)) || w; // the current view widget
			}
			w.performTransition(moveTo, evt.detail.transitionDir, evt.detail.transition, null, null);
		},

		_parse: function(text, id){
			// summary:
			//		Parses the given view content.
			// description:
			//		If the content is html fragment, constructs dom tree with it
			//		and runs the parser. If the content is json data, passes it
			//		to _instantiate().
			var container, view, i, j, len;
			var currentView	 = this.findCurrentView();
			var target = registry.byId(id) && registry.byId(id).containerNode
						|| dom.byId(id)
						|| currentView && currentView.domNode.parentNode
						|| win.body();
			// if a fixed bottom bar exists, a new view should be placed before it.
			var refNode = null;
			for(j = target.childNodes.length - 1; j >= 0; j--){
				var c = target.childNodes[j];
				if(c.nodeType === 1){
					if(c.getAttribute("fixed") === "bottom"){
						refNode = c;
					}
					break;
				}
			}
			if(text.charAt(0) === "<"){ // html markup
				container = domConstruct.create("DIV", {innerHTML: text});
				for(i = 0; i < container.childNodes.length; i++){
					var n = container.childNodes[i];
					if(n.nodeType === 1){
						view = n; // expecting <div dojoType="dojox.mobile.View">
						break;
					}
				}
				if(!view){
					console.log("dojox.mobile.ViewController#_parse: invalid view content");
					return;
				}
				view.style.visibility = "hidden";
				target.insertBefore(container, refNode);
				var ws = dojo.parser.parse(container);
				array.forEach(ws, function(w){
					if(w && !w._started && w.startup){
						w.startup();
					}
				});

				// allows multiple root nodes in the fragment,
				// but transition will be performed to the 1st view.
				for(i = 0, len = container.childNodes.length; i < len; i++){
					target.insertBefore(container.firstChild, refNode); // reparent
				}
				target.removeChild(container);

				registry.byNode(view)._visible = true;
			}else if(text.charAt(0) === "{"){ // json
				container = domConstruct.create("DIV");
				target.insertBefore(container, refNode);
				this._ws = [];
				view = this._instantiate(eval('('+text+')'), container);
				for(i = 0; i < this._ws.length; i++){
					var w = this._ws[i];
					w.startup && !w._started && (!w.getParent || !w.getParent()) && w.startup();
				}
				this._ws = null;
			}
			view.style.display = "none";
			view.style.visibility = "visible";
			return dojo.hash ? "#" + view.id : view.id;
		},

		_instantiate: function(/*Object*/obj, /*DomNode*/node, /*Widget*/parent){
			// summary:
			//		Given the evaluated json data, does the same thing as what
			//		the parser does.
			var widget;
			for(var key in obj){
				if(key.charAt(0) == "@"){ continue; }
				var cls = lang.getObject(key);
				if(!cls){ continue; }
				var params = {};
				var proto = cls.prototype;
				var objs = lang.isArray(obj[key]) ? obj[key] : [obj[key]];
				for(var i = 0; i < objs.length; i++){
					for(var prop in objs[i]){
						if(prop.charAt(0) == "@"){
							var val = objs[i][prop];
							prop = prop.substring(1);
							if(typeof proto[prop] == "string"){
								params[prop] = val;
							}else if(typeof proto[prop] == "number"){
								params[prop] = val - 0;
							}else if(typeof proto[prop] == "boolean"){
							params[prop] = (val != "false");
							}else if(typeof proto[prop] == "object"){
								params[prop] = eval("(" + val + ")");
							}
						}
					}
					widget = new cls(params, node);
					if(node){ // to call View's startup()
						widget._visible = true;
						this._ws.push(widget);
					}
					if(parent && parent.addChild){
						parent.addChild(widget);
					}
					this._instantiate(objs[i], null, widget);
				}
			}
			return widget && widget.domNode;
		}
	});
	new Controller(); // singleton
	return Controller;
});


},
'dojox/mobile/ToolBarButton':function(){
define("dojox/mobile/ToolBarButton", [
	"dojo/_base/declare",
	"dojo/_base/window",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/dom-style",
	"./common",
	"./_ItemBase"
], function(declare, win, domClass, domConstruct, domStyle, common, ItemBase){
/*=====
	var ItemBase = dojox.mobile._ItemBase;
=====*/

	// module:
	//		dojox/mobile/ToolBarButton
	// summary:
	//		A button widget that is placed in the Heading widget.

	return declare("dojox.mobile.ToolBarButton", ItemBase, {
		// summary:
		//		A button widget that is placed in the Heading widget.
		// description:
		//		ToolBarButton is a button that is placed in the Heading
		//		widget. It is a subclass of dojox.mobile._ItemBase just like
		//		ListItem or IconItem. So, unlike Button, it has basically the
		//		same capability as ListItem or IconItem, such as icon support,
		//		transition, etc.

		// selected: Boolean
		//		If true, the button is in the selected status.
		selected: false,

		// btnClass: String
		//		Deprecated.
		btnClass: "",

		/* internal properties */	
		_defaultColor: "mblColorDefault",
		_selColor: "mblColorDefaultSel",

		buildRendering: function(){
			this.domNode = this.containerNode = this.srcNodeRef || win.doc.createElement("div");
			this.inheritParams();
			domClass.add(this.domNode, "mblToolBarButton mblArrowButtonText");
			var color;
			if(this.selected){
				color = this._selColor;
			}else if(this.domNode.className.indexOf("mblColor") == -1){
				color = this._defaultColor;
			}
			domClass.add(this.domNode, color);
	
			if(!this.label){
				this.label = this.domNode.innerHTML;
			}

			if(this.icon && this.icon != "none"){
				this.iconNode = domConstruct.create("div", {className:"mblToolBarButtonIcon"}, this.domNode);
				common.createIcon(this.icon, this.iconPos, null, this.alt, this.iconNode);
				if(this.iconPos){
					domClass.add(this.iconNode.firstChild, "mblToolBarButtonSpriteIcon");
				}
			}else{
				if(common.createDomButton(this.domNode)){
					domClass.add(this.domNode, "mblToolBarButtonDomButton");
				}else{
					domClass.add(this.domNode, "mblToolBarButtonText");
				}
			}
			this.connect(this.domNode, "onclick", "onClick");
		},
	
		select: function(){
			// summary:
			//		Makes this widget in the selected state.
			domClass.toggle(this.domNode, this._selColor, !arguments[0]);
			this.selected = !arguments[0];
		},
		
		deselect: function(){
			// summary:
			//		Makes this widget in the deselected state.
			this.select(true);
		},
	
		onClick: function(e){
			this.setTransitionPos(e);
			this.defaultClickAction();
		},
	
		_setBtnClassAttr: function(/*String*/btnClass){
			var node = this.domNode;
			if(node.className.match(/(mblDomButton\w+)/)){
				domClass.remove(node, RegExp.$1);
			}
			domClass.add(node, btnClass);
			if(common.createDomButton(this.domNode)){
				domClass.add(this.domNode, "mblToolBarButtonDomButton");
			}
		},

		_setLabelAttr: function(/*String*/text){
			this.label = text;
			this.domNode.innerHTML = this._cv ? this._cv(text) : text;
		}
	});
});

},
'dojox/mobile/_ItemBase':function(){
define("dojox/mobile/_ItemBase", [
	"dojo/_base/kernel",
	"dojo/_base/config",
	"dojo/_base/declare",
	"dijit/registry",	// registry.getEnclosingWidget
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase",
	"./TransitionEvent",
	"./View"
], function(kernel, config, declare, registry, Contained, Container, WidgetBase, TransitionEvent, View){

/*=====
	var Contained = dijit._Contained;
	var Container = dijit._Container;
	var WidgetBase = dijit._WidgetBase;
	var TransitionEvent = dojox.mobile.TransitionEvent;
	var View = dojox.mobile.View;
=====*/

	// module:
	//		dojox/mobile/_ItemBase
	// summary:
	//		A base class for item classes (e.g. ListItem, IconItem, etc.)

	return declare("dojox.mobile._ItemBase", [WidgetBase, Container, Contained],{
		// summary:
		//		A base class for item classes (e.g. ListItem, IconItem, etc.)
		// description:
		//		_ItemBase is a base class for widgets that have capability to
		//		make a view transition when clicked.

		// icon: String
		//		An icon image to display. The value can be either a path for an
		//		image file or a class name of a DOM button. If icon is not
		//		specified, the iconBase parameter of the parent widget is used.
		icon: "",

		// iconPos: String
		//		The position of an aggregated icon. IconPos is comma separated
		//		values like top,left,width,height (ex. "0,0,29,29"). If iconPos
		//		is not specified, the iconPos parameter of the parent widget is
		//		used.
		iconPos: "", // top,left,width,height (ex. "0,0,29,29")

		// alt: String
		//		An alt text for the icon image.
		alt: "",

		// href: String
		//		A URL of another web page to go to.
		href: "",

		// hrefTarget: String
		//		A target that specifies where to open a page specified by
		//		href. The value will be passed to the 2nd argument of
		//		window.open().
		hrefTarget: "",

		// moveTo: String
		//		The id of the transition destination view which resides in the
		//		current page.
		//
		//		If the value has a hash sign ('#') before the id (e.g. #view1)
		//		and the dojo.hash module is loaded by the user application, the
		//		view transition updates the hash in the browser URL so that the
		//		user can bookmark the destination view. In this case, the user
		//		can also use the browser's back/forward button to navigate
		//		through the views in the browser history.
		//
		//		If null, transitions to a blank view.
		//		If '#', returns immediately without transition.
		moveTo: "",

		// scene: String
		//		The name of a scene. Used from dojox.mobile.app.
		scene: "",

		// clickable: Boolean
		//		If true, this item becomes clickable even if a transition
		//		destination (moveTo, etc.) is not specified.
		clickable: false,

		// url: String
		//		A URL of an html fragment page or JSON data that represents a
		//		new view content. The view content is loaded with XHR and
		//		inserted in the current page. Then a view transition occurs to
		//		the newly created view. The view is cached so that subsequent
		//		requests would not load the content again.
		url: "",

		// urlTarget: String
		//		Node id under which a new view will be created according to the
		//		url parameter. If not specified, The new view will be created as
		//		a sibling of the current view.
		urlTarget: "",

		// transition: String
		//		A type of animated transition effect. You can choose from the
		//		standard transition types, "slide", "fade", "flip", or from the
		//		extended transition types, "cover", "coverv", "dissolve",
		//		"reveal", "revealv", "scaleIn", "scaleOut", "slidev",
		//		"swirl", "zoomIn", "zoomOut". If "none" is specified, transition
		//		occurs immediately without animation.
		transition: "",

		// transitionDir: Number
		//		The transition direction. If 1, transition forward. If -1,
		//		transition backward. For example, the slide transition slides
		//		the view from right to left when dir == 1, and from left to
		//		right when dir == -1.
		transitionDir: 1,

		// transitionOptions: Object
		//		A hash object that holds transition options.
		transitionOptions: null,

		// callback: Function|String
		//		A callback function that is called when the transition has been
		//		finished. A function reference, or name of a function in
		//		context.
		callback: null,

		// sync: Boolean
		//		If true, XHR for the view content specified with the url
		//		parameter is performed synchronously. If false, it is done
		//		asynchronously and the progress indicator is displayed while
		//		loading the content. This parameter is effective only when the
		//		url parameter is used.
		sync: true,

		// label: String
		//		A label of the item. If the label is not specified, innerHTML is
		//		used as a label.
		label: "",

		// toggle: Boolean
		//		If true, the item acts like a toggle button.
		toggle: false,

		// _duration: Number
		//		Duration of selection, milliseconds.
		_duration: 800,

	
		inheritParams: function(){
			var parent = this.getParent();
			if(parent){
				if(!this.transition){ this.transition = parent.transition; }
				if(this.icon && parent.iconBase &&
					parent.iconBase.charAt(parent.iconBase.length - 1) === '/'){
					this.icon = parent.iconBase + this.icon;
				}
				if(!this.icon){ this.icon = parent.iconBase; }
				if(!this.iconPos){ this.iconPos = parent.iconPos; }
			}
		},
	
		select: function(){
			// summary:
			//		Makes this widget in the selected state.
			// description:
			//		Subclass must implement.
		},
	
		deselect: function(){
			// summary:
			//		Makes this widget in the deselected state.
			// description:
			//		Subclass must implement.
		},
	
		defaultClickAction: function(e){
			if(this.toggle){
				if(this.selected){
					this.deselect();
				}else{
					this.select();
				}
			}else if(!this.selected){
				this.select();
				if(!this.selectOne){
					var _this = this;
					setTimeout(function(){
						_this.deselect();
					}, this._duration);
				}
				var transOpts;
				if(this.moveTo || this.href || this.url || this.scene){
					transOpts = {moveTo: this.moveTo, href: this.href, url: this.url, scene: this.scene, transition: this.transition, transitionDir: this.transitionDir};
				}else if(this.transitionOptions){
					transOpts = this.transitionOptions;
				}	
				if(transOpts){
					return new TransitionEvent(this.domNode,transOpts,e).dispatch();
				}
			}
		},
	
		getParent: function(){
			// summary:
			//		Gets the parent widget.
			// description:
			//		Almost equivalent to _Contained#getParent, but this method
			//		does not cause a script error even if this widget has no
			//		parent yet.
			var ref = this.srcNodeRef || this.domNode;
			return ref && ref.parentNode ? registry.getEnclosingWidget(ref.parentNode) : null;
		},

		setTransitionPos: function(e){
			// summary:
			//		Stores the clicked position for later use.
			// description:
			//		Some of the transition animations (e.g. ScaleIn) needs the
			//		clicked position.
			var w = this;
			while(true){
				w = w.getParent();
				if(!w || w instanceof View){ break; }
			}
			if(w){
				w.clickedPosX = e.clientX;
				w.clickedPosY = e.clientY;
			}
		},

		transitionTo: function(moveTo, href, url, scene){
			// summary:
			//		Performs a view transition.
			// description:
			//		Given a transition destination, this method performs a view
			//		transition. This method is typically called when this item
			//		is clicked.
			if(config.isDebug){
				var alreadyCalledHash = arguments.callee._ach || (arguments.callee._ach = {}),
					caller = (arguments.callee.caller || "unknown caller").toString();
				if(!alreadyCalledHash[caller]){
					kernel.deprecated(this.declaredClass + "::transitionTo() is deprecated." +
					caller, "", "2.0");
					alreadyCalledHash[caller] = true;
				}
			}
			new TransitionEvent(this.domNode, {moveTo: moveTo, href: href, url: url, scene: scene,
						transition: this.transition, transitionDir: this.transitionDir}).dispatch();
		}
	});
});

},
'dijit/hccss':function(){
define("dijit/hccss", [
	"require",			// require.toUrl
	"dojo/_base/config", // config.blankGif
	"dojo/dom-class", // domClass.add domConstruct.create domStyle.getComputedStyle
	"dojo/dom-construct", // domClass.add domConstruct.create domStyle.getComputedStyle
	"dojo/dom-style", // domClass.add domConstruct.create domStyle.getComputedStyle
	"dojo/ready", // ready
	"dojo/_base/sniff", // has("ie") has("mozilla")
	"dojo/_base/window" // win.body
], function(require, config, domClass, domConstruct, domStyle, ready, has, win){

	// module:
	//		dijit/hccss
	// summary:
	//		Test if computer is in high contrast mode, and sets dijit_a11y flag on <body> if it is.

	if(has("ie") || has("mozilla")){	// NOTE: checking in Safari messes things up
		// priority is 90 to run ahead of parser priority of 100
		ready(90, function(){
			// summary:
			//		Detects if we are in high-contrast mode or not

			// create div for testing if high contrast mode is on or images are turned off
			var div = domConstruct.create("div",{
				id: "a11yTestNode",
				style:{
					cssText:'border: 1px solid;'
						+ 'border-color:red green;'
						+ 'position: absolute;'
						+ 'height: 5px;'
						+ 'top: -999px;'
						+ 'background-image: url("' + (config.blankGif || require.toUrl("dojo/resources/blank.gif")) + '");'
				}
			}, win.body());

			// test it
			var cs = domStyle.getComputedStyle(div);
			if(cs){
				var bkImg = cs.backgroundImage;
				var needsA11y = (cs.borderTopColor == cs.borderRightColor) || (bkImg != null && (bkImg == "none" || bkImg == "url(invalid-url:)" ));
				if(needsA11y){
					domClass.add(win.body(), "dijit_a11y");
				}
				if(has("ie")){
					div.outerHTML = "";		// prevent mixed-content warning, see http://support.microsoft.com/kb/925014
				}else{
					win.body().removeChild(div);
				}
			}
		});
	}
});

},
'dijit/_Contained':function(){
define("dijit/_Contained", [
	"dojo/_base/declare", // declare
	"./registry"	// registry.getEnclosingWidget(), registry.byNode()
], function(declare, registry){

	// module:
	//		dijit/_Contained
	// summary:
	//		Mixin for widgets that are children of a container widget

	return declare("dijit._Contained", null, {
		// summary:
		//		Mixin for widgets that are children of a container widget
		//
		// example:
		// | 	// make a basic custom widget that knows about it's parents
		// |	declare("my.customClass",[dijit._Widget,dijit._Contained],{});

		_getSibling: function(/*String*/ which){
			// summary:
			//      Returns next or previous sibling
			// which:
			//      Either "next" or "previous"
			// tags:
			//      private
			var node = this.domNode;
			do{
				node = node[which+"Sibling"];
			}while(node && node.nodeType != 1);
			return node && registry.byNode(node);	// dijit._Widget
		},

		getPreviousSibling: function(){
			// summary:
			//		Returns null if this is the first child of the parent,
			//		otherwise returns the next element sibling to the "left".

			return this._getSibling("previous"); // dijit._Widget
		},

		getNextSibling: function(){
			// summary:
			//		Returns null if this is the last child of the parent,
			//		otherwise returns the next element sibling to the "right".

			return this._getSibling("next"); // dijit._Widget
		},

		getIndexInParent: function(){
			// summary:
			//		Returns the index of this widget within its container parent.
			//		It returns -1 if the parent does not exist, or if the parent
			//		is not a dijit._Container

			var p = this.getParent();
			if(!p || !p.getIndexOfChild){
				return -1; // int
			}
			return p.getIndexOfChild(this); // int
		}
	});
});

},
'dijit/form/_TextBoxMixin':function(){
define("dijit/form/_TextBoxMixin", [
	"dojo/_base/array", // array.forEach
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.byId
	"dojo/_base/event", // event.stop
	"dojo/keys", // keys.ALT keys.CAPS_LOCK keys.CTRL keys.META keys.SHIFT
	"dojo/_base/lang", // lang.mixin
	".."	// for exporting dijit._setSelectionRange, dijit.selectInputText
], function(array, declare, dom, event, keys, lang, dijit){

// module:
//		dijit/form/_TextBoxMixin
// summary:
//		A mixin for textbox form input widgets

var _TextBoxMixin = declare("dijit.form._TextBoxMixin", null, {
	// summary:
	//		A mixin for textbox form input widgets

	// trim: Boolean
	//		Removes leading and trailing whitespace if true.  Default is false.
	trim: false,

	// uppercase: Boolean
	//		Converts all characters to uppercase if true.  Default is false.
	uppercase: false,

	// lowercase: Boolean
	//		Converts all characters to lowercase if true.  Default is false.
	lowercase: false,

	// propercase: Boolean
	//		Converts the first character of each word to uppercase if true.
	propercase: false,

	// maxLength: String
	//		HTML INPUT tag maxLength declaration.
	maxLength: "",

	// selectOnClick: [const] Boolean
	//		If true, all text will be selected when focused with mouse
	selectOnClick: false,

	// placeHolder: String
	//		Defines a hint to help users fill out the input field (as defined in HTML 5).
	//		This should only contain plain text (no html markup).
	placeHolder: "",

	_getValueAttr: function(){
		// summary:
		//		Hook so get('value') works as we like.
		// description:
		//		For `dijit.form.TextBox` this basically returns the value of the <input>.
		//
		//		For `dijit.form.MappedTextBox` subclasses, which have both
		//		a "displayed value" and a separate "submit value",
		//		This treats the "displayed value" as the master value, computing the
		//		submit value from it via this.parse().
		return this.parse(this.get('displayedValue'), this.constraints);
	},

	_setValueAttr: function(value, /*Boolean?*/ priorityChange, /*String?*/ formattedValue){
		// summary:
		//		Hook so set('value', ...) works.
		//
		// description:
		//		Sets the value of the widget to "value" which can be of
		//		any type as determined by the widget.
		//
		// value:
		//		The visual element value is also set to a corresponding,
		//		but not necessarily the same, value.
		//
		// formattedValue:
		//		If specified, used to set the visual element value,
		//		otherwise a computed visual value is used.
		//
		// priorityChange:
		//		If true, an onChange event is fired immediately instead of
		//		waiting for the next blur event.

		var filteredValue;
		if(value !== undefined){
			// TODO: this is calling filter() on both the display value and the actual value.
			// I added a comment to the filter() definition about this, but it should be changed.
			filteredValue = this.filter(value);
			if(typeof formattedValue != "string"){
				if(filteredValue !== null && ((typeof filteredValue != "number") || !isNaN(filteredValue))){
					formattedValue = this.filter(this.format(filteredValue, this.constraints));
				}else{ formattedValue = ''; }
			}
		}
		if(formattedValue != null && formattedValue != undefined && ((typeof formattedValue) != "number" || !isNaN(formattedValue)) && this.textbox.value != formattedValue){
			this.textbox.value = formattedValue;
			this._set("displayedValue", this.get("displayedValue"));
		}

		if(this.textDir == "auto"){
			this.applyTextDir(this.focusNode, formattedValue);
		}

		this.inherited(arguments, [filteredValue, priorityChange]);
	},

	// displayedValue: String
	//		For subclasses like ComboBox where the displayed value
	//		(ex: Kentucky) and the serialized value (ex: KY) are different,
	//		this represents the displayed value.
	//
	//		Setting 'displayedValue' through set('displayedValue', ...)
	//		updates 'value', and vice-versa.  Otherwise 'value' is updated
	//		from 'displayedValue' periodically, like onBlur etc.
	//
	//		TODO: move declaration to MappedTextBox?
	//		Problem is that ComboBox references displayedValue,
	//		for benefit of FilteringSelect.
	displayedValue: "",

	_getDisplayedValueAttr: function(){
		// summary:
		//		Hook so get('displayedValue') works.
		// description:
		//		Returns the displayed value (what the user sees on the screen),
		// 		after filtering (ie, trimming spaces etc.).
		//
		//		For some subclasses of TextBox (like ComboBox), the displayed value
		//		is different from the serialized value that's actually
		//		sent to the server (see dijit.form.ValidationTextBox.serialize)

		// TODO: maybe we should update this.displayedValue on every keystroke so that we don't need
		// this method
		// TODO: this isn't really the displayed value when the user is typing
		return this.filter(this.textbox.value);
	},

	_setDisplayedValueAttr: function(/*String*/ value){
		// summary:
		//		Hook so set('displayedValue', ...) works.
		// description:
		//		Sets the value of the visual element to the string "value".
		//		The widget value is also set to a corresponding,
		//		but not necessarily the same, value.

		if(value === null || value === undefined){ value = '' }
		else if(typeof value != "string"){ value = String(value) }

		this.textbox.value = value;

		// sets the serialized value to something corresponding to specified displayedValue
		// (if possible), and also updates the textbox.value, for example converting "123"
		// to "123.00"
		this._setValueAttr(this.get('value'), undefined);

		this._set("displayedValue", this.get('displayedValue'));

		// textDir support
		if(this.textDir == "auto"){
			this.applyTextDir(this.focusNode, value);
		}
	},

	format: function(value /*=====, constraints =====*/){
		// summary:
		//		Replaceable function to convert a value to a properly formatted string.
		// value: String
		// constraints: Object
		// tags:
		//		protected extension
		return ((value == null || value == undefined) ? "" : (value.toString ? value.toString() : value));
	},

	parse: function(value /*=====, constraints =====*/){
		// summary:
		//		Replaceable function to convert a formatted string to a value
		// value: String
		// constraints: Object
		// tags:
		//		protected extension

		return value;	// String
	},

	_refreshState: function(){
		// summary:
		//		After the user types some characters, etc., this method is
		//		called to check the field for validity etc.  The base method
		//		in `dijit.form.TextBox` does nothing, but subclasses override.
		// tags:
		//		protected
	},

	/*=====
	onInput: function(event){
		// summary:
		//		Connect to this function to receive notifications of various user data-input events.
		//		Return false to cancel the event and prevent it from being processed.
		// event:
		//		keydown | keypress | cut | paste | input
		// tags:
		//		callback
	},
	=====*/
	onInput: function(){},

	__skipInputEvent: false,
	_onInput: function(){
		// summary:
		//		Called AFTER the input event has happened
		// set text direction according to textDir that was defined in creation
		if(this.textDir == "auto"){
			this.applyTextDir(this.focusNode, this.focusNode.value);
		}

		this._refreshState();

		// In case someone is watch()'ing for changes to displayedValue
		this._set("displayedValue", this.get("displayedValue"));
	},

	postCreate: function(){
		// setting the value here is needed since value="" in the template causes "undefined"
		// and setting in the DOM (instead of the JS object) helps with form reset actions
		this.textbox.setAttribute("value", this.textbox.value); // DOM and JS values should be the same

		this.inherited(arguments);

		// normalize input events to reduce spurious event processing
		//	onkeydown: do not forward modifier keys
		//	           set charOrCode to numeric keycode
		//	onkeypress: do not forward numeric charOrCode keys (already sent through onkeydown)
		//	onpaste & oncut: set charOrCode to 229 (IME)
		//	oninput: if primary event not already processed, set charOrCode to 229 (IME), else do not forward
		var handleEvent = function(e){
			var charCode = e.charOrCode || e.keyCode || 229;
			if(e.type == "keydown"){
				switch(charCode){ // ignore "state" keys
					case keys.SHIFT:
					case keys.ALT:
					case keys.CTRL:
					case keys.META:
					case keys.CAPS_LOCK:
						return;
					default:
						if(charCode >= 65 && charCode <= 90){ return; } // keydown for A-Z can be processed with keypress
				}
			}
			if(e.type == "keypress" && typeof charCode != "string"){ return; }
			if(e.type == "input"){
				if(this.__skipInputEvent){ // duplicate event
					this.__skipInputEvent = false;
					return;
				}
			}else{
				this.__skipInputEvent = true;
			}
			// create fake event to set charOrCode and to know if preventDefault() was called
			var faux = lang.mixin({}, e, {
				charOrCode: charCode,
				wasConsumed: false,
				preventDefault: function(){
					faux.wasConsumed = true;
					e.preventDefault();
				},
				stopPropagation: function(){ e.stopPropagation(); }
			});
			// give web page author a chance to consume the event
			if(this.onInput(faux) === false){
				event.stop(faux); // return false means stop
			}
			if(faux.wasConsumed){ return; } // if preventDefault was called
			setTimeout(lang.hitch(this, "_onInput", faux), 0); // widget notification after key has posted
		};
		array.forEach([ "onkeydown", "onkeypress", "onpaste", "oncut", "oninput" ], function(event){
			this.connect(this.textbox, event, handleEvent);
		}, this);
	},

	_blankValue: '', // if the textbox is blank, what value should be reported
	filter: function(val){
		// summary:
		//		Auto-corrections (such as trimming) that are applied to textbox
		//		value on blur or form submit.
		// description:
		//		For MappedTextBox subclasses, this is called twice
		// 			- once with the display value
		//			- once the value as set/returned by set('value', ...)
		//		and get('value'), ex: a Number for NumberTextBox.
		//
		//		In the latter case it does corrections like converting null to NaN.  In
		//		the former case the NumberTextBox.filter() method calls this.inherited()
		//		to execute standard trimming code in TextBox.filter().
		//
		//		TODO: break this into two methods in 2.0
		//
		// tags:
		//		protected extension
		if(val === null){ return this._blankValue; }
		if(typeof val != "string"){ return val; }
		if(this.trim){
			val = lang.trim(val);
		}
		if(this.uppercase){
			val = val.toUpperCase();
		}
		if(this.lowercase){
			val = val.toLowerCase();
		}
		if(this.propercase){
			val = val.replace(/[^\s]+/g, function(word){
				return word.substring(0,1).toUpperCase() + word.substring(1);
			});
		}
		return val;
	},

	_setBlurValue: function(){
		this._setValueAttr(this.get('value'), true);
	},

	_onBlur: function(e){
		if(this.disabled){ return; }
		this._setBlurValue();
		this.inherited(arguments);

		if(this._selectOnClickHandle){
			this.disconnect(this._selectOnClickHandle);
		}
	},

	_isTextSelected: function(){
		return this.textbox.selectionStart == this.textbox.selectionEnd;
	},

	_onFocus: function(/*String*/ by){
		if(this.disabled || this.readOnly){ return; }

		// Select all text on focus via click if nothing already selected.
		// Since mouse-up will clear the selection need to defer selection until after mouse-up.
		// Don't do anything on focus by tabbing into the widget since there's no associated mouse-up event.
		if(this.selectOnClick && by == "mouse"){
			this._selectOnClickHandle = this.connect(this.domNode, "onmouseup", function(){
				// Only select all text on first click; otherwise users would have no way to clear
				// the selection.
				this.disconnect(this._selectOnClickHandle);

				// Check if the user selected some text manually (mouse-down, mouse-move, mouse-up)
				// and if not, then select all the text
				if(this._isTextSelected()){
					_TextBoxMixin.selectInputText(this.textbox);
				}
			});
		}
		// call this.inherited() before refreshState(), since this.inherited() will possibly scroll the viewport
		// (to scroll the TextBox into view), which will affect how _refreshState() positions the tooltip
		this.inherited(arguments);

		this._refreshState();
	},

	reset: function(){
		// Overrides dijit._FormWidget.reset().
		// Additionally resets the displayed textbox value to ''
		this.textbox.value = '';
		this.inherited(arguments);
	},
	_setTextDirAttr: function(/*String*/ textDir){
		// summary:
		//		Setter for textDir.
		// description:
		//		Users shouldn't call this function; they should be calling
		//		set('textDir', value)
		// tags:
		//		private

		// only if new textDir is different from the old one
		// and on widgets creation.
		if(!this._created
			|| this.textDir != textDir){
				this._set("textDir", textDir);
				// so the change of the textDir will take place immediately.
				this.applyTextDir(this.focusNode, this.focusNode.value);
		}
	}
});


_TextBoxMixin._setSelectionRange = dijit._setSelectionRange = function(/*DomNode*/ element, /*Number?*/ start, /*Number?*/ stop){
	if(element.setSelectionRange){
		element.setSelectionRange(start, stop);
	}
};

_TextBoxMixin.selectInputText = dijit.selectInputText = function(/*DomNode*/ element, /*Number?*/ start, /*Number?*/ stop){
	// summary:
	//		Select text in the input element argument, from start (default 0), to stop (default end).

	// TODO: use functions in _editor/selection.js?
	element = dom.byId(element);
	if(isNaN(start)){ start = 0; }
	if(isNaN(stop)){ stop = element.value ? element.value.length : 0; }
	try{
		element.focus();
		_TextBoxMixin._setSelectionRange(element, start, stop);
	}catch(e){ /* squelch random errors (esp. on IE) from unexpected focus changes or DOM nodes being hidden */ }
};

return _TextBoxMixin;
});

},
'dojox/mobile/parser':function(){
define([
	"dojo/_base/kernel",
	"dojo/_base/config",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojo/ready"
], function(dojo, config, lang, win, ready){

	// module:
	//		dojox/mobile/parser
	// summary:
	//		A lightweight parser.

	var dm = lang.getObject("dojox.mobile", true);

	var parser = new function(){
		// summary:
		//		A lightweight parser.
		// description:
		//		dojox.mobile.parser is an extremely small subset of
		//		dojo.parser. It has no extended features over dojo.parser, so
		//		there is no reason you have to use dojox.mobile.parser instead
		//		of dojo.parser. However, if dojox.mobile.parser's capability is
		//		enough for your application, use of it could reduce the total
		//		code size.

		this.instantiate = function(/* Array */nodes, /* Object? */mixin, /* Object? */args){
			// summary:
			//		Function for instantiating a list of widget nodes.
			// nodes:
			//		The list of DOMNodes to walk and instantiate widgets on.
			mixin = mixin || {};
			args = args || {};
			var i, ws = [];
			if(nodes){
				for(i = 0; i < nodes.length; i++){
					var n = nodes[i];
					var cls = lang.getObject(n.getAttribute("dojoType") || n.getAttribute("data-dojo-type"));
					var proto = cls.prototype;
					var params = {}, prop, v, t;
					lang.mixin(params, eval('({'+(n.getAttribute("data-dojo-props")||"")+'})'));
					lang.mixin(params, args.defaults);
					lang.mixin(params, mixin);
					for(prop in proto){
						v = n.getAttributeNode(prop);
						v = v && v.nodeValue;
						t = typeof proto[prop];
						if(!v && (t !== "boolean" || v !== "")){ continue; }
						if(t === "string"){
							params[prop] = v;
						}else if(t === "number"){
							params[prop] = v - 0;
						}else if(t === "boolean"){
							params[prop] = (v !== "false");
						}else if(t === "object"){
							params[prop] = eval("(" + v + ")");
						}
					}
					params["class"] = n.className;
					params.style = n.style && n.style.cssText;
					v = n.getAttribute("data-dojo-attach-point");
					if(v){ params.dojoAttachPoint = v; }
					v = n.getAttribute("data-dojo-attach-event");
					if(v){ params.dojoAttachEvent = v; }
					var instance = new cls(params, n);
					ws.push(instance);
					var jsId = n.getAttribute("jsId") || n.getAttribute("data-dojo-id");
					if(jsId){
						lang.setObject(jsId, instance);
					}
				}
				for(i = 0; i < ws.length; i++){
					var w = ws[i];
					!args.noStart && w.startup && !w._started && w.startup();
				}
			}
			return ws;
		};

		this.parse = function(rootNode, args){
			// summary:
			//		Function to handle parsing for widgets in the current document.
			//		It is not as powerful as the full parser, but it will handle basic
			//		use cases fine.
			// rootNode:
			//		The root node in the document to parse from
			if(!rootNode){
				rootNode = win.body();
			}else if(!args && rootNode.rootNode){
				// Case where 'rootNode' is really a params object.
				args = rootNode;
				rootNode = rootNode.rootNode;
			}

			var nodes = rootNode.getElementsByTagName("*");
			var i, list = [];
			for(i = 0; i < nodes.length; i++){
				var n = nodes[i];
				if(n.getAttribute("dojoType") || n.getAttribute("data-dojo-type")){
					list.push(n);
				}
			}
			var mixin = args && args.template ? {template: true} : null;
			return this.instantiate(list, mixin, args);
		};
	}();
	if(config.parseOnLoad){
		ready(100, parser, "parse");
	}
	dm.parser = parser; // for backward compatibility
	dojo.parser = parser; // in case user application calls dojo.parser
	return parser;
});

},
'dijit/_Container':function(){
define("dijit/_Container", [
	"dojo/_base/array", // array.forEach array.indexOf
	"dojo/_base/declare", // declare
	"dojo/dom-construct", // domConstruct.place
	"./registry"	// registry.byNode()
], function(array, declare, domConstruct, registry){

	// module:
	//		dijit/_Container
	// summary:
	//		Mixin for widgets that contain a set of widget children.

	return declare("dijit._Container", null, {
		// summary:
		//		Mixin for widgets that contain a set of widget children.
		// description:
		//		Use this mixin for widgets that needs to know about and
		//		keep track of their widget children. Suitable for widgets like BorderContainer
		//		and TabContainer which contain (only) a set of child widgets.
		//
		//		It's not suitable for widgets like ContentPane
		//		which contains mixed HTML (plain DOM nodes in addition to widgets),
		//		and where contained widgets are not necessarily directly below
		//		this.containerNode.   In that case calls like addChild(node, position)
		//		wouldn't make sense.

		buildRendering: function(){
			this.inherited(arguments);
			if(!this.containerNode){
				// all widgets with descendants must set containerNode
	 			this.containerNode = this.domNode;
			}
		},

		addChild: function(/*dijit._Widget*/ widget, /*int?*/ insertIndex){
			// summary:
			//		Makes the given widget a child of this widget.
			// description:
			//		Inserts specified child widget's dom node as a child of this widget's
			//		container node, and possibly does other processing (such as layout).

			var refNode = this.containerNode;
			if(insertIndex && typeof insertIndex == "number"){
				var children = this.getChildren();
				if(children && children.length >= insertIndex){
					refNode = children[insertIndex-1].domNode;
					insertIndex = "after";
				}
			}
			domConstruct.place(widget.domNode, refNode, insertIndex);

			// If I've been started but the child widget hasn't been started,
			// start it now.  Make sure to do this after widget has been
			// inserted into the DOM tree, so it can see that it's being controlled by me,
			// so it doesn't try to size itself.
			if(this._started && !widget._started){
				widget.startup();
			}
		},

		removeChild: function(/*Widget|int*/ widget){
			// summary:
			//		Removes the passed widget instance from this widget but does
			//		not destroy it.  You can also pass in an integer indicating
			//		the index within the container to remove

			if(typeof widget == "number"){
				widget = this.getChildren()[widget];
			}

			if(widget){
				var node = widget.domNode;
				if(node && node.parentNode){
					node.parentNode.removeChild(node); // detach but don't destroy
				}
			}
		},

		hasChildren: function(){
			// summary:
			//		Returns true if widget has children, i.e. if this.containerNode contains something.
			return this.getChildren().length > 0;	// Boolean
		},

		_getSiblingOfChild: function(/*dijit._Widget*/ child, /*int*/ dir){
			// summary:
			//		Get the next or previous widget sibling of child
			// dir:
			//		if 1, get the next sibling
			//		if -1, get the previous sibling
			// tags:
			//      private
			var node = child.domNode,
				which = (dir>0 ? "nextSibling" : "previousSibling");
			do{
				node = node[which];
			}while(node && (node.nodeType != 1 || !registry.byNode(node)));
			return node && registry.byNode(node);	// dijit._Widget
		},

		getIndexOfChild: function(/*dijit._Widget*/ child){
			// summary:
			//		Gets the index of the child in this container or -1 if not found
			return array.indexOf(this.getChildren(), child);	// int
		}
	});
});

},
'dojox/mobile/app/SceneController':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/_base"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.SceneController");
dojo.experimental("dojox.mobile.app.SceneController");
dojo.require("dojox.mobile._base");

(function(){

	var app = dojox.mobile.app;

	var templates = {};

	dojo.declare("dojox.mobile.app.SceneController", dojox.mobile.View, {

		stageController: null,

		keepScrollPos: false,

		init: function(sceneName, params){
			// summary:
			//    Initializes the scene by loading the HTML template and code, if it has
			//    not already been loaded

			this.sceneName = sceneName;
			this.params = params;
			var templateUrl = app.resolveTemplate(sceneName);

			this._deferredInit = new dojo.Deferred();

			if(templates[sceneName]){
				// If the template has been cached, do not load it again.
				this._setContents(templates[sceneName]);
			}else{
				// Otherwise load the template
				dojo.xhrGet({
					url: templateUrl,
					handleAs: "text"
				}).addCallback(dojo.hitch(this, this._setContents));
			}

			return this._deferredInit;
		},

		_setContents: function(templateHtml){
			// summary:
			//    Sets the content of the View, and invokes either the loading or
			//    initialization of the scene assistant.
			templates[this.sceneName] = templateHtml;

			this.domNode.innerHTML = "<div>" + templateHtml + "</div>";

			var sceneAssistantName = "";

			var nameParts = this.sceneName.split("-");

			for(var i = 0; i < nameParts.length; i++){
				sceneAssistantName += nameParts[i].substring(0, 1).toUpperCase()
							+ nameParts[i].substring(1);
			}
			sceneAssistantName += "Assistant";
			this.sceneAssistantName = sceneAssistantName;

			var _this = this;

			dojox.mobile.app.loadResourcesForScene(this.sceneName, function(){

				console.log("All resources for ",_this.sceneName," loaded");

				var assistant;
				if(typeof(dojo.global[sceneAssistantName]) != "undefined"){
					_this._initAssistant();
				}else{
					var assistantUrl = app.resolveAssistant(_this.sceneName);

					dojo.xhrGet({
						url: assistantUrl,
						handleAs: "text"
					}).addCallback(function(text){
						try{
							dojo.eval(text);
						}catch(e){
							console.log("Error initializing code for scene " + _this.sceneName
							+ '. Please check for syntax errors');
							throw e;
						}
						_this._initAssistant();
					});
				}
			});

		},

		_initAssistant: function(){
			// summary:
			//    Initializes the scene assistant. At this point, the View is
			//    populated with the HTML template, and the scene assistant type
			//    is declared.

			console.log("Instantiating the scene assistant " + this.sceneAssistantName);

			var cls = dojo.getObject(this.sceneAssistantName);

			if(!cls){
				throw Error("Unable to resolve scene assistant "
							+ this.sceneAssistantName);
			}

			this.assistant = new cls(this.params);

			this.assistant.controller = this;
			this.assistant.domNode = this.domNode.firstChild;

			this.assistant.setup();

			this._deferredInit.callback();
		},

		query: function(selector, node){
			// summary:
			//    Queries for DOM nodes within either the node passed in as an argument
			//    or within this view.

			return dojo.query(selector, node || this.domNode)
		},

		parse: function(node){
			var widgets = this._widgets =
				dojox.mobile.parser.parse(node || this.domNode, {
					controller: this
				});

			// Tell all widgets what their controller is.
			for(var i = 0; i < widgets.length; i++){
				widgets[i].set("controller", this);
			}
		},

		getWindowSize: function(){
			// TODO, this needs cross browser testing

			return {
				w: dojo.global.innerWidth,
				h: dojo.global.innerHeight
			}
		},

		showAlertDialog: function(props){

			var size = dojo.marginBox(this.assistant.domNode);
			var dialog = new dojox.mobile.app.AlertDialog(
					dojo.mixin(props, {controller: this}));
			this.assistant.domNode.appendChild(dialog.domNode);

			console.log("Appended " , dialog.domNode, " to ", this.assistant.domNode);
			dialog.show();
		},

		popupSubMenu: function(info){
			var widget = new dojox.mobile.app.ListSelector({
				controller: this,
				destroyOnHide: true,
				onChoose: info.onChoose
			});

			this.assistant.domNode.appendChild(widget.domNode);

			widget.set("data", info.choices);
			widget.show(info.fromNode);
		}
	});

})();
});

},
'dojox/mobile/app/_base':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dijit/_base,dijit/_WidgetBase,dojox/mobile,dojox/mobile/parser,dojox/mobile/Button,dojox/mobile/app/_event,dojox/mobile/app/_Widget,dojox/mobile/app/StageController,dojox/mobile/app/SceneController,dojox/mobile/app/SceneAssistant,dojox/mobile/app/AlertDialog,dojox/mobile/app/List,dojox/mobile/app/ListSelector,dojox/mobile/app/TextBox,dojox/mobile/app/ImageView,dojox/mobile/app/ImageThumbView"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app._base");
dojo.experimental("dojox.mobile.app._base");

dojo.require("dijit._base");
dojo.require("dijit._WidgetBase");
dojo.require("dojox.mobile");
dojo.require("dojox.mobile.parser");
dojo.require("dojox.mobile.Button");

dojo.require("dojox.mobile.app._event");
dojo.require("dojox.mobile.app._Widget");
dojo.require("dojox.mobile.app.StageController");
dojo.require("dojox.mobile.app.SceneController");
dojo.require("dojox.mobile.app.SceneAssistant");
dojo.require("dojox.mobile.app.AlertDialog");
dojo.require("dojox.mobile.app.List");
dojo.require("dojox.mobile.app.ListSelector");
dojo.require("dojox.mobile.app.TextBox");
dojo.require("dojox.mobile.app.ImageView");
dojo.require("dojox.mobile.app.ImageThumbView");

(function(){

	var stageController;
	var appInfo;

	var jsDependencies = [
		"dojox.mobile",
		"dojox.mobile.parser"
	];

	var loadedResources = {};
	var loadingDependencies;

	var rootNode;

	var sceneResources = [];

	// Load the required resources asynchronously, since not all mobile OSes
	// support dojo.require and sync XHR
	function loadResources(resources, callback){
		// summary:
		//		Loads one or more JavaScript files asynchronously. When complete,
		//		the first scene is pushed onto the stack.
		// resources:
		//		An array of module names, e.g. 'dojox.mobile.AlertDialog'

		var resource;
		var url;

		do {
			resource = resources.pop();
			if (resource.source) {
				url = resource.source;
			}else if (resource.module) {
				url= dojo.moduleUrl(resource.module)+".js";
			}else {
				console.log("Error: invalid JavaScript resource " + dojo.toJson(resource));
				return;
			}
		}while (resources.length > 0 && loadedResources[url]);

		if(resources.length < 1 && loadedResources[url]){
			// All resources have already been loaded
			callback();
			return;
		}

		dojo.xhrGet({
			url: url,
			sync: false
		}).addCallbacks(function(text){
			dojo["eval"](text);
			loadedResources[url] = true;
			if(resources.length > 0){
				loadResources(resources, callback);
			}else{
				callback();
			}
		},
		function(){
			console.log("Failed to load resource " + url);
		});
	}

	var pushFirstScene = function(){
		// summary:
		//		Pushes the first scene onto the stack.

		stageController = new dojox.mobile.app.StageController(rootNode);
		var defaultInfo = {
			id: "com.test.app",
			version: "1.0.0",
			initialScene: "main"
		};

		// If the application info has been defined, as it should be,
		// use it.
		if(dojo.global["appInfo"]){
			dojo.mixin(defaultInfo, dojo.global["appInfo"]);
		}
		appInfo = dojox.mobile.app.info = defaultInfo;

		// Set the document title from the app info title if it exists
		if(appInfo.title){
			var titleNode = dojo.query("head title")[0] ||
							dojo.create("title", {},dojo.query("head")[0]);
			document.title = appInfo.title;
		}

		stageController.pushScene(appInfo.initialScene);
	};

	var initBackButton = function(){
		var hasNativeBack = false;
		if(dojo.global.BackButton){
			// Android phonegap support
			BackButton.override();
			dojo.connect(document, 'backKeyDown', function(e) {
				dojo.publish("/dojox/mobile/app/goback");
			});
			hasNativeBack = true;
		}else if(dojo.global.Mojo){
			// TODO: add webOS support
		}
		if(hasNativeBack){
			dojo.addClass(dojo.body(), "mblNativeBack");
		}
	};

	dojo.mixin(dojox.mobile.app, {
		init: function(node){
			// summary:
			//    Initializes the mobile app. Creates the

			rootNode = node || dojo.body();
			dojox.mobile.app.STAGE_CONTROLLER_ACTIVE = true;

			dojo.subscribe("/dojox/mobile/app/goback", function(){
				stageController.popScene();
			});

			dojo.subscribe("/dojox/mobile/app/alert", function(params){
				dojox.mobile.app.getActiveSceneController().showAlertDialog(params);
			});

			dojo.subscribe("/dojox/mobile/app/pushScene", function(sceneName, params){
				stageController.pushScene(sceneName, params || {});
			});

			// Get the list of files to load per scene/view
			dojo.xhrGet({
				url: "view-resources.json",
				load: function(data){
					var resources = [];

					if(data){
						// Should be an array
						sceneResources = data = dojo.fromJson(data);

						// Get the list of files to load that have no scene
						// specified, and therefore should be loaded on
						// startup
						for(var i = 0; i < data.length; i++){
							if(!data[i].scene){
								resources.push(data[i]);
							}
						}
					}
					if(resources.length > 0){
						loadResources(resources, pushFirstScene);
					}else{
						pushFirstScene();
					}
				},
				error: pushFirstScene
			});

			initBackButton();
		},

		getActiveSceneController: function(){
			// summary:
			//		Gets the controller for the active scene.

			return stageController.getActiveSceneController();
		},

		getStageController: function(){
			// summary:
			//		Gets the stage controller.
			return stageController;
		},

		loadResources: function(resources, callback){
			loadResources(resources, callback);
		},

		loadResourcesForScene: function(sceneName, callback){
			var resources = [];

			// Get the list of files to load that have no scene
			// specified, and therefore should be loaded on
			// startup
			for(var i = 0; i < sceneResources.length; i++){
				if(sceneResources[i].scene == sceneName){
					resources.push(sceneResources[i]);
				}
			}

			if(resources.length > 0){
				loadResources(resources, callback);
			}else{
				callback();
			}
		},

		resolveTemplate: function(sceneName){
			// summary:
			//		Given the name of a scene, returns the path to it's template
			//		file.  For example, for a scene named 'main', the file
			//		returned is 'app/views/main/main-scene.html'
			//		This function can be overridden if it is desired to have
			//		a different name to file mapping.
			return "app/views/" + sceneName + "/" + sceneName + "-scene.html";
		},

		resolveAssistant: function(sceneName){
			// summary:
			//		Given the name of a scene, returns the path to it's assistant
			//		file.  For example, for a scene named 'main', the file
			//		returned is 'app/assistants/main-assistant.js'
			//		This function can be overridden if it is desired to have
			//		a different name to file mapping.
			return "app/assistants/" + sceneName + "-assistant.js";
		}
	});
})();
});

},
'dijit/_base/scroll':function(){
define("dijit/_base/scroll", [
	"dojo/window", // windowUtils.scrollIntoView
	".."	// export symbol to dijit
], function(windowUtils, dijit){
	// module:
	//		dijit/_base/scroll
	// summary:
	//		Back compatibility module, new code should use windowUtils directly instead of using this module.

	dijit.scrollIntoView = function(/*DomNode*/ node, /*Object?*/ pos){
		// summary:
		//		Scroll the passed node into view, if it is not already.
		//		Deprecated, use `windowUtils.scrollIntoView` instead.

		windowUtils.scrollIntoView(node, pos);
	};
});

},
'dojo/fx':function(){
define([
	"./_base/lang",
	"./Evented",
	"./_base/kernel",
	"./_base/array",
	"./_base/connect",
	"./_base/fx",
	"./dom",
	"./dom-style",
	"./dom-geometry",
	"./ready",
	"require" // for context sensitive loading of Toggler
], function(lang, Evented, dojo, arrayUtil, connect, baseFx, dom, domStyle, geom, ready, require) {

	// module:
	//		dojo/fx
	// summary:
	//		TODOC


	/*=====
	dojo.fx = {
		// summary: Effects library on top of Base animations
	};
	var coreFx = dojo.fx;
	=====*/
	
// For back-compat, remove in 2.0.
if(!dojo.isAsync){
	ready(0, function(){
		var requires = ["./fx/Toggler"];
		require(requires);	// use indirection so modules not rolled into a build
	});
}

	var coreFx = dojo.fx = {};

	var _baseObj = {
			_fire: function(evt, args){
				if(this[evt]){
					this[evt].apply(this, args||[]);
				}
				return this;
			}
		};

	var _chain = function(animations){
		this._index = -1;
		this._animations = animations||[];
		this._current = this._onAnimateCtx = this._onEndCtx = null;

		this.duration = 0;
		arrayUtil.forEach(this._animations, function(a){
			this.duration += a.duration;
			if(a.delay){ this.duration += a.delay; }
		}, this);
	};
	_chain.prototype = new Evented();
	lang.extend(_chain, {
		_onAnimate: function(){
			this._fire("onAnimate", arguments);
		},
		_onEnd: function(){
			connect.disconnect(this._onAnimateCtx);
			connect.disconnect(this._onEndCtx);
			this._onAnimateCtx = this._onEndCtx = null;
			if(this._index + 1 == this._animations.length){
				this._fire("onEnd");
			}else{
				// switch animations
				this._current = this._animations[++this._index];
				this._onAnimateCtx = connect.connect(this._current, "onAnimate", this, "_onAnimate");
				this._onEndCtx = connect.connect(this._current, "onEnd", this, "_onEnd");
				this._current.play(0, true);
			}
		},
		play: function(/*int?*/ delay, /*Boolean?*/ gotoStart){
			if(!this._current){ this._current = this._animations[this._index = 0]; }
			if(!gotoStart && this._current.status() == "playing"){ return this; }
			var beforeBegin = connect.connect(this._current, "beforeBegin", this, function(){
					this._fire("beforeBegin");
				}),
				onBegin = connect.connect(this._current, "onBegin", this, function(arg){
					this._fire("onBegin", arguments);
				}),
				onPlay = connect.connect(this._current, "onPlay", this, function(arg){
					this._fire("onPlay", arguments);
					connect.disconnect(beforeBegin);
					connect.disconnect(onBegin);
					connect.disconnect(onPlay);
				});
			if(this._onAnimateCtx){
				connect.disconnect(this._onAnimateCtx);
			}
			this._onAnimateCtx = connect.connect(this._current, "onAnimate", this, "_onAnimate");
			if(this._onEndCtx){
				connect.disconnect(this._onEndCtx);
			}
			this._onEndCtx = connect.connect(this._current, "onEnd", this, "_onEnd");
			this._current.play.apply(this._current, arguments);
			return this;
		},
		pause: function(){
			if(this._current){
				var e = connect.connect(this._current, "onPause", this, function(arg){
						this._fire("onPause", arguments);
						connect.disconnect(e);
					});
				this._current.pause();
			}
			return this;
		},
		gotoPercent: function(/*Decimal*/percent, /*Boolean?*/ andPlay){
			this.pause();
			var offset = this.duration * percent;
			this._current = null;
			arrayUtil.some(this._animations, function(a){
				if(a.duration <= offset){
					this._current = a;
					return true;
				}
				offset -= a.duration;
				return false;
			});
			if(this._current){
				this._current.gotoPercent(offset / this._current.duration, andPlay);
			}
			return this;
		},
		stop: function(/*boolean?*/ gotoEnd){
			if(this._current){
				if(gotoEnd){
					for(; this._index + 1 < this._animations.length; ++this._index){
						this._animations[this._index].stop(true);
					}
					this._current = this._animations[this._index];
				}
				var e = connect.connect(this._current, "onStop", this, function(arg){
						this._fire("onStop", arguments);
						connect.disconnect(e);
					});
				this._current.stop();
			}
			return this;
		},
		status: function(){
			return this._current ? this._current.status() : "stopped";
		},
		destroy: function(){
			if(this._onAnimateCtx){ connect.disconnect(this._onAnimateCtx); }
			if(this._onEndCtx){ connect.disconnect(this._onEndCtx); }
		}
	});
	lang.extend(_chain, _baseObj);

	coreFx.chain = /*===== dojo.fx.chain = =====*/ function(/*dojo.Animation[]*/ animations){
		// summary:
		//		Chain a list of `dojo.Animation`s to run in sequence
		//
		// description:
		//		Return a `dojo.Animation` which will play all passed
		//		`dojo.Animation` instances in sequence, firing its own
		//		synthesized events simulating a single animation. (eg:
		//		onEnd of this animation means the end of the chain,
		//		not the individual animations within)
		//
		// example:
		//	Once `node` is faded out, fade in `otherNode`
		//	|	dojo.fx.chain([
		//	|		dojo.fadeIn({ node:node }),
		//	|		dojo.fadeOut({ node:otherNode })
		//	|	]).play();
		//
		return new _chain(animations); // dojo.Animation
	};

	var _combine = function(animations){
		this._animations = animations||[];
		this._connects = [];
		this._finished = 0;

		this.duration = 0;
		arrayUtil.forEach(animations, function(a){
			var duration = a.duration;
			if(a.delay){ duration += a.delay; }
			if(this.duration < duration){ this.duration = duration; }
			this._connects.push(connect.connect(a, "onEnd", this, "_onEnd"));
		}, this);

		this._pseudoAnimation = new baseFx.Animation({curve: [0, 1], duration: this.duration});
		var self = this;
		arrayUtil.forEach(["beforeBegin", "onBegin", "onPlay", "onAnimate", "onPause", "onStop", "onEnd"],
			function(evt){
				self._connects.push(connect.connect(self._pseudoAnimation, evt,
					function(){ self._fire(evt, arguments); }
				));
			}
		);
	};
	lang.extend(_combine, {
		_doAction: function(action, args){
			arrayUtil.forEach(this._animations, function(a){
				a[action].apply(a, args);
			});
			return this;
		},
		_onEnd: function(){
			if(++this._finished > this._animations.length){
				this._fire("onEnd");
			}
		},
		_call: function(action, args){
			var t = this._pseudoAnimation;
			t[action].apply(t, args);
		},
		play: function(/*int?*/ delay, /*Boolean?*/ gotoStart){
			this._finished = 0;
			this._doAction("play", arguments);
			this._call("play", arguments);
			return this;
		},
		pause: function(){
			this._doAction("pause", arguments);
			this._call("pause", arguments);
			return this;
		},
		gotoPercent: function(/*Decimal*/percent, /*Boolean?*/ andPlay){
			var ms = this.duration * percent;
			arrayUtil.forEach(this._animations, function(a){
				a.gotoPercent(a.duration < ms ? 1 : (ms / a.duration), andPlay);
			});
			this._call("gotoPercent", arguments);
			return this;
		},
		stop: function(/*boolean?*/ gotoEnd){
			this._doAction("stop", arguments);
			this._call("stop", arguments);
			return this;
		},
		status: function(){
			return this._pseudoAnimation.status();
		},
		destroy: function(){
			arrayUtil.forEach(this._connects, connect.disconnect);
		}
	});
	lang.extend(_combine, _baseObj);

	coreFx.combine = /*===== dojo.fx.combine = =====*/ function(/*dojo.Animation[]*/ animations){
		// summary:
		//		Combine a list of `dojo.Animation`s to run in parallel
		//
		// description:
		//		Combine an array of `dojo.Animation`s to run in parallel,
		//		providing a new `dojo.Animation` instance encompasing each
		//		animation, firing standard animation events.
		//
		// example:
		//	Fade out `node` while fading in `otherNode` simultaneously
		//	|	dojo.fx.combine([
		//	|		dojo.fadeIn({ node:node }),
		//	|		dojo.fadeOut({ node:otherNode })
		//	|	]).play();
		//
		// example:
		//	When the longest animation ends, execute a function:
		//	|	var anim = dojo.fx.combine([
		//	|		dojo.fadeIn({ node: n, duration:700 }),
		//	|		dojo.fadeOut({ node: otherNode, duration: 300 })
		//	|	]);
		//	|	dojo.connect(anim, "onEnd", function(){
		//	|		// overall animation is done.
		//	|	});
		//	|	anim.play(); // play the animation
		//
		return new _combine(animations); // dojo.Animation
	};

	coreFx.wipeIn = /*===== dojo.fx.wipeIn = =====*/ function(/*Object*/ args){
		// summary:
		//		Expand a node to it's natural height.
		//
		// description:
		//		Returns an animation that will expand the
		//		node defined in 'args' object from it's current height to
		//		it's natural height (with no scrollbar).
		//		Node must have no margin/border/padding.
		//
		// args: Object
		//		A hash-map of standard `dojo.Animation` constructor properties
		//		(such as easing: node: duration: and so on)
		//
		// example:
		//	|	dojo.fx.wipeIn({
		//	|		node:"someId"
		//	|	}).play()
		var node = args.node = dom.byId(args.node), s = node.style, o;

		var anim = baseFx.animateProperty(lang.mixin({
			properties: {
				height: {
					// wrapped in functions so we wait till the last second to query (in case value has changed)
					start: function(){
						// start at current [computed] height, but use 1px rather than 0
						// because 0 causes IE to display the whole panel
						o = s.overflow;
						s.overflow = "hidden";
						if(s.visibility == "hidden" || s.display == "none"){
							s.height = "1px";
							s.display = "";
							s.visibility = "";
							return 1;
						}else{
							var height = domStyle.get(node, "height");
							return Math.max(height, 1);
						}
					},
					end: function(){
						return node.scrollHeight;
					}
				}
			}
		}, args));

		var fini = function(){
			s.height = "auto";
			s.overflow = o;
		};
		connect.connect(anim, "onStop", fini);
		connect.connect(anim, "onEnd", fini);

		return anim; // dojo.Animation
	};

	coreFx.wipeOut = /*===== dojo.fx.wipeOut = =====*/ function(/*Object*/ args){
		// summary:
		//		Shrink a node to nothing and hide it.
		//
		// description:
		//		Returns an animation that will shrink node defined in "args"
		//		from it's current height to 1px, and then hide it.
		//
		// args: Object
		//		A hash-map of standard `dojo.Animation` constructor properties
		//		(such as easing: node: duration: and so on)
		//
		// example:
		//	|	dojo.fx.wipeOut({ node:"someId" }).play()

		var node = args.node = dom.byId(args.node), s = node.style, o;

		var anim = baseFx.animateProperty(lang.mixin({
			properties: {
				height: {
					end: 1 // 0 causes IE to display the whole panel
				}
			}
		}, args));

		connect.connect(anim, "beforeBegin", function(){
			o = s.overflow;
			s.overflow = "hidden";
			s.display = "";
		});
		var fini = function(){
			s.overflow = o;
			s.height = "auto";
			s.display = "none";
		};
		connect.connect(anim, "onStop", fini);
		connect.connect(anim, "onEnd", fini);

		return anim; // dojo.Animation
	};

	coreFx.slideTo = /*===== dojo.fx.slideTo = =====*/ function(/*Object*/ args){
		// summary:
		//		Slide a node to a new top/left position
		//
		// description:
		//		Returns an animation that will slide "node"
		//		defined in args Object from its current position to
		//		the position defined by (args.left, args.top).
		//
		// args: Object
		//		A hash-map of standard `dojo.Animation` constructor properties
		//		(such as easing: node: duration: and so on). Special args members
		//		are `top` and `left`, which indicate the new position to slide to.
		//
		// example:
		//	|	.slideTo({ node: node, left:"40", top:"50", units:"px" }).play()

		var node = args.node = dom.byId(args.node),
			top = null, left = null;

		var init = (function(n){
			return function(){
				var cs = domStyle.getComputedStyle(n);
				var pos = cs.position;
				top = (pos == 'absolute' ? n.offsetTop : parseInt(cs.top) || 0);
				left = (pos == 'absolute' ? n.offsetLeft : parseInt(cs.left) || 0);
				if(pos != 'absolute' && pos != 'relative'){
					var ret = geom.position(n, true);
					top = ret.y;
					left = ret.x;
					n.style.position="absolute";
					n.style.top=top+"px";
					n.style.left=left+"px";
				}
			};
		})(node);
		init();

		var anim = baseFx.animateProperty(lang.mixin({
			properties: {
				top: args.top || 0,
				left: args.left || 0
			}
		}, args));
		connect.connect(anim, "beforeBegin", anim, init);

		return anim; // dojo.Animation
	};

	return coreFx;
});

},
'dijit/_base':function(){
define("dijit/_base", [
	".",
	"./a11y",	// used to be in dijit/_base/manager
	"./WidgetSet",	// used to be in dijit/_base/manager
	"./_base/focus",
	"./_base/manager",
	"./_base/place",
	"./_base/popup",
	"./_base/scroll",
	"./_base/sniff",
	"./_base/typematic",
	"./_base/wai",
	"./_base/window"
], function(dijit){

	// module:
	//		dijit/_base
	// summary:
	//		Includes all the modules in dijit/_base

	return dijit._base;
});

},
'dojox/mobile/sniff':function(){
define("dojox/mobile/sniff", [
	"dojo/_base/window",
	"dojo/_base/sniff"
], function(win, has){

	var ua = navigator.userAgent;

	// BlackBerry (OS 6 or later only)
	has.add("bb", ua.indexOf("BlackBerry") >= 0 && parseFloat(ua.split("Version/")[1]) || undefined, undefined, true);

	// Android
	has.add("android", parseFloat(ua.split("Android ")[1]) || undefined, undefined, true);

	// iPhone, iPod, or iPad
	// If iPod or iPad is detected, in addition to has("ipod") or has("ipad"),
	// has("iphone") will also have iOS version number.
	if(ua.match(/(iPhone|iPod|iPad)/)){
		var p = RegExp.$1.replace(/P/, 'p');
		var v = ua.match(/OS ([\d_]+)/) ? RegExp.$1 : "1";
		var os = parseFloat(v.replace(/_/, '.').replace(/_/g, ''));
		has.add(p, os, undefined, true);
		has.add("iphone", os, undefined, true);
	}

	if(has("webkit")){
		has.add("touch", (typeof win.doc.documentElement.ontouchstart != "undefined" &&
			navigator.appVersion.indexOf("Mobile") != -1) || !!has("android"), undefined, true);
	}

	return has;
});

},
'dojox/mobile/ProgressIndicator':function(){
define("dojox/mobile/ProgressIndicator", [
	"dojo/_base/config",
	"dojo/_base/declare",
	"dojo/dom-construct",
	"dojo/dom-style",
	"dojo/has"
], function(config, declare, domConstruct, domStyle, has){

	// module:
	//		dojox/mobile/ProgressIndicator
	// summary:
	//		A progress indication widget.

	var cls = declare("dojox.mobile.ProgressIndicator", null, {
		// summary:
		//		A progress indication widget.
		// description:
		//		ProgressIndicator is a round spinning graphical representation
		//		that indicates the current task is on-going.

		// interval: Number
		//		The time interval in milliseconds for updating the spinning
		//		indicator.
		interval: 100,

		// colors: Array
		//		An array of indicator colors.
		colors: [
			"#C0C0C0", "#C0C0C0", "#C0C0C0", "#C0C0C0",
			"#C0C0C0", "#C0C0C0", "#B8B9B8", "#AEAFAE",
			"#A4A5A4", "#9A9A9A", "#8E8E8E", "#838383"
		],

		constructor: function(){
			this._bars = [];
			this.domNode = domConstruct.create("DIV");
			this.domNode.className = "mblProgContainer";
			if(config["mblAndroidWorkaround"] !== false && has("android") >= 2.2 && has("android") < 3){
				// workaround to avoid the side effects of the fixes for android screen flicker problem
				domStyle.set(this.domNode, "webkitTransform", "translate3d(0,0,0)");
			}
			this.spinnerNode = domConstruct.create("DIV", null, this.domNode);
			for(var i = 0; i < this.colors.length; i++){
				var div = domConstruct.create("DIV", {className:"mblProg mblProg"+i}, this.spinnerNode);
				this._bars.push(div);
			}
		},
	
		start: function(){
			// summary:
			//		Starts the ProgressIndicator spinning.
			if(this.imageNode){
				var img = this.imageNode;
				var l = Math.round((this.domNode.offsetWidth - img.offsetWidth) / 2);
				var t = Math.round((this.domNode.offsetHeight - img.offsetHeight) / 2);
				img.style.margin = t+"px "+l+"px";
				return;
			}
			var cntr = 0;
			var _this = this;
			var n = this.colors.length;
			this.timer = setInterval(function(){
				cntr--;
				cntr = cntr < 0 ? n - 1 : cntr;
				var c = _this.colors;
				for(var i = 0; i < n; i++){
					var idx = (cntr + i) % n;
					_this._bars[i].style.backgroundColor = c[idx];
				}
			}, this.interval);
		},
	
		stop: function(){
			// summary:
			//		Stops the ProgressIndicator spinning.
			if(this.timer){
				clearInterval(this.timer);
			}
			this.timer = null;
			if(this.domNode.parentNode){
				this.domNode.parentNode.removeChild(this.domNode);
			}
		},

		setImage: function(/*String*/file){
			// summary:
			//		Sets an indicator icon image file (typically animated GIF).
			//		If null is specified, restores the default spinner.
			if(file){
				this.imageNode = domConstruct.create("IMG", {src:file}, this.domNode);
				this.spinnerNode.style.display = "none";
			}else{
				if(this.imageNode){
					this.domNode.removeChild(this.imageNode);
					this.imageNode = null;
				}
				this.spinnerNode.style.display = "";
			}
		}
	});

	cls._instance = null;
	cls.getInstance = function(){
		if(!cls._instance){
			cls._instance = new cls();
		}
		return cls._instance;
	};

	return cls;
});

},
'dijit/form/_FormWidgetMixin':function(){
define("dijit/form/_FormWidgetMixin", [
	"dojo/_base/array", // array.forEach
	"dojo/_base/declare", // declare
	"dojo/dom-attr", // domAttr.set
	"dojo/dom-style", // domStyle.get
	"dojo/_base/lang", // lang.hitch lang.isArray
	"dojo/mouse", // mouse.isLeft
	"dojo/_base/sniff", // has("webkit")
	"dojo/_base/window", // win.body
	"dojo/window", // winUtils.scrollIntoView
	"../a11y"	// a11y.hasDefaultTabStop
], function(array, declare, domAttr, domStyle, lang, mouse, has, win, winUtils, a11y){

// module:
//		dijit/form/_FormWidgetMixin
// summary:
//		Mixin for widgets corresponding to native HTML elements such as <checkbox> or <button>,
//		which can be children of a <form> node or a `dijit.form.Form` widget.

return declare("dijit.form._FormWidgetMixin", null, {
	// summary:
	//		Mixin for widgets corresponding to native HTML elements such as <checkbox> or <button>,
	//		which can be children of a <form> node or a `dijit.form.Form` widget.
	//
	// description:
	//		Represents a single HTML element.
	//		All these widgets should have these attributes just like native HTML input elements.
	//		You can set them during widget construction or afterwards, via `dijit._Widget.attr`.
	//
	//		They also share some common methods.

	// name: [const] String
	//		Name used when submitting form; same as "name" attribute or plain HTML elements
	name: "",

	// alt: String
	//		Corresponds to the native HTML <input> element's attribute.
	alt: "",

	// value: String
	//		Corresponds to the native HTML <input> element's attribute.
	value: "",

	// type: [const] String
	//		Corresponds to the native HTML <input> element's attribute.
	type: "text",

	// tabIndex: Integer
	//		Order fields are traversed when user hits the tab key
	tabIndex: "0",
	_setTabIndexAttr: "focusNode",	// force copy even when tabIndex default value, needed since Button is <span>

	// disabled: Boolean
	//		Should this widget respond to user input?
	//		In markup, this is specified as "disabled='disabled'", or just "disabled".
	disabled: false,

	// intermediateChanges: Boolean
	//		Fires onChange for each value change or only on demand
	intermediateChanges: false,

	// scrollOnFocus: Boolean
	//		On focus, should this widget scroll into view?
	scrollOnFocus: true,

	// Override _WidgetBase mapping id to this.domNode, needs to be on focusNode so <label> etc.
	// works with screen reader
	_setIdAttr: "focusNode",

	postCreate: function(){
		this.inherited(arguments);
		this.connect(this.domNode, "onmousedown", "_onMouseDown");
	},

	_setDisabledAttr: function(/*Boolean*/ value){
		this._set("disabled", value);
		domAttr.set(this.focusNode, 'disabled', value);
		if(this.valueNode){
			domAttr.set(this.valueNode, 'disabled', value);
		}
		this.focusNode.setAttribute("aria-disabled", value);

		if(value){
			// reset these, because after the domNode is disabled, we can no longer receive
			// mouse related events, see #4200
			this._set("hovering", false);
			this._set("active", false);

			// clear tab stop(s) on this widget's focusable node(s)  (ComboBox has two focusable nodes)
			var attachPointNames = "tabIndex" in this.attributeMap ? this.attributeMap.tabIndex :
				("_setTabIndexAttr" in this) ? this._setTabIndexAttr : "focusNode";
			array.forEach(lang.isArray(attachPointNames) ? attachPointNames : [attachPointNames], function(attachPointName){
				var node = this[attachPointName];
				// complex code because tabIndex=-1 on a <div> doesn't work on FF
				if(has("webkit") || a11y.hasDefaultTabStop(node)){	// see #11064 about webkit bug
					node.setAttribute('tabIndex', "-1");
				}else{
					node.removeAttribute('tabIndex');
				}
			}, this);
		}else{
			if(this.tabIndex != ""){
				this.set('tabIndex', this.tabIndex);
			}
		}
	},

	_onFocus: function(e){
		if(this.scrollOnFocus){
			winUtils.scrollIntoView(this.domNode);
		}
		this.inherited(arguments);
	},

	isFocusable: function(){
		// summary:
		//		Tells if this widget is focusable or not.  Used internally by dijit.
		// tags:
		//		protected
		return !this.disabled && this.focusNode && (domStyle.get(this.domNode, "display") != "none");
	},

	focus: function(){
		// summary:
		//		Put focus on this widget
		if(!this.disabled && this.focusNode.focus){
			try{ this.focusNode.focus(); }catch(e){}/*squelch errors from hidden nodes*/
		}
	},

	compare: function(/*anything*/ val1, /*anything*/ val2){
		// summary:
		//		Compare 2 values (as returned by get('value') for this widget).
		// tags:
		//		protected
		if(typeof val1 == "number" && typeof val2 == "number"){
			return (isNaN(val1) && isNaN(val2)) ? 0 : val1 - val2;
		}else if(val1 > val2){
			return 1;
		}else if(val1 < val2){
			return -1;
		}else{
			return 0;
		}
	},

	onChange: function(/*===== newValue =====*/){
		// summary:
		//		Callback when this widget's value is changed.
		// tags:
		//		callback
	},

	// _onChangeActive: [private] Boolean
	//		Indicates that changes to the value should call onChange() callback.
	//		This is false during widget initialization, to avoid calling onChange()
	//		when the initial value is set.
	_onChangeActive: false,

	_handleOnChange: function(/*anything*/ newValue, /*Boolean?*/ priorityChange){
		// summary:
		//		Called when the value of the widget is set.  Calls onChange() if appropriate
		// newValue:
		//		the new value
		// priorityChange:
		//		For a slider, for example, dragging the slider is priorityChange==false,
		//		but on mouse up, it's priorityChange==true.  If intermediateChanges==false,
		//		onChange is only called form priorityChange=true events.
		// tags:
		//		private
		if(this._lastValueReported == undefined && (priorityChange === null || !this._onChangeActive)){
			// this block executes not for a change, but during initialization,
			// and is used to store away the original value (or for ToggleButton, the original checked state)
			this._resetValue = this._lastValueReported = newValue;
		}
		this._pendingOnChange = this._pendingOnChange
			|| (typeof newValue != typeof this._lastValueReported)
			|| (this.compare(newValue, this._lastValueReported) != 0);
		if((this.intermediateChanges || priorityChange || priorityChange === undefined) && this._pendingOnChange){
			this._lastValueReported = newValue;
			this._pendingOnChange = false;
			if(this._onChangeActive){
				if(this._onChangeHandle){
					clearTimeout(this._onChangeHandle);
				}
				// setTimeout allows hidden value processing to run and
				// also the onChange handler can safely adjust focus, etc
				this._onChangeHandle = setTimeout(lang.hitch(this,
					function(){
						this._onChangeHandle = null;
						this.onChange(newValue);
					}), 0); // try to collapse multiple onChange's fired faster than can be processed
			}
		}
	},

	create: function(){
		// Overrides _Widget.create()
		this.inherited(arguments);
		this._onChangeActive = true;
	},

	destroy: function(){
		if(this._onChangeHandle){ // destroy called before last onChange has fired
			clearTimeout(this._onChangeHandle);
			this.onChange(this._lastValueReported);
		}
		this.inherited(arguments);
	},

	_onMouseDown: function(e){
		// If user clicks on the button, even if the mouse is released outside of it,
		// this button should get focus (to mimics native browser buttons).
		// This is also needed on chrome because otherwise buttons won't get focus at all,
		// which leads to bizarre focus restore on Dialog close etc.
		// IE exhibits strange scrolling behavior when focusing a node so only do it when !focused.
		// FF needs the extra help to make sure the mousedown actually gets to the focusNode
		if((!this.focused || !has("ie")) && !e.ctrlKey && mouse.isLeft(e) && this.isFocusable()){ // !e.ctrlKey to ignore right-click on mac
			// Set a global event to handle mouseup, so it fires properly
			// even if the cursor leaves this.domNode before the mouse up event.
			var mouseUpConnector = this.connect(win.body(), "onmouseup", function(){
				if(this.isFocusable()){
					this.focus();
				}
				this.disconnect(mouseUpConnector);
			});
		}
	}
});

});

},
'dijit/BackgroundIframe':function(){
define("dijit/BackgroundIframe", [
	"require",			// require.toUrl
	".",	// to export dijit.BackgroundIframe
	"dojo/_base/config",
	"dojo/dom-construct", // domConstruct.create
	"dojo/dom-style", // domStyle.set
	"dojo/_base/lang", // lang.extend lang.hitch
	"dojo/on",
	"dojo/_base/sniff", // has("ie"), has("mozilla"), has("quirks")
	"dojo/_base/window" // win.doc.createElement
], function(require, dijit, config, domConstruct, domStyle, lang, on, has, win){

	// module:
	//		dijit/BackgroundIFrame
	// summary:
	//		new dijit.BackgroundIframe(node)
	//		Makes a background iframe as a child of node, that fills
	//		area (and position) of node

	// TODO: remove _frames, it isn't being used much, since popups never release their
	// iframes (see [22236])
	var _frames = new function(){
		// summary:
		//		cache of iframes

		var queue = [];

		this.pop = function(){
			var iframe;
			if(queue.length){
				iframe = queue.pop();
				iframe.style.display="";
			}else{
				if(has("ie") < 9){
					var burl = config["dojoBlankHtmlUrl"] || require.toUrl("dojo/resources/blank.html") || "javascript:\"\"";
					var html="<iframe src='" + burl + "' role='presentation'"
						+ " style='position: absolute; left: 0px; top: 0px;"
						+ "z-index: -1; filter:Alpha(Opacity=\"0\");'>";
					iframe = win.doc.createElement(html);
				}else{
					iframe = domConstruct.create("iframe");
					iframe.src = 'javascript:""';
					iframe.className = "dijitBackgroundIframe";
					iframe.setAttribute("role", "presentation");
					domStyle.set(iframe, "opacity", 0.1);
				}
				iframe.tabIndex = -1; // Magic to prevent iframe from getting focus on tab keypress - as style didn't work.
			}
			return iframe;
		};

		this.push = function(iframe){
			iframe.style.display="none";
			queue.push(iframe);
		}
	}();


	dijit.BackgroundIframe = function(/*DomNode*/ node){
		// summary:
		//		For IE/FF z-index schenanigans. id attribute is required.
		//
		// description:
		//		new dijit.BackgroundIframe(node)
		//			Makes a background iframe as a child of node, that fills
		//			area (and position) of node

		if(!node.id){ throw new Error("no id"); }
		if(has("ie") || has("mozilla")){
			var iframe = (this.iframe = _frames.pop());
			node.appendChild(iframe);
			if(has("ie")<7 || has("quirks")){
				this.resize(node);
				this._conn = on(node, 'resize', lang.hitch(this, function(){
					this.resize(node);
				}));
			}else{
				domStyle.set(iframe, {
					width: '100%',
					height: '100%'
				});
			}
		}
	};

	lang.extend(dijit.BackgroundIframe, {
		resize: function(node){
			// summary:
			// 		Resize the iframe so it's the same size as node.
			//		Needed on IE6 and IE/quirks because height:100% doesn't work right.
			if(this.iframe){
				domStyle.set(this.iframe, {
					width: node.offsetWidth + 'px',
					height: node.offsetHeight + 'px'
				});
			}
		},
		destroy: function(){
			// summary:
			//		destroy the iframe
			if(this._conn){
				this._conn.remove();
				this._conn = null;
			}
			if(this.iframe){
				_frames.push(this.iframe);
				delete this.iframe;
			}
		}
	});

	return dijit.BackgroundIframe;
});

},
'dojox/mobile':function(){
define([
	".",
	"dojo/_base/lang",
	"dojox/mobile/_base"
], function(dojox, lang, base){
	lang.getObject("mobile", true, dojox);
	return dojox.mobile;
});

},
'dijit/form/_FormValueMixin':function(){
define("dijit/form/_FormValueMixin", [
	"dojo/_base/declare", // declare
	"dojo/dom-attr", // domAttr.set
	"dojo/keys", // keys.ESCAPE
	"dojo/_base/sniff", // has("ie"), has("quirks")
	"./_FormWidgetMixin"
], function(declare, domAttr, keys, has, _FormWidgetMixin){

/*=====
	var _FormWidgetMixin = dijit.form._FormWidgetMixin;
=====*/

	// module:
	//		dijit/form/_FormValueMixin
	// summary:
	//		Mixin for widgets corresponding to native HTML elements such as <input> or <select> that have user changeable values.

	return declare("dijit.form._FormValueMixin", _FormWidgetMixin, {
		// summary:
		//		Mixin for widgets corresponding to native HTML elements such as <input> or <select> that have user changeable values.
		// description:
		//		Each _FormValueMixin represents a single input value, and has a (possibly hidden) <input> element,
		//		to which it serializes it's input value, so that form submission (either normal submission or via FormBind?)
		//		works as expected.

		// readOnly: Boolean
		//		Should this widget respond to user input?
		//		In markup, this is specified as "readOnly".
		//		Similar to disabled except readOnly form values are submitted.
		readOnly: false,

		_setReadOnlyAttr: function(/*Boolean*/ value){
			domAttr.set(this.focusNode, 'readOnly', value);
			this.focusNode.setAttribute("aria-readonly", value);
			this._set("readOnly", value);
		},

		postCreate: function(){
			this.inherited(arguments);

			if(has("ie")){ // IE won't stop the event with keypress
				this.connect(this.focusNode || this.domNode, "onkeydown", this._onKeyDown);
			}
			// Update our reset value if it hasn't yet been set (because this.set()
			// is only called when there *is* a value)
			if(this._resetValue === undefined){
				this._lastValueReported = this._resetValue = this.value;
			}
		},

		_setValueAttr: function(/*anything*/ newValue, /*Boolean?*/ priorityChange){
			// summary:
			//		Hook so set('value', value) works.
			// description:
			//		Sets the value of the widget.
			//		If the value has changed, then fire onChange event, unless priorityChange
			//		is specified as null (or false?)
			this._handleOnChange(newValue, priorityChange);
		},

		_handleOnChange: function(/*anything*/ newValue, /*Boolean?*/ priorityChange){
			// summary:
			//		Called when the value of the widget has changed.  Saves the new value in this.value,
			//		and calls onChange() if appropriate.   See _FormWidget._handleOnChange() for details.
			this._set("value", newValue);
			this.inherited(arguments);
		},

		undo: function(){
			// summary:
			//		Restore the value to the last value passed to onChange
			this._setValueAttr(this._lastValueReported, false);
		},

		reset: function(){
			// summary:
			//		Reset the widget's value to what it was at initialization time
			this._hasBeenBlurred = false;
			this._setValueAttr(this._resetValue, true);
		},

		_onKeyDown: function(e){
			if(e.keyCode == keys.ESCAPE && !(e.ctrlKey || e.altKey || e.metaKey)){
				var te;
				if(has("ie") < 9 || (has("ie") && has("quirks"))){
					e.preventDefault(); // default behavior needs to be stopped here since keypress is too late
					te = document.createEventObject();
					te.keyCode = keys.ESCAPE;
					te.shiftKey = e.shiftKey;
					e.srcElement.fireEvent('onkeypress', te);
				}
			}
		}
	});
});

},
'dojox/mobile/common':function(){
define("dojox/mobile/common", [
	"dojo/_base/kernel", // to test dojo.hash
	"dojo/_base/array",
	"dojo/_base/config",
	"dojo/_base/connect",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/dom-style",
//	"dojo/hash", // optionally prereq'ed
	"dojo/ready",
	"dijit/registry",	// registry.toArray
	"./sniff",
	"./uacss"
], function(dojo, array, config, connect, lang, win, domClass, domConstruct, domStyle, ready, registry, has, uacss){

	var dm = lang.getObject("dojox.mobile", true);
/*=====
	var dm = dojox.mobile;
=====*/

	// module:
	//		dojox/mobile/common
	// summary:
	//		A common module for dojox.mobile.
	// description:
	//		This module includes common utility functions that are used by
	//		dojox.mobile widgets. Also, it provides functions that are commonly
	//		necessary for mobile web applications, such as the hide address bar
	//		function.

	dm.getScreenSize = function(){
		// summary:
		//		Returns the dimensions of the browser window.
		return {
			h: win.global.innerHeight || win.doc.documentElement.clientHeight,
			w: win.global.innerWidth || win.doc.documentElement.clientWidth
		};
	};

	dm.updateOrient = function(){
		// summary:
		//		Updates the orientation specific css classes, 'dj_portrait' and
		//		'dj_landscape'.
		var dim = dm.getScreenSize();
		domClass.replace(win.doc.documentElement,
				  dim.h > dim.w ? "dj_portrait" : "dj_landscape",
				  dim.h > dim.w ? "dj_landscape" : "dj_portrait");
	};
	dm.updateOrient();

	dm.tabletSize = 500;
	dm.detectScreenSize = function(/*Boolean?*/force){
		// summary:
		//		Detects the screen size and determines if the screen is like
		//		phone or like tablet. If the result is changed,
		//		it sets either of the following css class to <html>
		//			- 'dj_phone'
		//			- 'dj_tablet'
		//		and it publishes either of the following events.
		//			- '/dojox/mobile/screenSize/phone'
		//			- '/dojox/mobile/screenSize/tablet'
		var dim = dm.getScreenSize();
		var sz = Math.min(dim.w, dim.h);
		var from, to;
		if(sz >= dm.tabletSize && (force || (!this._sz || this._sz < dm.tabletSize))){
			from = "phone";
			to = "tablet";
		}else if(sz < dm.tabletSize && (force || (!this._sz || this._sz >= dm.tabletSize))){
			from = "tablet";
			to = "phone";
		}
		if(to){
			domClass.replace(win.doc.documentElement, "dj_"+to, "dj_"+from);
			connect.publish("/dojox/mobile/screenSize/"+to, [dim]);
		}
		this._sz = sz;
	};
	dm.detectScreenSize();

	dm.setupIcon = function(/*DomNode*/iconNode, /*String*/iconPos){
		// summary:
		//		Sets up CSS sprite for a foreground image.
		if(iconNode && iconPos){
			var arr = array.map(iconPos.split(/[ ,]/),function(item){return item-0});
			var t = arr[0]; // top
			var r = arr[1] + arr[2]; // right
			var b = arr[0] + arr[3]; // bottom
			var l = arr[1]; // left
			domStyle.set(iconNode, {
				clip: "rect("+t+"px "+r+"px "+b+"px "+l+"px)",
				top: (iconNode.parentNode ? domStyle.get(iconNode, "top") : 0) - t + "px",
				left: -l + "px"
			});
		}
	};

	// dojox.mobile.hideAddressBarWait: Number
	//		The time in milliseconds to wait before the fail-safe hiding address
	//		bar runs. The value must be larger than 800.
	dm.hideAddressBarWait = typeof(config["mblHideAddressBarWait"]) === "number" ?
		config["mblHideAddressBarWait"] : 1500;

	dm.hide_1 = function(force){
		// summary:
		//		Internal function to hide the address bar.
		scrollTo(0, 1);
		var h = dm.getScreenSize().h + "px";
		if(has("android")){
			if(force){
				win.body().style.minHeight = h;
			}
			dm.resizeAll();
		}else{
			if(force || dm._h === h && h !== win.body().style.minHeight){
				win.body().style.minHeight = h;
				dm.resizeAll();
			}
		}
		dm._h = h;
	};

	dm.hide_fs = function(){
		// summary:
		//		Internal function to hide the address bar for fail-safe.
		// description:
		//		Resets the height of the body, performs hiding the address
		//		bar, and calls resizeAll().
		//		This is for fail-safe, in case of failure to complete the
		//		address bar hiding in time.
		var t = win.body().style.minHeight;
		win.body().style.minHeight = (dm.getScreenSize().h * 2) + "px"; // to ensure enough height for scrollTo to work
		scrollTo(0, 1);
		setTimeout(function(){
			dm.hide_1(1);
			dm._hiding = false;
		}, 1000);
	};
	dm.hideAddressBar = function(/*Event?*/evt){
		// summary:
		//		Hides the address bar.
		// description:
		//		Tries hiding of the address bar a couple of times to do it as
		//		quick as possible while ensuring resize is done after the hiding
		//		finishes.
		if(dm.disableHideAddressBar || dm._hiding){ return; }
		dm._hiding = true;
		dm._h = 0;
		win.body().style.minHeight = (dm.getScreenSize().h * 2) + "px"; // to ensure enough height for scrollTo to work
		setTimeout(dm.hide_1, 0);
		setTimeout(dm.hide_1, 200);
		setTimeout(dm.hide_1, 800);
		setTimeout(dm.hide_fs, dm.hideAddressBarWait);
	};

	dm.resizeAll = function(/*Event?*/evt, /*Widget?*/root){
		// summary:
		//		Call the resize() method of all the top level resizable widgets.
		// description:
		//		Find all widgets that do not have a parent or the parent does not
		//		have the resize() method, and call resize() for them.
		//		If a widget has a parent that has resize(), call of the widget's
		//		resize() is its parent's responsibility.
		// evt:
		//		Native event object
		// root:
		//		If specified, search the specified widget recursively for top level
		//		resizable widgets.
		//		root.resize() is always called regardless of whether root is a
		//		top level widget or not.
		//		If omitted, search the entire page.
		if(dm.disableResizeAll){ return; }
		connect.publish("/dojox/mobile/resizeAll", [evt, root]);
		dm.updateOrient();
		dm.detectScreenSize();
		var isTopLevel = function(w){
			var parent = w.getParent && w.getParent();
			return !!((!parent || !parent.resize) && w.resize);
		};
		var resizeRecursively = function(w){
			array.forEach(w.getChildren(), function(child){
				if(isTopLevel(child)){ child.resize(); }
				resizeRecursively(child);
			});
		};
		if(root){
			if(root.resize){ root.resize(); }
			resizeRecursively(root);
		}else{
			array.forEach(array.filter(registry.toArray(), isTopLevel),
					function(w){ w.resize(); });
		}
	};

	dm.openWindow = function(url, target){
		// summary:
		//		Opens a new browser window with the given url.
		win.global.open(url, target || "_blank");
	};

	dm.createDomButton = function(/*DomNode*/refNode, /*Object?*/style, /*DomNode?*/toNode){
		// summary:
		//		Creates a DOM button.
		// description:
		//		DOM button is a simple graphical object that consists of one or
		//		more nested DIV elements with some CSS styling. It can be used
		//		in place of an icon image on ListItem, IconItem, and so on.
		//		The kind of DOM button to create is given as a class name of
		//		refNode. The number of DIVs to create is searched from the style
		//		sheets in the page. However, if the class name has a suffix that
		//		starts with an underscore, like mblDomButtonGoldStar_5, then the
		//		suffixed number is used instead. A class name for DOM button
		//		must starts with 'mblDomButton'.
		// refNode:
		//		A node that has a DOM button class name.
		// style:
		//		A hash object to set styles to the node.
		// toNode:
		//		A root node to create a DOM button. If omitted, refNode is used.

		if(!dm._domButtons){
			if(has("webkit")){
				var findDomButtons = function(sheet, dic){
					// summary:
					//		Searches the style sheets for DOM buttons.
					// description:
					//		Returns a key-value pair object whose keys are DOM
					//		button class names and values are the number of DOM
					//		elements they need.
					var i, j;
					if(!sheet){
						var dic = {};
						var ss = dojo.doc.styleSheets;
						for (i = 0; i < ss.length; i++){
							ss[i] && findDomButtons(ss[i], dic);
						}
						return dic;
					}
					var rules = sheet.cssRules || [];
					for (i = 0; i < rules.length; i++){
						var rule = rules[i];
						if(rule.href && rule.styleSheet){
							findDomButtons(rule.styleSheet, dic);
						}else if(rule.selectorText){
							var sels = rule.selectorText.split(/,/);
							for (j = 0; j < sels.length; j++){
								var sel = sels[j];
								var n = sel.split(/>/).length - 1;
								if(sel.match(/(mblDomButton\w+)/)){
									var cls = RegExp.$1;
									if(!dic[cls] || n > dic[cls]){
										dic[cls] = n;
									}
								}
							}
						}
					}
				}
				dm._domButtons = findDomButtons();
			}else{
				dm._domButtons = {};
			}
		}

		var s = refNode.className;
		var node = toNode || refNode;
		if(s.match(/(mblDomButton\w+)/) && s.indexOf("/") === -1){
			var btnClass = RegExp.$1;
			var nDiv = 4;
			if(s.match(/(mblDomButton\w+_(\d+))/)){
				nDiv = RegExp.$2 - 0;
			}else if(dm._domButtons[btnClass] !== undefined){
				nDiv = dm._domButtons[btnClass];
			}
			var props = null;
			if(has("bb") && config["mblBBBoxShadowWorkaround"] !== false){
				// Removes box-shadow because BlackBerry incorrectly renders it.
				props = {style:"-webkit-box-shadow:none"};
			}
			for(var i = 0, p = node; i < nDiv; i++){
				p = p.firstChild || domConstruct.create("DIV", props, p);
			}
			if(toNode){
				setTimeout(function(){
					domClass.remove(refNode, btnClass);
				}, 0);
				domClass.add(toNode, btnClass);
			}
		}else if(s.indexOf(".") !== -1){ // file name
			domConstruct.create("IMG", {src:s}, node);
		}else{
			return null;
		}
		domClass.add(node, "mblDomButton");
		if(config["mblAndroidWorkaround"] !== false && has("android") >= 2.2){
			// Android workaround for the issue that domButtons' -webkit-transform styles sometimes invalidated
			// by applying -webkit-transform:translated3d(x,y,z) style programmatically to non-ancestor elements,
			// which results in breaking domButtons.
			domStyle.set(node, "webkitTransform", "translate3d(0,0,0)");
		}
		!!style && domStyle.set(node, style);
		return node;
	};
	
	dm.createIcon = function(/*String*/icon, /*String*/iconPos, /*DomNode*/node, /*String?*/title, /*DomNode?*/parent){
		// summary:
		//		Creates or updates an icon node
		// description:
		//		If node exists, updates the existing node. Otherwise, creates a new one.
		// icon:
		//		Path for an image, or DOM button class name.
		if(icon && icon.indexOf("mblDomButton") === 0){
			// DOM button
			if(node && node.className.match(/(mblDomButton\w+)/)){
				domClass.remove(node, RegExp.$1);
			}else{
				node = domConstruct.create("DIV");
			}
			node.title = title;
			domClass.add(node, icon);
			dm.createDomButton(node);
		}else if(icon && icon !== "none"){
			// Image
			if(!node || node.nodeName !== "IMG"){
				node = domConstruct.create("IMG", {
					alt: title
				});
			}
			node.src = (icon || "").replace("${theme}", dm.currentTheme);
			dm.setupIcon(node, iconPos);
			if(parent && iconPos){
				var arr = iconPos.split(/[ ,]/);
				domStyle.set(parent, {
					width: arr[2] + "px",
					height: arr[3] + "px"
				});
			}
		}
		if(parent){
			parent.appendChild(node);
		}
		return node;
	};

	// flag for iphone flicker workaround
	dm._iw = config["mblIosWorkaround"] !== false && has("iphone");
	if(dm._iw){
		dm._iwBgCover = domConstruct.create("div"); // Cover to hide flicker in the background
	}
	
	if(config.parseOnLoad){
		ready(90, function(){
			// avoid use of query
			/*
			var list = query('[lazy=true] [dojoType]', null);
			list.forEach(function(node, index, nodeList){
				node.setAttribute("__dojoType", node.getAttribute("dojoType"));
				node.removeAttribute("dojoType");
			});
			*/
		
			var nodes = win.body().getElementsByTagName("*");
			var i, len, s;
			len = nodes.length;
			for(i = 0; i < len; i++){
				s = nodes[i].getAttribute("dojoType");
				if(s){
					if(nodes[i].parentNode.getAttribute("lazy") == "true"){
						nodes[i].setAttribute("__dojoType", s);
						nodes[i].removeAttribute("dojoType");
					}
				}
			}
		});
	}
	
	ready(function(){
		dm.detectScreenSize(true);
		if(config["mblApplyPageStyles"] !== false){
			domClass.add(win.doc.documentElement, "mobile");
		}
		if(has("chrome")){
			// dojox.mobile does not load uacss (only _compat does), but we need dj_chrome.
			domClass.add(win.doc.documentElement, "dj_chrome");
		}

		if(config["mblAndroidWorkaround"] !== false && has("android") >= 2.2){ // workaround for android screen flicker problem
			if(config["mblAndroidWorkaroundButtonStyle"] !== false){
				// workaround to avoid buttons disappear due to the side-effect of the webkitTransform workaroud below
				domConstruct.create("style", {innerHTML:"BUTTON,INPUT[type='button'],INPUT[type='submit'],INPUT[type='reset'],INPUT[type='file']::-webkit-file-upload-button{-webkit-appearance:none;}"}, win.doc.head, "first");
			}
			if(has("android") < 3){ // for Android 2.2.x and 2.3.x
				domStyle.set(win.doc.documentElement, "webkitTransform", "translate3d(0,0,0)");
				// workaround for auto-scroll issue when focusing input fields
				connect.connect(null, "onfocus", null, function(e){
					domStyle.set(win.doc.documentElement, "webkitTransform", "");
				});
				connect.connect(null, "onblur", null, function(e){
					domStyle.set(win.doc.documentElement, "webkitTransform", "translate3d(0,0,0)");
				});
			}else{ // for Android 3.x
				if(config["mblAndroid3Workaround"] !== false){
					domStyle.set(win.doc.documentElement, {
						webkitBackfaceVisibility: "hidden",
						webkitPerspective: 8000
					});
				}
			}
		}
	
		//	You can disable hiding the address bar with the following djConfig.
		//	var djConfig = { mblHideAddressBar: false };
		var f = dm.resizeAll;
		if(config["mblHideAddressBar"] !== false &&
			navigator.appVersion.indexOf("Mobile") != -1 ||
			config["mblForceHideAddressBar"] === true){
			dm.hideAddressBar();
			if(config["mblAlwaysHideAddressBar"] === true){
				f = dm.hideAddressBar;
			}
		}
		connect.connect(null, (win.global.onorientationchange !== undefined && !has("android"))
			? "onorientationchange" : "onresize", null, f);
	
		// avoid use of query
		/*
		var list = query('[__dojoType]', null);
		list.forEach(function(node, index, nodeList){
			node.setAttribute("dojoType", node.getAttribute("__dojoType"));
			node.removeAttribute("__dojoType");
		});
		*/
	
		var nodes = win.body().getElementsByTagName("*");
		var i, len = nodes.length, s;
		for(i = 0; i < len; i++){
			s = nodes[i].getAttribute("__dojoType");
			if(s){
				nodes[i].setAttribute("dojoType", s);
				nodes[i].removeAttribute("__dojoType");
			}
		}
	
		if(dojo.hash){
			// find widgets under root recursively
			var findWidgets = function(root){
				if(!root){ return []; }
				var arr = registry.findWidgets(root);
				var widgets = arr;
				for(var i = 0; i < widgets.length; i++){
					arr = arr.concat(findWidgets(widgets[i].containerNode));
				}
				return arr;
			};
			connect.subscribe("/dojo/hashchange", null, function(value){
				var view = dm.currentView;
				if(!view){ return; }
				var params = dm._params;
				if(!params){ // browser back/forward button was pressed
					var moveTo = value ? value : dm._defaultView.id;
					var widgets = findWidgets(view.domNode);
					var dir = 1, transition = "slide";
					for(i = 0; i < widgets.length; i++){
						var w = widgets[i];
						if("#"+moveTo == w.moveTo){
							// found a widget that has the given moveTo
							transition = w.transition;
							dir = (w instanceof dm.Heading) ? -1 : 1;
							break;
						}
					}
					params = [ moveTo, dir, transition ];
				}
				view.performTransition.apply(view, params);
				dm._params = null;
			});
		}
	
		win.body().style.visibility = "visible";
	});

	// To search _parentNode first.  TODO:1.8 reconsider this redefinition.
	registry.getEnclosingWidget = function(node){
		while(node){
			var id = node.getAttribute && node.getAttribute("widgetId");
			if(id){
				return registry.byId(id);
			}
			node = node._parentNode || node.parentNode;
		}
		return null;
	};

	return dm;
});

},
'dojox/mobile/Heading':function(){
define([
	"dojo/_base/array",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/dom-style",
	"dijit/registry",	// registry.byId
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase",
	"./View"
], function(array, connect, declare, lang, win, domClass, domConstruct, domStyle, registry, Contained, Container, WidgetBase, View){

	var dm = lang.getObject("dojox.mobile", true);

/*=====
	var Contained = dijit._Contained;
	var Container = dijit._Container;
	var WidgetBase = dijit._WidgetBase;
=====*/

	// module:
	//		dojox/mobile/Heading
	// summary:
	//		A widget that represents a navigation bar.

	return declare("dojox.mobile.Heading", [WidgetBase, Container, Contained],{
		// summary:
		//		A widget that represents a navigation bar.
		// description:
		//		Heading is a widget that represents a navigation bar, which
		//		usually appears at the top of an application. It usually
		//		displays the title of the current view and can contain a
		//		navigational control. If you use it with
		//		dojox.mobile.ScrollableView, it can also be used as a fixed
		//		header bar or a fixed footer bar. In such cases, specify the
		//		fixed="top" attribute to be a fixed header bar or the
		//		fixed="bottom" attribute to be a fixed footer bar. Heading can
		//		have one or more ToolBarButton widgets as its children.

		// back: String
		//		A label for the navigational control to return to the previous
		//		View.
		back: "",

		// href: String
		//		A URL to open when the navigational control is pressed.
		href: "",

		// moveTo: String
		//		The id of the transition destination view which resides in the
		//		current page.
		//
		//		If the value has a hash sign ('#') before the id (e.g. #view1)
		//		and the dojo.hash module is loaded by the user application, the
		//		view transition updates the hash in the browser URL so that the
		//		user can bookmark the destination view. In this case, the user
		//		can also use the browser's back/forward button to navigate
		//		through the views in the browser history.
		//
		//		If null, transitions to a blank view.
		//		If '#', returns immediately without transition.
		moveTo: "",

		// transition: String
		//		A type of animated transition effect. You can choose from the
		//		standard transition types, "slide", "fade", "flip", or from the
		//		extended transition types, "cover", "coverv", "dissolve",
		//		"reveal", "revealv", "scaleIn", "scaleOut", "slidev",
		//		"swirl", "zoomIn", "zoomOut". If "none" is specified, transition
		//		occurs immediately without animation.
		transition: "slide",

		// label: String
		//		A title text of the heading. If the label is not specified, the
		//		innerHTML of the node is used as a label.
		label: "",

		// iconBase: String
		//		The default icon path for child items.
		iconBase: "",

		// backProp: Object
		//		Properties for the back button.
		backProp: {className: "mblArrowButton"},

		// tag: String
		//		A name of html tag to create as domNode.
		tag: "H1",

		buildRendering: function(){
			this.domNode = this.containerNode = this.srcNodeRef || win.doc.createElement(this.tag);
			this.domNode.className = "mblHeading";
			if(!this.label){
				array.forEach(this.domNode.childNodes, function(n){
					if(n.nodeType == 3){
						var v = lang.trim(n.nodeValue);
						if(v){
							this.label = v;
							this.labelNode = domConstruct.create("SPAN", {innerHTML:v}, n, "replace");
						}
					}
				}, this);
			}
			if(!this.labelNode){
				this.labelNode = domConstruct.create("SPAN", null, this.domNode);
			}
			this.labelNode.className = "mblHeadingSpanTitle";
			this.labelDivNode = domConstruct.create("DIV", {
				className: "mblHeadingDivTitle",
				innerHTML: this.labelNode.innerHTML
			}, this.domNode);
		},

		startup: function(){
			if(this._started){ return; }
			var parent = this.getParent && this.getParent();
			if(!parent || !parent.resize){ // top level widget
				var _this = this;
				setTimeout(function(){ // necessary to render correctly
					_this.resize();
				}, 0);
			}
			this.inherited(arguments);
		},
	
		resize: function(){
			if(this._btn){
				this._btn.style.width = this._body.offsetWidth + this._head.offsetWidth + "px";
			}
			if(this.labelNode){
				// find the rightmost left button (B), and leftmost right button (C)
				// +-----------------------------+
				// | |A| |B|             |C| |D| |
				// +-----------------------------+
				var leftBtn, rightBtn;
				var children = this.containerNode.childNodes;
				for(var i = children.length - 1; i >= 0; i--){
					var c = children[i];
					if(c.nodeType === 1){
						if(!rightBtn && domClass.contains(c, "mblToolBarButton") && domStyle.get(c, "float") === "right"){
							rightBtn = c;
						}
						if(!leftBtn && (domClass.contains(c, "mblToolBarButton") && domStyle.get(c, "float") === "left" || c === this._btn)){
							leftBtn = c;
						}
					}
				}

				if(!this.labelNodeLen && this.label){
					this.labelNode.style.display = "inline";
					this.labelNodeLen = this.labelNode.offsetWidth;
					this.labelNode.style.display = "";
				}

				var bw = this.domNode.offsetWidth; // bar width
				var rw = rightBtn ? bw - rightBtn.offsetLeft + 5 : 0; // rightBtn width
				var lw = leftBtn ? leftBtn.offsetLeft + leftBtn.offsetWidth + 5 : 0; // leftBtn width
				var tw = this.labelNodeLen || 0; // title width
				domClass[bw - Math.max(rw,lw)*2 > tw ? "add" : "remove"](this.domNode, "mblHeadingCenterTitle");
			}
			array.forEach(this.getChildren(), function(child){
				if(child.resize){ child.resize(); }
			});
		},

		_setBackAttr: function(/*String*/back){
			if (!back){
				domConstruct.destroy(this._btn);
				this._btn = null;
				this.back = "";
			}else{
				if(!this._btn){
					var btn = domConstruct.create("DIV", this.backProp, this.domNode, "first");
					var head = domConstruct.create("DIV", {className:"mblArrowButtonHead"}, btn);
					var body = domConstruct.create("DIV", {className:"mblArrowButtonBody mblArrowButtonText"}, btn);

					this._body = body;
					this._head = head;
					this._btn = btn;
					this.backBtnNode = btn;
					this.connect(body, "onclick", "onClick");
				}
				this.back = back;
				this._body.innerHTML = this._cv ? this._cv(this.back) : this.back;
			}
			this.resize();
		},
	
		_setLabelAttr: function(/*String*/label){
			this.label = label;
			this.labelNode.innerHTML = this.labelDivNode.innerHTML = this._cv ? this._cv(label) : label;
		},
	
		findCurrentView: function(){
			// summary:
			//		Search for the view widget that contains this widget.
			var w = this;
			while(true){
				w = w.getParent();
				if(!w){ return null; }
				if(w instanceof View){ break; }
			}
			return w;
		},

		onClick: function(e){
			var h1 = this.domNode;
			domClass.add(h1, "mblArrowButtonSelected");
			setTimeout(function(){
				domClass.remove(h1, "mblArrowButtonSelected");
			}, 1000);

			if(this.back && !this.moveTo && !this.href && history){
				history.back();	
				return;
			}	
	
			// keep the clicked position for transition animations
			var view = this.findCurrentView();
			if(view){
				view.clickedPosX = e.clientX;
				view.clickedPosY = e.clientY;
			}
			this.goTo(this.moveTo, this.href);
		},
	
		goTo: function(moveTo, href){
			// summary:
			//		Given the destination, makes a view transition.
			var view = this.findCurrentView();
			if(!view){ return; }
			if(href){
				view.performTransition(null, -1, this.transition, this, function(){location.href = href;});
			}else{
				if(dm.app && dm.app.STAGE_CONTROLLER_ACTIVE){
					// If in a full mobile app, then use its mechanisms to move back a scene
					connect.publish("/dojox/mobile/app/goback");
				}else{
					// Basically transition should be performed between two
					// siblings that share the same parent.
					// However, when views are nested and transition occurs from
					// an inner view, search for an ancestor view that is a sibling
					// of the target view, and use it as a source view.
					var node = registry.byId(view.convertToId(moveTo));
					if(node){
						var parent = node.getParent();
						while(view){
							var myParent = view.getParent();
							if(parent === myParent){
								break;
							}
							view = myParent;
						}
					}
					if(view){
						view.performTransition(moveTo, -1, this.transition);
					}
				}
			}
		}
	});
});

},
'dojox/main':function(){
define("dojox/main", ["dojo/_base/kernel"], function(dojo) {
	// module:
	//		dojox/main
	// summary:
	//		The dojox package main module; dojox package is somewhat unusual in that the main module currently just provides an empty object.

	return dojo.dojox;
});
},
'dojox/mobile/RoundRectList':function(){
define([
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/_base/window",
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase"
], function(array, declare, win, Contained, Container, WidgetBase){

/*=====
	var Contained = dijit._Contained;
	var Container = dijit._Container;
	var WidgetBase = dijit._WidgetBase;
=====*/

	// module:
	//		dojox/mobile/RoundRectList
	// summary:
	//		A rounded rectangle list.

	return declare("dojox.mobile.RoundRectList", [WidgetBase, Container, Contained], {
		// summary:
		//		A rounded rectangle list.
		// description:
		//		RoundRectList is a rounded rectangle list, which can be used to
		//		display a group of items. Each item must be
		//		dojox.mobile.ListItem.

		// transition: String
		//		The default animated transition effect for child items.
		transition: "slide",

		// iconBase: String
		//		The default icon path for child items.
		iconBase: "",

		// iconPos: String
		//		The default icon position for child items.
		iconPos: "",

		// select: String
		//		Selection mode of the list. The check mark is shown for the
		//		selected list item(s). The value can be "single", "multiple", or
		//		"". If "single", there can be only one selected item at a time.
		//		If "multiple", there can be multiple selected items at a time.
		select: "",

		// stateful: String
		//		If true, the last selected item remains highlighted.
		stateful: false,

		buildRendering: function(){
			this.domNode = this.containerNode = this.srcNodeRef || win.doc.createElement("UL");
			this.domNode.className = "mblRoundRectList";
		},
	
		resize: function(){
			// summary:
			//		Calls resize() of each child widget.
			array.forEach(this.getChildren(), function(child){
				if(child.resize){ child.resize(); }
			});
		},

		onCheckStateChanged: function(/*Widget*/listItem, /*String*/newState){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called when the check state has been changed.
		},

		_setStatefulAttr: function(stateful){
			this.stateful = stateful;
			array.forEach(this.getChildren(), function(child){
				child.setArrow && child.setArrow();
			});
		},

		deselectItem: function(/*ListItem*/item){
			// summary:
			//		Deselects the given item.
			item.deselect();
		},

		deselectAll: function(){
			// summary:
			//		Deselects all the items.
			array.forEach(this.getChildren(), function(child){
				child.deselect && child.deselect();
			});
		},

		selectItem: function(/*ListItem*/item){
			// summary:
			//		Selects the given item.
			item.select();
		}
	});
});

},
'dojo/Stateful':function(){
define(["./_base/kernel", "./_base/declare", "./_base/lang", "./_base/array"], function(dojo, declare, lang, array) {
	// module:
	//		dojo/Stateful
	// summary:
	//		TODOC

return dojo.declare("dojo.Stateful", null, {
	// summary:
	//		Base class for objects that provide named properties with optional getter/setter
	//		control and the ability to watch for property changes
	// example:
	//	|	var obj = new dojo.Stateful();
	//	|	obj.watch("foo", function(){
	//	|		console.log("foo changed to " + this.get("foo"));
	//	|	});
	//	|	obj.set("foo","bar");
	postscript: function(mixin){
		if(mixin){
			lang.mixin(this, mixin);
		}
	},

	get: function(/*String*/name){
		// summary:
		//		Get a property on a Stateful instance.
		//	name:
		//		The property to get.
		//	returns:
		//		The property value on this Stateful instance.
		// description:
		//		Get a named property on a Stateful object. The property may
		//		potentially be retrieved via a getter method in subclasses. In the base class
		// 		this just retrieves the object's property.
		// 		For example:
		//	|	stateful = new dojo.Stateful({foo: 3});
		//	|	stateful.get("foo") // returns 3
		//	|	stateful.foo // returns 3

		return this[name]; //Any
	},
	set: function(/*String*/name, /*Object*/value){
		// summary:
		//		Set a property on a Stateful instance
		//	name:
		//		The property to set.
		//	value:
		//		The value to set in the property.
		//	returns:
		//		The function returns this dojo.Stateful instance.
		// description:
		//		Sets named properties on a stateful object and notifies any watchers of
		// 		the property. A programmatic setter may be defined in subclasses.
		// 		For example:
		//	|	stateful = new dojo.Stateful();
		//	|	stateful.watch(function(name, oldValue, value){
		//	|		// this will be called on the set below
		//	|	}
		//	|	stateful.set(foo, 5);
		//
		//	set() may also be called with a hash of name/value pairs, ex:
		//	|	myObj.set({
		//	|		foo: "Howdy",
		//	|		bar: 3
		//	|	})
		//	This is equivalent to calling set(foo, "Howdy") and set(bar, 3)
		if(typeof name === "object"){
			for(var x in name){
				this.set(x, name[x]);
			}
			return this;
		}
		var oldValue = this[name];
		this[name] = value;
		if(this._watchCallbacks){
			this._watchCallbacks(name, oldValue, value);
		}
		return this; //dojo.Stateful
	},
	watch: function(/*String?*/name, /*Function*/callback){
		// summary:
		//		Watches a property for changes
		//	name:
		//		Indicates the property to watch. This is optional (the callback may be the
		// 		only parameter), and if omitted, all the properties will be watched
		// returns:
		//		An object handle for the watch. The unwatch method of this object
		// 		can be used to discontinue watching this property:
		//		|	var watchHandle = obj.watch("foo", callback);
		//		|	watchHandle.unwatch(); // callback won't be called now
		//	callback:
		//		The function to execute when the property changes. This will be called after
		//		the property has been changed. The callback will be called with the |this|
		//		set to the instance, the first argument as the name of the property, the
		// 		second argument as the old value and the third argument as the new value.

		var callbacks = this._watchCallbacks;
		if(!callbacks){
			var self = this;
			callbacks = this._watchCallbacks = function(name, oldValue, value, ignoreCatchall){
				var notify = function(propertyCallbacks){
					if(propertyCallbacks){
                        propertyCallbacks = propertyCallbacks.slice();
						for(var i = 0, l = propertyCallbacks.length; i < l; i++){
							try{
								propertyCallbacks[i].call(self, name, oldValue, value);
							}catch(e){
								console.error(e);
							}
						}
					}
				};
				notify(callbacks['_' + name]);
				if(!ignoreCatchall){
					notify(callbacks["*"]); // the catch-all
				}
			}; // we use a function instead of an object so it will be ignored by JSON conversion
		}
		if(!callback && typeof name === "function"){
			callback = name;
			name = "*";
		}else{
			// prepend with dash to prevent name conflicts with function (like "name" property)
			name = '_' + name;
		}
		var propertyCallbacks = callbacks[name];
		if(typeof propertyCallbacks !== "object"){
			propertyCallbacks = callbacks[name] = [];
		}
		propertyCallbacks.push(callback);
		return {
			unwatch: function(){
				propertyCallbacks.splice(array.indexOf(propertyCallbacks, callback), 1);
			}
		}; //Object
	}

});

});

},
'dojox/mobile/app/List':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dojo/string,dijit/_WidgetBase"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.List");
dojo.experimental("dojox.mobile.app.List");

dojo.require("dojo.string");
dojo.require("dijit._WidgetBase");

(function(){

	var templateCache = {};

	dojo.declare("dojox.mobile.app.List", dijit._WidgetBase, {
		// summary:
		//		A templated list widget. Given a simple array of data objects
		//		and a HTML template, it renders a list of elements, with
		//		support for a swipe delete action.  An optional template
		//		can be provided for when the list is empty.

		// items: Array
		//    The array of data items that will be rendered.
		items: null,

		// itemTemplate: String
		//		The URL to the HTML file containing the markup for each individual
		//		data item.
		itemTemplate: "",

		// emptyTemplate: String
		//		The URL to the HTML file containing the HTML to display if there
		//		are no data items. This is optional.
		emptyTemplate: "",

		//  dividerTemplate: String
		//    The URL to the HTML file containing the markup for the dividers
		//    between groups of list items
		dividerTemplate: "",

		// dividerFunction: Function
		//    Function to create divider elements. This should return a divider
		//    value for each item in the list
		dividerFunction: null,

		// labelDelete: String
		//		The label to display for the Delete button
		labelDelete: "Delete",

		// labelCancel: String
		//		The label to display for the Cancel button
		labelCancel: "Cancel",

		// controller: Object
		//
		controller: null,

		// autoDelete: Boolean
		autoDelete: true,

		// enableDelete: Boolean
		enableDelete: true,

		// enableHold: Boolean
		enableHold: true,

		// formatters: Object
		//		A name/value map of functions used to format data for display
		formatters: null,

		// _templateLoadCount: Number
		//		The number of templates remaining to load before the list renders.
		_templateLoadCount: 0,

		// _mouseDownPos: Object
		//    The coordinates of where a mouseDown event was detected
		_mouseDownPos: null,

		baseClass: "list",

		constructor: function(){
			this._checkLoadComplete = dojo.hitch(this, this._checkLoadComplete);
			this._replaceToken = dojo.hitch(this, this._replaceToken);
			this._postDeleteAnim = dojo.hitch(this, this._postDeleteAnim);
		},

		postCreate: function(){

			var _this = this;

			if(this.emptyTemplate){
				this._templateLoadCount++;
			}
			if(this.itemTemplate){
				this._templateLoadCount++;
			}
			if(this.dividerTemplate){
				this._templateLoadCount++;
			}

			this.connect(this.domNode, "onmousedown", function(event){
				var touch = event;
				if(event.targetTouches && event.targetTouches.length > 0){
					touch = event.targetTouches[0];
				}

				// Find the node that was tapped/clicked
				var rowNode = _this._getRowNode(event.target);

				if(rowNode){
					// Add the rows data to the event so it can be picked up
					// by any listeners
					_this._setDataInfo(rowNode, event);

					// Select and highlight the row
					_this._selectRow(rowNode);

					// Record the position that was tapped
					_this._mouseDownPos = {
						x: touch.pageX,
						y: touch.pageY
					};
					_this._dragThreshold = null;
				}
			});

			this.connect(this.domNode, "onmouseup", function(event){
				// When the mouse/finger comes off the list,
				// call the onSelect function and deselect the row.
				if(event.targetTouches && event.targetTouches.length > 0){
					event = event.targetTouches[0];
				}
				var rowNode = _this._getRowNode(event.target);

				if(rowNode){

					_this._setDataInfo(rowNode, event);

					if(_this._selectedRow){
						_this.onSelect(rowNode._data, rowNode._idx, rowNode);
					}

					this._deselectRow();
				}
			});

			// If swipe-to-delete is enabled, listen for the mouse moving
			if(this.enableDelete){
				this.connect(this.domNode, "mousemove", function(event){
					dojo.stopEvent(event);
					if(!_this._selectedRow){
						return;
					}
					var rowNode = _this._getRowNode(event.target);

					// Still check for enableDelete in case it's changed after
					// this listener is added.
					if(_this.enableDelete && rowNode && !_this._deleting){
						_this.handleDrag(event);
					}
				});
			}

			// Put the data and index onto each onclick event.
			this.connect(this.domNode, "onclick", function(event){
				if(event.touches && event.touches.length > 0){
					event = event.touches[0];
				}
				var rowNode = _this._getRowNode(event.target, true);

				if(rowNode){
					_this._setDataInfo(rowNode, event);
				}
			});

			// If the mouse or finger moves off the selected row,
			// deselect it.
			this.connect(this.domNode, "mouseout", function(event){
				if(event.touches && event.touches.length > 0){
					event = event.touches[0];
				}
				if(event.target == _this._selectedRow){
					_this._deselectRow();
				}
			});

			// If no item template has been provided, it is an error.
			if(!this.itemTemplate){
				throw Error("An item template must be provided to " + this.declaredClass);
			}

			// Load the item template
			this._loadTemplate(this.itemTemplate, "itemTemplate", this._checkLoadComplete);

			if(this.emptyTemplate){
				// If the optional empty template has been provided, load it.
				this._loadTemplate(this.emptyTemplate, "emptyTemplate", this._checkLoadComplete);
			}

			if(this.dividerTemplate){
				this._loadTemplate(this.dividerTemplate, "dividerTemplate", this._checkLoadComplete);
			}
		},

		handleDrag: function(event){
			// summary:
			//		Handles rows being swiped for deletion.
			var touch = event;
			if(event.targetTouches && event.targetTouches.length > 0){
				touch = event.targetTouches[0];
			}

			// Get the distance that the mouse or finger has moved since
			// beginning the swipe action.
			var diff = touch.pageX - this._mouseDownPos.x;

			var absDiff = Math.abs(diff);
			if(absDiff > 10 && !this._dragThreshold){
				// Make the user drag the row 60% of the width to remove it
				this._dragThreshold = dojo.marginBox(this._selectedRow).w * 0.6;
				if(!this.autoDelete){
					this.createDeleteButtons(this._selectedRow);
				}
			}

			this._selectedRow.style.left = (absDiff > 10 ? diff : 0) + "px";

			// If the user has dragged the row more than the threshold, slide
			// it off the screen in preparation for deletion.
			if(this._dragThreshold && this._dragThreshold < absDiff){
				this.preDelete(diff);
			}
		},

		handleDragCancel: function(){
			// summary:
			//		Handle a drag action being cancelled, for whatever reason.
			//		Reset handles, remove CSS classes etc.
			if(this._deleting){
				return;
			}
			dojo.removeClass(this._selectedRow, "hold");
			this._selectedRow.style.left = 0;
			this._mouseDownPos = null;
			this._dragThreshold = null;

			this._deleteBtns && dojo.style(this._deleteBtns, "display", "none");
		},

		preDelete: function(currentLeftPos){
			// summary:
			//    Slides the row offscreen before it is deleted

			// TODO: do this with CSS3!
			var self = this;

			this._deleting = true;

			dojo.animateProperty({
				node: this._selectedRow,
				duration: 400,
				properties: {
					left: {
					end: currentLeftPos +
						((currentLeftPos > 0 ? 1 : -1) * this._dragThreshold * 0.8)
					}
				},
				onEnd: dojo.hitch(this, function(){
					if(this.autoDelete){
						this.deleteRow(this._selectedRow);
					}
				})
			}).play();
		},

		deleteRow: function(row){

			// First make the row invisible
			// Put it back where it came from
			dojo.style(row, {
				visibility: "hidden",
				minHeight: "0px"
			});
			dojo.removeClass(row, "hold");

			this._deleteAnimConn =
				this.connect(row, "webkitAnimationEnd", this._postDeleteAnim);

			dojo.addClass(row, "collapsed");
		},

		_postDeleteAnim: function(event){
			// summary:
			//		Completes the deletion of a row.

			if(this._deleteAnimConn){
				this.disconnect(this._deleteAnimConn);
				this._deleteAnimConn = null;
			}

			var row = this._selectedRow;
			var sibling = row.nextSibling;
			var prevSibling = row.previousSibling;

			// If the previous node is a divider and either this is
			// the last element in the list, or the next node is
			// also a divider, remove the divider for the deleted section.
			if(prevSibling && prevSibling._isDivider){
				if(!sibling || sibling._isDivider){
					prevSibling.parentNode.removeChild(prevSibling);
				}
			}

			row.parentNode.removeChild(row);
			this.onDelete(row._data, row._idx, this.items);

			// Decrement the index of each following row
			while(sibling){
				if(sibling._idx){
					sibling._idx--;
				}
				sibling = sibling.nextSibling;
			}

			dojo.destroy(row);

			// Fix up the 'first' and 'last' CSS classes on the rows
			dojo.query("> *:not(.buttons)", this.domNode).forEach(this.applyClass);

			this._deleting = false;
			this._deselectRow();
		},

		createDeleteButtons: function(aroundNode){
			// summary:
			//		Creates the two buttons displayed when confirmation is
			//		required before deletion of a row.
			// aroundNode:
			//		The DOM node of the row about to be deleted.
			var mb = dojo.marginBox(aroundNode);
			var pos = dojo._abs(aroundNode, true);

			if(!this._deleteBtns){
			// Create the delete buttons.
				this._deleteBtns = dojo.create("div",{
					"class": "buttons"
				}, this.domNode);

				this.buttons = [];

				this.buttons.push(new dojox.mobile.Button({
					btnClass: "mblRedButton",
					label: this.labelDelete
				}));
				this.buttons.push(new dojox.mobile.Button({
					btnClass: "mblBlueButton",
					label: this.labelCancel
				}));

				dojo.place(this.buttons[0].domNode, this._deleteBtns);
				dojo.place(this.buttons[1].domNode, this._deleteBtns);

				dojo.addClass(this.buttons[0].domNode, "deleteBtn");
				dojo.addClass(this.buttons[1].domNode, "cancelBtn");

				this._handleButtonClick = dojo.hitch(this._handleButtonClick);
				this.connect(this._deleteBtns, "onclick", this._handleButtonClick);
			}
			dojo.removeClass(this._deleteBtns, "fade out fast");
			dojo.style(this._deleteBtns, {
				display: "",
				width: mb.w + "px",
				height: mb.h + "px",
				top: (aroundNode.offsetTop) + "px",
				left: "0px"
			});
		},

		onDelete: function(data, index, array){
			// summary:
			//    Called when a row is deleted
			// data:
			//		The data related to the row being deleted
			// index:
			//		The index of the data in the total array
			// array:
			//		The array of data used.

			array.splice(index, 1);

			// If the data is empty, rerender in case an emptyTemplate has
			// been provided
			if(array.length < 1){
				this.render();
			}
		},

		cancelDelete: function(){
			// summary:
			//		Cancels the deletion of a row.
			this._deleting = false;
			this.handleDragCancel();
		},

		_handleButtonClick: function(event){
			// summary:
			//		Handles the click of one of the deletion buttons, either to
			//		delete the row or to cancel the deletion.
			if(event.touches && event.touches.length > 0){
				event = event.touches[0];
			}
			var node = event.target;
			if(dojo.hasClass(node, "deleteBtn")){
				this.deleteRow(this._selectedRow);
			}else if(dojo.hasClass(node, "cancelBtn")){
				this.cancelDelete();
			}else{
				return;
			}
			dojo.addClass(this._deleteBtns, "fade out");
		},

		applyClass: function(node, idx, array){
			// summary:
			//		Applies the 'first' and 'last' CSS classes to the relevant
			//		rows.

			dojo.removeClass(node, "first last");
			if(idx == 0){
				dojo.addClass(node, "first");
			}
			if(idx == array.length - 1){
				dojo.addClass(node, "last");
			}
		},

		_setDataInfo: function(rowNode, event){
			// summary:
			//    Attaches the data item and index for each row to any event
			//    that occurs on that row.
			event.item = rowNode._data;
			event.index = rowNode._idx;
		},

		onSelect: function(data, index, rowNode){
			// summary:
			//		Dummy function that is called when a row is tapped
		},

		_selectRow: function(row){
			// summary:
			//		Selects a row, applies the relevant CSS classes.
			if(this._deleting && this._selectedRow && row != this._selectedRow){
				this.cancelDelete();
			}

			if(!dojo.hasClass(row, "row")){
				return;
			}
			if(this.enableHold || this.enableDelete){
				dojo.addClass(row, "hold");
			}
			this._selectedRow = row;
		},

		_deselectRow: function(){
			// summary:
			//		Deselects a row, and cancels any drag actions that were
			//		occurring.
			if(!this._selectedRow || this._deleting){
				return;
			}
			this.handleDragCancel();
			dojo.removeClass(this._selectedRow, "hold");
			this._selectedRow = null;
		},

		_getRowNode: function(fromNode, ignoreNoClick){
			// summary:
			//		Gets the DOM node of the row that is equal to or the parent
			//		of the node passed to this function.
			while(fromNode && !fromNode._data && fromNode != this.domNode){
				if(!ignoreNoClick && dojo.hasClass(fromNode, "noclick")){
					return null;
				}
				fromNode = fromNode.parentNode;
			}
			return fromNode == this.domNode ? null : fromNode;
		},

		applyTemplate: function(template, data){
			return dojo._toDom(dojo.string.substitute(
					template, data, this._replaceToken, this.formatters || this));
		},

		render: function(){
			// summary:
			//		Renders the list.

			// Delete all existing nodes, except the deletion buttons.
			dojo.query("> *:not(.buttons)", this.domNode).forEach(dojo.destroy);

			// If there is no data, and an empty template has been provided,
			// render it.
			if(this.items.length < 1 && this.emptyTemplate){
				dojo.place(dojo._toDom(this.emptyTemplate), this.domNode, "first");
			}else{
				this.domNode.appendChild(this._renderRange(0, this.items.length));
			}
			if(dojo.hasClass(this.domNode.parentNode, "mblRoundRect")){
				dojo.addClass(this.domNode.parentNode, "mblRoundRectList")
			}

			var divs = dojo.query("> .row", this.domNode);
			if(divs.length > 0){
				dojo.addClass(divs[0], "first");
				dojo.addClass(divs[divs.length - 1], "last");
			}
		},

		_renderRange: function(startIdx, endIdx){

			var rows = [];
			var row, i;
			var frag = document.createDocumentFragment();
			startIdx = Math.max(0, startIdx);
			endIdx = Math.min(endIdx, this.items.length);

			for(i = startIdx; i < endIdx; i++){
				// Create a document fragment containing the templated row
				row = this.applyTemplate(this.itemTemplate, this.items[i]);
				dojo.addClass(row, 'row');
				row._data = this.items[i];
				row._idx = i;
				rows.push(row);
			}
			if(!this.dividerFunction || !this.dividerTemplate){
				for(i = startIdx; i < endIdx; i++){
					rows[i]._data = this.items[i];
					rows[i]._idx = i;
					frag.appendChild(rows[i]);
				}
			}else{
				var prevDividerValue = null;
				var dividerValue;
				var divider;
				for(i = startIdx; i < endIdx; i++){
					rows[i]._data = this.items[i];
					rows[i]._idx = i;

					dividerValue = this.dividerFunction(this.items[i]);
					if(dividerValue && dividerValue != prevDividerValue){
						divider = this.applyTemplate(this.dividerTemplate, {
							label: dividerValue,
							item: this.items[i]
						});
						divider._isDivider = true;
						frag.appendChild(divider);
						prevDividerValue = dividerValue;
					}
					frag.appendChild(rows[i]);
				}
			}
			return frag;
		},

		_replaceToken: function(value, key){
			if(key.charAt(0) == '!'){ value = dojo.getObject(key.substr(1), false, _this); }
			if(typeof value == "undefined"){ return ""; } // a debugging aide
			if(value == null){ return ""; }

			// Substitution keys beginning with ! will skip the transform step,
			// in case a user wishes to insert unescaped markup, e.g. ${!foo}
			return key.charAt(0) == "!" ? value :
				// Safer substitution, see heading "Attribute values" in
				// http://www.w3.org/TR/REC-html40/appendix/notes.html#h-B.3.2
				value.toString().replace(/"/g,"&quot;"); //TODO: add &amp? use encodeXML method?

		},

		_checkLoadComplete: function(){
			// summary:
			//		Checks if all templates have loaded
			this._templateLoadCount--;

			if(this._templateLoadCount < 1 && this.get("items")){
				this.render();
			}
		},

		_loadTemplate: function(url, thisAttr, callback){
			// summary:
			//		Loads a template
			if(!url){
				callback();
				return;
			}

			if(templateCache[url]){
				this.set(thisAttr, templateCache[url]);
				callback();
			}else{
				var _this = this;

				dojo.xhrGet({
					url: url,
					sync: false,
					handleAs: "text",
					load: function(text){
						templateCache[url] = dojo.trim(text);
						_this.set(thisAttr, templateCache[url]);
						callback();
					}
				});
			}
		},


		_setFormattersAttr: function(formatters){
			// summary:
			//    Sets the data items, and causes a rerender of the list
			this.formatters = formatters;
		},

		_setItemsAttr: function(items){
			// summary:
			//    Sets the data items, and causes a rerender of the list

			this.items = items || [];

			if(this._templateLoadCount < 1 && items){
				this.render();
			}
		},

		destroy: function(){
			if(this.buttons){
				dojo.forEach(this.buttons, function(button){
					button.destroy();
				});
				this.buttons = null;
			}

			this.inherited(arguments);
		}

	});

})();
});

},
'dojox/mobile/app/ListSelector':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/app/_Widget,dojo/fx"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.ListSelector");
dojo.experimental("dojox.mobile.app.ListSelector");

dojo.require("dojox.mobile.app._Widget");
dojo.require("dojo.fx");

dojo.declare("dojox.mobile.app.ListSelector", dojox.mobile.app._Widget, {

	// data: Array
	//    The array of items to display.  Each element in the array
	//    should have both a label and value attribute, e.g.
	//    [{label: "Open", value: 1} , {label: "Delete", value: 2}]
	data: null,

	// controller: Object
	//    The current SceneController widget.
	controller: null,

	// onChoose: Function
	//    The callback function for when an item is selected
	onChoose: null,

	destroyOnHide: false,

	_setDataAttr: function(data){
		this.data = data;
	
		if(this.data){
			this.render();
		}
	},

	postCreate: function(){
		dojo.addClass(this.domNode, "listSelector");
	
		var _this = this;
	
		this.connect(this.domNode, "onclick", function(event){
			if(!dojo.hasClass(event.target, "listSelectorRow")){
				return;
			}
	
			if(_this.onChoose){
				_this.onChoose(_this.data[event.target._idx].value);
			}
			_this.hide();
		});

		this.connect(this.domNode, "onmousedown", function(event){
			if(!dojo.hasClass(event.target, "listSelectorRow")){
				return;
			}
			dojo.addClass(event.target, "listSelectorRow-selected");
		});

		this.connect(this.domNode, "onmouseup", function(event){
			if(!dojo.hasClass(event.target, "listSelectorRow")){
				return;
			}
			dojo.removeClass(event.target, "listSelectorRow-selected");
		});

		this.connect(this.domNode, "onmouseout", function(event){
			if(!dojo.hasClass(event.target, "listSelectorRow")){
				return;
			}
			dojo.removeClass(event.target, "listSelectorRow-selected");
		});
	
		var viewportSize = this.controller.getWindowSize();
	
		this.mask = dojo.create("div", {"class": "dialogUnderlayWrapper",
			innerHTML: "<div class=\"dialogUnderlay\"></div>"
		}, this.controller.assistant.domNode);
	
		this.connect(this.mask, "onclick", function(){
			_this.onChoose && _this.onChoose();
			_this.hide();
		});
	},

	show: function(fromNode){

		// Using dojo.fx here. Must figure out how to do this with CSS animations!!
		var startPos;
	
		var windowSize = this.controller.getWindowSize();
		var fromNodePos;
		if(fromNode){
			fromNodePos = dojo._abs(fromNode);
			startPos = fromNodePos;
		}else{
			startPos.x = windowSize.w / 2;
			startPos.y = 200;
		}
		console.log("startPos = ", startPos);
	
		dojo.style(this.domNode, {
			opacity: 0,
			display: "",
			width: Math.floor(windowSize.w * 0.8) + "px"
		});
	
		var maxWidth = 0;
		dojo.query(">", this.domNode).forEach(function(node){
			dojo.style(node, {
				"float": "left"
			});
			maxWidth = Math.max(maxWidth, dojo.marginBox(node).w);
			dojo.style(node, {
				"float": "none"
			});
		});
		maxWidth = Math.min(maxWidth, Math.round(windowSize.w * 0.8))
					+ dojo.style(this.domNode, "paddingLeft")
					+ dojo.style(this.domNode, "paddingRight")
					+ 1;
	
		dojo.style(this.domNode, "width", maxWidth + "px");
		var targetHeight = dojo.marginBox(this.domNode).h;
	
		var _this = this;
	
	
		var targetY = fromNodePos ?
				Math.max(30, fromNodePos.y - targetHeight - 10) :
				this.getScroll().y + 30;
	
		console.log("fromNodePos = ", fromNodePos, " targetHeight = ", targetHeight,
				" targetY = " + targetY, " startPos ", startPos);
	
	
		var anim1 = dojo.animateProperty({
			node: this.domNode,
			duration: 400,
			properties: {
				width: {start: 1, end: maxWidth},
				height: {start: 1, end: targetHeight},
				top: {start: startPos.y, end: targetY},
				left: {start: startPos.x, end: (windowSize.w/2 - maxWidth/2)},
				opacity: {start: 0, end: 1},
				fontSize: {start: 1}
			},
			onEnd: function(){
				dojo.style(_this.domNode, "width", "inherit");
			}
		});
		var anim2 = dojo.fadeIn({
			node: this.mask,
			duration: 400
		});
		dojo.fx.combine([anim1, anim2]).play();

	},

	hide: function(){
		// Using dojo.fx here. Must figure out how to do this with CSS animations!!
	
		var _this = this;
	
		var anim1 = dojo.animateProperty({
			node: this.domNode,
			duration: 500,
			properties: {
				width: {end: 1},
				height: {end: 1},
				opacity: {end: 0},
				fontSize: {end: 1}
			},
			onEnd: function(){
				if(_this.get("destroyOnHide")){
					_this.destroy();
				}
			}
		});
	
		var anim2 = dojo.fadeOut({
			node: this.mask,
			duration: 400
		});
		dojo.fx.combine([anim1, anim2]).play();
	},

	render: function(){
		// summary:
		//    Renders
	
		dojo.empty(this.domNode);
		dojo.style(this.domNode, "opacity", 0);
	
		var row;
	
		for(var i = 0; i < this.data.length; i++){
			// Create each row and add any custom classes. Also set the _idx property.
			row = dojo.create("div", {
				"class": "listSelectorRow " + (this.data[i].className || ""),
				innerHTML: this.data[i].label
			}, this.domNode);
	
			row._idx = i;
	
			if(i == 0){
				dojo.addClass(row, "first");
			}
			if(i == this.data.length - 1){
				dojo.addClass(row, "last");
			}
	
		}
	},


	destroy: function(){
		this.inherited(arguments);
		dojo.destroy(this.mask);
	}

});

});

},
'dojox/mobile/EdgeToEdgeCategory':function(){
define("dojox/mobile/EdgeToEdgeCategory", [
	"dojo/_base/declare",
	"./RoundRectCategory"
], function(declare, RoundRectCategory){

/*=====
	var RoundRectCategory = dojox.mobile.RoundRectCategory;
=====*/

	// module:
	//		dojox/mobile/EdgeToEdgeCategory
	// summary:
	//		A category header for an edge-to-edge list.

	return declare("dojox.mobile.EdgeToEdgeCategory", RoundRectCategory, {
		// summary:
		//		A category header for an edge-to-edge list.
		buildRendering: function(){
			this.inherited(arguments);
			this.domNode.className = "mblEdgeToEdgeCategory";
		}
	});
});

},
'dojo/string':function(){
define(["./_base/kernel", "./_base/lang"], function(dojo, lang) {
	// module:
	//		dojo/string
	// summary:
	//		TODOC

lang.getObject("string", true, dojo);

/*=====
dojo.string = {
	// summary: String utilities for Dojo
};
=====*/

dojo.string.rep = function(/*String*/str, /*Integer*/num){
	// summary:
	//		Efficiently replicate a string `n` times.
	// str:
	//		the string to replicate
	// num:
	//		number of times to replicate the string

	if(num <= 0 || !str){ return ""; }

	var buf = [];
	for(;;){
		if(num & 1){
			buf.push(str);
		}
		if(!(num >>= 1)){ break; }
		str += str;
	}
	return buf.join("");	// String
};

dojo.string.pad = function(/*String*/text, /*Integer*/size, /*String?*/ch, /*Boolean?*/end){
	// summary:
	//		Pad a string to guarantee that it is at least `size` length by
	//		filling with the character `ch` at either the start or end of the
	//		string. Pads at the start, by default.
	// text:
	//		the string to pad
	// size:
	//		length to provide padding
	// ch:
	//		character to pad, defaults to '0'
	// end:
	//		adds padding at the end if true, otherwise pads at start
	// example:
	//	|	// Fill the string to length 10 with "+" characters on the right.  Yields "Dojo++++++".
	//	|	dojo.string.pad("Dojo", 10, "+", true);

	if(!ch){
		ch = '0';
	}
	var out = String(text),
		pad = dojo.string.rep(ch, Math.ceil((size - out.length) / ch.length));
	return end ? out + pad : pad + out;	// String
};

dojo.string.substitute = function(	/*String*/		template,
									/*Object|Array*/map,
									/*Function?*/	transform,
									/*Object?*/		thisObject){
	// summary:
	//		Performs parameterized substitutions on a string. Throws an
	//		exception if any parameter is unmatched.
	// template:
	//		a string with expressions in the form `${key}` to be replaced or
	//		`${key:format}` which specifies a format function. keys are case-sensitive.
	// map:
	//		hash to search for substitutions
	// transform:
	//		a function to process all parameters before substitution takes
	//		place, e.g. mylib.encodeXML
	// thisObject:
	//		where to look for optional format function; default to the global
	//		namespace
	// example:
	//		Substitutes two expressions in a string from an Array or Object
	//	|	// returns "File 'foo.html' is not found in directory '/temp'."
	//	|	// by providing substitution data in an Array
	//	|	dojo.string.substitute(
	//	|		"File '${0}' is not found in directory '${1}'.",
	//	|		["foo.html","/temp"]
	//	|	);
	//	|
	//	|	// also returns "File 'foo.html' is not found in directory '/temp'."
	//	|	// but provides substitution data in an Object structure.  Dotted
	//	|	// notation may be used to traverse the structure.
	//	|	dojo.string.substitute(
	//	|		"File '${name}' is not found in directory '${info.dir}'.",
	//	|		{ name: "foo.html", info: { dir: "/temp" } }
	//	|	);
	// example:
	//		Use a transform function to modify the values:
	//	|	// returns "file 'foo.html' is not found in directory '/temp'."
	//	|	dojo.string.substitute(
	//	|		"${0} is not found in ${1}.",
	//	|		["foo.html","/temp"],
	//	|		function(str){
	//	|			// try to figure out the type
	//	|			var prefix = (str.charAt(0) == "/") ? "directory": "file";
	//	|			return prefix + " '" + str + "'";
	//	|		}
	//	|	);
	// example:
	//		Use a formatter
	//	|	// returns "thinger -- howdy"
	//	|	dojo.string.substitute(
	//	|		"${0:postfix}", ["thinger"], null, {
	//	|			postfix: function(value, key){
	//	|				return value + " -- howdy";
	//	|			}
	//	|		}
	//	|	);

	thisObject = thisObject || dojo.global;
	transform = transform ?
		lang.hitch(thisObject, transform) : function(v){ return v; };

	return template.replace(/\$\{([^\s\:\}]+)(?:\:([^\s\:\}]+))?\}/g,
		function(match, key, format){
			var value = lang.getObject(key, false, map);
			if(format){
				value = lang.getObject(format, false, thisObject).call(thisObject, value, key);
			}
			return transform(value, key).toString();
		}); // String
};

/*=====
dojo.string.trim = function(str){
	// summary:
	//		Trims whitespace from both sides of the string
	// str: String
	//		String to be trimmed
	// returns: String
	//		Returns the trimmed string
	// description:
	//		This version of trim() was taken from [Steven Levithan's blog](http://blog.stevenlevithan.com/archives/faster-trim-javascript).
	//		The short yet performant version of this function is dojo.trim(),
	//		which is part of Dojo base.  Uses String.prototype.trim instead, if available.
	return "";	// String
}
=====*/

dojo.string.trim = String.prototype.trim ?
	lang.trim : // aliasing to the native function
	function(str){
		str = str.replace(/^\s+/, '');
		for(var i = str.length - 1; i >= 0; i--){
			if(/\S/.test(str.charAt(i))){
				str = str.substring(0, i + 1);
				break;
			}
		}
		return str;
	};

return dojo.string;
});

},
'dojox/mobile/TextBox':function(){
define("dojox/mobile/TextBox", [
	"dojo/_base/declare",
	"dojo/dom-construct",
	"dijit/_WidgetBase",
	"dijit/form/_FormValueMixin",
	"dijit/form/_TextBoxMixin"
], function(declare, domConstruct, WidgetBase, FormValueMixin, TextBoxMixin){

	/*=====
		WidgetBase = dijit._WidgetBase;
		FormValueMixin = dijit.form._FormValueMixin;
		TextBoxMixin = dijit.form._TextBoxMixin;
	=====*/
	return declare("dojox.mobile.TextBox",[WidgetBase, FormValueMixin, TextBoxMixin],{
		// summary:
		//		A non-templated base class for textbox form inputs

		baseClass: "mblTextBox",

		// Override automatic assigning type --> node, it causes exception on IE8.
		// Instead, type must be specified as this.type when the node is created, as part of the original DOM
		_setTypeAttr: null,

		// Map widget attributes to DOMNode attributes.
		_setPlaceHolderAttr: "textbox",

		buildRendering: function(){
			if(!this.srcNodeRef){
				this.srcNodeRef = domConstruct.create("input", {"type":this.type});
			}
			this.inherited(arguments);
			this.textbox = this.focusNode = this.domNode;
		},

		postCreate: function(){
			this.inherited(arguments);
			this.connect(this.textbox, "onfocus", "_onFocus");
			this.connect(this.textbox, "onblur", "_onBlur");
		}
	});
});

},
'dijit/registry':function(){
define("dijit/registry", [
	"dojo/_base/array", // array.forEach array.map
	"dojo/_base/sniff", // has("ie")
	"dojo/_base/unload", // unload.addOnWindowUnload
	"dojo/_base/window", // win.body
	"."	// dijit._scopeName
], function(array, has, unload, win, dijit){

	// module:
	//		dijit/registry
	// summary:
	//		Registry of existing widget on page, plus some utility methods.
	//		Must be accessed through AMD api, ex:
	//		require(["dijit/registry"], function(registry){ registry.byId("foo"); })

	var _widgetTypeCtr = {}, hash = {};

	var registry =  {
		// summary:
		//		A set of widgets indexed by id

		length: 0,

		add: function(/*dijit._Widget*/ widget){
			// summary:
			//		Add a widget to the registry. If a duplicate ID is detected, a error is thrown.
			//
			// widget: dijit._Widget
			//		Any dijit._Widget subclass.
			if(hash[widget.id]){
				throw new Error("Tried to register widget with id==" + widget.id + " but that id is already registered");
			}
			hash[widget.id] = widget;
			this.length++;
		},

		remove: function(/*String*/ id){
			// summary:
			//		Remove a widget from the registry. Does not destroy the widget; simply
			//		removes the reference.
			if(hash[id]){
				delete hash[id];
				this.length--;
			}
		},

		byId: function(/*String|Widget*/ id){
			// summary:
			//		Find a widget by it's id.
			//		If passed a widget then just returns the widget.
			return typeof id == "string" ? hash[id] : id;	// dijit._Widget
		},

		byNode: function(/*DOMNode*/ node){
			// summary:
			//		Returns the widget corresponding to the given DOMNode
			return hash[node.getAttribute("widgetId")]; // dijit._Widget
		},

		toArray: function(){
			// summary:
			//		Convert registry into a true Array
			//
			// example:
			//		Work with the widget .domNodes in a real Array
			//		|	array.map(dijit.registry.toArray(), function(w){ return w.domNode; });

			var ar = [];
			for(var id in hash){
				ar.push(hash[id]);
			}
			return ar;	// dijit._Widget[]
		},

		getUniqueId: function(/*String*/widgetType){
			// summary:
			//		Generates a unique id for a given widgetType

			var id;
			do{
				id = widgetType + "_" +
					(widgetType in _widgetTypeCtr ?
						++_widgetTypeCtr[widgetType] : _widgetTypeCtr[widgetType] = 0);
			}while(hash[id]);
			return dijit._scopeName == "dijit" ? id : dijit._scopeName + "_" + id; // String
		},

		findWidgets: function(/*DomNode*/ root){
			// summary:
			//		Search subtree under root returning widgets found.
			//		Doesn't search for nested widgets (ie, widgets inside other widgets).

			var outAry = [];

			function getChildrenHelper(root){
				for(var node = root.firstChild; node; node = node.nextSibling){
					if(node.nodeType == 1){
						var widgetId = node.getAttribute("widgetId");
						if(widgetId){
							var widget = hash[widgetId];
							if(widget){	// may be null on page w/multiple dojo's loaded
								outAry.push(widget);
							}
						}else{
							getChildrenHelper(node);
						}
					}
				}
			}

			getChildrenHelper(root);
			return outAry;
		},

		_destroyAll: function(){
			// summary:
			//		Code to destroy all widgets and do other cleanup on page unload

			// Clean up focus manager lingering references to widgets and nodes
			dijit._curFocus = null;
			dijit._prevFocus = null;
			dijit._activeStack = [];

			// Destroy all the widgets, top down
			array.forEach(registry.findWidgets(win.body()), function(widget){
				// Avoid double destroy of widgets like Menu that are attached to <body>
				// even though they are logically children of other widgets.
				if(!widget._destroyed){
					if(widget.destroyRecursive){
						widget.destroyRecursive();
					}else if(widget.destroy){
						widget.destroy();
					}
				}
			});
		},

		getEnclosingWidget: function(/*DOMNode*/ node){
			// summary:
			//		Returns the widget whose DOM tree contains the specified DOMNode, or null if
			//		the node is not contained within the DOM tree of any widget
			while(node){
				var id = node.getAttribute && node.getAttribute("widgetId");
				if(id){
					return hash[id];
				}
				node = node.parentNode;
			}
			return null;
		},

		// In case someone needs to access hash.
		// Actually, this is accessed from WidgetSet back-compatibility code
		_hash: hash
	};

	if(has("ie")){
		// Only run _destroyAll() for IE because we think it's only necessary in that case,
		// and because it causes problems on FF.  See bug #3531 for details.
		unload.addOnWindowUnload(function(){
			registry._destroyAll();
		});
	}

	/*=====
	dijit.registry = {
		// summary:
		//		A list of widgets on a page.
	};
	=====*/
	dijit.registry = registry;

	return registry;
});

},
'dijit/_base/manager':function(){
define("dijit/_base/manager", [
	"dojo/_base/array",
	"dojo/_base/config", // defaultDuration
	"../registry",
	".."	// for setting exports to dijit namespace
], function(array, config, registry, dijit){

	// module:
	//		dijit/_base/manager
	// summary:
	//		Shim to methods on registry, plus a few other declarations.
	//		New code should access dijit/registry directly when possible.

	/*=====
	dijit.byId = function(id){
		// summary:
		//		Returns a widget by it's id, or if passed a widget, no-op (like dom.byId())
		// id: String|dijit._Widget
		return registry.byId(id); // dijit._Widget
	};

	dijit.getUniqueId = function(widgetType){
		// summary:
		//		Generates a unique id for a given widgetType
		// widgetType: String
		return registry.getUniqueId(widgetType); // String
	};

	dijit.findWidgets = function(root){
		// summary:
		//		Search subtree under root returning widgets found.
		//		Doesn't search for nested widgets (ie, widgets inside other widgets).
		// root: DOMNode
		return registry.findWidgets(root);
	};

	dijit._destroyAll = function(){
		// summary:
		//		Code to destroy all widgets and do other cleanup on page unload

		return registry._destroyAll();
	};

	dijit.byNode = function(node){
		// summary:
		//		Returns the widget corresponding to the given DOMNode
		// node: DOMNode
		return registry.byNode(node); // dijit._Widget
	};

	dijit.getEnclosingWidget = function(node){
		// summary:
		//		Returns the widget whose DOM tree contains the specified DOMNode, or null if
		//		the node is not contained within the DOM tree of any widget
		// node: DOMNode
		return registry.getEnclosingWidget(node);
	};
	=====*/
	array.forEach(["byId", "getUniqueId", "findWidgets", "_destroyAll", "byNode", "getEnclosingWidget"], function(name){
		dijit[name] = registry[name];
	});

	/*=====
	dojo.mixin(dijit, {
		// defaultDuration: Integer
		//		The default fx.animation speed (in ms) to use for all Dijit
		//		transitional fx.animations, unless otherwise specified
		//		on a per-instance basis. Defaults to 200, overrided by
		//		`djConfig.defaultDuration`
		defaultDuration: 200
	});
	=====*/
	dijit.defaultDuration = config["defaultDuration"] || 200;

	return dijit;
});

},
'dijit/_base/place':function(){
define("dijit/_base/place", [
	"dojo/_base/array", // array.forEach
	"dojo/_base/lang", // lang.isArray
	"dojo/window", // windowUtils.getBox
	"../place",
	".."	// export to dijit namespace
], function(array, lang, windowUtils, place, dijit){

	// module:
	//		dijit/_base/place
	// summary:
	//		Back compatibility module, new code should use dijit/place directly instead of using this module.

	dijit.getViewport = function(){
		// summary:
		//		Deprecated method to return the dimensions and scroll position of the viewable area of a browser window.
		//		New code should use windowUtils.getBox()

		return windowUtils.getBox();
	};

	/*=====
	dijit.placeOnScreen = function(node, pos, corners, padding){
		// summary:
		//		Positions one of the node's corners at specified position
		//		such that node is fully visible in viewport.
		//		Deprecated, new code should use dijit.place.at() instead.
	};
	=====*/
	dijit.placeOnScreen = place.at;

	/*=====
	dijit.placeOnScreenAroundElement = function(node, aroundElement, aroundCorners, layoutNode){
		// summary:
		//		Like dijit.placeOnScreenAroundNode(), except it accepts an arbitrary object
		//		for the "around" argument and finds a proper processor to place a node.
		//		Deprecated, new code should use dijit.place.around() instead.
	};
	====*/
	dijit.placeOnScreenAroundElement = function(node, aroundNode, aroundCorners, layoutNode){
		// Convert old style {"BL": "TL", "BR": "TR"} type argument
		// to style needed by dijit.place code:
		//		[
		// 			{aroundCorner: "BL", corner: "TL" },
		//			{aroundCorner: "BR", corner: "TR" }
		//		]
		var positions;
		if(lang.isArray(aroundCorners)){
			positions = aroundCorners;
		}else{
			positions = [];
			for(var key in aroundCorners){
				positions.push({aroundCorner: key, corner: aroundCorners[key]});
			}
		}

		return place.around(node, aroundNode, positions, true, layoutNode);
	};

	/*=====
	dijit.placeOnScreenAroundNode = function(node, aroundNode, aroundCorners, layoutNode){
		// summary:
		//		Position node adjacent or kitty-corner to aroundNode
		//		such that it's fully visible in viewport.
		//		Deprecated, new code should use dijit.place.around() instead.
	};
	=====*/
	dijit.placeOnScreenAroundNode = dijit.placeOnScreenAroundElement;

	/*=====
	dijit.placeOnScreenAroundRectangle = function(node, aroundRect, aroundCorners, layoutNode){
		// summary:
		//		Like dijit.placeOnScreenAroundNode(), except that the "around"
		//		parameter is an arbitrary rectangle on the screen (x, y, width, height)
		//		instead of a dom node.
		//		Deprecated, new code should use dijit.place.around() instead.
	};
	=====*/
	dijit.placeOnScreenAroundRectangle = dijit.placeOnScreenAroundElement;

	dijit.getPopupAroundAlignment = function(/*Array*/ position, /*Boolean*/ leftToRight){
		// summary:
		//		Deprecated method, unneeded when using dijit/place directly.
		//		Transforms the passed array of preferred positions into a format suitable for
		//		passing as the aroundCorners argument to dijit.placeOnScreenAroundElement.
		//
		// position: String[]
		//		This variable controls the position of the drop down.
		//		It's an array of strings with the following values:
		//
		//			* before: places drop down to the left of the target node/widget, or to the right in
		//			  the case of RTL scripts like Hebrew and Arabic
		//			* after: places drop down to the right of the target node/widget, or to the left in
		//			  the case of RTL scripts like Hebrew and Arabic
		//			* above: drop down goes above target node
		//			* below: drop down goes below target node
		//
		//		The list is positions is tried, in order, until a position is found where the drop down fits
		//		within the viewport.
		//
		// leftToRight: Boolean
		//		Whether the popup will be displaying in leftToRight mode.
		//
		var align = {};
		array.forEach(position, function(pos){
			var ltr = leftToRight;
			switch(pos){
				case "after":
					align[leftToRight ? "BR" : "BL"] = leftToRight ? "BL" : "BR";
					break;
				case "before":
					align[leftToRight ? "BL" : "BR"] = leftToRight ? "BR" : "BL";
					break;
				case "below-alt":
					ltr = !ltr;
					// fall through
				case "below":
					// first try to align left borders, next try to align right borders (or reverse for RTL mode)
					align[ltr ? "BL" : "BR"] = ltr ? "TL" : "TR";
					align[ltr ? "BR" : "BL"] = ltr ? "TR" : "TL";
					break;
				case "above-alt":
					ltr = !ltr;
					// fall through
				case "above":
				default:
					// first try to align left borders, next try to align right borders (or reverse for RTL mode)
					align[ltr ? "TL" : "TR"] = ltr ? "BL" : "BR";
					align[ltr ? "TR" : "TL"] = ltr ? "BR" : "BL";
					break;
			}
		});
		return align;
	};

	return dijit;
});

},
'dojox/mobile/View':function(){
define("dojox/mobile/View", [
	"dojo/_base/kernel", // to test dojo.hash
	"dojo/_base/array",
	"dojo/_base/config",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/sniff",
	"dojo/_base/window",
	"dojo/_base/Deferred",
	"dojo/dom",
	"dojo/dom-class",
	"dojo/dom-geometry",
	"dojo/dom-style",
//	"dojo/hash", // optionally prereq'ed
	"dijit/registry",	// registry.byNode
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase",
	"./ViewController", // to load ViewController for you (no direct references)
	"./transition"
], function(dojo, array, config, connect, declare, lang, has, win, Deferred, dom, domClass, domGeometry, domStyle, registry, Contained, Container, WidgetBase, ViewController, transitDeferred){

/*=====
	var Contained = dijit._Contained;
	var Container = dijit._Container;
	var WidgetBase = dijit._WidgetBase;
	var ViewController = dojox.mobile.ViewController;
=====*/

	// module:
	//		dojox/mobile/View
	// summary:
	//		A widget that represents a view that occupies the full screen

	var dm = lang.getObject("dojox.mobile", true);

	return declare("dojox.mobile.View", [WidgetBase, Container, Contained], {
		// summary:
		//		A widget that represents a view that occupies the full screen
		// description:
		//		View acts as a container for any HTML and/or widgets. An entire
		//		HTML page can have multiple View widgets and the user can
		//		navigate through the views back and forth without page
		//		transitions.
	
		// selected: Boolean
		//		If true, the view is displayed at startup time.
		selected: false,

		// keepScrollPos: Boolean
		//		If true, the scroll position is kept between views.
		keepScrollPos: true,
	
		constructor: function(params, node){
			if(node){
				dom.byId(node).style.visibility = "hidden";
			}
			this._aw = has("android") >= 2.2 && has("android") < 3; // flag for android animation workaround
		},
	
		buildRendering: function(){
			this.domNode = this.containerNode = this.srcNodeRef || win.doc.createElement("DIV");
			this.domNode.className = "mblView";
			this.connect(this.domNode, "webkitAnimationEnd", "onAnimationEnd");
			this.connect(this.domNode, "webkitAnimationStart", "onAnimationStart");
			if(!config['mblCSS3Transition']){
			    this.connect(this.domNode, "webkitTransitionEnd", "onAnimationEnd");
			}
			var id = location.href.match(/#(\w+)([^\w=]|$)/) ? RegExp.$1 : null;
	
			this._visible = this.selected && !id || this.id == id;
	
			if(this.selected){
				dm._defaultView = this;
			}
		},

		startup: function(){
			if(this._started){ return; }
			var siblings = [];
			var children = this.domNode.parentNode.childNodes;
			var visible = false;
			// check if a visible view exists
			for(var i = 0; i < children.length; i++){
				var c = children[i];
				if(c.nodeType === 1 && domClass.contains(c, "mblView")){
					siblings.push(c);
					visible = visible || registry.byNode(c)._visible;
				}
			}
			var _visible = this._visible;
			// if no visible view exists, make the first view visible
			if(siblings.length === 1 || (!visible && siblings[0] === this.domNode)){
				_visible = true;
			}
			var _this = this;
			setTimeout(function(){ // necessary to render the view correctly
				if(!_visible){
					_this.domNode.style.display = "none";
				}else{
					dm.currentView = _this; //TODO:1.8 reconsider this. currentView may not have a currently showing view when views are nested.
					_this.onStartView();
					connect.publish("/dojox/mobile/startView", [_this]);
				}
				if(_this.domNode.style.visibility != "visible"){ // this check is to avoid screen flickers
					_this.domNode.style.visibility = "visible";
				}
				var parent = _this.getParent && _this.getParent();
				if(!parent || !parent.resize){ // top level widget
					_this.resize();
				}
			}, has("ie") ? 100 : 0); // give IE a little time to complete drawing
			this.inherited(arguments);
		},
	
		resize: function(){
			// summary:
			//		Calls resize() of each child widget.
			array.forEach(this.getChildren(), function(child){
				if(child.resize){ child.resize(); }
			});
		},

		onStartView: function(){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called only when this view is shown at startup time.
		},
	
		onBeforeTransitionIn: function(moveTo, dir, transition, context, method){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called before the arriving transition occurs.
		},
	
		onAfterTransitionIn: function(moveTo, dir, transition, context, method){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called after the arriving transition occurs.
		},
	
		onBeforeTransitionOut: function(moveTo, dir, transition, context, method){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called before the leaving transition occurs.
		},
	
		onAfterTransitionOut: function(moveTo, dir, transition, context, method){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called after the leaving transition occurs.
		},
	
		_saveState: function(moveTo, dir, transition, context, method){
			this._context = context;
			this._method = method;
			if(transition == "none"){
				transition = null;
			}
			this._moveTo = moveTo;
			this._dir = dir;
			this._transition = transition;
			this._arguments = lang._toArray(arguments);
			this._args = [];
			if(context || method){
				for(var i = 5; i < arguments.length; i++){
					this._args.push(arguments[i]);
				}
			}
		},
		
		_fixViewState: function(/*DomNode*/toNode){
			// summary:
			//		Sanity check for view transition states.
			// description:
			//		Sometimes uninitialization of Views fails after making view transition,
			//		and that results in failure of subsequent view transitions.
			//		This function does the uninitialization for all the sibling views.
			var nodes = this.domNode.parentNode.childNodes;
			for(var i = 0; i < nodes.length; i++){
				var n = nodes[i];
				if(n.nodeType === 1 && domClass.contains(n, "mblView")){
					n.className = "mblView"; //TODO: Should remove classes one by one. This would clear user defined classes or even mblScrollableView.
				}
			}
			toNode.className = "mblView"; // just in case toNode is a sibling of an ancestor.
		},
	
		convertToId: function(moveTo){
			if(typeof(moveTo) == "string"){
				// removes a leading hash mark (#) and params if exists
				// ex. "#bar&myParam=0003" -> "bar"
				moveTo.match(/^#?([^&?]+)/);
				return RegExp.$1;
			}
			return moveTo;
		},
	
		performTransition: function(/*String*/moveTo, /*Number*/dir, /*String*/transition,
									/*Object|null*/context, /*String|Function*/method /*optional args*/){
			// summary:
			//		Function to perform the various types of view transitions, such as fade, slide, and flip.
			// moveTo: String
			//		The id of the transition destination view which resides in
			//		the current page.
			//		If the value has a hash sign ('#') before the id
			//		(e.g. #view1) and the dojo.hash module is loaded by the user
			//		application, the view transition updates the hash in the
			//		browser URL so that the user can bookmark the destination
			//		view. In this case, the user can also use the browser's
			//		back/forward button to navigate through the views in the
			//		browser history.
			//		If null, transitions to a blank view.
			//		If '#', returns immediately without transition.
			// dir: Number
			//		The transition direction. If 1, transition forward. If -1, transition backward.
			//		For example, the slide transition slides the view from right to left when dir == 1,
			//		and from left to right when dir == -1.
			// transition: String
			//		A type of animated transition effect. You can choose from
			//		the standard transition types, "slide", "fade", "flip", or
			//		from the extended transition types, "cover", "coverv",
			//		"dissolve", "reveal", "revealv", "scaleIn",
			//		"scaleOut", "slidev", "swirl", "zoomIn", "zoomOut". If
			//		"none" is specified, transition occurs immediately without
			//		animation.
			// context: Object
			//		The object that the callback function will receive as "this".
			// method: String|Function
			//		A callback function that is called when the transition has been finished.
			//		A function reference, or name of a function in context.
			// tags:
			//		public
			//
			// example:
			//		Transition backward to a view whose id is "foo" with the slide animation.
			//	|	performTransition("foo", -1, "slide");
			//
			// example:
			//		Transition forward to a blank view, and then open another page.
			//	|	performTransition(null, 1, "slide", null, function(){location.href = href;});
			if(moveTo === "#"){ return; }
			if(dojo.hash){
				if(typeof(moveTo) == "string" && moveTo.charAt(0) == '#' && !dm._params){
					dm._params = [];
					for(var i = 0; i < arguments.length; i++){
						dm._params.push(arguments[i]);
					}
					dojo.hash(moveTo);
					return;
				}
			}
			this._saveState.apply(this, arguments);
			var toNode;
			if(moveTo){
				toNode = this.convertToId(moveTo);
			}else{
				if(!this._dummyNode){
					this._dummyNode = win.doc.createElement("DIV");
					win.body().appendChild(this._dummyNode);
				}
				toNode = this._dummyNode;
			}
			var fromNode = this.domNode;
			var fromTop = fromNode.offsetTop;
			toNode = this.toNode = dom.byId(toNode);
			if(!toNode){ console.log("dojox.mobile.View#performTransition: destination view not found: "+moveTo); return; }
			toNode.style.visibility = this._aw ? "visible" : "hidden";
			toNode.style.display = "";
			this._fixViewState(toNode);
			var toWidget = registry.byNode(toNode);
			if(toWidget){
				// Now that the target view became visible, it's time to run resize()
				if(config["mblAlwaysResizeOnTransition"] || !toWidget._resized){
					dm.resizeAll(null, toWidget);
					toWidget._resized = true;
				}
	
				if(transition && transition != "none"){
					// Temporarily add padding to align with the fromNode while transition
					toWidget.containerNode.style.paddingTop = fromTop + "px";
				}

				toWidget.movedFrom = fromNode.id;
			}
	
			this.onBeforeTransitionOut.apply(this, arguments);
			connect.publish("/dojox/mobile/beforeTransitionOut", [this].concat(lang._toArray(arguments)));
			if(toWidget){
				// perform view transition keeping the scroll position
				if(this.keepScrollPos && !this.getParent()){
					var scrollTop = win.body().scrollTop || win.doc.documentElement.scrollTop || win.global.pageYOffset || 0;
					fromNode._scrollTop = scrollTop;
					var toTop = (dir == 1) ? 0 : (toNode._scrollTop || 0);
					toNode.style.top = "0px";
					if(scrollTop > 1 || toTop !== 0){
						fromNode.style.top = toTop - scrollTop + "px";
						if(config["mblHideAddressBar"] !== false){
							setTimeout(function(){ // iPhone needs setTimeout
								win.global.scrollTo(0, (toTop || 1));
							}, 0);
						}
					}
				}else{
					toNode.style.top = "0px";
				}
				toWidget.onBeforeTransitionIn.apply(toWidget, arguments);
				connect.publish("/dojox/mobile/beforeTransitionIn", [toWidget].concat(lang._toArray(arguments)));
			}
			if(!this._aw){
				toNode.style.display = "none";
				toNode.style.visibility = "visible";
			}
			
			if(dm._iw && dm.scrollable){ // Workaround for iPhone flicker issue (only when scrollable.js is loaded)
				var ss = dm.getScreenSize();
				// Show cover behind the view.
				// cover's z-index is set to -10000, lower than z-index value specified in transition css.
				win.body().appendChild(dm._iwBgCover);
				domStyle.set(dm._iwBgCover, {
					position: "absolute",
					top: "0px",
					left: "0px",
					height: (ss.h + 1) + "px", // "+1" means the height of scrollTo(0,1)
					width: ss.w + "px",
					backgroundColor: domStyle.get(win.body(), "background-color"),
					zIndex: -10000,
					display: ""
				});
				// Show toNode behind the cover.
				domStyle.set(toNode, {
					position: "absolute",
					zIndex: -10001,
					visibility: "visible",
					display: ""
				});
				// setTimeout seems to be necessary to avoid flicker.
				// Also the duration of setTimeout should be long enough to avoid flicker.
				// 0 is not effective. 50 sometimes causes flicker.
				setTimeout(lang.hitch(this, function(){
					this._doTransition(fromNode, toNode, transition, dir);
				}), 80);
			}else{
				this._doTransition(fromNode, toNode, transition, dir);
			}
		},
		_toCls: function(s){
			// convert from transition name to corresponding class name
			// ex. "slide" -> "mblSlide"
			return "mbl"+s.charAt(0).toUpperCase() + s.substring(1);
		},
	
		_doTransition: function(fromNode, toNode, transition, dir){
			var rev = (dir == -1) ? " mblReverse" : "";
			if(dm._iw && dm.scrollable){ // Workaround for iPhone flicker issue (only when scrollable.js is loaded)
				// Show toNode after flicker ends
				domStyle.set(toNode, {
					position: "",
					zIndex: ""
				});
				// Remove cover
				win.body().removeChild(dm._iwBgCover);
			}else if(!this._aw){
				toNode.style.display = "";
			}
			if(!transition || transition == "none"){
				this.domNode.style.display = "none";
				this.invokeCallback();
			}else if(config['mblCSS3Transition']){
				//get dojox/css3/transit first
				Deferred.when(transitDeferred, lang.hitch(this, function(transit){
					//follow the style of .mblView.mblIn in View.css
					//need to set the toNode to absolute position
					var toPosition = domStyle.get(toNode, "position");
					domStyle.set(toNode, "position", "absolute");
					Deferred.when(transit(fromNode, toNode, {transition: transition, reverse: (dir===-1)?true:false}),lang.hitch(this,function(){
						domStyle.set(toNode, "position", toPosition);
						this.invokeCallback();
					}));
				}));
			}else{
				var s = this._toCls(transition);
				domClass.add(fromNode, s + " mblOut" + rev);
				domClass.add(toNode, s + " mblIn" + rev);
				setTimeout(function(){
					domClass.add(fromNode, "mblTransition");
					domClass.add(toNode, "mblTransition");
				}, 100);
				// set transform origin
				var fromOrigin = "50% 50%";
				var toOrigin = "50% 50%";
				var scrollTop, posX, posY;
				if(transition.indexOf("swirl") != -1 || transition.indexOf("zoom") != -1){
					if(this.keepScrollPos && !this.getParent()){
						scrollTop = win.body().scrollTop || win.doc.documentElement.scrollTop || win.global.pageYOffset || 0;
					}else{
						scrollTop = -domGeometry.position(fromNode, true).y;
					}
					posY = win.global.innerHeight / 2 + scrollTop;
					fromOrigin = "50% " + posY + "px";
					toOrigin = "50% " + posY + "px";
				}else if(transition.indexOf("scale") != -1){
					var viewPos = domGeometry.position(fromNode, true);
					posX = ((this.clickedPosX !== undefined) ? this.clickedPosX : win.global.innerWidth / 2) - viewPos.x;
					if(this.keepScrollPos && !this.getParent()){
						scrollTop = win.body().scrollTop || win.doc.documentElement.scrollTop || win.global.pageYOffset || 0;
					}else{
						scrollTop = -viewPos.y;
					}
					posY = ((this.clickedPosY !== undefined) ? this.clickedPosY : win.global.innerHeight / 2) + scrollTop;
					fromOrigin = posX + "px " + posY + "px";
					toOrigin = posX + "px " + posY + "px";
				}
				domStyle.set(fromNode, {webkitTransformOrigin:fromOrigin});
				domStyle.set(toNode, {webkitTransformOrigin:toOrigin});
			}
			dm.currentView = registry.byNode(toNode);
		},
	
		onAnimationStart: function(e){
		},


		onAnimationEnd: function(e){
			var name = e.animationName || e.target.className;
			if(name.indexOf("Out") === -1 &&
				name.indexOf("In") === -1 &&
				name.indexOf("Shrink") === -1){ return; }
			var isOut = false;
			if(domClass.contains(this.domNode, "mblOut")){
				isOut = true;
				this.domNode.style.display = "none";
				domClass.remove(this.domNode, [this._toCls(this._transition), "mblIn", "mblOut", "mblReverse"]);
			}else{
				// Reset the temporary padding
				this.containerNode.style.paddingTop = "";
			}
			domStyle.set(this.domNode, {webkitTransformOrigin:""});
			if(name.indexOf("Shrink") !== -1){
				var li = e.target;
				li.style.display = "none";
				domClass.remove(li, "mblCloseContent");
			}
			if(isOut){
				this.invokeCallback();
			}
			// this.domNode may be destroyed as a result of invoking the callback,
			// so check for that before accessing it.
			this.domNode && (this.domNode.className = "mblView");

			// clear the clicked position
			this.clickedPosX = this.clickedPosY = undefined;
		},

		invokeCallback: function(){
			this.onAfterTransitionOut.apply(this, this._arguments);
			connect.publish("/dojox/mobile/afterTransitionOut", [this].concat(this._arguments));
			var toWidget = registry.byNode(this.toNode);
			if(toWidget){
				toWidget.onAfterTransitionIn.apply(toWidget, this._arguments);
				connect.publish("/dojox/mobile/afterTransitionIn", [toWidget].concat(this._arguments));
				toWidget.movedFrom = undefined;
			}

			var c = this._context, m = this._method;
			if(!c && !m){ return; }
			if(!m){
				m = c;
				c = null;
			}
			c = c || win.global;
			if(typeof(m) == "string"){
				c[m].apply(c, this._args);
			}else{
				m.apply(c, this._args);
			}
		},
	
		getShowingView: function(){
			// summary:
			//		Find the currently showing view from my sibling views.
			// description:
			//		Note that dojox.mobile.currentView is the last shown view.
			//		If the page consists of a splitter, there are multiple showing views.
			var nodes = this.domNode.parentNode.childNodes;
			for(var i = 0; i < nodes.length; i++){
				var n = nodes[i];
				if(n.nodeType === 1 && domClass.contains(n, "mblView") && domStyle.get(n, "display") !== "none"){
					return registry.byNode(n);
				}
			}
			return null;
		},
	
		show: function(){
			// summary:
			//		Shows this view without a transition animation.
			var view = this.getShowingView();
			if(view){
				view.domNode.style.display = "none"; // from-style
			}
			this.domNode.style.display = ""; // to-style
			dm.currentView = this;
		}
	});
});

},
'dijit/WidgetSet':function(){
define("dijit/WidgetSet", [
	"dojo/_base/array", // array.forEach array.map
	"dojo/_base/declare", // declare
	"dojo/_base/window", // win.global
	"./registry"	// to add functions to dijit.registry
], function(array, declare, win, registry){

	// module:
	//		dijit/WidgetSet
	// summary:
	//		Legacy registry code.   New modules should just use registry.
	//		Will be removed in 2.0.

	var WidgetSet = declare("dijit.WidgetSet", null, {
		// summary:
		//		A set of widgets indexed by id. A default instance of this class is
		//		available as `dijit.registry`
		//
		// example:
		//		Create a small list of widgets:
		//		|	var ws = new dijit.WidgetSet();
		//		|	ws.add(dijit.byId("one"));
		//		| 	ws.add(dijit.byId("two"));
		//		|	// destroy both:
		//		|	ws.forEach(function(w){ w.destroy(); });
		//
		// example:
		//		Using dijit.registry:
		//		|	dijit.registry.forEach(function(w){ /* do something */ });

		constructor: function(){
			this._hash = {};
			this.length = 0;
		},

		add: function(/*dijit._Widget*/ widget){
			// summary:
			//		Add a widget to this list. If a duplicate ID is detected, a error is thrown.
			//
			// widget: dijit._Widget
			//		Any dijit._Widget subclass.
			if(this._hash[widget.id]){
				throw new Error("Tried to register widget with id==" + widget.id + " but that id is already registered");
			}
			this._hash[widget.id] = widget;
			this.length++;
		},

		remove: function(/*String*/ id){
			// summary:
			//		Remove a widget from this WidgetSet. Does not destroy the widget; simply
			//		removes the reference.
			if(this._hash[id]){
				delete this._hash[id];
				this.length--;
			}
		},

		forEach: function(/*Function*/ func, /* Object? */thisObj){
			// summary:
			//		Call specified function for each widget in this set.
			//
			// func:
			//		A callback function to run for each item. Is passed the widget, the index
			//		in the iteration, and the full hash, similar to `array.forEach`.
			//
			// thisObj:
			//		An optional scope parameter
			//
			// example:
			//		Using the default `dijit.registry` instance:
			//		|	dijit.registry.forEach(function(widget){
			//		|		console.log(widget.declaredClass);
			//		|	});
			//
			// returns:
			//		Returns self, in order to allow for further chaining.

			thisObj = thisObj || win.global;
			var i = 0, id;
			for(id in this._hash){
				func.call(thisObj, this._hash[id], i++, this._hash);
			}
			return this;	// dijit.WidgetSet
		},

		filter: function(/*Function*/ filter, /* Object? */thisObj){
			// summary:
			//		Filter down this WidgetSet to a smaller new WidgetSet
			//		Works the same as `array.filter` and `NodeList.filter`
			//
			// filter:
			//		Callback function to test truthiness. Is passed the widget
			//		reference and the pseudo-index in the object.
			//
			// thisObj: Object?
			//		Option scope to use for the filter function.
			//
			// example:
			//		Arbitrary: select the odd widgets in this list
			//		|	dijit.registry.filter(function(w, i){
			//		|		return i % 2 == 0;
			//		|	}).forEach(function(w){ /* odd ones */ });

			thisObj = thisObj || win.global;
			var res = new WidgetSet(), i = 0, id;
			for(id in this._hash){
				var w = this._hash[id];
				if(filter.call(thisObj, w, i++, this._hash)){
					res.add(w);
				}
			}
			return res; // dijit.WidgetSet
		},

		byId: function(/*String*/ id){
			// summary:
			//		Find a widget in this list by it's id.
			// example:
			//		Test if an id is in a particular WidgetSet
			//		| var ws = new dijit.WidgetSet();
			//		| ws.add(dijit.byId("bar"));
			//		| var t = ws.byId("bar") // returns a widget
			//		| var x = ws.byId("foo"); // returns undefined

			return this._hash[id];	// dijit._Widget
		},

		byClass: function(/*String*/ cls){
			// summary:
			//		Reduce this widgetset to a new WidgetSet of a particular `declaredClass`
			//
			// cls: String
			//		The Class to scan for. Full dot-notated string.
			//
			// example:
			//		Find all `dijit.TitlePane`s in a page:
			//		|	dijit.registry.byClass("dijit.TitlePane").forEach(function(tp){ tp.close(); });

			var res = new WidgetSet(), id, widget;
			for(id in this._hash){
				widget = this._hash[id];
				if(widget.declaredClass == cls){
					res.add(widget);
				}
			 }
			 return res; // dijit.WidgetSet
		},

		toArray: function(){
			// summary:
			//		Convert this WidgetSet into a true Array
			//
			// example:
			//		Work with the widget .domNodes in a real Array
			//		|	array.map(dijit.registry.toArray(), function(w){ return w.domNode; });

			var ar = [];
			for(var id in this._hash){
				ar.push(this._hash[id]);
			}
			return ar;	// dijit._Widget[]
		},

		map: function(/* Function */func, /* Object? */thisObj){
			// summary:
			//		Create a new Array from this WidgetSet, following the same rules as `array.map`
			// example:
			//		|	var nodes = dijit.registry.map(function(w){ return w.domNode; });
			//
			// returns:
			//		A new array of the returned values.
			return array.map(this.toArray(), func, thisObj); // Array
		},

		every: function(func, thisObj){
			// summary:
			// 		A synthetic clone of `array.every` acting explicitly on this WidgetSet
			//
			// func: Function
			//		A callback function run for every widget in this list. Exits loop
			//		when the first false return is encountered.
			//
			// thisObj: Object?
			//		Optional scope parameter to use for the callback

			thisObj = thisObj || win.global;
			var x = 0, i;
			for(i in this._hash){
				if(!func.call(thisObj, this._hash[i], x++, this._hash)){
					return false; // Boolean
				}
			}
			return true; // Boolean
		},

		some: function(func, thisObj){
			// summary:
			// 		A synthetic clone of `array.some` acting explicitly on this WidgetSet
			//
			// func: Function
			//		A callback function run for every widget in this list. Exits loop
			//		when the first true return is encountered.
			//
			// thisObj: Object?
			//		Optional scope parameter to use for the callback

			thisObj = thisObj || win.global;
			var x = 0, i;
			for(i in this._hash){
				if(func.call(thisObj, this._hash[i], x++, this._hash)){
					return true; // Boolean
				}
			}
			return false; // Boolean
		}

	});

	// Add in 1.x compatibility methods to dijit.registry.
	// These functions won't show up in the API doc but since they are deprecated anyway,
	// that's probably for the best.
	array.forEach(["forEach", "filter", "byClass", "map", "every", "some"], function(func){
		registry[func] = WidgetSet.prototype[func];
	});


	return WidgetSet;
});

},
'dojo/fx/easing':function(){
define(["../_base/lang"], function(lang) {
// module:
//		dojo/fx/easing
// summary:
//		This module defines standard easing functions that are useful for animations.

var easingFuncs = /*===== dojo.fx.easing= =====*/ {
	// summary:
	//		Collection of easing functions to use beyond the default
	//		`dojo._defaultEasing` function.
	//
	// description:
	//
	//		Easing functions are used to manipulate the iteration through
	//		an `dojo.Animation`s _Line. _Line being the properties of an Animation,
	//		and the easing function progresses through that Line determing
	//		how quickly (or slowly) it should go. Or more accurately: modify
	//		the value of the _Line based on the percentage of animation completed.
	//
	//		All functions follow a simple naming convention of "ease type" + "when".
	//		If the name of the function ends in Out, the easing described appears
	//		towards the end of the animation. "In" means during the beginning,
	//		and InOut means both ranges of the Animation will applied, both
	//		beginning and end.
	//
	//		One does not call the easing function directly, it must be passed to
	//		the `easing` property of an animation.
	//
	//	example:
	//	|	dojo.require("dojo.fx.easing");
	//	|	var anim = dojo.fadeOut({
	//	|		node: 'node',
	//	|		duration: 2000,
	//	|		//	note there is no ()
	//	|		easing: dojo.fx.easing.quadIn
	//	|	}).play();
	//

	linear: function(/* Decimal? */n){
		// summary: A linear easing function
		return n;
	},

	quadIn: function(/* Decimal? */n){
		return Math.pow(n, 2);
	},

	quadOut: function(/* Decimal? */n){
		return n * (n - 2) * -1;
	},

	quadInOut: function(/* Decimal? */n){
		n = n * 2;
		if(n < 1){ return Math.pow(n, 2) / 2; }
		return -1 * ((--n) * (n - 2) - 1) / 2;
	},

	cubicIn: function(/* Decimal? */n){
		return Math.pow(n, 3);
	},

	cubicOut: function(/* Decimal? */n){
		return Math.pow(n - 1, 3) + 1;
	},

	cubicInOut: function(/* Decimal? */n){
		n = n * 2;
		if(n < 1){ return Math.pow(n, 3) / 2; }
		n -= 2;
		return (Math.pow(n, 3) + 2) / 2;
	},

	quartIn: function(/* Decimal? */n){
		return Math.pow(n, 4);
	},

	quartOut: function(/* Decimal? */n){
		return -1 * (Math.pow(n - 1, 4) - 1);
	},

	quartInOut: function(/* Decimal? */n){
		n = n * 2;
		if(n < 1){ return Math.pow(n, 4) / 2; }
		n -= 2;
		return -1 / 2 * (Math.pow(n, 4) - 2);
	},

	quintIn: function(/* Decimal? */n){
		return Math.pow(n, 5);
	},

	quintOut: function(/* Decimal? */n){
		return Math.pow(n - 1, 5) + 1;
	},

	quintInOut: function(/* Decimal? */n){
		n = n * 2;
		if(n < 1){ return Math.pow(n, 5) / 2; }
		n -= 2;
		return (Math.pow(n, 5) + 2) / 2;
	},

	sineIn: function(/* Decimal? */n){
		return -1 * Math.cos(n * (Math.PI / 2)) + 1;
	},

	sineOut: function(/* Decimal? */n){
		return Math.sin(n * (Math.PI / 2));
	},

	sineInOut: function(/* Decimal? */n){
		return -1 * (Math.cos(Math.PI * n) - 1) / 2;
	},

	expoIn: function(/* Decimal? */n){
		return (n == 0) ? 0 : Math.pow(2, 10 * (n - 1));
	},

	expoOut: function(/* Decimal? */n){
		return (n == 1) ? 1 : (-1 * Math.pow(2, -10 * n) + 1);
	},

	expoInOut: function(/* Decimal? */n){
		if(n == 0){ return 0; }
		if(n == 1){ return 1; }
		n = n * 2;
		if(n < 1){ return Math.pow(2, 10 * (n - 1)) / 2; }
		--n;
		return (-1 * Math.pow(2, -10 * n) + 2) / 2;
	},

	circIn: function(/* Decimal? */n){
		return -1 * (Math.sqrt(1 - Math.pow(n, 2)) - 1);
	},

	circOut: function(/* Decimal? */n){
		n = n - 1;
		return Math.sqrt(1 - Math.pow(n, 2));
	},

	circInOut: function(/* Decimal? */n){
		n = n * 2;
		if(n < 1){ return -1 / 2 * (Math.sqrt(1 - Math.pow(n, 2)) - 1); }
		n -= 2;
		return 1 / 2 * (Math.sqrt(1 - Math.pow(n, 2)) + 1);
	},

	backIn: function(/* Decimal? */n){
		// summary:
		//		An easing function that starts away from the target,
		//		and quickly accelerates towards the end value.
		//
		//		Use caution when the easing will cause values to become
		//		negative as some properties cannot be set to negative values.
		var s = 1.70158;
		return Math.pow(n, 2) * ((s + 1) * n - s);
	},

	backOut: function(/* Decimal? */n){
		// summary:
		//		An easing function that pops past the range briefly, and slowly comes back.
		//
		// description:
		//		An easing function that pops past the range briefly, and slowly comes back.
		//
		//		Use caution when the easing will cause values to become negative as some
		//		properties cannot be set to negative values.

		n = n - 1;
		var s = 1.70158;
		return Math.pow(n, 2) * ((s + 1) * n + s) + 1;
	},

	backInOut: function(/* Decimal? */n){
		// summary:
		//		An easing function combining the effects of `backIn` and `backOut`
		//
		// description:
		//		An easing function combining the effects of `backIn` and `backOut`.
		//		Use caution when the easing will cause values to become negative
		//		as some properties cannot be set to negative values.
		var s = 1.70158 * 1.525;
		n = n * 2;
		if(n < 1){ return (Math.pow(n, 2) * ((s + 1) * n - s)) / 2; }
		n-=2;
		return (Math.pow(n, 2) * ((s + 1) * n + s) + 2) / 2;
	},

	elasticIn: function(/* Decimal? */n){
		// summary:
		//		An easing function the elastically snaps from the start value
		//
		// description:
		//		An easing function the elastically snaps from the start value
		//
		//		Use caution when the elasticity will cause values to become negative
		//		as some properties cannot be set to negative values.
		if(n == 0 || n == 1){ return n; }
		var p = .3;
		var s = p / 4;
		n = n - 1;
		return -1 * Math.pow(2, 10 * n) * Math.sin((n - s) * (2 * Math.PI) / p);
	},

	elasticOut: function(/* Decimal? */n){
		// summary:
		//		An easing function that elasticly snaps around the target value,
		//		near the end of the Animation
		//
		// description:
		//		An easing function that elasticly snaps around the target value,
		//		near the end of the Animation
		//
		//		Use caution when the elasticity will cause values to become
		//		negative as some properties cannot be set to negative values.
		if(n==0 || n == 1){ return n; }
		var p = .3;
		var s = p / 4;
		return Math.pow(2, -10 * n) * Math.sin((n - s) * (2 * Math.PI) / p) + 1;
	},

	elasticInOut: function(/* Decimal? */n){
		// summary:
		//		An easing function that elasticly snaps around the value, near
		//		the beginning and end of the Animation.
		//
		// description:
		//		An easing function that elasticly snaps around the value, near
		//		the beginning and end of the Animation.
		//
		//		Use caution when the elasticity will cause values to become
		//		negative as some properties cannot be set to negative values.
		if(n == 0) return 0;
		n = n * 2;
		if(n == 2) return 1;
		var p = .3 * 1.5;
		var s = p / 4;
		if(n < 1){
			n -= 1;
			return -.5 * (Math.pow(2, 10 * n) * Math.sin((n - s) * (2 * Math.PI) / p));
		}
		n -= 1;
		return .5 * (Math.pow(2, -10 * n) * Math.sin((n - s) * (2 * Math.PI) / p)) + 1;
	},

	bounceIn: function(/* Decimal? */n){
		// summary:
		//		An easing function that 'bounces' near the beginning of an Animation
		return (1 - easingFuncs.bounceOut(1 - n)); // Decimal
	},

	bounceOut: function(/* Decimal? */n){
		// summary:
		//		An easing function that 'bounces' near the end of an Animation
		var s = 7.5625;
		var p = 2.75;
		var l;
		if(n < (1 / p)){
			l = s * Math.pow(n, 2);
		}else if(n < (2 / p)){
			n -= (1.5 / p);
			l = s * Math.pow(n, 2) + .75;
		}else if(n < (2.5 / p)){
			n -= (2.25 / p);
			l = s * Math.pow(n, 2) + .9375;
		}else{
			n -= (2.625 / p);
			l = s * Math.pow(n, 2) + .984375;
		}
		return l;
	},

	bounceInOut: function(/* Decimal? */n){
		// summary:
		//		An easing function that 'bounces' at the beginning and end of the Animation
		if(n < 0.5){ return easingFuncs.bounceIn(n * 2) / 2; }
		return (easingFuncs.bounceOut(n * 2 - 1) / 2) + 0.5; // Decimal
	}
};

lang.setObject("dojo.fx.easing", easingFuncs);

return easingFuncs;
});

},
'dijit/a11y':function(){
define("dijit/a11y", [
	"dojo/_base/array", // array.forEach array.map
	"dojo/_base/config", // defaultDuration
	"dojo/_base/declare", // declare
	"dojo/dom",			// dom.byId
	"dojo/dom-attr", // domAttr.attr domAttr.has
	"dojo/dom-style", // style.style
	"dojo/_base/sniff", // has("ie")
	"./_base/manager",	// manager._isElementShown
	"."	// for exporting methods to dijit namespace
], function(array, config, declare, dom, domAttr, domStyle, has, manager, dijit){

	// module:
	//		dijit/a11y
	// summary:
	//		Accessibility utility functions (keyboard, tab stops, etc.)

	var shown = (dijit._isElementShown = function(/*Element*/ elem){
		var s = domStyle.get(elem);
		return (s.visibility != "hidden")
			&& (s.visibility != "collapsed")
			&& (s.display != "none")
			&& (domAttr.get(elem, "type") != "hidden");
	});

	dijit.hasDefaultTabStop = function(/*Element*/ elem){
		// summary:
		//		Tests if element is tab-navigable even without an explicit tabIndex setting

		// No explicit tabIndex setting, need to investigate node type
		switch(elem.nodeName.toLowerCase()){
			case "a":
				// An <a> w/out a tabindex is only navigable if it has an href
				return domAttr.has(elem, "href");
			case "area":
			case "button":
			case "input":
			case "object":
			case "select":
			case "textarea":
				// These are navigable by default
				return true;
			case "iframe":
				// If it's an editor <iframe> then it's tab navigable.
				var body;
				try{
					// non-IE
					var contentDocument = elem.contentDocument;
					if("designMode" in contentDocument && contentDocument.designMode == "on"){
						return true;
					}
					body = contentDocument.body;
				}catch(e1){
					// contentWindow.document isn't accessible within IE7/8
					// if the iframe.src points to a foreign url and this
					// page contains an element, that could get focus
					try{
						body = elem.contentWindow.document.body;
					}catch(e2){
						return false;
					}
				}
				return body && (body.contentEditable == 'true' ||
					(body.firstChild && body.firstChild.contentEditable == 'true'));
			default:
				return elem.contentEditable == 'true';
		}
	};

	var isTabNavigable = (dijit.isTabNavigable = function(/*Element*/ elem){
		// summary:
		//		Tests if an element is tab-navigable

		// TODO: convert (and rename method) to return effective tabIndex; will save time in _getTabNavigable()
		if(domAttr.get(elem, "disabled")){
			return false;
		}else if(domAttr.has(elem, "tabIndex")){
			// Explicit tab index setting
			return domAttr.get(elem, "tabIndex") >= 0; // boolean
		}else{
			// No explicit tabIndex setting, so depends on node type
			return dijit.hasDefaultTabStop(elem);
		}
	});

	dijit._getTabNavigable = function(/*DOMNode*/ root){
		// summary:
		//		Finds descendants of the specified root node.
		//
		// description:
		//		Finds the following descendants of the specified root node:
		//		* the first tab-navigable element in document order
		//		  without a tabIndex or with tabIndex="0"
		//		* the last tab-navigable element in document order
		//		  without a tabIndex or with tabIndex="0"
		//		* the first element in document order with the lowest
		//		  positive tabIndex value
		//		* the last element in document order with the highest
		//		  positive tabIndex value
		var first, last, lowest, lowestTabindex, highest, highestTabindex, radioSelected = {};

		function radioName(node){
			// If this element is part of a radio button group, return the name for that group.
			return node && node.tagName.toLowerCase() == "input" &&
				node.type && node.type.toLowerCase() == "radio" &&
				node.name && node.name.toLowerCase();
		}

		var walkTree = function(/*DOMNode*/parent){
			for(var child = parent.firstChild; child; child = child.nextSibling){
				// Skip text elements, hidden elements, and also non-HTML elements (those in custom namespaces) in IE,
				// since show() invokes getAttribute("type"), which crash on VML nodes in IE.
				if(child.nodeType != 1 || (has("ie") && child.scopeName !== "HTML") || !shown(child)){
					continue;
				}

				if(isTabNavigable(child)){
					var tabindex = domAttr.get(child, "tabIndex");
					if(!domAttr.has(child, "tabIndex") || tabindex == 0){
						if(!first){
							first = child;
						}
						last = child;
					}else if(tabindex > 0){
						if(!lowest || tabindex < lowestTabindex){
							lowestTabindex = tabindex;
							lowest = child;
						}
						if(!highest || tabindex >= highestTabindex){
							highestTabindex = tabindex;
							highest = child;
						}
					}
					var rn = radioName(child);
					if(domAttr.get(child, "checked") && rn){
						radioSelected[rn] = child;
					}
				}
				if(child.nodeName.toUpperCase() != 'SELECT'){
					walkTree(child);
				}
			}
		};
		if(shown(root)){
			walkTree(root);
		}
		function rs(node){
			// substitute checked radio button for unchecked one, if there is a checked one with the same name.
			return radioSelected[radioName(node)] || node;
		}

		return { first: rs(first), last: rs(last), lowest: rs(lowest), highest: rs(highest) };
	};
	dijit.getFirstInTabbingOrder = function(/*String|DOMNode*/ root){
		// summary:
		//		Finds the descendant of the specified root node
		//		that is first in the tabbing order
		var elems = dijit._getTabNavigable(dom.byId(root));
		return elems.lowest ? elems.lowest : elems.first; // DomNode
	};

	dijit.getLastInTabbingOrder = function(/*String|DOMNode*/ root){
		// summary:
		//		Finds the descendant of the specified root node
		//		that is last in the tabbing order
		var elems = dijit._getTabNavigable(dom.byId(root));
		return elems.last ? elems.last : elems.highest; // DomNode
	};

	return {
		hasDefaultTabStop: dijit.hasDefaultTabStop,
		isTabNavigable: dijit.isTabNavigable,
		_getTabNavigable: dijit._getTabNavigable,
		getFirstInTabbingOrder: dijit.getFirstInTabbingOrder,
		getLastInTabbingOrder: dijit.getLastInTabbingOrder
	};
});

},
'dijit/typematic':function(){
define("dijit/typematic", [
	"dojo/_base/array", // array.forEach
	"dojo/_base/connect", // connect.connect
	"dojo/_base/event", // event.stop
	"dojo/_base/kernel", // kernel.deprecated
	"dojo/_base/lang", // lang.mixin, lang.hitch
	"dojo/on",
	"dojo/_base/sniff", // has("ie")
	"."		// setting dijit.typematic global
], function(array, connect, event, kernel, lang, on, has, dijit){

// module:
//		dijit/typematic
// summary:
//		These functions are used to repetitively call a user specified callback
//		method when a specific key or mouse click over a specific DOM node is
//		held down for a specific amount of time.
//		Only 1 such event is allowed to occur on the browser page at 1 time.

var typematic = (dijit.typematic = {
	// summary:
	//		These functions are used to repetitively call a user specified callback
	//		method when a specific key or mouse click over a specific DOM node is
	//		held down for a specific amount of time.
	//		Only 1 such event is allowed to occur on the browser page at 1 time.

	_fireEventAndReload: function(){
		this._timer = null;
		this._callback(++this._count, this._node, this._evt);

		// Schedule next event, timer is at most minDelay (default 10ms) to avoid
		// browser overload (particularly avoiding starving DOH robot so it never gets to send a mouseup)
		this._currentTimeout = Math.max(
			this._currentTimeout < 0 ? this._initialDelay :
				(this._subsequentDelay > 1 ? this._subsequentDelay : Math.round(this._currentTimeout * this._subsequentDelay)),
			this._minDelay);
		this._timer = setTimeout(lang.hitch(this, "_fireEventAndReload"), this._currentTimeout);
	},

	trigger: function(/*Event*/ evt, /*Object*/ _this, /*DOMNode*/ node, /*Function*/ callback, /*Object*/ obj, /*Number*/ subsequentDelay, /*Number*/ initialDelay, /*Number?*/ minDelay){
		// summary:
		//		Start a timed, repeating callback sequence.
		//		If already started, the function call is ignored.
		//		This method is not normally called by the user but can be
		//		when the normal listener code is insufficient.
		// evt:
		//		key or mouse event object to pass to the user callback
		// _this:
		//		pointer to the user's widget space.
		// node:
		//		the DOM node object to pass the the callback function
		// callback:
		//		function to call until the sequence is stopped called with 3 parameters:
		// count:
		//		integer representing number of repeated calls (0..n) with -1 indicating the iteration has stopped
		// node:
		//		the DOM node object passed in
		// evt:
		//		key or mouse event object
		// obj:
		//		user space object used to uniquely identify each typematic sequence
		// subsequentDelay (optional):
		//		if > 1, the number of milliseconds until the 3->n events occur
		//		or else the fractional time multiplier for the next event's delay, default=0.9
		// initialDelay (optional):
		//		the number of milliseconds until the 2nd event occurs, default=500ms
		// minDelay (optional):
		//		the maximum delay in milliseconds for event to fire, default=10ms
		if(obj != this._obj){
			this.stop();
			this._initialDelay = initialDelay || 500;
			this._subsequentDelay = subsequentDelay || 0.90;
			this._minDelay = minDelay || 10;
			this._obj = obj;
			this._evt = evt;
			this._node = node;
			this._currentTimeout = -1;
			this._count = -1;
			this._callback = lang.hitch(_this, callback);
			this._fireEventAndReload();
			this._evt = lang.mixin({faux: true}, evt);
		}
	},

	stop: function(){
		// summary:
		//		Stop an ongoing timed, repeating callback sequence.
		if(this._timer){
			clearTimeout(this._timer);
			this._timer = null;
		}
		if(this._obj){
			this._callback(-1, this._node, this._evt);
			this._obj = null;
		}
	},

	addKeyListener: function(/*DOMNode*/ node, /*Object*/ keyObject, /*Object*/ _this, /*Function*/ callback, /*Number*/ subsequentDelay, /*Number*/ initialDelay, /*Number?*/ minDelay){
		// summary:
		//		Start listening for a specific typematic key.
		//		See also the trigger method for other parameters.
		// keyObject:
		//		an object defining the key to listen for:
		// 		charOrCode:
		//			the printable character (string) or keyCode (number) to listen for.
		// 		keyCode:
		//			(deprecated - use charOrCode) the keyCode (number) to listen for (implies charCode = 0).
		// 		charCode:
		//			(deprecated - use charOrCode) the charCode (number) to listen for.
		// 		ctrlKey:
		//			desired ctrl key state to initiate the callback sequence:
		//			- pressed (true)
		//			- released (false)
		//			- either (unspecified)
		// 		altKey:
		//			same as ctrlKey but for the alt key
		// 		shiftKey:
		//			same as ctrlKey but for the shift key
		// returns:
		//		a connection handle
		if(keyObject.keyCode){
			keyObject.charOrCode = keyObject.keyCode;
			kernel.deprecated("keyCode attribute parameter for dijit.typematic.addKeyListener is deprecated. Use charOrCode instead.", "", "2.0");
		}else if(keyObject.charCode){
			keyObject.charOrCode = String.fromCharCode(keyObject.charCode);
			kernel.deprecated("charCode attribute parameter for dijit.typematic.addKeyListener is deprecated. Use charOrCode instead.", "", "2.0");
		}
		var handles = [
			on(node, connect._keypress, lang.hitch(this, function(evt){
				if(evt.charOrCode == keyObject.charOrCode &&
				(keyObject.ctrlKey === undefined || keyObject.ctrlKey == evt.ctrlKey) &&
				(keyObject.altKey === undefined || keyObject.altKey == evt.altKey) &&
				(keyObject.metaKey === undefined || keyObject.metaKey == (evt.metaKey || false)) && // IE doesn't even set metaKey
				(keyObject.shiftKey === undefined || keyObject.shiftKey == evt.shiftKey)){
					event.stop(evt);
					typematic.trigger(evt, _this, node, callback, keyObject, subsequentDelay, initialDelay, minDelay);
				}else if(typematic._obj == keyObject){
					typematic.stop();
				}
			})),
			on(node, "keyup", lang.hitch(this, function(){
				if(typematic._obj == keyObject){
					typematic.stop();
				}
			}))
		];
		return { remove: function(){ array.forEach(handles, function(h){ h.remove(); }); } };
	},

	addMouseListener: function(/*DOMNode*/ node, /*Object*/ _this, /*Function*/ callback, /*Number*/ subsequentDelay, /*Number*/ initialDelay, /*Number?*/ minDelay){
		// summary:
		//		Start listening for a typematic mouse click.
		//		See the trigger method for other parameters.
		// returns:
		//		a connection handle
		var handles =  [
			on(node, "mousedown", lang.hitch(this, function(evt){
				event.stop(evt);
				typematic.trigger(evt, _this, node, callback, node, subsequentDelay, initialDelay, minDelay);
			})),
			on(node, "mouseup", lang.hitch(this, function(evt){
				if(this._obj){
					event.stop(evt);
				}
				typematic.stop();
			})),
			on(node, "mouseout", lang.hitch(this, function(evt){
				event.stop(evt);
				typematic.stop();
			})),
			on(node, "mousemove", lang.hitch(this, function(evt){
				evt.preventDefault();
			})),
			on(node, "dblclick", lang.hitch(this, function(evt){
				event.stop(evt);
				if(has("ie")){
					typematic.trigger(evt, _this, node, callback, node, subsequentDelay, initialDelay, minDelay);
					setTimeout(lang.hitch(this, typematic.stop), 50);
				}
			}))
		];
		return { remove: function(){ array.forEach(handles, function(h){ h.remove(); }); } };
	},

	addListener: function(/*Node*/ mouseNode, /*Node*/ keyNode, /*Object*/ keyObject, /*Object*/ _this, /*Function*/ callback, /*Number*/ subsequentDelay, /*Number*/ initialDelay, /*Number?*/ minDelay){
		// summary:
		//		Start listening for a specific typematic key and mouseclick.
		//		This is a thin wrapper to addKeyListener and addMouseListener.
		//		See the addMouseListener and addKeyListener methods for other parameters.
		// mouseNode:
		//		the DOM node object to listen on for mouse events.
		// keyNode:
		//		the DOM node object to listen on for key events.
		// returns:
		//		a connection handle
		var handles = [
			this.addKeyListener(keyNode, keyObject, _this, callback, subsequentDelay, initialDelay, minDelay),
			this.addMouseListener(mouseNode, _this, callback, subsequentDelay, initialDelay, minDelay)
		];
		return { remove: function(){ array.forEach(handles, function(h){ h.remove(); }); } };
	}
});

return typematic;

});

},
'dojox/mobile/app/ImageView':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/app/_Widget,dojo/fx/easing"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.ImageView");
dojo.experimental("dojox.mobile.app.ImageView");
dojo.require("dojox.mobile.app._Widget");

dojo.require("dojo.fx.easing");

dojo.declare("dojox.mobile.app.ImageView", dojox.mobile.app._Widget, {

	// zoom: Number
	//		The current level of zoom.  This should not be set manually.
	zoom: 1,

	// zoomCenterX: Number
	//		The X coordinate in the image where the zoom is focused
	zoomCenterX: 0,

	// zoomCenterY: Number
	//		The Y coordinate in the image where the zoom is focused
	zoomCenterY: 0,

	// maxZoom: Number
	//		The highest degree to which an image can be zoomed.  For example,
	//		a maxZoom of 5 means that the image will be 5 times larger than normal
	maxZoom: 5,

	// autoZoomLevel: Number
	//		The degree to which the image is zoomed when auto zoom is invoked.
	//		The higher the number, the more the image is zoomed in.
	autoZoomLevel: 3,

	// disableAutoZoom: Boolean
	//		Disables auto zoom
	disableAutoZoom: false,

	// disableSwipe: Boolean
	//		Disables the users ability to swipe from one image to the next.
	disableSwipe: false,

	// autoZoomEvent: String
	//		Overrides the default event listened to which invokes auto zoom
	autoZoomEvent: null,

	// _leftImg: Node
	//		The full sized image to the left
	_leftImg: null,

	// _centerImg: Node
	//		The full sized image in the center
	_centerImg: null,

	// _rightImg: Node
	//		The full sized image to the right
	_rightImg: null,

	// _leftImg: Node
	//		The small sized image to the left
	_leftSmallImg: null,

	// _centerImg: Node
	//		The small sized image in the center
	_centerSmallImg: null,

	// _rightImg: Node
	//		The small sized image to the right
	_rightSmallImg: null,

	constructor: function(){

		this.panX = 0;
		this.panY = 0;

		this.handleLoad = dojo.hitch(this, this.handleLoad);
		this._updateAnimatedZoom = dojo.hitch(this, this._updateAnimatedZoom);
		this._updateAnimatedPan = dojo.hitch(this, this._updateAnimatedPan);
		this._onAnimPanEnd = dojo.hitch(this, this._onAnimPanEnd);
	},

	buildRendering: function(){
		this.inherited(arguments);

		this.canvas = dojo.create("canvas", {}, this.domNode);

		dojo.addClass(this.domNode, "mblImageView");
	},

	postCreate: function(){
		this.inherited(arguments);

		this.size = dojo.marginBox(this.domNode);

		dojo.style(this.canvas, {
			width: this.size.w + "px",
			height: this.size.h + "px"
		});
		this.canvas.height = this.size.h;
		this.canvas.width = this.size.w;

		var _this = this;

		// Listen to the mousedown/touchstart event.  Record the position
		// so we can use it to pan the image.
		this.connect(this.domNode, "onmousedown", function(event){
			if(_this.isAnimating()){
				return;
			}
			if(_this.panX){
				_this.handleDragEnd();
			}

			_this.downX = event.targetTouches ? event.targetTouches[0].clientX : event.clientX;
			_this.downY = event.targetTouches ? event.targetTouches[0].clientY : event.clientY;
		});

		// record the movement of the mouse.
		this.connect(this.domNode, "onmousemove", function(event){
			if(_this.isAnimating()){
				return;
			}
			if((!_this.downX && _this.downX !== 0) || (!_this.downY && _this.downY !== 0)){
				// If the touch didn't begin on this widget, ignore the movement
				return;
			}

			if((!_this.disableSwipe && _this.zoom == 1)
				|| (!_this.disableAutoZoom && _this.zoom != 1)){
				var x = event.targetTouches ?
							event.targetTouches[0].clientX : event.pageX;
				var y = event.targetTouches ?
							event.targetTouches[0].clientY : event.pageY;

				_this.panX = x - _this.downX;
				_this.panY = y - _this.downY;

				if(_this.zoom == 1){
					// If not zoomed in, then try to move to the next or prev image
					// but only if the mouse has moved more than 10 pixels
					// in the X direction
					if(Math.abs(_this.panX) > 10){
						_this.render();
					}
				}else{
					// If zoomed in, pan the image if the mouse has moved more
					// than 10 pixels in either direction.
					if(Math.abs(_this.panX) > 10 || Math.abs(_this.panY) > 10){
						_this.render();
					}
				}
			}
		});

		this.connect(this.domNode, "onmouseout", function(event){
			if(!_this.isAnimating() && _this.panX){
				_this.handleDragEnd();
			}
		});

		this.connect(this.domNode, "onmouseover", function(event){
			_this.downX = _this.downY = null;
		});

		// Set up AutoZoom, which zooms in a fixed amount when the user taps
		// a part of the canvas
		this.connect(this.domNode, "onclick", function(event){
			if(_this.isAnimating()){
				return;
			}
			if(_this.downX == null || _this.downY == null){
				return;
			}

			var x = (event.targetTouches ?
						event.targetTouches[0].clientX : event.pageX);
			var y = (event.targetTouches ?
						event.targetTouches[0].clientY : event.pageY);

			// If the mouse/finger has moved more than 14 pixels from where it
			// started, do not treat it as a click.  It is a drag.
			if(Math.abs(_this.panX) > 14 || Math.abs(_this.panY) > 14){
				_this.downX = _this.downY = null;
				_this.handleDragEnd();
				return;
			}
			_this.downX = _this.downY = null;

			if(!_this.disableAutoZoom){

				if(!_this._centerImg || !_this._centerImg._loaded){
					// Do nothing until the image is loaded
					return;
				}
				if(_this.zoom != 1){
					_this.set("animatedZoom", 1);
					return;
				}

				var pos = dojo._abs(_this.domNode);

				// Translate the clicked point to a point on the source image
				var xRatio = _this.size.w / _this._centerImg.width;
				var yRatio = _this.size.h / _this._centerImg.height;

				// Do an animated zoom to the point which was clicked.
				_this.zoomTo(
					((x - pos.x) / xRatio) - _this.panX,
					((y - pos.y) / yRatio) - _this.panY,
					_this.autoZoomLevel);
			}
		});

		// Listen for Flick events
		dojo.connect(this.domNode, "flick", this, "handleFlick");
	},

	isAnimating: function(){
		// summary:
		//		Returns true if an animation is in progress, false otherwise.
		return this._anim && this._anim.status() == "playing";
	},

	handleDragEnd: function(){
		// summary:
		//		Handles the end of a dragging event. If not zoomed in, it
		//		determines if the next or previous image should be transitioned
		//		to.
		this.downX = this.downY = null;
		console.log("handleDragEnd");

		if(this.zoom == 1){
			if(!this.panX){
				return;
			}

			var leftLoaded = (this._leftImg && this._leftImg._loaded)
							|| (this._leftSmallImg && this._leftSmallImg._loaded);
			var rightLoaded = (this._rightImg && this._rightImg._loaded)
							|| (this._rightSmallImg && this._rightSmallImg._loaded);

			// Check if the drag has moved the image more than half its length.
			// If so, move to either the previous or next image.
			var doMove =
				!(Math.abs(this.panX) < this._centerImg._baseWidth / 2) &&
				(
					(this.panX > 0 && leftLoaded ? 1 : 0) ||
					(this.panX < 0 && rightLoaded ? 1 : 0)
				);


			if(!doMove){
				// If not moving to another image, animate the sliding of the
				// image back into place.
				this._animPanTo(0, dojo.fx.easing.expoOut, 700);
			}else{
				// Move to another image.
				this.moveTo(this.panX);
			}
		}else{
			if(!this.panX && !this.panY){
				return;
			}
			// Recenter the zoomed image based on where it was panned to
			// previously
			this.zoomCenterX -= (this.panX / this.zoom);
			this.zoomCenterY -= (this.panY / this.zoom);

			this.panX = this.panY = 0;
		}

	},

	handleFlick: function(event){
		// summary:
		//		Handle a flick event.
		if(this.zoom == 1 && event.duration < 500){
			// Only handle quick flicks here, less than 0.5 seconds

			// If not zoomed in, then check if we should move to the next photo
			// or not
			if(event.direction == "ltr"){
				this.moveTo(1);
			}else if(event.direction == "rtl"){
				this.moveTo(-1);
			}
			// If an up or down flick occurs, it means nothing so ignore it
			this.downX = this.downY = null;
		}
	},

	moveTo: function(direction){
		direction = direction > 0 ? 1 : -1;
		var toImg;

		if(direction < 1){
			if(this._rightImg && this._rightImg._loaded){
				toImg = this._rightImg;
			}else if(this._rightSmallImg && this._rightSmallImg._loaded){
				toImg = this._rightSmallImg;
			}
		}else{
			if(this._leftImg && this._leftImg._loaded){
				toImg = this._leftImg;
			}else if(this._leftSmallImg && this._leftSmallImg._loaded){
				toImg = this._leftSmallImg;
			}
		}

		this._moveDir = direction;
		var _this = this;

		if(toImg && toImg._loaded){
			// If the image is loaded, make a linear animation to show it
			this._animPanTo(this.size.w * direction, null, 500, function(){
				_this.panX = 0;
				_this.panY = 0;

				if(direction < 0){
					// Moving to show the right image
					_this._switchImage("left", "right");
				}else{
					// Moving to show the left image
					_this._switchImage("right", "left");
				}

				_this.render();
				_this.onChange(direction * -1);
			});

		}else{
			// If the next image is not loaded, make an animation to
			// move the center image to half the width of the widget and back
			// again

			console.log("moveTo image not loaded!", toImg);

			this._animPanTo(0, dojo.fx.easing.expoOut, 700);
		}
	},

	_switchImage: function(toImg, fromImg){
		var toSmallImgName = "_" + toImg + "SmallImg";
		var toImgName = "_" + toImg + "Img";

		var fromSmallImgName = "_" + fromImg + "SmallImg";
		var fromImgName = "_" + fromImg + "Img";

		this[toImgName] = this._centerImg;
		this[toSmallImgName] = this._centerSmallImg;

		this[toImgName]._type = toImg;

		if(this[toSmallImgName]){
			this[toSmallImgName]._type = toImg;
		}

		this._centerImg = this[fromImgName];
		this._centerSmallImg = this[fromSmallImgName];
		this._centerImg._type = "center";

		if(this._centerSmallImg){
			this._centerSmallImg._type = "center";
		}
		this[fromImgName] = this[fromSmallImgName] = null;
	},

	_animPanTo: function(to, easing, duration, callback){
		this._animCallback = callback;
		this._anim = new dojo.Animation({
			curve: [this.panX, to],
			onAnimate: this._updateAnimatedPan,
			duration: duration || 500,
			easing: easing,
			onEnd: this._onAnimPanEnd
		});

		this._anim.play();
		return this._anim;
	},

	onChange: function(direction){
		// summary:
		//		Stub function that can be listened to in order to provide
		//		new images when the displayed image changes
	},

	_updateAnimatedPan: function(amount){
		this.panX = amount;
		this.render();
	},

	_onAnimPanEnd: function(){
		this.panX = this.panY = 0;

		if(this._animCallback){
			this._animCallback();
		}
	},

	zoomTo: function(centerX, centerY, zoom){
		this.set("zoomCenterX", centerX);
		this.set("zoomCenterY", centerY);

		this.set("animatedZoom", zoom);
	},

	render: function(){
		var cxt = this.canvas.getContext('2d');

		cxt.clearRect(0, 0, this.canvas.width, this.canvas.height);

		// Render the center image
		this._renderImg(
			this._centerSmallImg,
			this._centerImg,
			this.zoom == 1 ? (this.panX < 0 ? 1 : this.panX > 0 ? -1 : 0) : 0);

		if(this.zoom == 1 && this.panX != 0){
			if(this.panX > 0){
				// Render the left image, showing the right side of it
				this._renderImg(this._leftSmallImg, this._leftImg, 1);
			}else{
				// Render the right image, showing the left side of it
				this._renderImg(this._rightSmallImg, this._rightImg, -1);
			}
		}
	},

	_renderImg: function(smallImg, largeImg, panDir){
		// summary:
		//		Renders a single image


		// If zoomed, we just display the center img
		var img = (largeImg && largeImg._loaded) ? largeImg : smallImg;

		if(!img || !img._loaded){
			// If neither the large or small image is loaded, display nothing
			return;
		}
		var cxt = this.canvas.getContext('2d');

		var baseWidth = img._baseWidth;
		var baseHeight = img._baseHeight;

		// Calculate the size the image would be if there were no bounds
		var desiredWidth = baseWidth * this.zoom;
		var desiredHeight = baseHeight * this.zoom;

		// Calculate the actual size of the viewable image
		var destWidth = Math.min(this.size.w, desiredWidth);
		var destHeight = Math.min(this.size.h, desiredHeight);


		// Calculate the size of the window on the original image to use
		var sourceWidth = this.dispWidth = img.width * (destWidth / desiredWidth);
		var sourceHeight = this.dispHeight = img.height * (destHeight / desiredHeight);

		var zoomCenterX = this.zoomCenterX - (this.panX / this.zoom);
		var zoomCenterY = this.zoomCenterY - (this.panY / this.zoom);

		// Calculate where the center of the view should be
		var centerX = Math.floor(Math.max(sourceWidth / 2,
				Math.min(img.width - sourceWidth / 2, zoomCenterX)));
		var centerY = Math.floor(Math.max(sourceHeight / 2,
				Math.min(img.height - sourceHeight / 2, zoomCenterY)));


		var sourceX = Math.max(0,
			Math.round((img.width - sourceWidth)/2 + (centerX - img._centerX)) );
		var sourceY = Math.max(0,
			Math.round((img.height - sourceHeight) / 2 + (centerY - img._centerY))
						);

		var destX = Math.round(Math.max(0, this.canvas.width - destWidth)/2);
		var destY = Math.round(Math.max(0, this.canvas.height - destHeight)/2);

		var oldDestWidth = destWidth;
		var oldSourceWidth = sourceWidth;

		if(this.zoom == 1 && panDir && this.panX){

			if(this.panX < 0){
				if(panDir > 0){
					// If the touch is moving left, and the right side of the
					// image should be shown, then reduce the destination width
					// by the absolute value of panX
					destWidth -= Math.abs(this.panX);
					destX = 0;
				}else if(panDir < 0){
					// If the touch is moving left, and the left side of the
					// image should be shown, then set the displayed width
					// to the absolute value of panX, less some pixels for
					// a padding between images
					destWidth = Math.max(1, Math.abs(this.panX) - 5);
					destX = this.size.w - destWidth;
				}
			}else{
				if(panDir > 0){
					// If the touch is moving right, and the right side of the
					// image should be shown, then set the destination width
					// to the absolute value of the pan, less some pixels for
					// padding
					destWidth = Math.max(1, Math.abs(this.panX) - 5);
					destX = 0;
				}else if(panDir < 0){
					// If the touch is moving right, and the left side of the
					// image should be shown, then reduce the destination width
					// by the widget width minus the absolute value of panX
					destWidth -= Math.abs(this.panX);
					destX = this.size.w - destWidth;
				}
			}

			sourceWidth = Math.max(1,
						Math.floor(sourceWidth * (destWidth / oldDestWidth)));

			if(panDir > 0){
				// If the right side of the image should be displayed, move
				// the sourceX to be the width of the image minus the difference
				// between the original sourceWidth and the new sourceWidth
				sourceX = (sourceX + oldSourceWidth) - (sourceWidth);
			}
			sourceX = Math.floor(sourceX);
		}

		try{

			// See https://developer.mozilla.org/en/Canvas_tutorial/Using_images
			cxt.drawImage(
				img,
				Math.max(0, sourceX),
				sourceY,
				Math.min(oldSourceWidth, sourceWidth),
				sourceHeight,
				destX, 	// Xpos
				destY, // Ypos
				Math.min(oldDestWidth, destWidth),
				destHeight
			);
		}catch(e){
			console.log("Caught Error",e,

					"type=", img._type,
					"oldDestWidth = ", oldDestWidth,
					"destWidth", destWidth,
					"destX", destX
					, "oldSourceWidth=",oldSourceWidth,
					"sourceWidth=", sourceWidth,
					"sourceX = " + sourceX
			);
		}
	},

	_setZoomAttr: function(amount){
		this.zoom = Math.min(this.maxZoom, Math.max(1, amount));

		if(this.zoom == 1
				&& this._centerImg
				&& this._centerImg._loaded){

			if(!this.isAnimating()){
				this.zoomCenterX = this._centerImg.width / 2;
				this.zoomCenterY = this._centerImg.height / 2;
			}
			this.panX = this.panY = 0;
		}

		this.render();
	},

	_setZoomCenterXAttr: function(value){
		if(value != this.zoomCenterX){
			if(this._centerImg && this._centerImg._loaded){
				value = Math.min(this._centerImg.width, value);
			}
			this.zoomCenterX = Math.max(0, Math.round(value));
		}
	},

	_setZoomCenterYAttr: function(value){
		if(value != this.zoomCenterY){
			if(this._centerImg && this._centerImg._loaded){
				value = Math.min(this._centerImg.height, value);
			}
			this.zoomCenterY = Math.max(0, Math.round(value));
		}
	},

	_setZoomCenterAttr: function(value){
		if(value.x != this.zoomCenterX || value.y != this.zoomCenterY){
			this.set("zoomCenterX", value.x);
			this.set("zoomCenterY", value.y);
			this.render();
		}
	},

	_setAnimatedZoomAttr: function(amount){
		if(this._anim && this._anim.status() == "playing"){
			return;
		}

		this._anim = new dojo.Animation({
			curve: [this.zoom, amount],
			onAnimate: this._updateAnimatedZoom,
			onEnd: this._onAnimEnd
		});

		this._anim.play();
	},

	_updateAnimatedZoom: function(amount){
		this._setZoomAttr(amount);
	},

	_setCenterUrlAttr: function(urlOrObj){
		this._setImage("center", urlOrObj);
	},
	_setLeftUrlAttr: function(urlOrObj){
		this._setImage("left", urlOrObj);
	},
	_setRightUrlAttr: function(urlOrObj){
		this._setImage("right", urlOrObj);
	},

	_setImage: function(name, urlOrObj){
		var smallUrl = null;

		var largeUrl = null;

		if(dojo.isString(urlOrObj)){
			// If the argument is a string, then just load the large url
			largeUrl = urlOrObj;
		}else{
			largeUrl = urlOrObj.large;
			smallUrl = urlOrObj.small;
		}

		if(this["_" + name + "Img"] && this["_" + name + "Img"]._src == largeUrl){
			// Identical URL, ignore it
			return;
		}

		// Just do the large image for now
		var largeImg = this["_" + name + "Img"] = new Image();
		largeImg._type = name;
		largeImg._loaded = false;
		largeImg._src = largeUrl;
		largeImg._conn = dojo.connect(largeImg, "onload", this.handleLoad);

		if(smallUrl){
			// If a url to a small version of the image has been provided,
			// load that image first.
			var smallImg = this["_" + name + "SmallImg"] = new Image();
			smallImg._type = name;
			smallImg._loaded = false;
			smallImg._conn = dojo.connect(smallImg, "onload", this.handleLoad);
			smallImg._isSmall = true;
			smallImg._src = smallUrl;
			smallImg.src = smallUrl;
		}

		// It's important that the large url's src is set after the small image
		// to ensure it's loaded second.
		largeImg.src = largeUrl;
	},

	handleLoad: function(evt){
		// summary:
		//		Handles the loading of an image, both the large and small
		//		versions.  A render is triggered as a result of each image load.

		var img = evt.target;
		img._loaded = true;

		dojo.disconnect(img._conn);

		var type = img._type;

		switch(type){
			case "center":
				this.zoomCenterX = img.width / 2;
				this.zoomCenterY = img.height / 2;
				break;
		}

		var height = img.height;
		var width = img.width;

		if(width / this.size.w < height / this.size.h){
			// Fit the height to the height of the canvas
			img._baseHeight = this.canvas.height;
			img._baseWidth = width / (height / this.size.h);
		}else{
			// Fix the width to the width of the canvas
			img._baseWidth = this.canvas.width;
			img._baseHeight = height / (width / this.size.w);
		}
		img._centerX = width / 2;
		img._centerY = height / 2;

		this.render();

		this.onLoad(img._type, img._src, img._isSmall);
	},

	onLoad: function(type, url, isSmall){
		// summary:
		//		Dummy function that is called whenever an image loads.
		// type: String
		//		The position of the image that has loaded, either
		//		"center", "left" or "right"
		// url: String
		//		The src of the image
		// isSmall: Boolean
		//		True if it is a small version of the image that has loaded,
		//		false otherwise.
	}
});

});

},
'dijit/_base/focus':function(){
define("dijit/_base/focus", [
	"dojo/_base/array", // array.forEach
	"dojo/dom", // dom.isDescendant
	"dojo/_base/lang", // lang.isArray
	"dojo/topic", // publish
	"dojo/_base/window", // win.doc win.doc.selection win.global win.global.getSelection win.withGlobal
	"../focus",
	".."	// for exporting symbols to dijit
], function(array, dom, lang, topic, win, focus, dijit){

	// module:
	//		dijit/_base/focus
	// summary:
	//		Deprecated module to monitor currently focused node and stack of currently focused widgets.
	//		New code should access dijit/focus directly.

	lang.mixin(dijit, {
		// _curFocus: DomNode
		//		Currently focused item on screen
		_curFocus: null,

		// _prevFocus: DomNode
		//		Previously focused item on screen
		_prevFocus: null,

		isCollapsed: function(){
			// summary:
			//		Returns true if there is no text selected
			return dijit.getBookmark().isCollapsed;
		},

		getBookmark: function(){
			// summary:
			//		Retrieves a bookmark that can be used with moveToBookmark to return to the same range
			var bm, rg, tg, sel = win.doc.selection, cf = focus.curNode;

			if(win.global.getSelection){
				//W3C Range API for selections.
				sel = win.global.getSelection();
				if(sel){
					if(sel.isCollapsed){
						tg = cf? cf.tagName : "";
						if(tg){
							//Create a fake rangelike item to restore selections.
							tg = tg.toLowerCase();
							if(tg == "textarea" ||
									(tg == "input" && (!cf.type || cf.type.toLowerCase() == "text"))){
								sel = {
									start: cf.selectionStart,
									end: cf.selectionEnd,
									node: cf,
									pRange: true
								};
								return {isCollapsed: (sel.end <= sel.start), mark: sel}; //Object.
							}
						}
						bm = {isCollapsed:true};
						if(sel.rangeCount){
							bm.mark = sel.getRangeAt(0).cloneRange();
						}
					}else{
						rg = sel.getRangeAt(0);
						bm = {isCollapsed: false, mark: rg.cloneRange()};
					}
				}
			}else if(sel){
				// If the current focus was a input of some sort and no selection, don't bother saving
				// a native bookmark.  This is because it causes issues with dialog/page selection restore.
				// So, we need to create psuedo bookmarks to work with.
				tg = cf ? cf.tagName : "";
				tg = tg.toLowerCase();
				if(cf && tg && (tg == "button" || tg == "textarea" || tg == "input")){
					if(sel.type && sel.type.toLowerCase() == "none"){
						return {
							isCollapsed: true,
							mark: null
						}
					}else{
						rg = sel.createRange();
						return {
							isCollapsed: rg.text && rg.text.length?false:true,
							mark: {
								range: rg,
								pRange: true
							}
						};
					}
				}
				bm = {};

				//'IE' way for selections.
				try{
					// createRange() throws exception when dojo in iframe
					//and nothing selected, see #9632
					rg = sel.createRange();
					bm.isCollapsed = !(sel.type == 'Text' ? rg.htmlText.length : rg.length);
				}catch(e){
					bm.isCollapsed = true;
					return bm;
				}
				if(sel.type.toUpperCase() == 'CONTROL'){
					if(rg.length){
						bm.mark=[];
						var i=0,len=rg.length;
						while(i<len){
							bm.mark.push(rg.item(i++));
						}
					}else{
						bm.isCollapsed = true;
						bm.mark = null;
					}
				}else{
					bm.mark = rg.getBookmark();
				}
			}else{
				console.warn("No idea how to store the current selection for this browser!");
			}
			return bm; // Object
		},

		moveToBookmark: function(/*Object*/ bookmark){
			// summary:
			//		Moves current selection to a bookmark
			// bookmark:
			//		This should be a returned object from dijit.getBookmark()

			var _doc = win.doc,
				mark = bookmark.mark;
			if(mark){
				if(win.global.getSelection){
					//W3C Rangi API (FF, WebKit, Opera, etc)
					var sel = win.global.getSelection();
					if(sel && sel.removeAllRanges){
						if(mark.pRange){
							var n = mark.node;
							n.selectionStart = mark.start;
							n.selectionEnd = mark.end;
						}else{
							sel.removeAllRanges();
							sel.addRange(mark);
						}
					}else{
						console.warn("No idea how to restore selection for this browser!");
					}
				}else if(_doc.selection && mark){
					//'IE' way.
					var rg;
					if(mark.pRange){
						rg = mark.range;
					}else if(lang.isArray(mark)){
						rg = _doc.body.createControlRange();
						//rg.addElement does not have call/apply method, so can not call it directly
						//rg is not available in "range.addElement(item)", so can't use that either
						array.forEach(mark, function(n){
							rg.addElement(n);
						});
					}else{
						rg = _doc.body.createTextRange();
						rg.moveToBookmark(mark);
					}
					rg.select();
				}
			}
		},

		getFocus: function(/*Widget?*/ menu, /*Window?*/ openedForWindow){
			// summary:
			//		Called as getFocus(), this returns an Object showing the current focus
			//		and selected text.
			//
			//		Called as getFocus(widget), where widget is a (widget representing) a button
			//		that was just pressed, it returns where focus was before that button
			//		was pressed.   (Pressing the button may have either shifted focus to the button,
			//		or removed focus altogether.)   In this case the selected text is not returned,
			//		since it can't be accurately determined.
			//
			// menu: dijit._Widget or {domNode: DomNode} structure
			//		The button that was just pressed.  If focus has disappeared or moved
			//		to this button, returns the previous focus.  In this case the bookmark
			//		information is already lost, and null is returned.
			//
			// openedForWindow:
			//		iframe in which menu was opened
			//
			// returns:
			//		A handle to restore focus/selection, to be passed to `dijit.focus`
			var node = !focus.curNode || (menu && dom.isDescendant(focus.curNode, menu.domNode)) ? dijit._prevFocus : focus.curNode;
			return {
				node: node,
				bookmark: node && (node == focus.curNode) && win.withGlobal(openedForWindow || win.global, dijit.getBookmark),
				openedForWindow: openedForWindow
			}; // Object
		},

		// _activeStack: dijit._Widget[]
		//		List of currently active widgets (focused widget and it's ancestors)
		_activeStack: [],

		registerIframe: function(/*DomNode*/ iframe){
			// summary:
			//		Registers listeners on the specified iframe so that any click
			//		or focus event on that iframe (or anything in it) is reported
			//		as a focus/click event on the <iframe> itself.
			// description:
			//		Currently only used by editor.
			// returns:
			//		Handle to pass to unregisterIframe()
			return focus.registerIframe(iframe);
		},

		unregisterIframe: function(/*Object*/ handle){
			// summary:
			//		Unregisters listeners on the specified iframe created by registerIframe.
			//		After calling be sure to delete or null out the handle itself.
			// handle:
			//		Handle returned by registerIframe()

			handle && handle.remove();
		},

		registerWin: function(/*Window?*/targetWindow, /*DomNode?*/ effectiveNode){
			// summary:
			//		Registers listeners on the specified window (either the main
			//		window or an iframe's window) to detect when the user has clicked somewhere
			//		or focused somewhere.
			// description:
			//		Users should call registerIframe() instead of this method.
			// targetWindow:
			//		If specified this is the window associated with the iframe,
			//		i.e. iframe.contentWindow.
			// effectiveNode:
			//		If specified, report any focus events inside targetWindow as
			//		an event on effectiveNode, rather than on evt.target.
			// returns:
			//		Handle to pass to unregisterWin()

			return focus.registerWin(targetWindow, effectiveNode);
		},

		unregisterWin: function(/*Handle*/ handle){
			// summary:
			//		Unregisters listeners on the specified window (either the main
			//		window or an iframe's window) according to handle returned from registerWin().
			//		After calling be sure to delete or null out the handle itself.

			handle && handle.remove();
		}
	});

	// Override focus singleton's focus function so that dijit.focus()
	// has backwards compatible behavior of restoring selection (although
	// probably no one is using that).
	focus.focus = function(/*Object || DomNode */ handle){
		// summary:
		//		Sets the focused node and the selection according to argument.
		//		To set focus to an iframe's content, pass in the iframe itself.
		// handle:
		//		object returned by get(), or a DomNode

		if(!handle){ return; }

		var node = "node" in handle ? handle.node : handle,		// because handle is either DomNode or a composite object
			bookmark = handle.bookmark,
			openedForWindow = handle.openedForWindow,
			collapsed = bookmark ? bookmark.isCollapsed : false;

		// Set the focus
		// Note that for iframe's we need to use the <iframe> to follow the parentNode chain,
		// but we need to set focus to iframe.contentWindow
		if(node){
			var focusNode = (node.tagName.toLowerCase() == "iframe") ? node.contentWindow : node;
			if(focusNode && focusNode.focus){
				try{
					// Gecko throws sometimes if setting focus is impossible,
					// node not displayed or something like that
					focusNode.focus();
				}catch(e){/*quiet*/}
			}
			focus._onFocusNode(node);
		}

		// set the selection
		// do not need to restore if current selection is not empty
		// (use keyboard to select a menu item) or if previous selection was collapsed
		// as it may cause focus shift (Esp in IE).
		if(bookmark && win.withGlobal(openedForWindow || win.global, dijit.isCollapsed) && !collapsed){
			if(openedForWindow){
				openedForWindow.focus();
			}
			try{
				win.withGlobal(openedForWindow || win.global, dijit.moveToBookmark, null, [bookmark]);
			}catch(e2){
				/*squelch IE internal error, see http://trac.dojotoolkit.org/ticket/1984 */
			}
		}
	};

	// For back compatibility, monitor changes to focused node and active widget stack,
	// publishing events and copying changes from focus manager variables into dijit (top level) variables
	focus.watch("curNode", function(name, oldVal, newVal){
		dijit._curFocus = newVal;
		dijit._prevFocus = oldVal;
		if(newVal){
			topic.publish("focusNode", newVal);	// publish
		}
	});
	focus.watch("activeStack", function(name, oldVal, newVal){
		dijit._activeStack = newVal;
	});

	focus.on("widget-blur", function(widget, by){
		topic.publish("widgetBlur", widget, by);	// publish
	});
	focus.on("widget-focus", function(widget, by){
		topic.publish("widgetFocus", widget, by);	// publish
	});

	return dijit;
});

},
'dojox/mobile/ListItem':function(){
define("dojox/mobile/ListItem", [
	"dojo/_base/array",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/has",
	"./common",
	"./_ItemBase",
	"./TransitionEvent"
], function(array, connect, declare, lang, domClass, domConstruct, has, common, ItemBase, TransitionEvent){

/*=====
	var ItemBase = dojox.mobile._ItemBase;
=====*/

	// module:
	//		dojox/mobile/ListItem
	// summary:
	//		An item of either RoundRectList or EdgeToEdgeList.

	return declare("dojox.mobile.ListItem", ItemBase, {
		// summary:
		//		An item of either RoundRectList or EdgeToEdgeList.
		// description:
		//		ListItem represents an item of either RoundRectList or
		//		EdgeToEdgeList. There are three ways to move to a different
		//		view, moveTo, href, and url. You can choose only one of them.

		// rightText: String
		//		A right-aligned text to display on the item.
		rightText: "",

		// rightIcon: String
		//		An icon to display at the right hand side of the item. The value
		//		can be either a path for an image file or a class name of a DOM
		//		button.
		rightIcon: "",

		// rightIcon2: String
		//		An icon to display at the left of the rightIcon. The value can
		//		be either a path for an image file or a class name of a DOM
		//		button.
		rightIcon2: "",


		// anchorLabel: Boolean
		//		If true, the label text becomes a clickable anchor text. When
		//		the user clicks on the text, the onAnchorLabelClicked handler is
		//		called. You can override or connect to the handler and implement
		//		any action. The handler has no default action.
		anchorLabel: false,

		// noArrow: Boolean
		//		If true, the right hand side arrow is not displayed.
		noArrow: false,

		// selected: Boolean
		//		If true, the item is highlighted to indicate it is selected.
		selected: false,

		// checked: Boolean
		//		If true, a check mark is displayed at the right of the item.
		checked: false,

		// arrowClass: String
		//		An icon to display as an arrow. The value can be either a path
		//		for an image file or a class name of a DOM button.
		arrowClass: "mblDomButtonArrow",

		// checkClass: String
		//		An icon to display as a check mark. The value can be either a
		//		path for an image file or a class name of a DOM button.
		checkClass: "mblDomButtonCheck",

		// variableHeight: Boolean
		//		If true, the height of the item varies according to its
		//		content. In dojo 1.6 or older, the "mblVariableHeight" class was
		//		used for this purpose. In dojo 1.7, adding the mblVariableHeight
		//		class still works for backward compatibility.
		variableHeight: false,


		// rightIconTitle: String
		//		An alt text for the right icon.
		rightIconTitle: "",

		// rightIcon2Title: String
		//		An alt text for the right icon2.
		rightIcon2Title: "",


		// btnClass: String
		//		Deprecated. For backward compatibility.
		btnClass: "",

		// btnClass2: String
		//		Deprecated. For backward compatibility.
		btnClass2: "",

		// tag: String
		//		A name of html tag to create as domNode.
		tag: "li",

		postMixInProperties: function(){
			// for backward compatibility
			if(this.btnClass){
				this.rightIcon = this.btnClass;
			}
			this._setBtnClassAttr = this._setRightIconAttr;
			this._setBtnClass2Attr = this._setRightIcon2Attr;
		},

		buildRendering: function(){
			this.domNode = this.srcNodeRef || domConstruct.create(this.tag);
			this.inherited(arguments);
			this.domNode.className = "mblListItem" + (this.selected ? " mblItemSelected" : "");

			// label
			var box = this.box = domConstruct.create("DIV");
			box.className = "mblListItemTextBox";
			if(this.anchorLabel){
				box.style.cursor = "pointer";
			}
			var r = this.srcNodeRef;
			if(r && !this.label){
				this.label = "";
				for(var i = 0, len = r.childNodes.length; i < len; i++){
					var n = r.firstChild;
					if(n.nodeType === 3 && lang.trim(n.nodeValue) !== ""){
						n.nodeValue = this._cv ? this._cv(n.nodeValue) : n.nodeValue;
						this.labelNode = domConstruct.create("SPAN", {className:"mblListItemLabel"});
						this.labelNode.appendChild(n);
						n = this.labelNode;
					}
					box.appendChild(n);
				}
			}
			if(!this.labelNode){
				this.labelNode = domConstruct.create("SPAN", {className:"mblListItemLabel"}, box);
			}
			if(this.anchorLabel){
				box.style.display = "inline"; // to narrow the text region
			}

			var a = this.anchorNode = domConstruct.create("A");
			a.className = "mblListItemAnchor";
			this.domNode.appendChild(a);
			a.appendChild(box);
		},

		startup: function(){
			if(this._started){ return; }
			this.inheritParams();
			var parent = this.getParent();
			if(this.moveTo || this.href || this.url || this.clickable || (parent && parent.select)){
				this._onClickHandle = this.connect(this.anchorNode, "onclick", "onClick");
			}
			this.setArrow();

			if(domClass.contains(this.domNode, "mblVariableHeight")){
				this.variableHeight = true;
			}
			if(this.variableHeight){
				domClass.add(this.domNode, "mblVariableHeight");
				setTimeout(lang.hitch(this, "layoutVariableHeight"));
			}

			this.set("icon", this.icon); // _setIconAttr may be called twice but this is necessary for offline instantiation
			if(!this.checked && this.checkClass.indexOf(',') !== -1){
				this.set("checked", this.checked);
			}
			this.inherited(arguments);
		},

		resize: function(){
			if(this.variableHeight){
				this.layoutVariableHeight();
			}
		},

		onClick: function(e){
			var a = e.currentTarget;
			var li = a.parentNode;
			if(domClass.contains(li, "mblItemSelected")){ return; } // already selected
			if(this.anchorLabel){
				for(var p = e.target; p.tagName !== this.tag.toUpperCase(); p = p.parentNode){
					if(p.className == "mblListItemTextBox"){
						domClass.add(p, "mblListItemTextBoxSelected");
						setTimeout(function(){
							domClass.remove(p, "mblListItemTextBoxSelected");
						}, has("android") ? 300 : 1000);
						this.onAnchorLabelClicked(e);
						return;
					}
				}
			}
			var parent = this.getParent();
			if(parent.select){
				if(parent.select === "single"){
					if(!this.checked){
						this.set("checked", true);
					}
				}else if(parent.select === "multiple"){
					this.set("checked", !this.checked);
				}
			}
			this.select();

			if (this.href && this.hrefTarget) {
				common.openWindow(this.href, this.hrefTarget);
				return;
			}
			var transOpts;
			if(this.moveTo || this.href || this.url || this.scene){
				transOpts = {moveTo: this.moveTo, href: this.href, url: this.url, scene: this.scene, transition: this.transition, transitionDir: this.transitionDir};
			}else if(this.transitionOptions){
				transOpts = this.transitionOptions;
			}	

			if(transOpts){
				this.setTransitionPos(e);
				return new TransitionEvent(this.domNode,transOpts,e).dispatch();
			}
		},
	
		select: function(){
			// summary:
			//		Makes this widget in the selected state.
			var parent = this.getParent();
			if(parent.stateful){
				parent.deselectAll();
			}else{
				var _this = this;
				setTimeout(function(){
					_this.deselect();
				}, has("android") ? 300 : 1000);
			}
			domClass.add(this.domNode, "mblItemSelected");
		},
	
		deselect: function(){
			// summary:
			//		Makes this widget in the deselected state.
			domClass.remove(this.domNode, "mblItemSelected");
		},
	
		onAnchorLabelClicked: function(e){
			// summary:
			//		Stub function to connect to from your application.
		},

		layoutVariableHeight: function(){
			var h = this.anchorNode.offsetHeight;
			if(h === this.anchorNodeHeight){ return; }
			this.anchorNodeHeight = h;
			array.forEach([
					this.rightTextNode,
					this.rightIcon2Node,
					this.rightIconNode,
					this.iconNode
				], function(n){
					if(n){
						var t = Math.round((h - n.offsetHeight) / 2);
						n.style.marginTop = t + "px";
					}
				});
		},

		setArrow: function(){
			// summary:
			//		Sets the arrow icon if necessary.
			if(this.checked){ return; }
			var c = "";
			var parent = this.getParent();
			if(this.moveTo || this.href || this.url || this.clickable){
				if(!this.noArrow && !(parent && parent.stateful)){
					c = this.arrowClass;
				}
			}
			if(c){
				this._setRightIconAttr(c);
			}
		},

		_setIconAttr: function(icon){
			if(!this.getParent()){ return; } // icon may be invalid because inheritParams is not called yet
			this.icon = icon;
			var a = this.anchorNode;
			if(!this.iconNode){
				if(icon){
					var ref = this.rightIconNode || this.rightIcon2Node || this.rightTextNode || this.box;
					this.iconNode = domConstruct.create("DIV", {className:"mblListItemIcon"}, ref, "before");
				}
			}else{
				domConstruct.empty(this.iconNode);
			}
			if(icon && icon !== "none"){
				common.createIcon(icon, this.iconPos, null, this.alt, this.iconNode);
				if(this.iconPos){
					domClass.add(this.iconNode.firstChild, "mblListItemSpriteIcon");
				}
				domClass.remove(a, "mblListItemAnchorNoIcon");
			}else{
				domClass.add(a, "mblListItemAnchorNoIcon");
			}
		},
	
		_setCheckedAttr: function(/*Boolean*/checked){
			var parent = this.getParent();
			if(parent && parent.select === "single" && checked){
				array.forEach(parent.getChildren(), function(child){
					child.set("checked", false);
				});
			}
			this._setRightIconAttr(this.checkClass);

			var icons = this.rightIconNode.childNodes;
			if(icons.length === 1){
				this.rightIconNode.style.display = checked ? "" : "none";
			}else{
				icons[0].style.display = checked ? "" : "none";
				icons[1].style.display = !checked ? "" : "none";
			}

			domClass.toggle(this.domNode, "mblListItemChecked", checked);
			if(parent && this.checked !== checked){
				parent.onCheckStateChanged(this, checked);
			}
			this.checked = checked;
		},
	
		_setRightTextAttr: function(/*String*/text){
			if(!this.rightTextNode){
				this.rightTextNode = domConstruct.create("DIV", {className:"mblListItemRightText"}, this.box, "before");
			}
			this.rightText = text;
			this.rightTextNode.innerHTML = this._cv ? this._cv(text) : text;
		},
	
		_setRightIconAttr: function(/*String*/icon){
			if(!this.rightIconNode){
				var ref = this.rightIcon2Node || this.rightTextNode || this.box;
				this.rightIconNode = domConstruct.create("DIV", {className:"mblListItemRightIcon"}, ref, "before");
			}else{
				domConstruct.empty(this.rightIconNode);
			}
			this.rightIcon = icon;
			var arr = (icon || "").split(/,/);
			if(arr.length === 1){
				common.createIcon(icon, null, null, this.rightIconTitle, this.rightIconNode);
			}else{
				common.createIcon(arr[0], null, null, this.rightIconTitle, this.rightIconNode);
				common.createIcon(arr[1], null, null, this.rightIconTitle, this.rightIconNode);
			}
		},
	
		_setRightIcon2Attr: function(/*String*/icon){
			if(!this.rightIcon2Node){
				var ref = this.rightTextNode || this.box;
				this.rightIcon2Node = domConstruct.create("DIV", {className:"mblListItemRightIcon2"}, ref, "before");
			}else{
				domConstruct.empty(this.rightIcon2Node);
			}
			this.rightIcon2 = icon;
			common.createIcon(icon, null, null, this.rightIcon2Title, this.rightIcon2Node);
		},
	
		_setLabelAttr: function(/*String*/text){
			this.label = text;
			this.labelNode.innerHTML = this._cv ? this._cv(text) : text;
		}
	});
});

},
'dojox/mobile/app/StageController':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/app/SceneController"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.StageController");
dojo.experimental("dojox.mobile.app.StageController");

dojo.require("dojox.mobile.app.SceneController");

dojo.declare("dojox.mobile.app.StageController", null,{

	// scenes: Array
	//    The list of scenes currently in existance in the app.
	scenes: null,

	effect: "fade",

	constructor: function(node){
		this.domNode = node;
		this.scenes = [];

		if(dojo.config.mobileAnim){
			this.effect = dojo.config.mobileAnim;
		}
	},

	getActiveSceneController: function(){
		return this.scenes[this.scenes.length - 1];
	},

	pushScene: function(sceneName, params){
		if(this._opInProgress){
			return;
		}
		this._opInProgress = true;

		// Push new scenes as the first element on the page.
		var node = dojo.create("div", {
			"class": "scene-wrapper",
			style: {
				visibility: "hidden"
			}
		}, this.domNode);

		var controller = new dojox.mobile.app.SceneController({}, node);

		if(this.scenes.length > 0){
			this.scenes[this.scenes.length -1].assistant.deactivate();
		}

		this.scenes.push(controller);

		var _this = this;

		dojo.forEach(this.scenes, this.setZIndex);

		controller.stageController = this;

		controller.init(sceneName, params).addCallback(function(){

			if(_this.scenes.length == 1){
				controller.domNode.style.visibility = "visible";
				_this.scenes[_this.scenes.length - 1].assistant.activate(params);
				_this._opInProgress = false;
			}else{
				_this.scenes[_this.scenes.length - 2]
					.performTransition(
						_this.scenes[_this.scenes.length - 1].domNode,
						1,
						_this.effect,
						null,
						function(){
							// When the scene is ready, activate it.
							_this.scenes[_this.scenes.length - 1].assistant.activate(params);
							_this._opInProgress = false;
						});
			}
		});
	},

	setZIndex: function(controller, idx){
		dojo.style(controller.domNode, "zIndex", idx + 1);
	},

	popScene: function(data){
		//	performTransition: function(/*String*/moveTo, /*Number*/dir, /*String*/transition,
			//						/*Object|null*/context, /*String|Function*/method /*optional args*/){
		if(this._opInProgress){
			return;
		}

		var _this = this;
		if(this.scenes.length > 1){

			this._opInProgress = true;
			this.scenes[_this.scenes.length - 2].assistant.activate(data);
			this.scenes[_this.scenes.length - 1]
				.performTransition(
					_this.scenes[this.scenes.length - 2].domNode,
					-1,
					this.effect,
					null,
					function(){
						// When the scene is no longer visible, destroy it
						_this._destroyScene(_this.scenes[_this.scenes.length - 1]);
						_this.scenes.splice(_this.scenes.length - 1, 1);
						_this._opInProgress = false;
					});
		}else{
			console.log("cannot pop the scene if there is just one");
		}
	},

	popScenesTo: function(sceneName, data){
		if(this._opInProgress){
			return;
		}

		while(this.scenes.length > 2 &&
				this.scenes[this.scenes.length - 2].sceneName != sceneName){
			this._destroyScene(this.scenes[this.scenes.length - 2]);
			this.scenes.splice(this.scenes.length - 2, 1);
		}

		this.popScene(data);
	},

	_destroyScene: function(scene){
		scene.assistant.deactivate();
		scene.assistant.destroy();
		scene.destroyRecursive();
	}


});


});

},
'dijit/place':function(){
define("dijit/place", [
	"dojo/_base/array", // array.forEach array.map array.some
	"dojo/dom-geometry", // domGeometry.getMarginBox domGeometry.position
	"dojo/dom-style", // domStyle.getComputedStyle
	"dojo/_base/kernel", // kernel.deprecated
	"dojo/_base/window", // win.body
	"dojo/window", // winUtils.getBox
	"."	// dijit (defining dijit.place to match API doc)
], function(array, domGeometry, domStyle, kernel, win, winUtils, dijit){

	// module:
	//		dijit/place
	// summary:
	//		Code to place a popup relative to another node


	function _place(/*DomNode*/ node, choices, layoutNode, aroundNodeCoords){
		// summary:
		//		Given a list of spots to put node, put it at the first spot where it fits,
		//		of if it doesn't fit anywhere then the place with the least overflow
		// choices: Array
		//		Array of elements like: {corner: 'TL', pos: {x: 10, y: 20} }
		//		Above example says to put the top-left corner of the node at (10,20)
		// layoutNode: Function(node, aroundNodeCorner, nodeCorner, size)
		//		for things like tooltip, they are displayed differently (and have different dimensions)
		//		based on their orientation relative to the parent.	 This adjusts the popup based on orientation.
		//		It also passes in the available size for the popup, which is useful for tooltips to
		//		tell them that their width is limited to a certain amount.	 layoutNode() may return a value expressing
		//		how much the popup had to be modified to fit into the available space.	 This is used to determine
		//		what the best placement is.
		// aroundNodeCoords: Object
		//		Size of aroundNode, ex: {w: 200, h: 50}

		// get {x: 10, y: 10, w: 100, h:100} type obj representing position of
		// viewport over document
		var view = winUtils.getBox();

		// This won't work if the node is inside a <div style="position: relative">,
		// so reattach it to win.doc.body.	 (Otherwise, the positioning will be wrong
		// and also it might get cutoff)
		if(!node.parentNode || String(node.parentNode.tagName).toLowerCase() != "body"){
			win.body().appendChild(node);
		}

		var best = null;
		array.some(choices, function(choice){
			var corner = choice.corner;
			var pos = choice.pos;
			var overflow = 0;

			// calculate amount of space available given specified position of node
			var spaceAvailable = {
				w: {
					'L': view.l + view.w - pos.x,
					'R': pos.x - view.l,
					'M': view.w
				   }[corner.charAt(1)],
				h: {
					'T': view.t + view.h - pos.y,
					'B': pos.y - view.t,
					'M': view.h
				   }[corner.charAt(0)]
			};

			// configure node to be displayed in given position relative to button
			// (need to do this in order to get an accurate size for the node, because
			// a tooltip's size changes based on position, due to triangle)
			if(layoutNode){
				var res = layoutNode(node, choice.aroundCorner, corner, spaceAvailable, aroundNodeCoords);
				overflow = typeof res == "undefined" ? 0 : res;
			}

			// get node's size
			var style = node.style;
			var oldDisplay = style.display;
			var oldVis = style.visibility;
			if(style.display == "none"){
				style.visibility = "hidden";
				style.display = "";
			}
			var mb = domGeometry. getMarginBox(node);
			style.display = oldDisplay;
			style.visibility = oldVis;

			// coordinates and size of node with specified corner placed at pos,
			// and clipped by viewport
			var
				startXpos = {
					'L': pos.x,
					'R': pos.x - mb.w,
					'M': Math.max(view.l, Math.min(view.l + view.w, pos.x + (mb.w >> 1)) - mb.w) // M orientation is more flexible
				}[corner.charAt(1)],
				startYpos = {
					'T': pos.y,
					'B': pos.y - mb.h,
					'M': Math.max(view.t, Math.min(view.t + view.h, pos.y + (mb.h >> 1)) - mb.h)
				}[corner.charAt(0)],
				startX = Math.max(view.l, startXpos),
				startY = Math.max(view.t, startYpos),
				endX = Math.min(view.l + view.w, startXpos + mb.w),
				endY = Math.min(view.t + view.h, startYpos + mb.h),
				width = endX - startX,
				height = endY - startY;

			overflow += (mb.w - width) + (mb.h - height);

			if(best == null || overflow < best.overflow){
				best = {
					corner: corner,
					aroundCorner: choice.aroundCorner,
					x: startX,
					y: startY,
					w: width,
					h: height,
					overflow: overflow,
					spaceAvailable: spaceAvailable
				};
			}

			return !overflow;
		});

		// In case the best position is not the last one we checked, need to call
		// layoutNode() again.
		if(best.overflow && layoutNode){
			layoutNode(node, best.aroundCorner, best.corner, best.spaceAvailable, aroundNodeCoords);
		}

		// And then position the node.  Do this last, after the layoutNode() above
		// has sized the node, due to browser quirks when the viewport is scrolled
		// (specifically that a Tooltip will shrink to fit as though the window was
		// scrolled to the left).
		//
		// In RTL mode, set style.right rather than style.left so in the common case,
		// window resizes move the popup along with the aroundNode.
		var l = domGeometry.isBodyLtr(),
			s = node.style;
		s.top = best.y + "px";
		s[l ? "left" : "right"] = (l ? best.x : view.w - best.x - best.w) + "px";
		s[l ? "right" : "left"] = "auto";	// needed for FF or else tooltip goes to far left

		return best;
	}

	/*=====
	dijit.place.__Position = function(){
		// x: Integer
		//		horizontal coordinate in pixels, relative to document body
		// y: Integer
		//		vertical coordinate in pixels, relative to document body

		this.x = x;
		this.y = y;
	};
	=====*/

	/*=====
	dijit.place.__Rectangle = function(){
		// x: Integer
		//		horizontal offset in pixels, relative to document body
		// y: Integer
		//		vertical offset in pixels, relative to document body
		// w: Integer
		//		width in pixels.   Can also be specified as "width" for backwards-compatibility.
		// h: Integer
		//		height in pixels.   Can also be specified as "height" from backwards-compatibility.

		this.x = x;
		this.y = y;
		this.w = w;
		this.h = h;
	};
	=====*/

	return (dijit.place = {
		// summary:
		//		Code to place a DOMNode relative to another DOMNode.
		//		Load using require(["dijit/place"], function(place){ ... }).

		at: function(node, pos, corners, padding){
			// summary:
			//		Positions one of the node's corners at specified position
			//		such that node is fully visible in viewport.
			// description:
			//		NOTE: node is assumed to be absolutely or relatively positioned.
			// node: DOMNode
			//		The node to position
			// pos: dijit.place.__Position
			//		Object like {x: 10, y: 20}
			// corners: String[]
			//		Array of Strings representing order to try corners in, like ["TR", "BL"].
			//		Possible values are:
			//			* "BL" - bottom left
			//			* "BR" - bottom right
			//			* "TL" - top left
			//			* "TR" - top right
			// padding: dijit.place.__Position?
			//		optional param to set padding, to put some buffer around the element you want to position.
			// example:
			//		Try to place node's top right corner at (10,20).
			//		If that makes node go (partially) off screen, then try placing
			//		bottom left corner at (10,20).
			//	|	place(node, {x: 10, y: 20}, ["TR", "BL"])
			var choices = array.map(corners, function(corner){
				var c = { corner: corner, pos: {x:pos.x,y:pos.y} };
				if(padding){
					c.pos.x += corner.charAt(1) == 'L' ? padding.x : -padding.x;
					c.pos.y += corner.charAt(0) == 'T' ? padding.y : -padding.y;
				}
				return c;
			});

			return _place(node, choices);
		},

		around: function(
			/*DomNode*/		node,
			/*DomNode || dijit.place.__Rectangle*/ anchor,
			/*String[]*/	positions,
			/*Boolean*/		leftToRight,
			/*Function?*/	layoutNode){

			// summary:
			//		Position node adjacent or kitty-corner to anchor
			//		such that it's fully visible in viewport.
			//
			// description:
			//		Place node such that corner of node touches a corner of
			//		aroundNode, and that node is fully visible.
			//
			// anchor:
			//		Either a DOMNode or a __Rectangle (object with x, y, width, height).
			//
			// positions:
			//		Ordered list of positions to try matching up.
			//			* before: places drop down to the left of the anchor node/widget, or to the right in
			//				the case of RTL scripts like Hebrew and Arabic
			//			* after: places drop down to the right of the anchor node/widget, or to the left in
			//				the case of RTL scripts like Hebrew and Arabic
			//			* above: drop down goes above anchor node
			//			* above-alt: same as above except right sides aligned instead of left
			//			* below: drop down goes below anchor node
			//			* below-alt: same as below except right sides aligned instead of left
			//
			// layoutNode: Function(node, aroundNodeCorner, nodeCorner)
			//		For things like tooltip, they are displayed differently (and have different dimensions)
			//		based on their orientation relative to the parent.	 This adjusts the popup based on orientation.
			//
			// leftToRight:
			//		True if widget is LTR, false if widget is RTL.   Affects the behavior of "above" and "below"
			//		positions slightly.
			//
			// example:
			//	|	placeAroundNode(node, aroundNode, {'BL':'TL', 'TR':'BR'});
			//		This will try to position node such that node's top-left corner is at the same position
			//		as the bottom left corner of the aroundNode (ie, put node below
			//		aroundNode, with left edges aligned).	If that fails it will try to put
			// 		the bottom-right corner of node where the top right corner of aroundNode is
			//		(ie, put node above aroundNode, with right edges aligned)
			//

			// if around is a DOMNode (or DOMNode id), convert to coordinates
			var aroundNodePos = (typeof anchor == "string" || "offsetWidth" in anchor)
				? domGeometry.position(anchor, true)
				: anchor;

			// Adjust anchor positioning for the case that a parent node has overflw hidden, therefore cuasing the anchor not to be completely visible
			if(anchor.parentNode){
				var parent = anchor.parentNode;
				while(parent && parent.nodeType == 1 && parent.nodeName != "BODY"){  //ignoring the body will help performance
					var parentPos = domGeometry.position(parent, true);
					var parentStyleOverflow = domStyle.getComputedStyle(parent).overflow;
					if(parentStyleOverflow == "hidden" || parentStyleOverflow == "auto" || parentStyleOverflow == "scroll"){
						var bottomYCoord = Math.min(aroundNodePos.y + aroundNodePos.h, parentPos.y + parentPos.h);
						var rightXCoord = Math.min(aroundNodePos.x + aroundNodePos.w, parentPos.x + parentPos.w);
						aroundNodePos.x = Math.max(aroundNodePos.x, parentPos.x);
						aroundNodePos.y = Math.max(aroundNodePos.y, parentPos.y);
						aroundNodePos.h = bottomYCoord - aroundNodePos.y;
						aroundNodePos.w = rightXCoord - aroundNodePos.x;
					}	
					parent = parent.parentNode;
				}
			}			

			var x = aroundNodePos.x,
				y = aroundNodePos.y,
				width = "w" in aroundNodePos ? aroundNodePos.w : (aroundNodePos.w = aroundNodePos.width),
				height = "h" in aroundNodePos ? aroundNodePos.h : (kernel.deprecated("place.around: dijit.place.__Rectangle: { x:"+x+", y:"+y+", height:"+aroundNodePos.height+", width:"+width+" } has been deprecated.  Please use { x:"+x+", y:"+y+", h:"+aroundNodePos.height+", w:"+width+" }", "", "2.0"), aroundNodePos.h = aroundNodePos.height);

			// Convert positions arguments into choices argument for _place()
			var choices = [];
			function push(aroundCorner, corner){
				choices.push({
					aroundCorner: aroundCorner,
					corner: corner,
					pos: {
						x: {
							'L': x,
							'R': x + width,
							'M': x + (width >> 1)
						   }[aroundCorner.charAt(1)],
						y: {
							'T': y,
							'B': y + height,
							'M': y + (height >> 1)
						   }[aroundCorner.charAt(0)]
					}
				})
			}
			array.forEach(positions, function(pos){
				var ltr =  leftToRight;
				switch(pos){
					case "above-centered":
						push("TM", "BM");
						break;
					case "below-centered":
						push("BM", "TM");
						break;
					case "after":
						ltr = !ltr;
						// fall through
					case "before":
						push(ltr ? "ML" : "MR", ltr ? "MR" : "ML");
						break;
					case "below-alt":
						ltr = !ltr;
						// fall through
					case "below":
						// first try to align left borders, next try to align right borders (or reverse for RTL mode)
						push(ltr ? "BL" : "BR", ltr ? "TL" : "TR");
						push(ltr ? "BR" : "BL", ltr ? "TR" : "TL");
						break;
					case "above-alt":
						ltr = !ltr;
						// fall through
					case "above":
						// first try to align left borders, next try to align right borders (or reverse for RTL mode)
						push(ltr ? "TL" : "TR", ltr ? "BL" : "BR");
						push(ltr ? "TR" : "TL", ltr ? "BR" : "BL");
						break;
					default:
						// To assist dijit/_base/place, accept arguments of type {aroundCorner: "BL", corner: "TL"}.
						// Not meant to be used directly.
						push(pos.aroundCorner, pos.corner);
				}
			});

			var position = _place(node, choices, layoutNode, {w: width, h: height});
			position.aroundNodePos = aroundNodePos;

			return position;
		}
	});
});

},
'dojox/mobile/app/_event':function(){
// wrapped by build app
define(["dijit","dojo","dojox"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app._event");
dojo.experimental("dojox.mobile.app._event.js");

dojo.mixin(dojox.mobile.app, {
	eventMap: {},

	connectFlick: function(target, context, method){
		// summary:
		//		Listens for a flick event on a DOM node.  If the mouse/touch
		//		moves more than 15 pixels in any given direction it is a flick.
		//		The synthetic event fired specifies the direction as
		//		<ul>
		//			<li><b>'ltr'</b> Left To Right</li>
		//			<li><b>'rtl'</b> Right To Left</li>
		//			<li><b>'ttb'</b> Top To Bottom</li>
		//			<li><b>'btt'</b> Bottom To Top</li>
		//		</ul>
		// target: Node
		//		The DOM node to connect to

		var startX;
		var startY;
		var isFlick = false;

		var currentX;
		var currentY;

		var connMove;
		var connUp;

		var direction;

		var time;

		// Listen to to the mousedown/touchstart event
		var connDown = dojo.connect("onmousedown", target, function(event){
			isFlick = false;
			startX = event.targetTouches ? event.targetTouches[0].clientX : event.clientX;
			startY = event.targetTouches ? event.targetTouches[0].clientY : event.clientY;

			time = (new Date()).getTime();

			connMove = dojo.connect(target, "onmousemove", onMove);
			connUp = dojo.connect(target, "onmouseup", onUp);
		});

		// The function that handles the mousemove/touchmove event
		var onMove = function(event){
			dojo.stopEvent(event);

			currentX = event.targetTouches ? event.targetTouches[0].clientX : event.clientX;
			currentY = event.targetTouches ? event.targetTouches[0].clientY : event.clientY;
			if(Math.abs(Math.abs(currentX) - Math.abs(startX)) > 15){
				isFlick = true;

				direction = (currentX > startX) ? "ltr" : "rtl";
			}else if(Math.abs(Math.abs(currentY) - Math.abs(startY)) > 15){
				isFlick = true;

				direction = (currentY > startY) ? "ttb" : "btt";
			}
		};

		var onUp = function(event){
			dojo.stopEvent(event);

			connMove && dojo.disconnect(connMove);
			connUp && dojo.disconnect(connUp);

			if(isFlick){
				var flickEvt = {
					target: target,
					direction: direction,
					duration: (new Date()).getTime() - time
				};
				if(context && method){
					context[method](flickEvt);
				}else{
					method(flickEvt);
				}
			}
		};

	}
});

dojox.mobile.app.isIPhone = (dojo.isSafari
	&& (navigator.userAgent.indexOf("iPhone") > -1 ||
		navigator.userAgent.indexOf("iPod") > -1
	));
dojox.mobile.app.isWebOS = (navigator.userAgent.indexOf("webOS") > -1);
dojox.mobile.app.isAndroid = (navigator.userAgent.toLowerCase().indexOf("android") > -1);

if(dojox.mobile.app.isIPhone || dojox.mobile.app.isAndroid){
	// We are touchable.
	// Override the dojo._connect function to replace mouse events with touch events

	dojox.mobile.app.eventMap = {
		onmousedown: "ontouchstart",
		mousedown: "ontouchstart",
		onmouseup: "ontouchend",
		mouseup: "ontouchend",
		onmousemove: "ontouchmove",
		mousemove: "ontouchmove"
	};

}
dojo._oldConnect = dojo._connect;
dojo._connect = function(obj, event, context, method, dontFix){
	event = dojox.mobile.app.eventMap[event] || event;
	if(event == "flick" || event == "onflick"){
		if(dojo.global["Mojo"]){
			event = Mojo.Event.flick;
		} else{
			return dojox.mobile.app.connectFlick(obj, context, method);
		}
	}

	return dojo._oldConnect(obj, event, context, method, dontFix);
};
});

},
'dojox/mobile/_base':function(){
define("dojox/mobile/_base", [
	"./common",
	"./View",
	"./Heading",
	"./RoundRect",
	"./RoundRectCategory",
	"./EdgeToEdgeCategory",
	"./RoundRectList",
	"./EdgeToEdgeList",
	"./ListItem",
	"./Switch",
	"./ToolBarButton",
	"./ProgressIndicator"
], function(common, View, Heading, RoundRect, RoundRectCategory, EdgeToEdgeCategory, RoundRectList, EdgeToEdgeList, ListItem, Switch, ToolBarButton, ProgressIndicator){
	// module:
	//		dojox/mobile/_base
	// summary:
	//		Includes the basic dojox.mobile modules

	return common;
});

},
'dojox/mobile/Button':function(){
define([
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dijit/_WidgetBase",
	"dijit/form/_ButtonMixin",
	"dijit/form/_FormWidgetMixin"
],
	function(array, declare, domClass, domConstruct, WidgetBase, ButtonMixin, FormWidgetMixin){

	/*=====
		WidgetBase = dijit._WidgetBase;
		FormWidgetMixin = dijit.form._FormWidgetMixin;
		ButtonMixin = dijit.form._ButtonMixin;
	=====*/
	return declare("dojox.mobile.Button", [WidgetBase, FormWidgetMixin, ButtonMixin], {
		// summary:
		//	Non-templated BUTTON widget with a thin API wrapper for click events and setting the label
		//
		// description:
		//              Buttons can display a label, an icon, or both.
		//              A label should always be specified (through innerHTML) or the label
		//              attribute.  It can be hidden via showLabel=false.
		// example:
		// |    <button dojoType="dijit.form.Button" onClick="...">Hello world</button>

		baseClass: "mblButton",

		// Override automatic assigning type --> node, it causes exception on IE.
		// Instead, type must be specified as this.type when the node is created, as part of the original DOM
		_setTypeAttr: null,

		// duration: Number
		//	duration of selection, milliseconds or -1 for no post-click CSS styling
		duration: 1000,

		_onClick: function(e){
			var ret = this.inherited(arguments);
			if(ret && this.duration >= 0){ // if its not a button with a state, then emulate press styles
				var button = this.focusNode || this.domNode;
				var newStateClasses = (this.baseClass+' '+this["class"]).split(" ");
				newStateClasses = array.map(newStateClasses, function(c){ return c+"Selected"; });
				domClass.add(button, newStateClasses);
				setTimeout(function(){
					domClass.remove(button, newStateClasses);
				}, this.duration);
			}
			return ret;
		},

		isFocusable: function(){ return false; },

		buildRendering: function(){
			if(!this.srcNodeRef){
				this.srcNodeRef = domConstruct.create("button", {"type": this.type});
			}else if(this._cv){
				var n = this.srcNodeRef.firstChild;
				if(n && n.nodeType === 3){
					n.nodeValue = this._cv(n.nodeValue);
				}
			}
			this.inherited(arguments);
			this.focusNode = this.domNode;
		},

		postCreate: function(){
			this.inherited(arguments);
			this.connect(this.domNode, "onclick", "_onClick");
		},

		_setLabelAttr: function(/*String*/ content){
			this.inherited(arguments, [this._cv ? this._cv(content) : content]);
		}
	});

});

},
'dojox/mobile/Switch':function(){
define("dojox/mobile/Switch", [
	"dojo/_base/array",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/_base/event",
	"dojo/_base/window",
	"dojo/dom-class",
	"dijit/_Contained",
	"dijit/_WidgetBase",
	"./sniff"
], function(array, connect, declare, event, win, domClass, Contained, WidgetBase, has){

/*=====
	Contained = dijit._Contained;
	WidgetBase = dijit._WidgetBase;
=====*/

	// module:
	//		dojox/mobile/Switch
	// summary:
	//		A toggle switch with a sliding knob.

	return declare("dojox.mobile.Switch", [WidgetBase, Contained],{
		// summary:
		//		A toggle switch with a sliding knob.
		// description:
		//		Switch is a toggle switch with a sliding knob. You can either
		//		tap or slide the knob to toggle the switch. The onStateChanged
		//		handler is called when the switch is manipulated.

		// value: String
		//		The initial state of the switch. "on" or "off". The default
		//		value is "on".
		value: "on",

		// name: String
		//		A name for a hidden input field, which holds the current value.
		name: "",

		// leftLabel: String
		//		The left-side label of the switch.
		leftLabel: "ON",

		// rightLabel: String
		//		The right-side label of the switch.
		rightLabel: "OFF",

		/* internal properties */	
		_width: 53,

		buildRendering: function(){
			this.domNode = win.doc.createElement("DIV");
			var c = (this.srcNodeRef && this.srcNodeRef.className) || this.className || this["class"];
			this._swClass = (c || "").replace(/ .*/,"");
			this.domNode.className = "mblSwitch";
			var nameAttr = this.name ? " name=\"" + this.name + "\"" : "";
			this.domNode.innerHTML =
				  '<div class="mblSwitchInner">'
				+	'<div class="mblSwitchBg mblSwitchBgLeft">'
				+		'<div class="mblSwitchText mblSwitchTextLeft"></div>'
				+	'</div>'
				+	'<div class="mblSwitchBg mblSwitchBgRight">'
				+		'<div class="mblSwitchText mblSwitchTextRight"></div>'
				+	'</div>'
				+	'<div class="mblSwitchKnob"></div>'
				+	'<input type="hidden"'+nameAttr+'></div>'
				+ '</div>';
			var n = this.inner = this.domNode.firstChild;
			this.left = n.childNodes[0];
			this.right = n.childNodes[1];
			this.knob = n.childNodes[2];
			this.input = n.childNodes[3];
		},

		postCreate: function(){
			this.connect(this.domNode, "onclick", "onClick");
			this.connect(this.domNode, has("touch") ? "touchstart" : "onmousedown", "onTouchStart");
			this._initialValue = this.value; // for reset()
		},

		_changeState: function(/*String*/state, /*Boolean*/anim){
			var on = (state === "on");
			this.left.style.display = "";
			this.right.style.display = "";
			this.inner.style.left = "";
			if(anim){
				domClass.add(this.domNode, "mblSwitchAnimation");
			}
			domClass.remove(this.domNode, on ? "mblSwitchOff" : "mblSwitchOn");
			domClass.add(this.domNode, on ? "mblSwitchOn" : "mblSwitchOff");
	
			var _this = this;
			setTimeout(function(){
				_this.left.style.display = on ? "" : "none";
				_this.right.style.display = !on ? "" : "none";
				domClass.remove(_this.domNode, "mblSwitchAnimation");
			}, anim ? 300 : 0);
		},

		startup: function(){
			if(this._swClass.indexOf("Round") != -1){
				var r = Math.round(this.domNode.offsetHeight / 2);
				this.createRoundMask(this._swClass, r, this.domNode.offsetWidth);
			}
		},
	
		createRoundMask: function(className, r, w){
			if(!has("webkit") || !className){ return; }
			if(!this._createdMasks){ this._createdMasks = []; }
			if(this._createdMasks[className]){ return; }
			this._createdMasks[className] = 1;
	
			var ctx = win.doc.getCSSCanvasContext("2d", className+"Mask", w, 100);
			ctx.fillStyle = "#000000";
			ctx.beginPath();
			ctx.moveTo(r, 0);
			ctx.arcTo(0, 0, 0, 2*r, r);
			ctx.arcTo(0, 2*r, r, 2*r, r);
			ctx.lineTo(w - r, 2*r);
			ctx.arcTo(w, 2*r, w, r, r);
			ctx.arcTo(w, 0, w - r, 0, r);
			ctx.closePath();
			ctx.fill();
		},
	
		onClick: function(e){
			if(this._moved){ return; }
			this.value = this.input.value = (this.value == "on") ? "off" : "on";
			this._changeState(this.value, true);
			this.onStateChanged(this.value);
		},
	
		onTouchStart: function(e){
			// summary:
			//		Internal function to handle touchStart events.
			this._moved = false;
			this.innerStartX = this.inner.offsetLeft;
			if(!this._conn){
				this._conn = [];
				this._conn.push(connect.connect(this.inner, has("touch") ? "touchmove" : "onmousemove", this, "onTouchMove"));
				this._conn.push(connect.connect(this.inner, has("touch") ? "touchend" : "onmouseup", this, "onTouchEnd"));
			}
			this.touchStartX = e.touches ? e.touches[0].pageX : e.clientX;
			this.left.style.display = "";
			this.right.style.display = "";
			event.stop(e);
		},
	
		onTouchMove: function(e){
			// summary:
			//		Internal function to handle touchMove events.
			e.preventDefault();
			var dx;
			if(e.targetTouches){
				if(e.targetTouches.length != 1){ return false; }
				dx = e.targetTouches[0].clientX - this.touchStartX;
			}else{
				dx = e.clientX - this.touchStartX;
			}
			var pos = this.innerStartX + dx;
			var d = 10;
			if(pos <= -(this._width-d)){ pos = -this._width; }
			if(pos >= -d){ pos = 0; }
			this.inner.style.left = pos + "px";
			if(Math.abs(dx) > d){
				this._moved = true;
			}
		},
	
		onTouchEnd: function(e){
			// summary:
			//		Internal function to handle touchEnd events.
			array.forEach(this._conn, connect.disconnect);
			this._conn = null;
			if(this.innerStartX == this.inner.offsetLeft){
				if(has("touch")){
					var ev = win.doc.createEvent("MouseEvents");
					ev.initEvent("click", true, true);
					this.inner.dispatchEvent(ev);
				}
				return;
			}
			var newState = (this.inner.offsetLeft < -(this._width/2)) ? "off" : "on";
			this._changeState(newState, true);
			if(newState != this.value){
				this.value = this.input.value = newState;
				this.onStateChanged(newState);
			}
		},
	
		onStateChanged: function(/*String*/newState){
			// summary:
			//		Stub function to connect to from your application.
			// description:
			//		Called when the state has been changed.
		},
	
		_setValueAttr: function(/*String*/value){
			this._changeState(value, false);
			if(this.value != value){
				this.onStateChanged(value);
			}
			this.value = this.input.value = value;
		},
	
		_setLeftLabelAttr: function(/*String*/label){
			this.leftLabel = label;
			this.left.firstChild.innerHTML = this._cv ? this._cv(label) : label;
		},
	
		_setRightLabelAttr: function(/*String*/label){
			this.rightLabel = label;
			this.right.firstChild.innerHTML = this._cv ? this._cv(label) : label;
		},

		reset: function(){
			// summary:
			//		Reset the widget's value to what it was at initialization time
			this.set("value", this._initialValue);
		}
	});
});

},
'dijit/focus':function(){
define("dijit/focus", [
	"dojo/aspect",
	"dojo/_base/declare", // declare
	"dojo/dom", // domAttr.get dom.isDescendant
	"dojo/dom-attr", // domAttr.get dom.isDescendant
	"dojo/dom-construct", // connect to domConstruct.empty, domConstruct.destroy
	"dojo/Evented",
	"dojo/_base/lang", // lang.hitch
	"dojo/on",
	"dojo/ready",
	"dojo/_base/sniff", // has("ie")
	"dojo/Stateful",
	"dojo/_base/unload", // unload.addOnWindowUnload
	"dojo/_base/window", // win.body
	"dojo/window", // winUtils.get
	"./a11y",	// a11y.isTabNavigable
	"./registry",	// registry.byId
	"."		// to set dijit.focus
], function(aspect, declare, dom, domAttr, domConstruct, Evented, lang, on, ready, has, Stateful, unload, win, winUtils,
			a11y, registry, dijit){

	// module:
	//		dijit/focus
	// summary:
	//		Returns a singleton that tracks the currently focused node, and which widgets are currently "active".

/*=====
	dijit.focus = {
		// summary:
		//		Tracks the currently focused node, and which widgets are currently "active".
		//		Access via require(["dijit/focus"], function(focus){ ... }).
		//
		//		A widget is considered active if it or a descendant widget has focus,
		//		or if a non-focusable node of this widget or a descendant was recently clicked.
		//
		//		Call focus.watch("curNode", callback) to track the current focused DOMNode,
		//		or focus.watch("activeStack", callback) to track the currently focused stack of widgets.
		//
		//		Call focus.on("widget-blur", func) or focus.on("widget-focus", ...) to monitor when
		//		when widgets become active/inactive
		//
		//		Finally, focus(node) will focus a node, suppressing errors if the node doesn't exist.

		// curNode: DomNode
		//		Currently focused item on screen
		curNode: null,

		// activeStack: dijit._Widget[]
		//		List of currently active widgets (focused widget and it's ancestors)
		activeStack: [],

		registerIframe: function(iframe){
			// summary:
			//		Registers listeners on the specified iframe so that any click
			//		or focus event on that iframe (or anything in it) is reported
			//		as a focus/click event on the <iframe> itself.
			// description:
			//		Currently only used by editor.
			// returns:
			//		Handle with remove() method to deregister.
		},

		registerWin: function(targetWindow, effectiveNode){
			// summary:
			//		Registers listeners on the specified window (either the main
			//		window or an iframe's window) to detect when the user has clicked somewhere
			//		or focused somewhere.
			// description:
			//		Users should call registerIframe() instead of this method.
			// targetWindow: Window?
			//		If specified this is the window associated with the iframe,
			//		i.e. iframe.contentWindow.
			// effectiveNode: DOMNode?
			//		If specified, report any focus events inside targetWindow as
			//		an event on effectiveNode, rather than on evt.target.
			// returns:
			//		Handle with remove() method to deregister.
		}
	};
=====*/

	var FocusManager = declare([Stateful, Evented], {
		// curNode: DomNode
		//		Currently focused item on screen
		curNode: null,

		// activeStack: dijit._Widget[]
		//		List of currently active widgets (focused widget and it's ancestors)
		activeStack: [],

		constructor: function(){
			// Don't leave curNode/prevNode pointing to bogus elements
			var check = lang.hitch(this, function(node){
				if(dom.isDescendant(this.curNode, node)){
					this.set("curNode", null);
				}
				if(dom.isDescendant(this.prevNode, node)){
					this.set("prevNode", null);
				}
			});
			aspect.before(domConstruct, "empty", check);
			aspect.before(domConstruct, "destroy", check);
		},

		registerIframe: function(/*DomNode*/ iframe){
			// summary:
			//		Registers listeners on the specified iframe so that any click
			//		or focus event on that iframe (or anything in it) is reported
			//		as a focus/click event on the <iframe> itself.
			// description:
			//		Currently only used by editor.
			// returns:
			//		Handle with remove() method to deregister.
			return this.registerWin(iframe.contentWindow, iframe);
		},

		registerWin: function(/*Window?*/targetWindow, /*DomNode?*/ effectiveNode){
			// summary:
			//		Registers listeners on the specified window (either the main
			//		window or an iframe's window) to detect when the user has clicked somewhere
			//		or focused somewhere.
			// description:
			//		Users should call registerIframe() instead of this method.
			// targetWindow:
			//		If specified this is the window associated with the iframe,
			//		i.e. iframe.contentWindow.
			// effectiveNode:
			//		If specified, report any focus events inside targetWindow as
			//		an event on effectiveNode, rather than on evt.target.
			// returns:
			//		Handle with remove() method to deregister.

			// TODO: make this function private in 2.0; Editor/users should call registerIframe(),

			var _this = this;
			var mousedownListener = function(evt){
				_this._justMouseDowned = true;
				setTimeout(function(){ _this._justMouseDowned = false; }, 0);

				// workaround weird IE bug where the click is on an orphaned node
				// (first time clicking a Select/DropDownButton inside a TooltipDialog)
				if(has("ie") && evt && evt.srcElement && evt.srcElement.parentNode == null){
					return;
				}

				_this._onTouchNode(effectiveNode || evt.target || evt.srcElement, "mouse");
			};

			// Listen for blur and focus events on targetWindow's document.
			// IIRC, I'm using attachEvent() rather than dojo.connect() because focus/blur events don't bubble
			// through dojo.connect(), and also maybe to catch the focus events early, before onfocus handlers
			// fire.
			// Connect to <html> (rather than document) on IE to avoid memory leaks, but document on other browsers because
			// (at least for FF) the focus event doesn't fire on <html> or <body>.
			var doc = has("ie") ? targetWindow.document.documentElement : targetWindow.document;
			if(doc){
				if(has("ie")){
					targetWindow.document.body.attachEvent('onmousedown', mousedownListener);
					var activateListener = function(evt){
						// IE reports that nodes like <body> have gotten focus, even though they have tabIndex=-1,
						// ignore those events
						var tag = evt.srcElement.tagName.toLowerCase();
						if(tag == "#document" || tag == "body"){ return; }

						// Previous code called _onTouchNode() for any activate event on a non-focusable node.   Can
						// probably just ignore such an event as it will be handled by onmousedown handler above, but
						// leaving the code for now.
						if(a11y.isTabNavigable(evt.srcElement)){
							_this._onFocusNode(effectiveNode || evt.srcElement);
						}else{
							_this._onTouchNode(effectiveNode || evt.srcElement);
						}
					};
					doc.attachEvent('onactivate', activateListener);
					var deactivateListener =  function(evt){
						_this._onBlurNode(effectiveNode || evt.srcElement);
					};
					doc.attachEvent('ondeactivate', deactivateListener);

					return {
						remove: function(){
							targetWindow.document.detachEvent('onmousedown', mousedownListener);
							doc.detachEvent('onactivate', activateListener);
							doc.detachEvent('ondeactivate', deactivateListener);
							doc = null;	// prevent memory leak (apparent circular reference via closure)
						}
					};
				}else{
					doc.body.addEventListener('mousedown', mousedownListener, true);
					doc.body.addEventListener('touchstart', mousedownListener, true);
					var focusListener = function(evt){
						_this._onFocusNode(effectiveNode || evt.target);
					};
					doc.addEventListener('focus', focusListener, true);
					var blurListener = function(evt){
						_this._onBlurNode(effectiveNode || evt.target);
					};
					doc.addEventListener('blur', blurListener, true);

					return {
						remove: function(){
							doc.body.removeEventListener('mousedown', mousedownListener, true);
							doc.body.removeEventListener('touchstart', mousedownListener, true);
							doc.removeEventListener('focus', focusListener, true);
							doc.removeEventListener('blur', blurListener, true);
							doc = null;	// prevent memory leak (apparent circular reference via closure)
						}
					};
				}
			}
		},

		_onBlurNode: function(/*DomNode*/ /*===== node =====*/){
			// summary:
			// 		Called when focus leaves a node.
			//		Usually ignored, _unless_ it *isn't* followed by touching another node,
			//		which indicates that we tabbed off the last field on the page,
			//		in which case every widget is marked inactive
			this.set("prevNode", this.curNode);
			this.set("curNode", null);

			if(this._justMouseDowned){
				// the mouse down caused a new widget to be marked as active; this blur event
				// is coming late, so ignore it.
				return;
			}

			// if the blur event isn't followed by a focus event then mark all widgets as inactive.
			if(this._clearActiveWidgetsTimer){
				clearTimeout(this._clearActiveWidgetsTimer);
			}
			this._clearActiveWidgetsTimer = setTimeout(lang.hitch(this, function(){
				delete this._clearActiveWidgetsTimer;
				this._setStack([]);
				this.prevNode = null;
			}), 100);
		},

		_onTouchNode: function(/*DomNode*/ node, /*String*/ by){
			// summary:
			//		Callback when node is focused or mouse-downed
			// node:
			//		The node that was touched.
			// by:
			//		"mouse" if the focus/touch was caused by a mouse down event

			// ignore the recent blurNode event
			if(this._clearActiveWidgetsTimer){
				clearTimeout(this._clearActiveWidgetsTimer);
				delete this._clearActiveWidgetsTimer;
			}

			// compute stack of active widgets (ex: ComboButton --> Menu --> MenuItem)
			var newStack=[];
			try{
				while(node){
					var popupParent = domAttr.get(node, "dijitPopupParent");
					if(popupParent){
						node=registry.byId(popupParent).domNode;
					}else if(node.tagName && node.tagName.toLowerCase() == "body"){
						// is this the root of the document or just the root of an iframe?
						if(node === win.body()){
							// node is the root of the main document
							break;
						}
						// otherwise, find the iframe this node refers to (can't access it via parentNode,
						// need to do this trick instead). window.frameElement is supported in IE/FF/Webkit
						node=winUtils.get(node.ownerDocument).frameElement;
					}else{
						// if this node is the root node of a widget, then add widget id to stack,
						// except ignore clicks on disabled widgets (actually focusing a disabled widget still works,
						// to support MenuItem)
						var id = node.getAttribute && node.getAttribute("widgetId"),
							widget = id && registry.byId(id);
						if(widget && !(by == "mouse" && widget.get("disabled"))){
							newStack.unshift(id);
						}
						node=node.parentNode;
					}
				}
			}catch(e){ /* squelch */ }

			this._setStack(newStack, by);
		},

		_onFocusNode: function(/*DomNode*/ node){
			// summary:
			//		Callback when node is focused

			if(!node){
				return;
			}

			if(node.nodeType == 9){
				// Ignore focus events on the document itself.  This is here so that
				// (for example) clicking the up/down arrows of a spinner
				// (which don't get focus) won't cause that widget to blur. (FF issue)
				return;
			}

			this._onTouchNode(node);

			if(node == this.curNode){ return; }
			this.set("curNode", node);
		},

		_setStack: function(/*String[]*/ newStack, /*String*/ by){
			// summary:
			//		The stack of active widgets has changed.  Send out appropriate events and records new stack.
			// newStack:
			//		array of widget id's, starting from the top (outermost) widget
			// by:
			//		"mouse" if the focus/touch was caused by a mouse down event

			var oldStack = this.activeStack;
			this.set("activeStack", newStack);

			// compare old stack to new stack to see how many elements they have in common
			for(var nCommon=0; nCommon<Math.min(oldStack.length, newStack.length); nCommon++){
				if(oldStack[nCommon] != newStack[nCommon]){
					break;
				}
			}

			var widget;
			// for all elements that have gone out of focus, set focused=false
			for(var i=oldStack.length-1; i>=nCommon; i--){
				widget = registry.byId(oldStack[i]);
				if(widget){
					widget._hasBeenBlurred = true;		// TODO: used by form widgets, should be moved there
					widget.set("focused", false);
					if(widget._focusManager == this){
						widget._onBlur(by);
					}
					this.emit("widget-blur", widget, by);
				}
			}

			// for all element that have come into focus, set focused=true
			for(i=nCommon; i<newStack.length; i++){
				widget = registry.byId(newStack[i]);
				if(widget){
					widget.set("focused", true);
					if(widget._focusManager == this){
						widget._onFocus(by);
					}
					this.emit("widget-focus", widget, by);
				}
			}
		},

		focus: function(node){
			// summary:
			//		Focus the specified node, suppressing errors if they occur
			if(node){
				try{ node.focus(); }catch(e){/*quiet*/}
			}
		}
	});

	var singleton = new FocusManager();

	// register top window and all the iframes it contains
	ready(function(){
		var handle = singleton.registerWin(win.doc.parentWindow || win.doc.defaultView);
		if(has("ie")){
			unload.addOnWindowUnload(function(){
				handle.remove();
				handle = null;
			})
		}
	});

	// Setup dijit.focus as a pointer to the singleton but also (for backwards compatibility)
	// as a function to set focus.
	dijit.focus = function(node){
		singleton.focus(node);	// indirection here allows dijit/_base/focus.js to override behavior
	};
	for(var attr in singleton){
		if(!/^_/.test(attr)){
			dijit.focus[attr] = typeof singleton[attr] == "function" ? lang.hitch(singleton, attr) : singleton[attr];
		}
	}
	singleton.watch(function(attr, oldVal, newVal){
		dijit.focus[attr] = newVal;
	});

	return singleton;
});

},
'dijit/_base/sniff':function(){
define("dijit/_base/sniff", [ "dojo/uacss" ], function(){
	// module:
	//		dijit/_base/sniff
	// summary:
	//		Back compatibility module, new code should require dojo/uacss directly instead of this module.
});

},
'dijit/main':function(){
define("dijit/main", [
	"dojo/_base/kernel"
], function(dojo){
	// module:
	//		dijit
	// summary:
	//		The dijit package main module

	return dojo.dijit;
});

},
'dojox/mobile/RoundRect':function(){
define("dojox/mobile/RoundRect", [
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/_base/window",
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase"
], function(array, declare, win, Contained, Container, WidgetBase){

/*=====
	var Contained = dijit._Contained;
	var Container = dijit._Container;
	var WidgetBase = dijit._WidgetBase;
=====*/

	// module:
	//		dojox/mobile/RoundRect
	// summary:
	//		A simple round rectangle container.

	return declare("dojox.mobile.RoundRect", [WidgetBase, Container, Contained], {
		// summary:
		//		A simple round rectangle container.
		// description:
		//		RoundRect is a simple round rectangle container for any HTML
		//		and/or widgets. You can achieve the same appearance by just
		//		applying the -webkit-border-radius style to a div tag. However,
		//		if you use RoundRect, you can get a round rectangle even on
		//		non-CSS3 browsers such as (older) IE.

		// shadow: Boolean
		//		If true, adds a shadow effect to the container element.
		shadow: false,

		buildRendering: function(){
			this.domNode = this.containerNode = this.srcNodeRef || win.doc.createElement("DIV");
			this.domNode.className = this.shadow ? "mblRoundRect mblShadow" : "mblRoundRect";
		},

		resize: function(){
			// summary:
			//		Calls resize() of each child widget.
			array.forEach(this.getChildren(), function(child){
				if(child.resize){ child.resize(); }
			});
		}
	});
});

},
'dijit/form/_ButtonMixin':function(){
define("dijit/form/_ButtonMixin", [
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.setSelectable
	"dojo/_base/event", // event.stop
	"../registry"		// registry.byNode
], function(declare, dom, event, registry){

// module:
//		dijit/form/_ButtonMixin
// summary:
//		A mixin to add a thin standard API wrapper to a normal HTML button

return declare("dijit.form._ButtonMixin", null, {
	// summary:
	//		A mixin to add a thin standard API wrapper to a normal HTML button
	// description:
	//		A label should always be specified (through innerHTML) or the label attribute.
	//		Attach points:
	//			focusNode (required): this node receives focus
	//			valueNode (optional): this node's value gets submitted with FORM elements
	//			containerNode (optional): this node gets the innerHTML assignment for label
	// example:
	// |	<button data-dojo-type="dijit.form.Button" onClick="...">Hello world</button>
	//
	// example:
	// |	var button1 = new dijit.form.Button({label: "hello world", onClick: foo});
	// |	dojo.body().appendChild(button1.domNode);

	// label: HTML String
	//		Content to display in button.
	label: "",

	// type: [const] String
	//		Type of button (submit, reset, button, checkbox, radio)
	type: "button",

	_onClick: function(/*Event*/ e){
		// summary:
		//		Internal function to handle click actions
		if(this.disabled){
			event.stop(e);
			return false;
		}
		var preventDefault = this.onClick(e) === false; // user click actions
		if(!preventDefault && this.type == "submit" && !(this.valueNode||this.focusNode).form){ // see if a non-form widget needs to be signalled
			for(var node=this.domNode; node.parentNode; node=node.parentNode){
				var widget=registry.byNode(node);
				if(widget && typeof widget._onSubmit == "function"){
					widget._onSubmit(e);
					preventDefault = true;
					break;
				}
			}
		}
		if(preventDefault){
			e.preventDefault();
		}
		return !preventDefault;
	},

	postCreate: function(){
		this.inherited(arguments);
		dom.setSelectable(this.focusNode, false);
	},

	onClick: function(/*Event*/ /*===== e =====*/){
		// summary:
		//		Callback for when button is clicked.
		//		If type="submit", return true to perform submit, or false to cancel it.
		// type:
		//		callback
		return true;		// Boolean
	},

	_setLabelAttr: function(/*String*/ content){
		// summary:
		//		Hook for set('label', ...) to work.
		// description:
		//		Set the label (text) of the button; takes an HTML string.
		this._set("label", content);
		(this.containerNode||this.focusNode).innerHTML = content;
	}
});

});

},
'dijit/_base/typematic':function(){
define("dijit/_base/typematic", ["../typematic"], function(){
	// for back-compat, just loads top level module
});

},
'dojox/mobile/RoundRectCategory':function(){
define("dojox/mobile/RoundRectCategory", [
	"dojo/_base/declare",
	"dojo/_base/window",
	"dijit/_Contained",
	"dijit/_WidgetBase"
], function(declare, win, Contained, WidgetBase){

/*=====
	var Contained = dijit._Contained;
	var WidgetBase = dijit._WidgetBase;
=====*/

	// module:
	//		dojox/mobile/RoundRectCategory
	// summary:
	//		A category header for a rounded rectangle list.

	return declare("dojox.mobile.RoundRectCategory", [WidgetBase, Contained],{
		// summary:
		//		A category header for a rounded rectangle list.

		// label: String
		//		A label text for the widget.
		label: "",

		buildRendering: function(){
			this.domNode = this.containerNode = this.srcNodeRef || win.doc.createElement("H2");
			this.domNode.className = "mblRoundRectCategory";
			if(!this.label){
				this.label = this.domNode.innerHTML;
			}
		},

		_setLabelAttr: function(/*String*/label){
			this.label = label;
			this.domNode.innerHTML = this._cv ? this._cv(label) : label;
		}
	});

});

},
'dojox/mobile/app/TextBox':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/TextBox"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.TextBox");
dojo.deprecated("dojox.mobile.app.TextBox is deprecated", "dojox.mobile.app.TextBox moved to dojox.mobile.TextBox", 1.8);

dojo.require("dojox.mobile.TextBox");

dojox.mobile.app.TextBox = dojox.mobile.TextBox;
});

},
'dojox/mobile/app/SceneAssistant':function(){
// wrapped by build app
define(["dijit","dojo","dojox"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.SceneAssistant");
dojo.experimental("dojox.mobile.app.SceneAssistant");

dojo.declare("dojox.mobile.app.SceneAssistant", null, {
	// summary:
	//    The base class for all scene assistants.

	constructor: function(){

	},

	setup: function(){
		// summary:
		//    Called to set up the widget.  The UI is not visible at this time

	},

	activate: function(params){
		// summary:
		//    Called each time the scene becomes visible.  This can be as a result
		//    of a new scene being created, or a subsequent scene being destroyed
		//    and control transferring back to this scene assistant.
		// params:
		//    Optional paramters, only passed when a subsequent scene pops itself
		//    off the stack and passes back data.
	},

	deactivate: function(){
		// summary:
		//    Called each time the scene becomes invisible.  This can be as a result
		//    of it being popped off the stack and destroyed,
		//    or another scene being created and pushed on top of it on the stack
	},

	destroy: function(){
	
		var children =
			dojo.query("> [widgetId]", this.containerNode).map(dijit.byNode);
		dojo.forEach(children, function(child){ child.destroyRecursive(); });
	
		this.disconnect();
	},

	connect: function(obj, method, callback){
		if(!this._connects){
			this._connects = [];
		}
		this._connects.push(dojo.connect(obj, method, callback));
	},

	disconnect: function(){
		dojo.forEach(this._connects, dojo.disconnect);
		this._connects = [];
	}
});


});

},
'dijit/_base/popup':function(){
define("dijit/_base/popup", [
	"dojo/dom-class", // domClass.contains
	"../popup",
	"../BackgroundIframe"	// just loading for back-compat, in case client code is referencing it
], function(domClass, popup){

// module:
//		dijit/_base/popup
// summary:
//		Old module for popups, new code should use dijit/popup directly


// Hack support for old API passing in node instead of a widget (to various methods)
var origCreateWrapper = popup._createWrapper;
popup._createWrapper = function(widget){
	if(!widget.declaredClass){
		// make fake widget to pass to new API
		widget = {
			_popupWrapper: (widget.parentNode && domClass.contains(widget.parentNode, "dijitPopup")) ?
				widget.parentNode : null,
			domNode: widget,
			destroy: function(){}
		};
	}
	return origCreateWrapper.call(this, widget);
};

// Support old format of orient parameter
var origOpen = popup.open;
popup.open = function(/*dijit.popup.__OpenArgs*/ args){
	// Convert old hash structure (ex: {"BL": "TL", ...}) of orient to format compatible w/new popup.open() API.
	// Don't do conversion for:
	//		- null parameter (that means to use the default positioning)
	//		- "R" or "L" strings used to indicate positioning for context menus (when there is no around node)
	//		- new format, ex: ["below", "above"]
	//		- return value from deprecated dijit.getPopupAroundAlignment() method,
	//			ex: ["below", "above"]
	if(args.orient && typeof args.orient != "string" && !("length" in args.orient)){
		var ary = [];
		for(var key in args.orient){
			ary.push({aroundCorner: key, corner: args.orient[key]});
		}
		args.orient = ary;
	}

	return origOpen.call(this, args);
};

return popup;
});

},
'dojox/mobile/transition':function(){
define([
	"dojo/_base/Deferred",
	"dojo/_base/config"
], function(Deferred, config){
	/* summary: this is the wrapper module which load
	 * dojox/css3/transit conditionally. If mblCSS3Transition
	 * is set to 'dojox/css3/transit', it will be loaded as
	 * the module to conduct the view transition.
	 */
	if(config['mblCSS3Transition']){
		//require dojox/css3/transit and resolve it as the result of transitDeferred.
		var transitDeferred = new Deferred();
		require([config['mblCSS3Transition']], function(transit){
			transitDeferred.resolve(transit);
		});
		return transitDeferred;
	}
	return null;
});

},
'dijit/_base/wai':function(){
define("dijit/_base/wai", [
	"dojo/dom-attr", // domAttr.attr
	"dojo/_base/lang", // lang.mixin
	"..",	// export symbols to dijit
	"../hccss"			// not using this module directly, but loading it sets CSS flag on <html>
], function(domAttr, lang, dijit){

	// module:
	//		dijit/_base/wai
	// summary:
	//		Deprecated methods for setting/getting wai roles and states.
	//		New code should call setAttribute()/getAttribute() directly.
	//
	//		Also loads hccss to apply dijit_a11y class to root node if machine is in high-contrast mode.

	lang.mixin(dijit, {
		hasWaiRole: function(/*Element*/ elem, /*String?*/ role){
			// summary:
			//		Determines if an element has a particular role.
			// returns:
			//		True if elem has the specific role attribute and false if not.
			// 		For backwards compatibility if role parameter not provided,
			// 		returns true if has a role
			var waiRole = this.getWaiRole(elem);
			return role ? (waiRole.indexOf(role) > -1) : (waiRole.length > 0);
		},

		getWaiRole: function(/*Element*/ elem){
			// summary:
			//		Gets the role for an element (which should be a wai role).
			// returns:
			//		The role of elem or an empty string if elem
			//		does not have a role.
			 return lang.trim((domAttr.get(elem, "role") || "").replace("wairole:",""));
		},

		setWaiRole: function(/*Element*/ elem, /*String*/ role){
			// summary:
			//		Sets the role on an element.
			// description:
			//		Replace existing role attribute with new role.

			domAttr.set(elem, "role", role);
		},

		removeWaiRole: function(/*Element*/ elem, /*String*/ role){
			// summary:
			//		Removes the specified role from an element.
			// 		Removes role attribute if no specific role provided (for backwards compat.)

			var roleValue = domAttr.get(elem, "role");
			if(!roleValue){ return; }
			if(role){
				var t = lang.trim((" " + roleValue + " ").replace(" " + role + " ", " "));
				domAttr.set(elem, "role", t);
			}else{
				elem.removeAttribute("role");
			}
		},

		hasWaiState: function(/*Element*/ elem, /*String*/ state){
			// summary:
			//		Determines if an element has a given state.
			// description:
			//		Checks for an attribute called "aria-"+state.
			// returns:
			//		true if elem has a value for the given state and
			//		false if it does not.

			return elem.hasAttribute ? elem.hasAttribute("aria-"+state) : !!elem.getAttribute("aria-"+state);
		},

		getWaiState: function(/*Element*/ elem, /*String*/ state){
			// summary:
			//		Gets the value of a state on an element.
			// description:
			//		Checks for an attribute called "aria-"+state.
			// returns:
			//		The value of the requested state on elem
			//		or an empty string if elem has no value for state.

			return elem.getAttribute("aria-"+state) || "";
		},

		setWaiState: function(/*Element*/ elem, /*String*/ state, /*String*/ value){
			// summary:
			//		Sets a state on an element.
			// description:
			//		Sets an attribute called "aria-"+state.

			elem.setAttribute("aria-"+state, value);
		},

		removeWaiState: function(/*Element*/ elem, /*String*/ state){
			// summary:
			//		Removes a state from an element.
			// description:
			//		Sets an attribute called "aria-"+state.

			elem.removeAttribute("aria-"+state);
		}
	});

	return dijit;
});

},
'dojo/window':function(){
define(["./_base/kernel", "./_base/lang", "./_base/sniff", "./_base/window", "./dom", "./dom-geometry", "./dom-style"], function(dojo, lang, has, baseWindow, dom, geom, style) {
	// module:
	//		dojo/window
	// summary:
	//		TODOC

lang.getObject("window", true, dojo);

dojo.window.getBox = function(){
	// summary:
	//		Returns the dimensions and scroll position of the viewable area of a browser window

	var scrollRoot = (baseWindow.doc.compatMode == 'BackCompat') ? baseWindow.body() : baseWindow.doc.documentElement;

	// get scroll position
	var scroll = geom.docScroll(); // scrollRoot.scrollTop/Left should work

	var uiWindow = baseWindow.doc.parentWindow || baseWindow.doc.defaultView;   // use UI window, not dojo.global window
	// dojo.global.innerWidth||dojo.global.innerHeight is for mobile
	return {
		l: scroll.x,
		t: scroll.y,
		w: uiWindow.innerWidth || scrollRoot.clientWidth,
		h: uiWindow.innerHeight || scrollRoot.clientHeight
	};
};

dojo.window.get = function(doc){
	// summary:
	// 		Get window object associated with document doc

	// In some IE versions (at least 6.0), document.parentWindow does not return a
	// reference to the real window object (maybe a copy), so we must fix it as well
	// We use IE specific execScript to attach the real window reference to
	// document._parentWindow for later use
	if(has("ie") && window !== document.parentWindow){
		/*
		In IE 6, only the variable "window" can be used to connect events (others
		may be only copies).
		*/
		doc.parentWindow.execScript("document._parentWindow = window;", "Javascript");
		//to prevent memory leak, unset it after use
		//another possibility is to add an onUnload handler which seems overkill to me (liucougar)
		var win = doc._parentWindow;
		doc._parentWindow = null;
		return win;	//	Window
	}

	return doc.parentWindow || doc.defaultView;	//	Window
};

dojo.window.scrollIntoView = function(/*DomNode*/ node, /*Object?*/ pos){
	// summary:
	//		Scroll the passed node into view, if it is not already.

	// don't rely on node.scrollIntoView working just because the function is there

	try{ // catch unexpected/unrecreatable errors (#7808) since we can recover using a semi-acceptable native method
		node = dom.byId(node);
		var doc = node.ownerDocument || baseWindow.doc,
			body = doc.body || baseWindow.body(),
			html = doc.documentElement || body.parentNode,
			isIE = has("ie"), isWK = has("webkit");
		// if an untested browser, then use the native method
		if((!(has("mozilla") || isIE || isWK || has("opera")) || node == body || node == html) && (typeof node.scrollIntoView != "undefined")){
			node.scrollIntoView(false); // short-circuit to native if possible
			return;
		}
		var backCompat = doc.compatMode == 'BackCompat',
			clientAreaRoot = (isIE >= 9 && node.ownerDocument.parentWindow.frameElement)
				? ((html.clientHeight > 0 && html.clientWidth > 0 && (body.clientHeight == 0 || body.clientWidth == 0 || body.clientHeight > html.clientHeight || body.clientWidth > html.clientWidth)) ? html : body)
				: (backCompat ? body : html),
			scrollRoot = isWK ? body : clientAreaRoot,
			rootWidth = clientAreaRoot.clientWidth,
			rootHeight = clientAreaRoot.clientHeight,
			rtl = !geom.isBodyLtr(),
			nodePos = pos || geom.position(node),
			el = node.parentNode,
			isFixed = function(el){
				return ((isIE <= 6 || (isIE && backCompat))? false : (style.get(el, 'position').toLowerCase() == "fixed"));
			};
		if(isFixed(node)){ return; } // nothing to do

		while(el){
			if(el == body){ el = scrollRoot; }
			var elPos = geom.position(el),
				fixedPos = isFixed(el);

			if(el == scrollRoot){
				elPos.w = rootWidth; elPos.h = rootHeight;
				if(scrollRoot == html && isIE && rtl){ elPos.x += scrollRoot.offsetWidth-elPos.w; } // IE workaround where scrollbar causes negative x
				if(elPos.x < 0 || !isIE){ elPos.x = 0; } // IE can have values > 0
				if(elPos.y < 0 || !isIE){ elPos.y = 0; }
			}else{
				var pb = geom.getPadBorderExtents(el);
				elPos.w -= pb.w; elPos.h -= pb.h; elPos.x += pb.l; elPos.y += pb.t;
				var clientSize = el.clientWidth,
					scrollBarSize = elPos.w - clientSize;
				if(clientSize > 0 && scrollBarSize > 0){
					elPos.w = clientSize;
					elPos.x += (rtl && (isIE || el.clientLeft > pb.l/*Chrome*/)) ? scrollBarSize : 0;
				}
				clientSize = el.clientHeight;
				scrollBarSize = elPos.h - clientSize;
				if(clientSize > 0 && scrollBarSize > 0){
					elPos.h = clientSize;
				}
			}
			if(fixedPos){ // bounded by viewport, not parents
				if(elPos.y < 0){
					elPos.h += elPos.y; elPos.y = 0;
				}
				if(elPos.x < 0){
					elPos.w += elPos.x; elPos.x = 0;
				}
				if(elPos.y + elPos.h > rootHeight){
					elPos.h = rootHeight - elPos.y;
				}
				if(elPos.x + elPos.w > rootWidth){
					elPos.w = rootWidth - elPos.x;
				}
			}
			// calculate overflow in all 4 directions
			var l = nodePos.x - elPos.x, // beyond left: < 0
				t = nodePos.y - Math.max(elPos.y, 0), // beyond top: < 0
				r = l + nodePos.w - elPos.w, // beyond right: > 0
				bot = t + nodePos.h - elPos.h; // beyond bottom: > 0
			if(r * l > 0){
				var s = Math[l < 0? "max" : "min"](l, r);
				if(rtl && ((isIE == 8 && !backCompat) || isIE >= 9)){ s = -s; }
				nodePos.x += el.scrollLeft;
				el.scrollLeft += s;
				nodePos.x -= el.scrollLeft;
			}
			if(bot * t > 0){
				nodePos.y += el.scrollTop;
				el.scrollTop += Math[t < 0? "max" : "min"](t, bot);
				nodePos.y -= el.scrollTop;
			}
			el = (el != scrollRoot) && !fixedPos && el.parentNode;
		}
	}catch(error){
		console.error('scrollIntoView: ' + error);
		node.scrollIntoView(false);
	}
};

return dojo.window;
});

},
'dojox/mobile/EdgeToEdgeList':function(){
define("dojox/mobile/EdgeToEdgeList", [
	"dojo/_base/declare",
	"./RoundRectList"
], function(declare, RoundRectList){

/*=====
	var RoundRectList = dojox.mobile.RoundRectList;
=====*/

	// module:
	//		dojox/mobile/EdgeToEdgeCategory
	// summary:
	//		An edge-to-edge layout list.

	return declare("dojox.mobile.EdgeToEdgeList", RoundRectList, {
		// summary:
		//		An edge-to-edge layout list.
		// description:
		//		EdgeToEdgeList is an edge-to-edge layout list, which displays
		//		all items in equally sized rows. Each item must be
		//		dojox.mobile.ListItem.

		buildRendering: function(){
			this.inherited(arguments);
			this.domNode.className = "mblEdgeToEdgeList";
		}
	});
});

},
'dijit/popup':function(){
define("dijit/popup", [
	"dojo/_base/array", // array.forEach array.some
	"dojo/aspect",
	"dojo/_base/connect",	// connect._keypress
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.isDescendant
	"dojo/dom-attr", // domAttr.set
	"dojo/dom-construct", // domConstruct.create domConstruct.destroy
	"dojo/dom-geometry", // domGeometry.isBodyLtr
	"dojo/dom-style", // domStyle.set
	"dojo/_base/event", // event.stop
	"dojo/keys",
	"dojo/_base/lang", // lang.hitch
	"dojo/on",
	"dojo/_base/sniff", // has("ie") has("mozilla")
	"dojo/_base/window", // win.body
	"./place",
	"./BackgroundIframe",
	"."	// dijit (defining dijit.popup to match API doc)
], function(array, aspect, connect, declare, dom, domAttr, domConstruct, domGeometry, domStyle, event, keys, lang, on, has, win,
			place, BackgroundIframe, dijit){

	// module:
	//		dijit/popup
	// summary:
	//		Used to show drop downs (ex: the select list of a ComboBox)
	//		or popups (ex: right-click context menus)


	/*=====
	dijit.popup.__OpenArgs = function(){
		// popup: Widget
		//		widget to display
		// parent: Widget
		//		the button etc. that is displaying this popup
		// around: DomNode
		//		DOM node (typically a button); place popup relative to this node.  (Specify this *or* "x" and "y" parameters.)
		// x: Integer
		//		Absolute horizontal position (in pixels) to place node at.  (Specify this *or* "around" parameter.)
		// y: Integer
		//		Absolute vertical position (in pixels) to place node at.  (Specify this *or* "around" parameter.)
		// orient: Object|String
		//		When the around parameter is specified, orient should be a list of positions to try, ex:
		//	|	[ "below", "above" ]
		//		For backwards compatibility it can also be an (ordered) hash of tuples of the form
		//		(around-node-corner, popup-node-corner), ex:
		//	|	{ "BL": "TL", "TL": "BL" }
		//		where BL means "bottom left" and "TL" means "top left", etc.
		//
		//		dijit.popup.open() tries to position the popup according to each specified position, in order,
		//		until the popup appears fully within the viewport.
		//
		//		The default value is ["below", "above"]
		//
		//		When an (x,y) position is specified rather than an around node, orient is either
		//		"R" or "L".  R (for right) means that it tries to put the popup to the right of the mouse,
		//		specifically positioning the popup's top-right corner at the mouse position, and if that doesn't
		//		fit in the viewport, then it tries, in order, the bottom-right corner, the top left corner,
		//		and the top-right corner.
		// onCancel: Function
		//		callback when user has canceled the popup by
		//			1. hitting ESC or
		//			2. by using the popup widget's proprietary cancel mechanism (like a cancel button in a dialog);
		//			   i.e. whenever popupWidget.onCancel() is called, args.onCancel is called
		// onClose: Function
		//		callback whenever this popup is closed
		// onExecute: Function
		//		callback when user "executed" on the popup/sub-popup by selecting a menu choice, etc. (top menu only)
		// padding: dijit.__Position
		//		adding a buffer around the opening position. This is only useful when around is not set.
		this.popup = popup;
		this.parent = parent;
		this.around = around;
		this.x = x;
		this.y = y;
		this.orient = orient;
		this.onCancel = onCancel;
		this.onClose = onClose;
		this.onExecute = onExecute;
		this.padding = padding;
	}
	=====*/

	/*=====
	dijit.popup = {
		// summary:
		//		Used to show drop downs (ex: the select list of a ComboBox)
		//		or popups (ex: right-click context menus).
		//
		//		Access via require(["dijit/popup"], function(popup){ ... }).

		moveOffScreen: function(widget){
			// summary:
			//		Moves the popup widget off-screen.
			//		Do not use this method to hide popups when not in use, because
			//		that will create an accessibility issue: the offscreen popup is
			//		still in the tabbing order.
			// widget: dijit._WidgetBase
			//		The widget
		},

		hide: function(widget){
			// summary:
			//		Hide this popup widget (until it is ready to be shown).
			//		Initialization for widgets that will be used as popups
			//
			// 		Also puts widget inside a wrapper DIV (if not already in one)
			//
			//		If popup widget needs to layout it should
			//		do so when it is made visible, and popup._onShow() is called.
			// widget: dijit._WidgetBase
			//		The widget
		},

		open: function(args){
			// summary:
			//		Popup the widget at the specified position
			// example:
			//		opening at the mouse position
			//		|		popup.open({popup: menuWidget, x: evt.pageX, y: evt.pageY});
			// example:
			//		opening the widget as a dropdown
			//		|		popup.open({parent: this, popup: menuWidget, around: this.domNode, onClose: function(){...}});
			//
			//		Note that whatever widget called dijit.popup.open() should also listen to its own _onBlur callback
			//		(fired from _base/focus.js) to know that focus has moved somewhere else and thus the popup should be closed.
			// args: dijit.popup.__OpenArgs
			//		Parameters
			return {};	// Object specifying which position was chosen
		},

		close: function(popup){
			// summary:
			//		Close specified popup and any popups that it parented.
			//		If no popup is specified, closes all popups.
			// widget: dijit._WidgetBase?
			//		The widget, optional
		}
	};
	=====*/

	var PopupManager = declare(null, {
		// _stack: dijit._Widget[]
		//		Stack of currently popped up widgets.
		//		(someone opened _stack[0], and then it opened _stack[1], etc.)
		_stack: [],

		// _beginZIndex: Number
		//		Z-index of the first popup.   (If first popup opens other
		//		popups they get a higher z-index.)
		_beginZIndex: 1000,

		_idGen: 1,

		_createWrapper: function(/*Widget*/ widget){
			// summary:
			//		Initialization for widgets that will be used as popups.
			//		Puts widget inside a wrapper DIV (if not already in one),
			//		and returns pointer to that wrapper DIV.

			var wrapper = widget._popupWrapper,
				node = widget.domNode;

			if(!wrapper){
				// Create wrapper <div> for when this widget [in the future] will be used as a popup.
				// This is done early because of IE bugs where creating/moving DOM nodes causes focus
				// to go wonky, see tests/robot/Toolbar.html to reproduce
				wrapper = domConstruct.create("div",{
					"class":"dijitPopup",
					style:{ display: "none"},
					role: "presentation"
				}, win.body());
				wrapper.appendChild(node);

				var s = node.style;
				s.display = "";
				s.visibility = "";
				s.position = "";
				s.top = "0px";

				widget._popupWrapper = wrapper;
				aspect.after(widget, "destroy", function(){
					domConstruct.destroy(wrapper);
					delete widget._popupWrapper;
				});
			}

			return wrapper;
		},

		moveOffScreen: function(/*Widget*/ widget){
			// summary:
			//		Moves the popup widget off-screen.
			//		Do not use this method to hide popups when not in use, because
			//		that will create an accessibility issue: the offscreen popup is
			//		still in the tabbing order.

			// Create wrapper if not already there
			var wrapper = this._createWrapper(widget);

			domStyle.set(wrapper, {
				visibility: "hidden",
				top: "-9999px",		// prevent transient scrollbar causing misalign (#5776), and initial flash in upper left (#10111)
				display: ""
			});
		},

		hide: function(/*Widget*/ widget){
			// summary:
			//		Hide this popup widget (until it is ready to be shown).
			//		Initialization for widgets that will be used as popups
			//
			// 		Also puts widget inside a wrapper DIV (if not already in one)
			//
			//		If popup widget needs to layout it should
			//		do so when it is made visible, and popup._onShow() is called.

			// Create wrapper if not already there
			var wrapper = this._createWrapper(widget);

			domStyle.set(wrapper, "display", "none");
		},

		getTopPopup: function(){
			// summary:
			//		Compute the closest ancestor popup that's *not* a child of another popup.
			//		Ex: For a TooltipDialog with a button that spawns a tree of menus, find the popup of the button.
			var stack = this._stack;
			for(var pi=stack.length-1; pi > 0 && stack[pi].parent === stack[pi-1].widget; pi--){
				/* do nothing, just trying to get right value for pi */
			}
			return stack[pi];
		},

		open: function(/*dijit.popup.__OpenArgs*/ args){
			// summary:
			//		Popup the widget at the specified position
			//
			// example:
			//		opening at the mouse position
			//		|		popup.open({popup: menuWidget, x: evt.pageX, y: evt.pageY});
			//
			// example:
			//		opening the widget as a dropdown
			//		|		popup.open({parent: this, popup: menuWidget, around: this.domNode, onClose: function(){...}});
			//
			//		Note that whatever widget called dijit.popup.open() should also listen to its own _onBlur callback
			//		(fired from _base/focus.js) to know that focus has moved somewhere else and thus the popup should be closed.

			var stack = this._stack,
				widget = args.popup,
				orient = args.orient || ["below", "below-alt", "above", "above-alt"],
				ltr = args.parent ? args.parent.isLeftToRight() : domGeometry.isBodyLtr(),
				around = args.around,
				id = (args.around && args.around.id) ? (args.around.id+"_dropdown") : ("popup_"+this._idGen++);

			// If we are opening a new popup that isn't a child of a currently opened popup, then
			// close currently opened popup(s).   This should happen automatically when the old popups
			// gets the _onBlur() event, except that the _onBlur() event isn't reliable on IE, see [22198].
			while(stack.length && (!args.parent || !dom.isDescendant(args.parent.domNode, stack[stack.length-1].widget.domNode))){
				this.close(stack[stack.length-1].widget);
			}

			// Get pointer to popup wrapper, and create wrapper if it doesn't exist
			var wrapper = this._createWrapper(widget);


			domAttr.set(wrapper, {
				id: id,
				style: {
					zIndex: this._beginZIndex + stack.length
				},
				"class": "dijitPopup " + (widget.baseClass || widget["class"] || "").split(" ")[0] +"Popup",
				dijitPopupParent: args.parent ? args.parent.id : ""
			});

			if(has("ie") || has("mozilla")){
				if(!widget.bgIframe){
					// setting widget.bgIframe triggers cleanup in _Widget.destroy()
					widget.bgIframe = new BackgroundIframe(wrapper);
				}
			}

			// position the wrapper node and make it visible
			var best = around ?
				place.around(wrapper, around, orient, ltr, widget.orient ? lang.hitch(widget, "orient") : null) :
				place.at(wrapper, args, orient == 'R' ? ['TR','BR','TL','BL'] : ['TL','BL','TR','BR'], args.padding);

			wrapper.style.display = "";
			wrapper.style.visibility = "visible";
			widget.domNode.style.visibility = "visible";	// counteract effects from _HasDropDown

			var handlers = [];

			// provide default escape and tab key handling
			// (this will work for any widget, not just menu)
			handlers.push(on(wrapper, connect._keypress, lang.hitch(this, function(evt){
				if(evt.charOrCode == keys.ESCAPE && args.onCancel){
					event.stop(evt);
					args.onCancel();
				}else if(evt.charOrCode === keys.TAB){
					event.stop(evt);
					var topPopup = this.getTopPopup();
					if(topPopup && topPopup.onCancel){
						topPopup.onCancel();
					}
				}
			})));

			// watch for cancel/execute events on the popup and notify the caller
			// (for a menu, "execute" means clicking an item)
			if(widget.onCancel && args.onCancel){
				handlers.push(widget.on("cancel", args.onCancel));
			}

			handlers.push(widget.on(widget.onExecute ? "execute" : "change", lang.hitch(this, function(){
				var topPopup = this.getTopPopup();
				if(topPopup && topPopup.onExecute){
					topPopup.onExecute();
				}
			})));

			stack.push({
				widget: widget,
				parent: args.parent,
				onExecute: args.onExecute,
				onCancel: args.onCancel,
				onClose: args.onClose,
				handlers: handlers
			});

			if(widget.onOpen){
				// TODO: in 2.0 standardize onShow() (used by StackContainer) and onOpen() (used here)
				widget.onOpen(best);
			}

			return best;
		},

		close: function(/*Widget?*/ popup){
			// summary:
			//		Close specified popup and any popups that it parented.
			//		If no popup is specified, closes all popups.

			var stack = this._stack;

			// Basically work backwards from the top of the stack closing popups
			// until we hit the specified popup, but IIRC there was some issue where closing
			// a popup would cause others to close too.  Thus if we are trying to close B in [A,B,C]
			// closing C might close B indirectly and then the while() condition will run where stack==[A]...
			// so the while condition is constructed defensively.
			while((popup && array.some(stack, function(elem){return elem.widget == popup;})) ||
				(!popup && stack.length)){
				var top = stack.pop(),
					widget = top.widget,
					onClose = top.onClose;

				if(widget.onClose){
					// TODO: in 2.0 standardize onHide() (used by StackContainer) and onClose() (used here)
					widget.onClose();
				}

				var h;
				while(h = top.handlers.pop()){ h.remove(); }

				// Hide the widget and it's wrapper unless it has already been destroyed in above onClose() etc.
				if(widget && widget.domNode){
					this.hide(widget);
				}

				if(onClose){
					onClose();
				}
			}
		}
	});

	return (dijit.popup = new PopupManager());
});

},
'dojox/mobile/uacss':function(){
define("dojox/mobile/uacss", [
	"dojo/_base/kernel",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojox/mobile/sniff"
], function(dojo, lang, win, has){
	win.doc.documentElement.className += lang.trim([
		has("bb") ? "dj_bb" : "",
		has("android") ? "dj_android" : "",
		has("iphone") ? "dj_iphone" : "",
		has("ipod") ? "dj_ipod" : "",
		has("ipad") ? "dj_ipad" : ""
	].join(" ").replace(/ +/g," "));
	return dojo;
});

},
'dijit/_base/window':function(){
define("dijit/_base/window", [
	"dojo/window", // windowUtils.get
	".."	// export symbol to dijit
], function(windowUtils, dijit){
	// module:
	//		dijit/_base/window
	// summary:
	//		Back compatibility module, new code should use windowUtils directly instead of using this module.

	dijit.getDocumentWindow = function(doc){
		return windowUtils.get(doc);
	};
});

},
'dijit/_WidgetBase':function(){
define("dijit/_WidgetBase", [
	"require",			// require.toUrl
	"dojo/_base/array", // array.forEach array.map
	"dojo/aspect",
	"dojo/_base/config", // config.blankGif
	"dojo/_base/connect", // connect.connect
	"dojo/_base/declare", // declare
	"dojo/dom", // dom.byId
	"dojo/dom-attr", // domAttr.set domAttr.remove
	"dojo/dom-class", // domClass.add domClass.replace
	"dojo/dom-construct", // domConstruct.create domConstruct.destroy domConstruct.place
	"dojo/dom-geometry",	// isBodyLtr
	"dojo/dom-style", // domStyle.set, domStyle.get
	"dojo/_base/kernel",
	"dojo/_base/lang", // mixin(), isArray(), etc.
	"dojo/on",
	"dojo/ready",
	"dojo/Stateful", // Stateful
	"dojo/topic",
	"dojo/_base/window", // win.doc.createTextNode
	"./registry"	// registry.getUniqueId(), registry.findWidgets()
], function(require, array, aspect, config, connect, declare,
			dom, domAttr, domClass, domConstruct, domGeometry, domStyle, kernel,
			lang, on, ready, Stateful, topic, win, registry){

/*=====
var Stateful = dojo.Stateful;
=====*/

// module:
//		dijit/_WidgetBase
// summary:
//		Future base class for all Dijit widgets.

// For back-compat, remove in 2.0.
if(!kernel.isAsync){
	ready(0, function(){
		var requires = ["dijit/_base/manager"];
		require(requires);	// use indirection so modules not rolled into a build
	});
}

// Nested hash listing attributes for each tag, all strings in lowercase.
// ex: {"div": {"style": true, "tabindex" true}, "form": { ...
var tagAttrs = {};
function getAttrs(obj){
	var ret = {};
	for(var attr in obj){
		ret[attr.toLowerCase()] = true;
	}
	return ret;
}

function nonEmptyAttrToDom(attr){
	// summary:
	//		Returns a setter function that copies the attribute to this.domNode,
	//		or removes the attribute from this.domNode, depending on whether the
	//		value is defined or not.
	return function(val){
		domAttr[val ? "set" : "remove"](this.domNode, attr, val);
		this._set(attr, val);
	};
}

return declare("dijit._WidgetBase", Stateful, {
	// summary:
	//		Future base class for all Dijit widgets.
	// description:
	//		Future base class for all Dijit widgets.
	//		_Widget extends this class adding support for various features needed by desktop.
	//
	//		Provides stubs for widget lifecycle methods for subclasses to extend, like postMixInProperties(), buildRendering(),
	//		postCreate(), startup(), and destroy(), and also public API methods like set(), get(), and watch().
	//
	//		Widgets can provide custom setters/getters for widget attributes, which are called automatically by set(name, value).
	//		For an attribute XXX, define methods _setXXXAttr() and/or _getXXXAttr().
	//
	//		_setXXXAttr can also be a string/hash/array mapping from a widget attribute XXX to the widget's DOMNodes:
	//
	//		- DOM node attribute
	// |		_setFocusAttr: {node: "focusNode", type: "attribute"}
	// |		_setFocusAttr: "focusNode"	(shorthand)
	// |		_setFocusAttr: ""		(shorthand, maps to this.domNode)
	// 		Maps this.focus to this.focusNode.focus, or (last example) this.domNode.focus
	//
	//		- DOM node innerHTML
	//	|		_setTitleAttr: { node: "titleNode", type: "innerHTML" }
	//		Maps this.title to this.titleNode.innerHTML
	//
	//		- DOM node innerText
	//	|		_setTitleAttr: { node: "titleNode", type: "innerText" }
	//		Maps this.title to this.titleNode.innerText
	//
	//		- DOM node CSS class
	// |		_setMyClassAttr: { node: "domNode", type: "class" }
	//		Maps this.myClass to this.domNode.className
	//
	//		If the value of _setXXXAttr is an array, then each element in the array matches one of the
	//		formats of the above list.
	//
	//		If the custom setter is null, no action is performed other than saving the new value
	//		in the widget (in this).
	//
	//		If no custom setter is defined for an attribute, then it will be copied
	//		to this.focusNode (if the widget defines a focusNode), or this.domNode otherwise.
	//		That's only done though for attributes that match DOMNode attributes (title,
	//		alt, aria-labelledby, etc.)

	// id: [const] String
	//		A unique, opaque ID string that can be assigned by users or by the
	//		system. If the developer passes an ID which is known not to be
	//		unique, the specified ID is ignored and the system-generated ID is
	//		used instead.
	id: "",
	_setIdAttr: "domNode",	// to copy to this.domNode even for auto-generated id's

	// lang: [const] String
	//		Rarely used.  Overrides the default Dojo locale used to render this widget,
	//		as defined by the [HTML LANG](http://www.w3.org/TR/html401/struct/dirlang.html#adef-lang) attribute.
	//		Value must be among the list of locales specified during by the Dojo bootstrap,
	//		formatted according to [RFC 3066](http://www.ietf.org/rfc/rfc3066.txt) (like en-us).
	lang: "",
	// set on domNode even when there's a focus node.   but don't set lang="", since that's invalid.
	_setLangAttr: nonEmptyAttrToDom("lang"),

	// dir: [const] String
	//		Bi-directional support, as defined by the [HTML DIR](http://www.w3.org/TR/html401/struct/dirlang.html#adef-dir)
	//		attribute. Either left-to-right "ltr" or right-to-left "rtl".  If undefined, widgets renders in page's
	//		default direction.
	dir: "",
	// set on domNode even when there's a focus node.   but don't set dir="", since that's invalid.
	_setDirAttr: nonEmptyAttrToDom("dir"),	// to set on domNode even when there's a focus node

	// textDir: String
	//		Bi-directional support,	the main variable which is responsible for the direction of the text.
	//		The text direction can be different than the GUI direction by using this parameter in creation
	//		of a widget.
	// 		Allowed values:
	//			1. "ltr"
	//			2. "rtl"
	//			3. "auto" - contextual the direction of a text defined by first strong letter.
	//		By default is as the page direction.
	textDir: "",

	// class: String
	//		HTML class attribute
	"class": "",
	_setClassAttr: { node: "domNode", type: "class" },

	// style: String||Object
	//		HTML style attributes as cssText string or name/value hash
	style: "",

	// title: String
	//		HTML title attribute.
	//
	//		For form widgets this specifies a tooltip to display when hovering over
	//		the widget (just like the native HTML title attribute).
	//
	//		For TitlePane or for when this widget is a child of a TabContainer, AccordionContainer,
	//		etc., it's used to specify the tab label, accordion pane title, etc.
	title: "",

	// tooltip: String
	//		When this widget's title attribute is used to for a tab label, accordion pane title, etc.,
	//		this specifies the tooltip to appear when the mouse is hovered over that text.
	tooltip: "",

	// baseClass: [protected] String
	//		Root CSS class of the widget (ex: dijitTextBox), used to construct CSS classes to indicate
	//		widget state.
	baseClass: "",

	// srcNodeRef: [readonly] DomNode
	//		pointer to original DOM node
	srcNodeRef: null,

	// domNode: [readonly] DomNode
	//		This is our visible representation of the widget! Other DOM
	//		Nodes may by assigned to other properties, usually through the
	//		template system's data-dojo-attach-point syntax, but the domNode
	//		property is the canonical "top level" node in widget UI.
	domNode: null,

	// containerNode: [readonly] DomNode
	//		Designates where children of the source DOM node will be placed.
	//		"Children" in this case refers to both DOM nodes and widgets.
	//		For example, for myWidget:
	//
	//		|	<div data-dojo-type=myWidget>
	//		|		<b> here's a plain DOM node
	//		|		<span data-dojo-type=subWidget>and a widget</span>
	//		|		<i> and another plain DOM node </i>
	//		|	</div>
	//
	//		containerNode would point to:
	//
	//		|		<b> here's a plain DOM node
	//		|		<span data-dojo-type=subWidget>and a widget</span>
	//		|		<i> and another plain DOM node </i>
	//
	//		In templated widgets, "containerNode" is set via a
	//		data-dojo-attach-point assignment.
	//
	//		containerNode must be defined for any widget that accepts innerHTML
	//		(like ContentPane or BorderContainer or even Button), and conversely
	//		is null for widgets that don't, like TextBox.
	containerNode: null,

/*=====
	// _started: Boolean
	//		startup() has completed.
	_started: false,
=====*/

	// attributeMap: [protected] Object
	//		Deprecated.   Instead of attributeMap, widget should have a _setXXXAttr attribute
	//		for each XXX attribute to be mapped to the DOM.
	//
	//		attributeMap sets up a "binding" between attributes (aka properties)
	//		of the widget and the widget's DOM.
	//		Changes to widget attributes listed in attributeMap will be
	//		reflected into the DOM.
	//
	//		For example, calling set('title', 'hello')
	//		on a TitlePane will automatically cause the TitlePane's DOM to update
	//		with the new title.
	//
	//		attributeMap is a hash where the key is an attribute of the widget,
	//		and the value reflects a binding to a:
	//
	//		- DOM node attribute
	// |		focus: {node: "focusNode", type: "attribute"}
	// 		Maps this.focus to this.focusNode.focus
	//
	//		- DOM node innerHTML
	//	|		title: { node: "titleNode", type: "innerHTML" }
	//		Maps this.title to this.titleNode.innerHTML
	//
	//		- DOM node innerText
	//	|		title: { node: "titleNode", type: "innerText" }
	//		Maps this.title to this.titleNode.innerText
	//
	//		- DOM node CSS class
	// |		myClass: { node: "domNode", type: "class" }
	//		Maps this.myClass to this.domNode.className
	//
	//		If the value is an array, then each element in the array matches one of the
	//		formats of the above list.
	//
	//		There are also some shorthands for backwards compatibility:
	//		- string --> { node: string, type: "attribute" }, for example:
	//	|	"focusNode" ---> { node: "focusNode", type: "attribute" }
	//		- "" --> { node: "domNode", type: "attribute" }
	attributeMap: {},

	// _blankGif: [protected] String
	//		Path to a blank 1x1 image.
	//		Used by <img> nodes in templates that really get their image via CSS background-image.
	_blankGif: config.blankGif || require.toUrl("dojo/resources/blank.gif"),

	//////////// INITIALIZATION METHODS ///////////////////////////////////////

	postscript: function(/*Object?*/params, /*DomNode|String*/srcNodeRef){
		// summary:
		//		Kicks off widget instantiation.  See create() for details.
		// tags:
		//		private
		this.create(params, srcNodeRef);
	},

	create: function(/*Object?*/params, /*DomNode|String?*/srcNodeRef){
		// summary:
		//		Kick off the life-cycle of a widget
		// params:
		//		Hash of initialization parameters for widget, including
		//		scalar values (like title, duration etc.) and functions,
		//		typically callbacks like onClick.
		// srcNodeRef:
		//		If a srcNodeRef (DOM node) is specified:
		//			- use srcNodeRef.innerHTML as my contents
		//			- if this is a behavioral widget then apply behavior
		//			  to that srcNodeRef
		//			- otherwise, replace srcNodeRef with my generated DOM
		//			  tree
		// description:
		//		Create calls a number of widget methods (postMixInProperties, buildRendering, postCreate,
		//		etc.), some of which of you'll want to override. See http://dojotoolkit.org/reference-guide/dijit/_WidgetBase.html
		//		for a discussion of the widget creation lifecycle.
		//
		//		Of course, adventurous developers could override create entirely, but this should
		//		only be done as a last resort.
		// tags:
		//		private

		// store pointer to original DOM tree
		this.srcNodeRef = dom.byId(srcNodeRef);

		// For garbage collection.  An array of listener handles returned by this.connect() / this.subscribe()
		this._connects = [];

		// For widgets internal to this widget, invisible to calling code
		this._supportingWidgets = [];

		// this is here for back-compat, remove in 2.0 (but check NodeList-instantiate.html test)
		if(this.srcNodeRef && (typeof this.srcNodeRef.id == "string")){ this.id = this.srcNodeRef.id; }

		// mix in our passed parameters
		if(params){
			this.params = params;
			lang.mixin(this, params);
		}
		this.postMixInProperties();

		// generate an id for the widget if one wasn't specified
		// (be sure to do this before buildRendering() because that function might
		// expect the id to be there.)
		if(!this.id){
			this.id = registry.getUniqueId(this.declaredClass.replace(/\./g,"_"));
		}
		registry.add(this);

		this.buildRendering();

		if(this.domNode){
			// Copy attributes listed in attributeMap into the [newly created] DOM for the widget.
			// Also calls custom setters for all attributes with custom setters.
			this._applyAttributes();

			// If srcNodeRef was specified, then swap out original srcNode for this widget's DOM tree.
			// For 2.0, move this after postCreate().  postCreate() shouldn't depend on the
			// widget being attached to the DOM since it isn't when a widget is created programmatically like
			// new MyWidget({}).   See #11635.
			var source = this.srcNodeRef;
			if(source && source.parentNode && this.domNode !== source){
				source.parentNode.replaceChild(this.domNode, source);
			}
		}

		if(this.domNode){
			// Note: for 2.0 may want to rename widgetId to dojo._scopeName + "_widgetId",
			// assuming that dojo._scopeName even exists in 2.0
			this.domNode.setAttribute("widgetId", this.id);
		}
		this.postCreate();

		// If srcNodeRef has been processed and removed from the DOM (e.g. TemplatedWidget) then delete it to allow GC.
		if(this.srcNodeRef && !this.srcNodeRef.parentNode){
			delete this.srcNodeRef;
		}

		this._created = true;
	},

	_applyAttributes: function(){
		// summary:
		//		Step during widget creation to copy  widget attributes to the
		//		DOM according to attributeMap and _setXXXAttr objects, and also to call
		//		custom _setXXXAttr() methods.
		//
		//		Skips over blank/false attribute values, unless they were explicitly specified
		//		as parameters to the widget, since those are the default anyway,
		//		and setting tabIndex="" is different than not setting tabIndex at all.
		//
		//		For backwards-compatibility reasons attributeMap overrides _setXXXAttr when
		//		_setXXXAttr is a hash/string/array, but _setXXXAttr as a functions override attributeMap.
		// tags:
		//		private

		// Get list of attributes where this.set(name, value) will do something beyond
		// setting this[name] = value.  Specifically, attributes that have:
		//		- associated _setXXXAttr() method/hash/string/array
		//		- entries in attributeMap.
		var ctor = this.constructor,
			list = ctor._setterAttrs;
		if(!list){
			list = (ctor._setterAttrs = []);
			for(var attr in this.attributeMap){
				list.push(attr);
			}

			var proto = ctor.prototype;
			for(var fxName in proto){
				if(fxName in this.attributeMap){ continue; }
				var setterName = "_set" + fxName.replace(/^[a-z]|-[a-zA-Z]/g, function(c){ return c.charAt(c.length-1).toUpperCase(); }) + "Attr";
				if(setterName in proto){
					list.push(fxName);
				}
			}
		}

		// Call this.set() for each attribute that was either specified as parameter to constructor,
		// or was found above and has a default non-null value.   For correlated attributes like value and displayedValue, the one
		// specified as a parameter should take precedence, so apply attributes in this.params last.
		// Particularly important for new DateTextBox({displayedValue: ...}) since DateTextBox's default value is
		// NaN and thus is not ignored like a default value of "".
		array.forEach(list, function(attr){
			if(this.params && attr in this.params){
				// skip this one, do it below
			}else if(this[attr]){
				this.set(attr, this[attr]);
			}
		}, this);
		for(var param in this.params){
			this.set(param, this[param]);
		}
	},

	postMixInProperties: function(){
		// summary:
		//		Called after the parameters to the widget have been read-in,
		//		but before the widget template is instantiated. Especially
		//		useful to set properties that are referenced in the widget
		//		template.
		// tags:
		//		protected
	},

	buildRendering: function(){
		// summary:
		//		Construct the UI for this widget, setting this.domNode.
		//		Most widgets will mixin `dijit._TemplatedMixin`, which implements this method.
		// tags:
		//		protected

		if(!this.domNode){
			// Create root node if it wasn't created by _Templated
			this.domNode = this.srcNodeRef || domConstruct.create('div');
		}

		// baseClass is a single class name or occasionally a space-separated list of names.
		// Add those classes to the DOMNode.  If RTL mode then also add with Rtl suffix.
		// TODO: make baseClass custom setter
		if(this.baseClass){
			var classes = this.baseClass.split(" ");
			if(!this.isLeftToRight()){
				classes = classes.concat( array.map(classes, function(name){ return name+"Rtl"; }));
			}
			domClass.add(this.domNode, classes);
		}
	},

	postCreate: function(){
		// summary:
		//		Processing after the DOM fragment is created
		// description:
		//		Called after the DOM fragment has been created, but not necessarily
		//		added to the document.  Do not include any operations which rely on
		//		node dimensions or placement.
		// tags:
		//		protected
	},

	startup: function(){
		// summary:
		//		Processing after the DOM fragment is added to the document
		// description:
		//		Called after a widget and its children have been created and added to the page,
		//		and all related widgets have finished their create() cycle, up through postCreate().
		//		This is useful for composite widgets that need to control or layout sub-widgets.
		//		Many layout widgets can use this as a wiring phase.
		if(this._started){ return; }
		this._started = true;
		array.forEach(this.getChildren(), function(obj){
			if(!obj._started && !obj._destroyed && lang.isFunction(obj.startup)){
				obj.startup();
				obj._started = true;
			}
		});
	},

	//////////// DESTROY FUNCTIONS ////////////////////////////////

	destroyRecursive: function(/*Boolean?*/ preserveDom){
		// summary:
		// 		Destroy this widget and its descendants
		// description:
		//		This is the generic "destructor" function that all widget users
		// 		should call to cleanly discard with a widget. Once a widget is
		// 		destroyed, it is removed from the manager object.
		// preserveDom:
		//		If true, this method will leave the original DOM structure
		//		alone of descendant Widgets. Note: This will NOT work with
		//		dijit._Templated widgets.

		this._beingDestroyed = true;
		this.destroyDescendants(preserveDom);
		this.destroy(preserveDom);
	},

	destroy: function(/*Boolean*/ preserveDom){
		// summary:
		// 		Destroy this widget, but not its descendants.
		//		This method will, however, destroy internal widgets such as those used within a template.
		// preserveDom: Boolean
		//		If true, this method will leave the original DOM structure alone.
		//		Note: This will not yet work with _Templated widgets

		this._beingDestroyed = true;
		this.uninitialize();

		// remove this.connect() and this.subscribe() listeners
		var c;
		while(c = this._connects.pop()){
			c.remove();
		}

		// destroy widgets created as part of template, etc.
		var w;
		while(w = this._supportingWidgets.pop()){
			if(w.destroyRecursive){
				w.destroyRecursive();
			}else if(w.destroy){
				w.destroy();
			}
		}

		this.destroyRendering(preserveDom);
		registry.remove(this.id);
		this._destroyed = true;
	},

	destroyRendering: function(/*Boolean?*/ preserveDom){
		// summary:
		//		Destroys the DOM nodes associated with this widget
		// preserveDom:
		//		If true, this method will leave the original DOM structure alone
		//		during tear-down. Note: this will not work with _Templated
		//		widgets yet.
		// tags:
		//		protected

		if(this.bgIframe){
			this.bgIframe.destroy(preserveDom);
			delete this.bgIframe;
		}

		if(this.domNode){
			if(preserveDom){
				domAttr.remove(this.domNode, "widgetId");
			}else{
				domConstruct.destroy(this.domNode);
			}
			delete this.domNode;
		}

		if(this.srcNodeRef){
			if(!preserveDom){
				domConstruct.destroy(this.srcNodeRef);
			}
			delete this.srcNodeRef;
		}
	},

	destroyDescendants: function(/*Boolean?*/ preserveDom){
		// summary:
		//		Recursively destroy the children of this widget and their
		//		descendants.
		// preserveDom:
		//		If true, the preserveDom attribute is passed to all descendant
		//		widget's .destroy() method. Not for use with _Templated
		//		widgets.

		// get all direct descendants and destroy them recursively
		array.forEach(this.getChildren(), function(widget){
			if(widget.destroyRecursive){
				widget.destroyRecursive(preserveDom);
			}
		});
	},

	uninitialize: function(){
		// summary:
		//		Stub function. Override to implement custom widget tear-down
		//		behavior.
		// tags:
		//		protected
		return false;
	},

	////////////////// GET/SET, CUSTOM SETTERS, ETC. ///////////////////

	_setStyleAttr: function(/*String||Object*/ value){
		// summary:
		//		Sets the style attribute of the widget according to value,
		//		which is either a hash like {height: "5px", width: "3px"}
		//		or a plain string
		// description:
		//		Determines which node to set the style on based on style setting
		//		in attributeMap.
		// tags:
		//		protected

		var mapNode = this.domNode;

		// Note: technically we should revert any style setting made in a previous call
		// to his method, but that's difficult to keep track of.

		if(lang.isObject(value)){
			domStyle.set(mapNode, value);
		}else{
			if(mapNode.style.cssText){
				mapNode.style.cssText += "; " + value;
			}else{
				mapNode.style.cssText = value;
			}
		}

		this._set("style", value);
	},

	_attrToDom: function(/*String*/ attr, /*String*/ value, /*Object?*/ commands){
		// summary:
		//		Reflect a widget attribute (title, tabIndex, duration etc.) to
		//		the widget DOM, as specified by commands parameter.
		//		If commands isn't specified then it's looked up from attributeMap.
		//		Note some attributes like "type"
		//		cannot be processed this way as they are not mutable.
		//
		// tags:
		//		private

		commands = arguments.length >= 3 ? commands : this.attributeMap[attr];

		array.forEach(lang.isArray(commands) ? commands : [commands], function(command){

			// Get target node and what we are doing to that node
			var mapNode = this[command.node || command || "domNode"];	// DOM node
			var type = command.type || "attribute";	// class, innerHTML, innerText, or attribute

			switch(type){
				case "attribute":
					if(lang.isFunction(value)){ // functions execute in the context of the widget
						value = lang.hitch(this, value);
					}

					// Get the name of the DOM node attribute; usually it's the same
					// as the name of the attribute in the widget (attr), but can be overridden.
					// Also maps handler names to lowercase, like onSubmit --> onsubmit
					var attrName = command.attribute ? command.attribute :
						(/^on[A-Z][a-zA-Z]*$/.test(attr) ? attr.toLowerCase() : attr);

					domAttr.set(mapNode, attrName, value);
					break;
				case "innerText":
					mapNode.innerHTML = "";
					mapNode.appendChild(win.doc.createTextNode(value));
					break;
				case "innerHTML":
					mapNode.innerHTML = value;
					break;
				case "class":
					domClass.replace(mapNode, value, this[attr]);
					break;
			}
		}, this);
	},

	get: function(name){
		// summary:
		//		Get a property from a widget.
		//	name:
		//		The property to get.
		// description:
		//		Get a named property from a widget. The property may
		//		potentially be retrieved via a getter method. If no getter is defined, this
		// 		just retrieves the object's property.
		//
		// 		For example, if the widget has properties `foo` and `bar`
		//		and a method named `_getFooAttr()`, calling:
		//		`myWidget.get("foo")` would be equivalent to calling
		//		`widget._getFooAttr()` and `myWidget.get("bar")`
		//		would be equivalent to the expression
		//		`widget.bar2`
		var names = this._getAttrNames(name);
		return this[names.g] ? this[names.g]() : this[name];
	},

	set: function(name, value){
		// summary:
		//		Set a property on a widget
		//	name:
		//		The property to set.
		//	value:
		//		The value to set in the property.
		// description:
		//		Sets named properties on a widget which may potentially be handled by a
		// 		setter in the widget.
		//
		// 		For example, if the widget has properties `foo` and `bar`
		//		and a method named `_setFooAttr()`, calling
		//		`myWidget.set("foo", "Howdy!")` would be equivalent to calling
		//		`widget._setFooAttr("Howdy!")` and `myWidget.set("bar", 3)`
		//		would be equivalent to the statement `widget.bar = 3;`
		//
		//		set() may also be called with a hash of name/value pairs, ex:
		//
		//	|	myWidget.set({
		//	|		foo: "Howdy",
		//	|		bar: 3
		//	|	});
		//
		//	This is equivalent to calling `set(foo, "Howdy")` and `set(bar, 3)`

		if(typeof name === "object"){
			for(var x in name){
				this.set(x, name[x]);
			}
			return this;
		}
		var names = this._getAttrNames(name),
			setter = this[names.s];
		if(lang.isFunction(setter)){
			// use the explicit setter
			var result = setter.apply(this, Array.prototype.slice.call(arguments, 1));
		}else{
			// Mapping from widget attribute to DOMNode attribute/value/etc.
			// Map according to:
			//		1. attributeMap setting, if one exists (TODO: attributeMap deprecated, remove in 2.0)
			//		2. _setFooAttr: {...} type attribute in the widget (if one exists)
			//		3. apply to focusNode or domNode if standard attribute name, excluding funcs like onClick.
			// Checks if an attribute is a "standard attribute" by whether the DOMNode JS object has a similar
			// attribute name (ex: accept-charset attribute matches jsObject.acceptCharset).
			// Note also that Tree.focusNode() is a function not a DOMNode, so test for that.
			var defaultNode = this.focusNode && !lang.isFunction(this.focusNode) ? "focusNode" : "domNode",
				tag = this[defaultNode].tagName,
				attrsForTag = tagAttrs[tag] || (tagAttrs[tag] = getAttrs(this[defaultNode])),
				map =	name in this.attributeMap ? this.attributeMap[name] :
						names.s in this ? this[names.s] :
						((names.l in attrsForTag && typeof value != "function") ||
							/^aria-|^data-|^role$/.test(name)) ? defaultNode : null;
			if(map != null){
				this._attrToDom(name, value, map);
			}
			this._set(name, value);
		}
		return result || this;
	},

	_attrPairNames: {},		// shared between all widgets
	_getAttrNames: function(name){
		// summary:
		//		Helper function for get() and set().
		//		Caches attribute name values so we don't do the string ops every time.
		// tags:
		//		private

		var apn = this._attrPairNames;
		if(apn[name]){ return apn[name]; }
		var uc = name.replace(/^[a-z]|-[a-zA-Z]/g, function(c){ return c.charAt(c.length-1).toUpperCase(); });
		return (apn[name] = {
			n: name+"Node",
			s: "_set"+uc+"Attr",	// converts dashes to camel case, ex: accept-charset --> _setAcceptCharsetAttr
			g: "_get"+uc+"Attr",
			l: uc.toLowerCase()		// lowercase name w/out dashes, ex: acceptcharset
		});
	},

	_set: function(/*String*/ name, /*anything*/ value){
		// summary:
		//		Helper function to set new value for specified attribute, and call handlers
		//		registered with watch() if the value has changed.
		var oldValue = this[name];
		this[name] = value;
		if(this._watchCallbacks && this._created && value !== oldValue){
			this._watchCallbacks(name, oldValue, value);
		}
	},

	on: function(/*String*/ type, /*Function*/ func){
		// summary:
		//		Call specified function when event occurs, ex: myWidget.on("click", function(){ ... }).
		// description:
		//		Call specified function when event `type` occurs, ex: `myWidget.on("click", function(){ ... })`.
		//		Note that the function is not run in any particular scope, so if (for example) you want it to run in the
		//		widget's scope you must do `myWidget.on("click", lang.hitch(myWidget, func))`.

		return aspect.after(this, this._onMap(type), func, true);
	},

	_onMap: function(/*String*/ type){
		// summary:
		//		Maps on() type parameter (ex: "mousemove") to method name (ex: "onMouseMove")
		var ctor = this.constructor, map = ctor._onMap;
		if(!map){
			map = (ctor._onMap = {});
			for(var attr in ctor.prototype){
				if(/^on/.test(attr)){
					map[attr.replace(/^on/, "").toLowerCase()] = attr;
				}
			}
		}
		return map[type.toLowerCase()];	// String
	},

	toString: function(){
		// summary:
		//		Returns a string that represents the widget
		// description:
		//		When a widget is cast to a string, this method will be used to generate the
		//		output. Currently, it does not implement any sort of reversible
		//		serialization.
		return '[Widget ' + this.declaredClass + ', ' + (this.id || 'NO ID') + ']'; // String
	},

	getChildren: function(){
		// summary:
		//		Returns all the widgets contained by this, i.e., all widgets underneath this.containerNode.
		//		Does not return nested widgets, nor widgets that are part of this widget's template.
		return this.containerNode ? registry.findWidgets(this.containerNode) : []; // dijit._Widget[]
	},

	getParent: function(){
		// summary:
		//		Returns the parent widget of this widget
		return registry.getEnclosingWidget(this.domNode.parentNode);
	},

	connect: function(
			/*Object|null*/ obj,
			/*String|Function*/ event,
			/*String|Function*/ method){
		// summary:
		//		Connects specified obj/event to specified method of this object
		//		and registers for disconnect() on widget destroy.
		// description:
		//		Provide widget-specific analog to dojo.connect, except with the
		//		implicit use of this widget as the target object.
		//		Events connected with `this.connect` are disconnected upon
		//		destruction.
		// returns:
		//		A handle that can be passed to `disconnect` in order to disconnect before
		//		the widget is destroyed.
		// example:
		//	|	var btn = new dijit.form.Button();
		//	|	// when foo.bar() is called, call the listener we're going to
		//	|	// provide in the scope of btn
		//	|	btn.connect(foo, "bar", function(){
		//	|		console.debug(this.toString());
		//	|	});
		// tags:
		//		protected

		var handle = connect.connect(obj, event, this, method);
		this._connects.push(handle);
		return handle;		// _Widget.Handle
	},

	disconnect: function(handle){
		// summary:
		//		Disconnects handle created by `connect`.
		//		Also removes handle from this widget's list of connects.
		// tags:
		//		protected
		var i = array.indexOf(this._connects, handle);
		if(i != -1){
			handle.remove();
			this._connects.splice(i, 1);
		}
	},

	subscribe: function(t, method){
		// summary:
		//		Subscribes to the specified topic and calls the specified method
		//		of this object and registers for unsubscribe() on widget destroy.
		// description:
		//		Provide widget-specific analog to dojo.subscribe, except with the
		//		implicit use of this widget as the target object.
		// t: String
		//		The topic
		// method: Function
		//		The callback
		// example:
		//	|	var btn = new dijit.form.Button();
		//	|	// when /my/topic is published, this button changes its label to
		//	|   // be the parameter of the topic.
		//	|	btn.subscribe("/my/topic", function(v){
		//	|		this.set("label", v);
		//	|	});
		// tags:
		//		protected
		var handle = topic.subscribe(t, lang.hitch(this, method));
		this._connects.push(handle);
		return handle;		// _Widget.Handle
	},

	unsubscribe: function(/*Object*/ handle){
		// summary:
		//		Unsubscribes handle created by this.subscribe.
		//		Also removes handle from this widget's list of subscriptions
		// tags:
		//		protected
		this.disconnect(handle);
	},

	isLeftToRight: function(){
		// summary:
		//		Return this widget's explicit or implicit orientation (true for LTR, false for RTL)
		// tags:
		//		protected
		return this.dir ? (this.dir == "ltr") : domGeometry.isBodyLtr(); //Boolean
	},

	isFocusable: function(){
		// summary:
		//		Return true if this widget can currently be focused
		//		and false if not
		return this.focus && (domStyle.get(this.domNode, "display") != "none");
	},

	placeAt: function(/* String|DomNode|_Widget */reference, /* String?|Int? */position){
		// summary:
		//		Place this widget's domNode reference somewhere in the DOM based
		//		on standard domConstruct.place conventions, or passing a Widget reference that
		//		contains and addChild member.
		//
		// description:
		//		A convenience function provided in all _Widgets, providing a simple
		//		shorthand mechanism to put an existing (or newly created) Widget
		//		somewhere in the dom, and allow chaining.
		//
		// reference:
		//		The String id of a domNode, a domNode reference, or a reference to a Widget possessing
		//		an addChild method.
		//
		// position:
		//		If passed a string or domNode reference, the position argument
		//		accepts a string just as domConstruct.place does, one of: "first", "last",
		//		"before", or "after".
		//
		//		If passed a _Widget reference, and that widget reference has an ".addChild" method,
		//		it will be called passing this widget instance into that method, supplying the optional
		//		position index passed.
		//
		// returns:
		//		dijit._Widget
		//		Provides a useful return of the newly created dijit._Widget instance so you
		//		can "chain" this function by instantiating, placing, then saving the return value
		//		to a variable.
		//
		// example:
		// | 	// create a Button with no srcNodeRef, and place it in the body:
		// | 	var button = new dijit.form.Button({ label:"click" }).placeAt(win.body());
		// | 	// now, 'button' is still the widget reference to the newly created button
		// | 	button.on("click", function(e){ console.log('click'); }));
		//
		// example:
		// |	// create a button out of a node with id="src" and append it to id="wrapper":
		// | 	var button = new dijit.form.Button({},"src").placeAt("wrapper");
		//
		// example:
		// |	// place a new button as the first element of some div
		// |	var button = new dijit.form.Button({ label:"click" }).placeAt("wrapper","first");
		//
		// example:
		// |	// create a contentpane and add it to a TabContainer
		// |	var tc = dijit.byId("myTabs");
		// |	new dijit.layout.ContentPane({ href:"foo.html", title:"Wow!" }).placeAt(tc)

		if(reference.declaredClass && reference.addChild){
			reference.addChild(this, position);
		}else{
			domConstruct.place(this.domNode, reference, position);
		}
		return this;
	},

	getTextDir: function(/*String*/ text,/*String*/ originalDir){
		// summary:
		//		Return direction of the text.
		//		The function overridden in the _BidiSupport module,
		//		its main purpose is to calculate the direction of the
		//		text, if was defined by the programmer through textDir.
		//	tags:
		//		protected.
		return originalDir;
	},

	applyTextDir: function(/*===== element, text =====*/){
		// summary:
		//		The function overridden in the _BidiSupport module,
		//		originally used for setting element.dir according to this.textDir.
		//		In this case does nothing.
		// element: DOMNode
		// text: String
		// tags:
		//		protected.
	}
});

});

},
'dojox/mobile/app/AlertDialog':function(){
// wrapped by build app
define(["dijit","dojo","dojox","dojo/require!dijit/_WidgetBase"], function(dijit,dojo,dojox){
dojo.provide("dojox.mobile.app.AlertDialog");
dojo.experimental("dojox.mobile.app.AlertDialog");
dojo.require("dijit._WidgetBase");

dojo.declare("dojox.mobile.app.AlertDialog", dijit._WidgetBase, {

	// title: String
	//		The title of the AlertDialog
	title: "",

	// text: String
	//		The text message displayed in the AlertDialog
	text: "",

	// controller: Object
	//		The SceneController for the currently active scene
	controller: null,

	// buttons: Array
	buttons: null,

	defaultButtonLabel: "OK",

	// onChoose: Function
	//		The callback function that is invoked when a button is tapped.
	//		If the dialog is cancelled, no parameter is passed to this function.
	onChoose: null,

	constructor: function(){
		this.onClick = dojo.hitch(this, this.onClick);
		this._handleSelect = dojo.hitch(this, this._handleSelect);
	},

	buildRendering: function(){
		this.domNode = dojo.create("div",{
			"class": "alertDialog"
		});

		// Create the outer dialog body
		var dlgBody = dojo.create("div", {"class": "alertDialogBody"}, this.domNode);

		// Create the title
		dojo.create("div", {"class": "alertTitle", innerHTML: this.title || ""}, dlgBody);

		// Create the text
		dojo.create("div", {"class": "alertText", innerHTML: this.text || ""}, dlgBody);

		// Create the node that encapsulates all the buttons
		var btnContainer = dojo.create("div", {"class": "alertBtns"}, dlgBody);

		// If no buttons have been defined, default to a single button saying OK
		if(!this.buttons || this.buttons.length == 0){
			this.buttons = [{
				label: this.defaultButtonLabel,
				value: "ok",
				"class": "affirmative"
			}];
		}

		var _this = this;

		// Create each of the buttons
		dojo.forEach(this.buttons, function(btnInfo){
			var btn = new dojox.mobile.Button({
				btnClass: btnInfo["class"] || "",
				label: btnInfo.label
			});
			btn._dialogValue = btnInfo.value;
			dojo.place(btn.domNode, btnContainer);
			_this.connect(btn, "onClick", _this._handleSelect);
		});

		var viewportSize = this.controller.getWindowSize();

		// Create the mask that blocks out the rest of the screen
		this.mask = dojo.create("div", {"class": "dialogUnderlayWrapper",
			innerHTML: "<div class=\"dialogUnderlay\"></div>",
			style: {
				width: viewportSize.w + "px",
				height: viewportSize.h + "px"
			}
		}, this.controller.assistant.domNode);

		this.connect(this.mask, "onclick", function(){
			_this.onChoose && _this.onChoose();
			_this.hide();
		});
	},

	postCreate: function(){
		this.subscribe("/dojox/mobile/app/goback", this._handleSelect);
	},

	_handleSelect: function(event){
		// summary:
		//		Handle the selection of a value
		var node;
		console.log("handleSelect");
		if(event && event.target){
			node = event.target;

			// Find the widget that was tapped.
			while(!dijit.byNode(node)){
				node - node.parentNode;
			}
		}

		// If an onChoose function was provided, tell it what button
		// value was chosen
		if(this.onChoose){
			this.onChoose(node ? dijit.byNode(node)._dialogValue: undefined);
		}
		// Hide the dialog
		this.hide();
	},

	show: function(){
		// summary:
		//		Show the dialog
		this._doTransition(1);
	},

	hide: function(){
		// summary:
		//		Hide the dialog
		this._doTransition(-1);
	},

	_doTransition: function(dir){
		// summary:
		//		Either shows or hides the dialog.
		// dir:
		//		An integer.  If positive, the dialog is shown. If negative,
		//		the dialog is hidden.

		// TODO: replace this with CSS transitions

		var anim;
		var h = dojo.marginBox(this.domNode.firstChild).h;


		var bodyHeight = this.controller.getWindowSize().h;
		console.log("dialog height = " + h, " body height = " + bodyHeight);

		var high = bodyHeight - h;
		var low = bodyHeight;

		var anim1 = dojo.fx.slideTo({
			node: this.domNode,
			duration: 400,
			top: {start: dir < 0 ? high : low, end: dir < 0 ? low: high}
		});

		var anim2 = dojo[dir < 0 ? "fadeOut" : "fadeIn"]({
			node: this.mask,
			duration: 400
		});

		var anim = dojo.fx.combine([anim1, anim2]);

		var _this = this;

		dojo.connect(anim, "onEnd", this, function(){
			if(dir < 0){
				_this.domNode.style.display = "none";
				dojo.destroy(_this.domNode);
				dojo.destroy(_this.mask);
			}
		});
		anim.play();
	},

	destroy: function(){
		this.inherited(arguments);
		dojo.destroy(this.mask);
	},


	onClick: function(){

	}
});
});

}}});

define("dojox/mobile/app", [
	"./app/_base"
], function(appBase){
	return appBase;
});
