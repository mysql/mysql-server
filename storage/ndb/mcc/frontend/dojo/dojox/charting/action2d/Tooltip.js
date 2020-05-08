define(["dijit/Tooltip", "dojo/_base/lang", "dojo/_base/declare", "dojo/_base/window", "dojo/_base/connect", "dojo/dom-style",
	"./PlotAction", "dojox/gfx/matrix", "dojo/has", "dojo/has!dojo-bidi?../bidi/action2d/Tooltip", 
	"dojox/lang/functional", "dojox/lang/functional/scan", "dojox/lang/functional/fold"],
	function(DijitTooltip, lang, declare, win, hub, domStyle, PlotAction, m, has, BidiTooltip, df){
	
	/*=====
	var __TooltipCtorArgs = {
			// summary:
			//		Additional arguments for tooltip actions.
			// duration: Number?
			//		The amount of time in milliseconds for an animation to last.  Default is 400.
			// easing: dojo/fx/easing/*?
			//		An easing object (see dojo.fx.easing) for use in an animation.  The
			//		default is dojo.fx.easing.backOut.
			// text: Function?
			//		The function that produces the text to be shown within a tooltip.  By default this will be
			//		set by the plot in question, by returning the value of the element.
			// mouseOver: Boolean?
            //		Whether the tooltip is enabled on mouse over or on mouse click / touch down. Default is true.
	};
	=====*/

	var DEFAULT_TEXT = function(o, plot){
		var t = o.run && o.run.data && o.run.data[o.index];
		if(t && typeof t != "number" && (t.tooltip || t.text)){
			return t.tooltip || t.text;
		}
		if(plot.tooltipFunc){
			return plot.tooltipFunc(o);
		}else{
			return o.y;
		}
	};

	var pi4 = Math.PI / 4, pi2 = Math.PI / 2;
	
	var Tooltip = declare(has("dojo-bidi")? "dojox.charting.action2d.NonBidiTooltip" : "dojox.charting.action2d.Tooltip", PlotAction, {
		// summary:
		//		Create an action on a plot where a tooltip is shown when hovering over an element.

		// the data description block for the widget parser
		defaultParams: {
			text: DEFAULT_TEXT,	// the function to produce a tooltip from the object
            mouseOver: true,
			defaultPosition: ["after-centered", "before-centered"]
		},
		optionalParams: {},	// no optional parameters

		constructor: function(chart, plot, kwArgs){
			// summary:
			//		Create the tooltip action and connect it to the plot.
			// chart: dojox/charting/Chart
			//		The chart this action belongs to.
			// plot: String?
			//		The plot this action is attached to.  If not passed, "default" is assumed.
			// kwArgs: __TooltipCtorArgs?
			//		Optional keyword arguments object for setting parameters.
			this.text = kwArgs && kwArgs.text ? kwArgs.text : DEFAULT_TEXT;
			this.mouseOver = kwArgs && kwArgs.mouseOver != undefined ? kwArgs.mouseOver : true;
			this.defaultPosition = kwArgs && kwArgs.defaultPosition != undefined ? kwArgs.defaultPosition : ["after-centered", "before-centered"];
			this.connect();
		},
		
		process: function(o){
			// summary:
			//		Process the action on the given object.
			// o: dojox/gfx/shape.Shape
			//		The object on which to process the highlighting action.
			if(o.type === "onplotreset" || o.type === "onmouseout"){
                DijitTooltip.hide(this.aroundRect);
				this.aroundRect = null;
				if(o.type === "onplotreset"){
					delete this.angles;
				}
				return;
			}
			
			if(!o.shape || (this.mouseOver && o.type !== "onmouseover") || (!this.mouseOver && o.type !== "onclick")){ return; }
			
			// calculate relative coordinates and the position
			var aroundRect = {type: "rect"}, position = this.defaultPosition;
			switch(o.element){
				case "marker":
					aroundRect.x = o.cx;
					aroundRect.y = o.cy;
					aroundRect.w = aroundRect.h = 1;
					break;
				case "circle":
					aroundRect.x = o.cx - o.cr;
					aroundRect.y = o.cy - o.cr;
					aroundRect.w = aroundRect.h = 2 * o.cr;
					break;
				case "spider_circle":
					aroundRect.x = o.cx;
					aroundRect.y = o.cy ;
					aroundRect.w = aroundRect.h = 1;
					break;
				case "spider_plot":
					return;
				case "column":
					position = ["above-centered", "below-centered"];
					// intentional fall down
				case "bar":
					aroundRect = lang.clone(o.shape.getShape());
					aroundRect.w = aroundRect.width;
					aroundRect.h = aroundRect.height;
					break;
				case "candlestick":
					aroundRect.x = o.x;
					aroundRect.y = o.y;
					aroundRect.w = o.width;
					aroundRect.h = o.height;
					break;
				default:
				//case "slice":
					if(!this.angles){
						// calculate the running total of slice angles
						var filteredRun = typeof o.run.data[0] == "number" ?
								df.map(o.run.data, "x ? Math.max(x, 0) : 0") : df.map(o.run.data, "x ? Math.max(x.y, 0) : 0");
						this.angles = df.map(df.scanl(filteredRun, "+", 0),
							"* 2 * Math.PI / this", df.foldl(filteredRun, "+", 0));
					}
					var startAngle = m._degToRad(o.plot.opt.startAngle),
						angle = (this.angles[o.index] + this.angles[o.index + 1]) / 2 + startAngle;
					aroundRect.x = o.cx + o.cr * Math.cos(angle);
					aroundRect.y = o.cy + o.cr * Math.sin(angle);
					aroundRect.w = aroundRect.h = 1;
                    // depending on startAngle we might go out of the 0-2*PI range, normalize that
                    if(startAngle && (angle < 0 || angle > 2 * Math.PI)){
						angle = Math.abs(2 * Math.PI  - Math.abs(angle));
					}
					// calculate the position
					if(angle < pi4){
						// do nothing: the position is right
					}else if(angle < pi2 + pi4){
						position = ["below-centered", "above-centered"];
					}else if(angle < Math.PI + pi4){
						position = ["before-centered", "after-centered"];
					}else if(angle < 2 * Math.PI - pi4){
						position = ["above-centered", "below-centered"];
					}
					/*
					else{
						// do nothing: the position is right
					}
					*/
					break;
			}
			if(has("dojo-bidi")){
				this._recheckPosition(o,aroundRect,position);
			}
			// adjust relative coordinates to absolute, and remove fractions
			var lt = this.chart.getCoords();
			aroundRect.x += lt.x;
			aroundRect.y += lt.y;
			aroundRect.x = Math.round(aroundRect.x);
			aroundRect.y = Math.round(aroundRect.y);
			aroundRect.w = Math.ceil(aroundRect.w);
			aroundRect.h = Math.ceil(aroundRect.h);
			this.aroundRect = aroundRect;

			var tooltipText = this.text(o, this.plot);
			if(tooltipText){
				DijitTooltip.show(this._format(tooltipText), this.aroundRect, position);
			}
			if(!this.mouseOver){
				this._handle = hub.connect(win.doc, "onclick", this, "onClick");
			}
		},
		onClick: function(){
			this.process({ type: "onmouseout"});
		},
		_recheckPosition: function(obj,rect,position){			
		},
		_format: function(tooltipText){
			return tooltipText;
		}
	});
	return has("dojo-bidi")? declare("dojox.charting.action2d.Tooltip", [Tooltip, BidiTooltip]) : Tooltip;
});
