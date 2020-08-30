define("dijit/_editor/plugins/ViewSource", [
	"dojo/_base/array", // array.forEach
	"dojo/aspect", // Aspect commands for advice
	"dojo/_base/declare", // declare
	"dojo/dom-attr", // domAttr.set
	"dojo/dom-construct", // domConstruct.create domConstruct.place
	"dojo/dom-geometry", // domGeometry.setMarginBox domGeometry.position
	"dojo/dom-style", // domStyle.set
	"dojo/i18n", // i18n.getLocalization
	"dojo/keys", // keys.F12
	"dojo/_base/lang", // lang.hitch
	"dojo/on", // on()
	"dojo/sniff", // has("ie")
	"dojo/window", // winUtils.getBox
	"../../focus", // focus.focus()
	"../_Plugin",
	"../../form/ToggleButton",
	"../..", // dijit._scopeName
	"../../registry", // registry.getEnclosingWidget()
	"dojo/i18n!../nls/commands"
], function(array, aspect, declare, domAttr, domConstruct, domGeometry, domStyle, i18n, keys, lang, on, has, winUtils,
			focus, _Plugin, ToggleButton, dijit, registry){

	// module:
	//		dijit/_editor/plugins/ViewSource

	var ViewSource = declare("dijit._editor.plugins.ViewSource", _Plugin, {
		// summary:
		//		This plugin provides a simple view source capability.  When view
		//		source mode is enabled, it disables all other buttons/plugins on the RTE.
		//		It also binds to the hotkey: CTRL-SHIFT-F11 for toggling ViewSource mode.

		// stripScripts: [public] Boolean
		//		Boolean flag used to indicate if script tags should be stripped from the document.
		//		Defaults to true.
		stripScripts: true,

		// stripComments: [public] Boolean
		//		Boolean flag used to indicate if comment tags should be stripped from the document.
		//		Defaults to true.
		stripComments: true,

		// stripComments: [public] Boolean
		//		Boolean flag used to indicate if iframe tags should be stripped from the document.
		//		Defaults to true.
		stripIFrames: true,

		// stripEventHandlers: [public] Boolean
		//		Boolean flag used to indicate if event handler attributes like onload should be
		//		stripped from the document.
		//		Defaults to true.
		stripEventHandlers: true,

		// readOnly: [const] Boolean
		//		Boolean flag used to indicate if the source view should be readonly or not.
		//		Cannot be changed after initialization of the plugin.
		//		Defaults to false.
		readOnly: false,

		// _fsPlugin: [private] Object
		//		Reference to a registered fullscreen plugin so that viewSource knows
		//		how to scale.
		_fsPlugin: null,

		toggle: function(){
			// summary:
			//		Function to allow programmatic toggling of the view.

			// For Webkit, we have to focus a very particular way.
			// when swapping views, otherwise focus doesn't shift right
			// but can't focus this way all the time, only for VS changes.
			// If we did it all the time, buttons like bold, italic, etc
			// break.
			if(has("webkit")){
				this._vsFocused = true;
			}
			this.button.set("checked", !this.button.get("checked"));

		},

		_initButton: function(){
			// summary:
			//		Over-ride for creation of the resize button.
			var strings = i18n.getLocalization("dijit._editor", "commands"),
				editor = this.editor;
			this.button = new ToggleButton({
				label: strings["viewSource"],
				ownerDocument: editor.ownerDocument,
				dir: editor.dir,
				lang: editor.lang,
				showLabel: false,
				iconClass: this.iconClassPrefix + " " + this.iconClassPrefix + "ViewSource",
				tabIndex: "-1",
				onChange: lang.hitch(this, "_showSource")
			});

			// Make sure readonly mode doesn't make the wrong cursor appear over the button.
			this.button.set("readOnly", false);
		},


		setEditor: function(/*dijit/Editor*/ editor){
			// summary:
			//		Tell the plugin which Editor it is associated with.
			// editor: Object
			//		The editor object to attach the print capability to.
			this.editor = editor;
			this._initButton();

			// Filter the html content when it is set and retrieved in the editor.
			this.removeValueFilterHandles();
			this._setValueFilterHandle = aspect.before(this.editor, "setValue", lang.hitch(this, function (html) {
				return [this._filter(html)];
			}));
			this._getValueFilterHandle = aspect.after(this.editor, "getValue", lang.hitch(this, function (html) {
				return this._filter(html);
			}));

			this.editor.addKeyHandler(keys.F12, true, true, lang.hitch(this, function(e){
				// Move the focus before switching
				// It'll focus back.  Hiding a focused
				// node causes issues.
				this.button.focus();
				this.toggle();
				e.stopPropagation();
				e.preventDefault();

				// Call the focus shift outside of the handler.
				setTimeout(lang.hitch(this, function(){
					// Focus the textarea... unless focus has moved outside of the editor completely during the timeout.
					// Since we override focus, so we just need to call it.
					if(this.editor.focused){
						this.editor.focus();
					}
				}), 100);
			}));
		},

		_showSource: function(source){
			// summary:
			//		Function to toggle between the source and RTE views.
			// source: boolean
			//		Boolean value indicating if it should be in source mode or not.
			// tags:
			//		private
			var ed = this.editor;
			var edPlugins = ed._plugins;
			var html;
			this._sourceShown = source;
			var self = this;
			try{
				if(!this.sourceArea){
					this._createSourceView();
				}
				if(source){
					// Update the QueryCommandEnabled function to disable everything but
					// the source view mode.  Have to over-ride a function, then kick all
					// plugins to check their state.
					ed._sourceQueryCommandEnabled = ed.queryCommandEnabled;
					ed.queryCommandEnabled = function(cmd){
						return cmd.toLowerCase() === "viewsource";
					};
					this.editor.onDisplayChanged();
					array.forEach(edPlugins, function(p){
						// Turn off any plugins not controlled by queryCommandenabled.
						if(p && !(p instanceof ViewSource) && p.isInstanceOf(_Plugin)){
							p.set("disabled", true)
						}
					});

					// We actually do need to trap this plugin and adjust how we
					// display the textarea.
					if(this._fsPlugin){
						this._fsPlugin._getAltViewNode = function(){
							return self.sourceArea;
						};
					}

					this.sourceArea.value = ed.get("value");

					// Since neither iframe nor textarea have margin, border, or padding,
					// just set sizes equal.
					this.sourceArea.style.height = ed.iframe.style.height;
					this.sourceArea.style.width = ed.iframe.style.width;

					// Hide the iframe and show the HTML source <textarea>.  But don't use display:none because
					// that loses scroll position, and also causes weird problems on FF (see #18607).
					ed.iframe.parentNode.style.position = "relative";
					domStyle.set(ed.iframe, {
						position: "absolute",
						top: 0,
						visibility: "hidden"
					});
					domStyle.set(this.sourceArea, {
						display: "block"
					});

					var resizer = function(){
						// function to handle resize events.
						// Will check current VP and only resize if
						// different.
						var vp = winUtils.getBox(ed.ownerDocument);

						if("_prevW" in this && "_prevH" in this){
							// No actual size change, ignore.
							if(vp.w === this._prevW && vp.h === this._prevH){
								return;
							}else{
								this._prevW = vp.w;
								this._prevH = vp.h;
							}
						}else{
							this._prevW = vp.w;
							this._prevH = vp.h;
						}
						if(this._resizer){
							clearTimeout(this._resizer);
							delete this._resizer;
						}
						// Timeout it to help avoid spamming resize on IE.
						// Works for all browsers.
						this._resizer = setTimeout(lang.hitch(this, function(){
							delete this._resizer;
							this._resize();
						}), 10);
					};
					this._resizeHandle = on(window, "resize", lang.hitch(this, resizer));

					//Call this on a delay once to deal with IE glitchiness on initial size.
					setTimeout(lang.hitch(this, this._resize), 100);

					//Trigger a check for command enablement/disablement.
					this.editor.onNormalizedDisplayChanged();

					this.editor.__oldGetValue = this.editor.getValue;
					this.editor.getValue = lang.hitch(this, function(){
						var txt = this.sourceArea.value;
						txt = this._filter(txt);
						return txt;
					});

					this._setListener = aspect.after(this.editor, "setValue", lang.hitch(this, function(htmlTxt){
						htmlTxt = htmlTxt || "";
						// htmlTxt was filtered in setValue before aspect.
						this.sourceArea.value = htmlTxt;
					}), true);
				}else{
					// First check that we were in source view before doing anything.
					// corner case for being called with a value of false and we hadn't
					// actually been in source display mode.
					if(!ed._sourceQueryCommandEnabled){
						return;
					}

					// Remove the set listener.
					this._setListener.remove();
					delete this._setListener;

					this._resizeHandle.remove();
					delete this._resizeHandle;

					if(this.editor.__oldGetValue){
						this.editor.getValue = this.editor.__oldGetValue;
						delete this.editor.__oldGetValue;
					}

					// Restore all the plugin buttons state.
					ed.queryCommandEnabled = ed._sourceQueryCommandEnabled;
					if(!this._readOnly){
						html = this.sourceArea.value;
						ed.beginEditing();
						// html will be filtered in setValue aspect.
						ed.set("value", html);
						ed.endEditing();
					}

					array.forEach(edPlugins, function(p){
						// Turn back on any plugins we turned off.
						if(p && p.isInstanceOf(_Plugin)){
							p.set("disabled", false);
						}
					});

					domStyle.set(this.sourceArea, "display", "none");
					domStyle.set(ed.iframe, {
						position: "relative",
						visibility: "visible"
					});
					delete ed._sourceQueryCommandEnabled;

					//Trigger a check for command enablement/disablement.
					this.editor.onDisplayChanged();
				}
				// Call a delayed resize to wait for some things to display in header/footer.
				setTimeout(lang.hitch(this, function(){
					// Make resize calls.
					var parent = ed.domNode.parentNode;
					if(parent){
						var container = registry.getEnclosingWidget(parent);
						if(container && container.resize){
							container.resize();
						}
					}
					ed.resize();
				}), 300);
			}catch(e){
				console.log(e);
			}
		},

		updateState: function(){
			// summary:
			//		Over-ride for button state control for disabled to work.
			this.button.set("disabled", this.get("disabled"));
		},

		_resize: function(){
			// summary:
			//		Internal function to resize the source view
			// tags:
			//		private
			var ed = this.editor;
			var tbH = ed.getHeaderHeight();
			var fH = ed.getFooterHeight();
			var eb = domGeometry.position(ed.domNode);

			// Styles are now applied to the internal source container, so we have
			// to subtract them off.
			var containerPadding = domGeometry.getPadBorderExtents(ed.iframe.parentNode);
			var containerMargin = domGeometry.getMarginExtents(ed.iframe.parentNode);

			var extents = domGeometry.getPadBorderExtents(ed.domNode);
			var edb = {
				w: eb.w - extents.w,
				h: eb.h - (tbH + extents.h + fH)
			};

			// Fullscreen gets odd, so we need to check for the FS plugin and
			// adapt.
			if(this._fsPlugin && this._fsPlugin.isFullscreen){
				// Okay, probably in FS, adjust.
				var vp = winUtils.getBox(ed.ownerDocument);
				edb.w = (vp.w - extents.w);
				edb.h = (vp.h - (tbH + extents.h + fH));
			}

			domGeometry.setMarginBox(this.sourceArea, {
				w: Math.round(edb.w - (containerPadding.w + containerMargin.w)),
				h: Math.round(edb.h - (containerPadding.h + containerMargin.h))
			});
		},

		_createSourceView: function(){
			// summary:
			//		Internal function for creating the source view area.
			// tags:
			//		private
			var ed = this.editor;
			var edPlugins = ed._plugins;
			this.sourceArea = domConstruct.create("textarea");
			if(this.readOnly){
				domAttr.set(this.sourceArea, "readOnly", true);
				this._readOnly = true;
			}
			domStyle.set(this.sourceArea, {
				padding: "0px",
				margin: "0px",
				borderWidth: "0px",
				borderStyle: "none"
			});
			domAttr.set(this.sourceArea, "aria-label", this.editor.id);

			domConstruct.place(this.sourceArea, ed.iframe, "before");

			if(has("ie") && ed.iframe.parentNode.lastChild !== ed.iframe){
				// There's some weirdo div in IE used for focus control
				// But is messed up scaling the textarea if we don't config
				// it some so it doesn't have a varying height.
				domStyle.set(ed.iframe.parentNode.lastChild, {
					width: "0px",
					height: "0px",
					padding: "0px",
					margin: "0px",
					borderWidth: "0px",
					borderStyle: "none"
				});
			}

			// We also need to take over editor focus a bit here, so that focus calls to
			// focus the editor will focus to the right node when VS is active.
			ed._viewsource_oldFocus = ed.focus;
			var self = this;
			ed.focus = function(){
				if(self._sourceShown){
					self.setSourceAreaCaret();
				}else{
					try{
						if(this._vsFocused){
							delete this._vsFocused;
							// Must focus edit node in this case (webkit only) or
							// focus doesn't shift right, but in normal
							// cases we focus with the regular function.
							focus.focus(ed.editNode);
						}else{
							ed._viewsource_oldFocus();
						}
					}catch(e){
						console.log("ViewSource focus code error: " + e);
					}
				}
			};

			var i, p;
			for(i = 0; i < edPlugins.length; i++){
				// We actually do need to trap this plugin and adjust how we
				// display the textarea.
				p = edPlugins[i];
				if(p && (p.declaredClass === "dijit._editor.plugins.FullScreen" ||
					p.declaredClass === (dijit._scopeName +
						"._editor.plugins.FullScreen"))){
					this._fsPlugin = p;
					break;
				}
			}
			if(this._fsPlugin){
				// Found, we need to over-ride the alt-view node function
				// on FullScreen with our own, chain up to parent call when appropriate.
				this._fsPlugin._viewsource_getAltViewNode = this._fsPlugin._getAltViewNode;
				this._fsPlugin._getAltViewNode = function(){
					return self._sourceShown ? self.sourceArea : this._viewsource_getAltViewNode();
				};
			}

			// Listen to the source area for key events as well, as we need to be able to hotkey toggle
			// it from there too.
			this.own(on(this.sourceArea, "keydown", lang.hitch(this, function(e){
				if(this._sourceShown && e.keyCode == keys.F12 && e.ctrlKey && e.shiftKey){
					this.button.focus();
					this.button.set("checked", false);
					setTimeout(lang.hitch(this, function(){
						ed.focus();
					}), 100);
					e.stopPropagation();
					e.preventDefault();
				}
			})));
		},

		_stripScripts: function(html){
			// summary:
			//		Strips out script tags from the HTML used in editor.
			// html: String
			//		The HTML to filter
			// tags:
			//		private
			if(html){
				// Look for closed and unclosed (malformed) script attacks.
				html = html.replace(/<\s*script[^>]*>((.|\s)*?)<\\?\/\s*script\s*>/ig, "");
				html = html.replace(/<\s*script\b([^<>]|\s)*>?/ig, "");
				html = html.replace(/<[^>]*=(\s|)*[("|')]javascript:[^$1][(\s|.)]*[$1][^>]*>/ig, "");
			}
			return html;
		},

		_stripComments: function(html){
			// summary:
			//		Strips out comments from the HTML used in editor.
			// html: String
			//		The HTML to filter
			// tags:
			//		private
			if(html){
				html = html.replace(/<!--(.|\s){1,}?-->/g, "");
			}
			return html;
		},

		_stripIFrames: function(html){
			// summary:
			//		Strips out iframe tags from the content, to avoid iframe script
			//		style injection attacks.
			// html: String
			//		The HTML to filter
			// tags:
			//		private
			if(html){
				html = html.replace(/<\s*iframe[^>]*>((.|\s)*?)<\\?\/\s*iframe\s*>/ig, "");
			}
			return html;
		},

		_stripEventHandlers: function (html) {
			if(html){
				// Find all tags that contain an event handler attribute (an on* attribute).
				var matches = html.match(/<[a-z]+?\b(.*?on.*?(['"]).*?\2.*?)+>/gim);
				if(matches){
					for(var i = 0, l = matches.length; i < l; i++){
						// For each tag, remove only the event handler attributes.
						var match = matches[i];
						var replacement = match.replace(/\s+on[a-z]*\s*=\s*(['"])(.*?)\1/igm, "");
						html = html.replace(match, replacement);
					}
				}
			}
			return html;
		},

		_filter: function(html){
			// summary:
			//		Internal function to perform some filtering on the HTML.
			// html: String
			//		The HTML to filter
			// tags:
			//		private
			if(html){
				if(this.stripScripts){
					html = this._stripScripts(html);
				}
				if(this.stripComments){
					html = this._stripComments(html);
				}
				if(this.stripIFrames){
					html = this._stripIFrames(html);
				}
				if(this.stripEventHandlers){
					html = this._stripEventHandlers(html);
				}
			}
			return html;
		},

		removeValueFilterHandles: function () {
			if (this._setValueFilterHandle) {
				this._setValueFilterHandle.remove();
				delete this._setValueFilterHandle;
			}
			if (this._getValueFilterHandle) {
				this._getValueFilterHandle.remove();
				delete this._getValueFilterHandle;
			}
		},

		setSourceAreaCaret: function(){
			// summary:
			//		Internal function to set the caret in the sourceArea
			//		to 0x0
			var elem = this.sourceArea;
			focus.focus(elem);
			if(this._sourceShown && !this.readOnly){
				if(elem.setSelectionRange){
					elem.setSelectionRange(0, 0);
				}else if(this.sourceArea.createTextRange){
					// IE
					var range = elem.createTextRange();
					range.collapse(true);
					range.moveStart("character", -99999); // move to 0
					range.moveStart("character", 0); // delta from 0 is the correct position
					range.moveEnd("character", 0);
					range.select();
				}
			}
		},

		destroy: function(){
			if(this._resizer){
				clearTimeout(this._resizer);
				delete this._resizer;
			}
			if(this._resizeHandle){
				this._resizeHandle.remove();
				delete this._resizeHandle;
			}
			if(this._setListener){
				this._setListener.remove();
				delete this._setListener;
			}
			this.removeValueFilterHandles();
			this.inherited(arguments);
		}
	});

	// Register this plugin.
	// For back-compat accept "viewsource" (all lowercase) too, remove in 2.0
	_Plugin.registry["viewSource"] = _Plugin.registry["viewsource"] = function(args){
		return new ViewSource({
			readOnly: ("readOnly" in args) ? args.readOnly : false,
			stripComments: ("stripComments" in args) ? args.stripComments : true,
			stripScripts: ("stripScripts" in args) ? args.stripScripts : true,
			stripIFrames: ("stripIFrames" in args) ? args.stripIFrames : true,
			stripEventHandlers: ("stripEventHandlers" in args) ? args.stripEventHandlers : true
		});
	};

	return ViewSource;
});
