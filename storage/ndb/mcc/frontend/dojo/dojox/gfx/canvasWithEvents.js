define(["dojo/_base/lang", "dojo/_base/declare", "dojo/has", "dojo/on", "dojo/aspect", "dojo/touch", "dojo/_base/Color", "dojo/dom",
		"dojo/dom-geometry", "dojo/_base/window", "./_base","./canvas", "./shape", "./matrix"],
function(lang, declare, has, on, aspect, touch, Color, dom, domGeom, win, g, canvas, shapeLib, m){
	function makeFakeEvent(event){
		// summary:
		//		Generates a "fake", fully mutable event object by copying the properties from an original host Event
		//		object to a new standard JavaScript object.

		var fakeEvent = {};
		for(var k in event){
			if(typeof event[k] === "function"){
				// Methods (like preventDefault) must be invoked on the original event object, or they will not work
				fakeEvent[k] = lang.hitch(event, k);
			}
			else{
				fakeEvent[k] = event[k];
			}
		}
		return fakeEvent;
	}

	// Browsers that implement the current (January 2013) WebIDL spec allow Event object properties to be mutated
	// using Object.defineProperty; some older WebKits (Safari 6-) and at least IE10- do not follow the spec. Direct
	// mutation is, of course, much faster when it can be done.
    has.add("dom-mutableEvents", function(){
        var event = document.createEvent("UIEvents");
        try {
            if(Object.defineProperty){
                Object.defineProperty(event, "type", { value: "foo" });
            }else{
                event.type = "foo";
            }
            return event.type === "foo";
        }catch(e){
            return false;
        }
    });

	var canvasWithEvents = g.canvasWithEvents = {
		// summary:
		//		This the graphics rendering bridge for W3C Canvas compliant browsers which extends
		//		the basic canvas drawing renderer bridge to add additional support for graphics events
		//		on Shapes.
		//		Since Canvas is an immediate mode graphics api, with no object graph or
		//		eventing capabilities, use of the canvas module alone will only add in drawing support.
		//		This additional module, canvasWithEvents extends this module with additional support
		//		for handling events on Canvas.  By default, the support for events is now included
		//		however, if only drawing capabilities are needed, canvas event module can be disabled
		//		using the dojoConfig option, canvasEvents:true|false.
	};

	canvasWithEvents.Shape = declare("dojox.gfx.canvasWithEvents.Shape", canvas.Shape, {
		_testInputs: function(/* Object */ ctx, /* Array */ pos){
			if(this.clip || (!this.canvasFill && this.strokeStyle)){
				// pixel-based until a getStrokedPath-like api is available on the path
				this._hitTestPixel(ctx, pos);
			}else{
				this._renderShape(ctx);
				var length = pos.length,
					t = this.getTransform();

				for(var i = 0; i < length; ++i){
					var input = pos[i];
					// already hit
					if(input.target){continue;}
					var x = input.x,
						y = input.y,
						p = t ? m.multiplyPoint(m.invert(t), x, y) : { x: x, y: y };
					input.target = this._hitTestGeometry(ctx, p.x, p.y);
				}
			}
		},

		_hitTestPixel: function(/* Object */ ctx, /* Array */ pos){
			for(var i = 0; i < pos.length; ++i){
				var input = pos[i];
				if(input.target){continue;}
				var x = input.x,
					y = input.y;
				ctx.clearRect(0,0,1,1);
				ctx.save();
				ctx.translate(-x, -y);
				this._render(ctx, true);
				input.target = ctx.getImageData(0, 0, 1, 1).data[0] ? this : null;
				ctx.restore();
			}
		},

		_hitTestGeometry: function(ctx, x, y){
			return ctx.isPointInPath(x, y) ? this : null;
		},

		_renderFill: function(/* Object */ ctx, /* Boolean */ apply){
			// summary:
			//		render fill for the shape
			// ctx:
			//		a canvas context object
			// apply:
			//		whether ctx.fill() shall be called
			if(ctx.pickingMode){
				if("canvasFill" in this && apply){
					ctx.fill();
				}
				return;
			}
			this.inherited(arguments);
		},

		_renderStroke: function(/* Object */ ctx){
			// summary:
			//		render stroke for the shape
			// ctx:
			//		a canvas context object
			// apply:
			//		whether ctx.stroke() shall be called
			if(this.strokeStyle && ctx.pickingMode){
				var c = this.strokeStyle.color;
				try{
					this.strokeStyle.color = new Color(ctx.strokeStyle);
					this.inherited(arguments);
				}finally{
					this.strokeStyle.color = c;
				}
			}else{
				this.inherited(arguments);
			}
		},

		// events

		getEventSource: function(){
			return this.surface.rawNode;
		},

		on: function(type, listener){
			// summary:
			//		Connects an event to this shape.

			var expectedTarget = this.rawNode;

			// note that event listeners' targets are automatically fixed up in the canvas's addEventListener method
			return on(this.getEventSource(), type, function(event){
				if(dom.isDescendant(event.target, expectedTarget)){
					listener.apply(expectedTarget, arguments);
				}
			});
		},

		connect: function(name, object, method){
			// summary:
			//		Deprecated. Connects a handler to an event on this shape. Use `on` instead.

			if(name.substring(0, 2) == "on"){
				name = name.substring(2);
			}
			return this.on(name, method ? lang.hitch(object, method) : lang.hitch(null, object));
		},

		disconnect: function(handle){
			// summary:
			//		Deprecated. Disconnects an event handler. Use `handle.remove` instead.

			handle.remove();
		}
	});

	canvasWithEvents.Group = declare("dojox.gfx.canvasWithEvents.Group", [canvasWithEvents.Shape, canvas.Group], {
		_testInputs: function(/*Object*/ ctx, /*Array*/ pos){
			var children = this.children,
				t = this.getTransform(),
				i,
				j,
				input;

			if(children.length === 0){
				return;
			}
			var posbk = [];
			for(i = 0; i < pos.length; ++i){
				input = pos[i];
				// backup position before transform applied
				posbk[i] = {
					x: input.x,
					y: input.y
				};
				if(input.target){continue;}
				var x = input.x, y = input.y;
				var p = t ? m.multiplyPoint(m.invert(t), x, y) : { x: x, y: y };
				input.x = p.x;
				input.y = p.y;
			}
			for(i = children.length - 1; i >= 0; --i){
				children[i]._testInputs(ctx, pos);
				// does it need more hit tests ?
				var allFound = true;
				for(j = 0; j < pos.length; ++j){
					if(pos[j].target == null){
						allFound = false;
						break;
					}
				}
				if(allFound){
					break;
				}
			}
			if(this.clip){
				// filter positive hittests against the group clipping area
				for(i = 0; i < pos.length; ++i){
					input = pos[i];
					input.x = posbk[i].x;
					input.y = posbk[i].y;
					if(input.target){
						ctx.clearRect(0,0,1,1);
						ctx.save();
						ctx.translate(-input.x, -input.y);
						this._render(ctx, true);
						if(!ctx.getImageData(0, 0, 1, 1).data[0]){
							input.target = null;
						}
						ctx.restore();
					}
				}
			}else{
				for(i = 0; i < pos.length; ++i){
					pos[i].x = posbk[i].x;
					pos[i].y = posbk[i].y;
				}
			}
		}

	});

	canvasWithEvents.Image = declare("dojox.gfx.canvasWithEvents.Image", [canvasWithEvents.Shape, canvas.Image], {
		_renderShape: function(/* Object */ ctx){
			// summary:
			//		render image
			// ctx:
			//		a canvas context object
			var s = this.shape;
			if(ctx.pickingMode){
				ctx.fillRect(s.x, s.y, s.width, s.height);
			}else{
				this.inherited(arguments);
			}
		},
		_hitTestGeometry: function(ctx, x, y){
			// TODO: improve hit testing to take into account transparency
			var s = this.shape;
			return x >= s.x && x <= s.x + s.width && y >= s.y && y <= s.y + s.height ? this : null;
		}
	});

	canvasWithEvents.Text = declare("dojox.gfx.canvasWithEvents.Text", [canvasWithEvents.Shape, canvas.Text], {
		_testInputs: function(ctx, pos){
			return this._hitTestPixel(ctx, pos);
		}
	});

	canvasWithEvents.Rect = declare("dojox.gfx.canvasWithEvents.Rect", [canvasWithEvents.Shape, canvas.Rect], {});
	canvasWithEvents.Circle = declare("dojox.gfx.canvasWithEvents.Circle", [canvasWithEvents.Shape, canvas.Circle], {});
	canvasWithEvents.Ellipse = declare("dojox.gfx.canvasWithEvents.Ellipse", [canvasWithEvents.Shape, canvas.Ellipse],{});
	canvasWithEvents.Line = declare("dojox.gfx.canvasWithEvents.Line", [canvasWithEvents.Shape, canvas.Line],{});
	canvasWithEvents.Polyline = declare("dojox.gfx.canvasWithEvents.Polyline", [canvasWithEvents.Shape, canvas.Polyline],{});
	canvasWithEvents.Path = declare("dojox.gfx.canvasWithEvents.Path", [canvasWithEvents.Shape, canvas.Path],{});
	canvasWithEvents.TextPath = declare("dojox.gfx.canvasWithEvents.TextPath", [canvasWithEvents.Shape, canvas.TextPath],{});

	// When events are dispatched using on.emit, certain properties of these events (like target) get overwritten by
	// the DOM. The only real way to deal with this at the moment, short of never using any standard event properties,
	// is to store this data out-of-band and fix up the event object passed to the listener by wrapping the listener.
	// The out-of-band data is stored here.
	var fixedEventData = null;

	canvasWithEvents.Surface = declare("dojox.gfx.canvasWithEvents.Surface", canvas.Surface, {
		constructor: function(){
			this._elementUnderPointer = null;
		},

		fixTarget: function(listener){
			// summary:
			//		Corrects the `target` properties of the event object passed to the actual listener.
			// listener: Function
			//		An event listener function.

			var surface = this;

			return function(event){
				var k;
				if(fixedEventData){
					if(has("dom-mutableEvents")){
						Object.defineProperties(event, fixedEventData);
					}else{
						event = makeFakeEvent(event);
						for(k in fixedEventData){
							event[k] = fixedEventData[k].value;
						}
					}
				}else{
					// non-synthetic events need to have target correction too, but since there is no out-of-band
					// data we need to figure out the target ourselves
					var canvas = surface.getEventSource(),
						target = canvas._dojoElementFromPoint(
							// touch events may not be fixed at this point, so clientX/Y may not be set on the
							// event object
							(event.changedTouches ? event.changedTouches[0] : event).pageX,
							(event.changedTouches ? event.changedTouches[0] : event).pageY
						);
					if(has("dom-mutableEvents")){
						Object.defineProperties(event, {
							target: {
								value: target,
								configurable: true,
								enumerable: true
							},
							gfxTarget: {
								value: target.shape,
								configurable: true,
								enumerable: true
							}
						});
					}else{
						event = makeFakeEvent(event);
						event.target = target;
						event.gfxTarget = target.shape;
					}
				}

				// fixTouchListener in dojo/on undoes target changes by copying everything from changedTouches even
				// if the value already exists on the event; of course, this canvas implementation currently only
				// supports one pointer at a time. if we wanted to make sure all the touches arrays' targets were
				// updated correctly as well, we could support multi-touch and this workaround would not be needed
				if(has("touch")){
					// some standard properties like clientX/Y are not provided on the main touch event object,
					// so copy them over if we need to
					if(event.changedTouches && event.changedTouches[0]){
						var changedTouch = event.changedTouches[0];
						for(k in changedTouch){
							if(!event[k]){
								if(has("dom-mutableEvents")){
									Object.defineProperty(event, k, {
										value: changedTouch[k],
										configurable: true,
										enumerable: true
									});
								}else{
									event[k] = changedTouch[k];
								}
							}
						}
					}
					event.corrected = event;
				}

				return listener.call(this, event);
			};
		},

		_checkPointer: function(event){
			// summary:
			//		Emits enter/leave/over/out events in response to the pointer entering/leaving the inner elements
			//		within the canvas.

			function emit(types, target, relatedTarget){
				// summary:
				//		Emits multiple synthetic events defined in `types` with the given target `target`.

				var oldBubbles = event.bubbles;

				for(var i = 0, type; (type = types[i]); ++i){
					// targets get reset when the event is dispatched so we need to give information to fixTarget to
					// restore the target on the dispatched event through a back channel
					fixedEventData = {
						target: { value: target, configurable: true, enumerable: true},
						gfxTarget: { value: target.shape, configurable: true, enumerable: true },
						relatedTarget: { value: relatedTarget, configurable: true, enumerable: true }
					};

					// bubbles can be set directly, though.
					Object.defineProperty(event, "bubbles", {
						value: type.bubbles,
						configurable: true,
						enumerable: true
					});

					on.emit(canvas, type.type, event);
					fixedEventData = null;
				}

				Object.defineProperty(event, "bubbles", { value: oldBubbles, configurable: true, enumerable: true });
			}

			// Types must be arrays because hash map order is not guaranteed but we must fire in order to match normal
			// event behaviour
			var TYPES = {
					out: [
						{ type: "mouseout", bubbles: true },
						{ type: "MSPointerOut", bubbles: true },
						{ type: "pointerout", bubbles: true },
						{ type: "mouseleave", bubbles: false },
						{ type: "dojotouchout", bubbles: true}
					],
					over: [
						{ type: "mouseover", bubbles: true },
						{ type: "MSPointerOver", bubbles: true },
						{ type: "pointerover", bubbles: true },
						{ type: "mouseenter", bubbles: false },
						{ type: "dojotouchover", bubbles: true}
					]
				},
				elementUnderPointer = event.target,
				oldElementUnderPointer = this._elementUnderPointer,
				canvas = this.getEventSource();

			if(oldElementUnderPointer !== elementUnderPointer){
				if(oldElementUnderPointer && oldElementUnderPointer !== canvas){
					emit(TYPES.out, oldElementUnderPointer, elementUnderPointer);
				}

				this._elementUnderPointer = elementUnderPointer;

				if(elementUnderPointer && elementUnderPointer !== canvas){
					emit(TYPES.over, elementUnderPointer, oldElementUnderPointer);
				}
			}
		},

		getEventSource: function(){
			return this.rawNode;
		},

		on: function(type, listener){
			// summary:
			//		Connects an event to this surface.

			return on(this.getEventSource(), type, listener);
		},

		connect: function(/*String*/ name, /*Object*/ object, /*Function|String*/ method){
			// summary:
			//		Deprecated. Connects a handler to an event on this surface. Use `on` instead.
			// name: String
			//		The event name
			// object: Object
			//		The object that method will receive as "this".
			// method: Function
			//		A function reference, or name of a function in context.

			if(name.substring(0, 2) == "on"){
				name = name.substring(2);
			}
			return this.on(name, method ? lang.hitch(object, method) : object);
		},

		disconnect: function(handle){
			// summary:
			//		Deprecated. Disconnects a handler. Use `handle.remove` instead.

			handle.remove();
		},

		_initMirrorCanvas: function(){
			// summary:
			//		Initialises a mirror canvas used for event hit detection.

			this._initMirrorCanvas = function(){};

			var canvas = this.getEventSource(),
				mirror = this.mirrorCanvas = canvas.ownerDocument.createElement("canvas");

			mirror.width = 1;
			mirror.height = 1;
			mirror.style.position = "absolute";
			mirror.style.left = mirror.style.top = "-99999px";
			canvas.parentNode.appendChild(mirror);

			var moveEvt = "mousemove";
			if(has("pointer-events")){
				moveEvt = "pointermove";
			}else if(has("MSPointer")){
				moveEvt = "MSPointerMove";
			}else if(has("touch-events")){
				moveEvt = "touchmove";
			}
			on(canvas, moveEvt, lang.hitch(this, "_checkPointer"));
		},

		destroy: function(){
			if(this.mirrorCanvas){
				this.mirrorCanvas.parentNode.removeChild(this.mirrorCanvas);
				this.mirrorCanvas = null;
			}
			this.inherited(arguments);
		}
	});

	canvasWithEvents.createSurface = function(parentNode, width, height){
		// summary:
		//		creates a surface (Canvas)
		// parentNode: Node
		//		a parent node
		// width: String
		//		width of surface, e.g., "100px"
		// height: String
		//		height of surface, e.g., "100px"

		if(!width && !height){
			var pos = domGeom.position(parentNode);
			width  = width  || pos.w;
			height = height || pos.h;
		}
		if(typeof width === "number"){
			width = width + "px";
		}
		if(typeof height === "number"){
			height = height + "px";
		}

		var surface = new canvasWithEvents.Surface(),
			parent = dom.byId(parentNode),
			canvas = parent.ownerDocument.createElement("canvas");

		canvas.width  = g.normalizedLength(width);	// in pixels
		canvas.height = g.normalizedLength(height);	// in pixels

		parent.appendChild(canvas);
		surface.rawNode = canvas;
		surface._parent = parent;
		surface.surface = surface;

		g._base._fixMsTouchAction(surface);

		// any event handler added to the canvas needs to have its target fixed.
		var oldAddEventListener = canvas.addEventListener,
			oldRemoveEventListener = canvas.removeEventListener,
			listeners = [];

		var addEventListenerImpl = function(type, listener, useCapture){
			surface._initMirrorCanvas();

			var actualListener = surface.fixTarget(listener);
			listeners.push({ original: listener, actual: actualListener });
			oldAddEventListener.call(this, type, actualListener, useCapture);
		};
		var removeEventListenerImpl = function(type, listener, useCapture){
			for(var i = 0, record; (record = listeners[i]); ++i){
				if(record.original === listener){
					oldRemoveEventListener.call(this, type, record.actual, useCapture);
					listeners.splice(i, 1);
					break;
				}
			}
		};
		try{
			Object.defineProperties(canvas, {
				addEventListener: {
					value: addEventListenerImpl,
					enumerable: true,
					configurable: true
				},
				removeEventListener: {
					value: removeEventListenerImpl
				}
			});
		}catch(e){
			// Object.defineProperties fails on iOS 4-5. "Not supported on DOM objects").
			canvas.addEventListener = addEventListenerImpl;
			canvas.removeEventListener = removeEventListenerImpl;
		}


		canvas._dojoElementFromPoint = function(x, y){
			// summary:
			//		Returns the shape under the given (x, y) coordinate.
			// evt:
			//		mouse event

			if(!surface.mirrorCanvas){
				return this;
			}

			var surfacePosition = domGeom.position(this, true);

			// use canvas-relative positioning
			x -= surfacePosition.x;
			y -= surfacePosition.y;

			var mirror = surface.mirrorCanvas,
				ctx = mirror.getContext("2d"),
				children = surface.children;

			ctx.clearRect(0, 0, mirror.width, mirror.height);
			ctx.save();
			ctx.strokeStyle = "rgba(127,127,127,1.0)";
			ctx.fillStyle = "rgba(127,127,127,1.0)";
			ctx.pickingMode = true;

			// TODO: Make inputs non-array
			var inputs = [ { x: x, y: y } ];

			// process the inputs to find the target.
			for(var i = children.length - 1; i >= 0; i--){
				children[i]._testInputs(ctx, inputs);

				if(inputs[0].target){
					break;
				}
			}
			ctx.restore();
			return inputs[0] && inputs[0].target ? inputs[0].target.rawNode : this;
		};


		return surface; // dojox/gfx.Surface
	};

	var Creator = {
		createObject: function(){
			// summary:
			//		Creates a synthetic, partially-interoperable Element object used to uniquely identify the given
			//		shape within the canvas pseudo-DOM.

			var shape = this.inherited(arguments),
				listeners = {};

			shape.rawNode = {
				shape: shape,
				ownerDocument: shape.surface.rawNode.ownerDocument,
				parentNode: shape.parent ? shape.parent.rawNode : null,
				addEventListener: function(type, listener){
					var listenersOfType = listeners[type] = (listeners[type] || []);
					for(var i = 0, record; (record = listenersOfType[i]); ++i){
						if(record.listener === listener){
							return;
						}
					}

					listenersOfType.push({
						listener: listener,
						handle: aspect.after(this, "on" + type, shape.surface.fixTarget(listener), true)
					});
				},
				removeEventListener: function(type, listener){
					var listenersOfType = listeners[type];
					if(!listenersOfType){
						return;
					}
					for(var i = 0, record; (record = listenersOfType[i]); ++i){
						if(record.listener === listener){
							record.handle.remove();
							listenersOfType.splice(i, 1);
							return;
						}
					}
				}
			};
			return shape;
		}
	};

	canvasWithEvents.Group.extend(Creator);
	canvasWithEvents.Surface.extend(Creator);

	return canvasWithEvents;
});
