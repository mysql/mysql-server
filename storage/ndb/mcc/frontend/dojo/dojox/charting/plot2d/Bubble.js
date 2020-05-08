define(["dojo/_base/lang", "dojo/_base/declare", "dojo/_base/array", "dojo/has",
		"./CartesianBase", "./_PlotEvents", "./common", "dojox/lang/functional",
		"dojox/lang/utils", "dojox/gfx/fx"],
	function(lang, declare, arr, has, CartesianBase, _PlotEvents, dc, df, du, fx){

	return declare("dojox.charting.plot2d.Bubble", [CartesianBase, _PlotEvents], {
		// summary:
		//		A plot representing bubbles.  Note that data for Bubbles requires 3 parameters,
		//		in the form of:  { x, y, size }, where size determines the size of the bubble.
		defaultParams: {
			animate: null   // animate bars into place
		},
		optionalParams: {
			// theme component
			stroke:		{},
			outline:	{},
			shadow:		{},
			fill:		{},
			filter:     {},
			styleFunc:	null,
			font:		"",
			fontColor:	"",
			labelFunc: null
		},

		constructor: function(chart, kwArgs){
			// summary:
			//		Create a plot of bubbles.
			// chart: dojox/charting/Chart
			//		The chart this plot belongs to.
			// kwArgs: dojox.charting.plot2d.__DefaultCtorArgs?
			//		Optional keyword arguments object to help define plot parameters.
			this.opt = lang.clone(lang.mixin(this.opt, this.defaultParams));
			du.updateWithObject(this.opt, kwArgs);
			du.updateWithPattern(this.opt, kwArgs, this.optionalParams);
			if(!this.opt.labelFunc){
				this.opt.labelFunc = function(value, fixed, precision){
					return this._getLabel(value.size, fixed, precision);
				};
			}
			this.animate = this.opt.animate;
		},

		//	override the render so that we are plotting only circles.
		render: function(dim, offsets){
			// summary:
			//		Run the calculations for any axes for this plot.
			// dim: Object
			//		An object in the form of { width, height }
			// offsets: Object
			//		An object of the form { l, r, t, b}.
			// returns: dojox/charting/plot2d/Bubble
			//		A reference to this plot for functional chaining.
			var s;
			if(this.zoom && !this.isDataDirty()){
				return this.performZoom(dim, offsets);
			}
			this.resetEvents();
			this.dirty = this.isDirty();
			if(this.dirty){
				arr.forEach(this.series, dc.purgeGroup);
				this._eventSeries = {};
				this.cleanGroup();
				s = this.getGroup();
				df.forEachRev(this.series, function(item){ item.cleanGroup(s); });
			}

			var t = this.chart.theme,
				ht = this._hScaler.scaler.getTransformerFromModel(this._hScaler),
				vt = this._vScaler.scaler.getTransformerFromModel(this._vScaler),
				events = this.events();

			for(var i = 0; i < this.series.length; i++){
				var run = this.series[i];
				if(!this.dirty && !run.dirty){
					t.skip();
					this._reconnectEvents(run.name);
					continue;
				}
				run.cleanGroup();
				if(!run.data.length){
					run.dirty = false;
					t.skip();
					continue;
				}

				if(typeof run.data[0] == "number"){
					console.warn("dojox.charting.plot2d.Bubble: the data in the following series cannot be rendered as a bubble chart; ", run);
					continue;
				}

				var theme = t.next("circle", [this.opt, run]),
					points = arr.map(run.data, function(v){
						return v ? {
							x: ht(v.x) + offsets.l,
							y: dim.height - offsets.b - vt(v.y),
							radius: this._vScaler.bounds.scale * (v.size / 2)
						} : null;
					}, this);

				if(run.hidden){
					run.dyn.fill = theme.series.fill;
					run.dyn.stroke =  theme.series.stroke;
					continue;
				}
				s = run.group;

				var frontCircles = null, outlineCircles = null, shadowCircles = null, styleFunc = this.opt.styleFunc;

				var getFinalTheme = function(item){
					if(styleFunc){
						return t.addMixin(theme, "circle", [item, styleFunc(item)], true);
					}
					return t.addMixin(theme, "circle", item, true);
				};

				// make shadows if needed
				if(theme.series.shadow){
					shadowCircles = arr.map(points, function(item, i){
						if(!this.isNullValue(item)){
							var finalTheme = getFinalTheme(run.data[i]),
								shadow = finalTheme.series.shadow;
							var shape = s.createCircle({
								cx: item.x + shadow.dx, cy: item.y + shadow.dy, r: item.radius
							}).setStroke(shadow).setFill(shadow.color);
							if(this.animate){
								this._animateBubble(shape, dim.height - offsets.b, item.radius);
							}
							return shape;
						}
						return null;
					}, this);
					if(shadowCircles.length){
						run.dyn.shadow = shadowCircles[shadowCircles.length - 1].getStroke();
					}
				}

				// make outlines if needed
				if(theme.series.outline){
					outlineCircles = arr.map(points, function(item, i){
						if(!this.isNullValue(item)){
							var finalTheme = getFinalTheme(run.data[i]),
								outline = dc.makeStroke(finalTheme.series.outline);
							outline.width = 2 * outline.width + (theme.series.stroke && theme.series.stroke.width || 0);
							var shape = s.createCircle({
								cx: item.x, cy: item.y, r: item.radius
							}).setStroke(outline);
							if(this.animate){
								this._animateBubble(shape, dim.height - offsets.b, item.radius);
							}
							return shape;
						}
						return null;
					}, this);
					if(outlineCircles.length){
						run.dyn.outline = outlineCircles[outlineCircles.length - 1].getStroke();
					}
				}

				//	run through the data and add the circles.
				frontCircles = arr.map(points, function(item, i){
					if(!this.isNullValue(item)){
						var finalTheme = getFinalTheme(run.data[i]),
							rect = {
								x: item.x - item.radius,
								y: item.y - item.radius,
								width:  2 * item.radius,
								height: 2 * item.radius
							};
						var specialFill = this._plotFill(finalTheme.series.fill, dim, offsets);
						specialFill = this._shapeFill(specialFill, rect);
						var shape = s.createCircle({
							cx: item.x, cy: item.y, r: item.radius
						}).setFill(specialFill).setStroke(finalTheme.series.stroke);
						if(shape.setFilter && finalTheme.series.filter){
							shape.setFilter(finalTheme.series.filter);
						}
						if(this.animate){
							this._animateBubble(shape, dim.height - offsets.b, item.radius);
						}
						this.createLabel(s, run.data[i], rect, finalTheme);
						return shape;
					}
					return null;
				}, this);
				if(frontCircles.length){
					run.dyn.fill   = frontCircles[frontCircles.length - 1].getFill();
					run.dyn.stroke = frontCircles[frontCircles.length - 1].getStroke();
				}

				if(events){
					var eventSeries = new Array(frontCircles.length);
					arr.forEach(frontCircles, function(s, i){
						if(s !== null){
							var o = {
								element: "circle",
								index:   i,
								run:     run,
								shape:   s,
								outline: outlineCircles && outlineCircles[i] || null,
								shadow:  shadowCircles && shadowCircles[i] || null,
								x:       run.data[i].x,
								y:       run.data[i].y,
								r:       run.data[i].size / 2,
								cx:      points[i].x,
								cy:      points[i].y,
								cr:      points[i].radius
							};
							this._connectEvents(o);
							eventSeries[i] = o;
						}
					}, this);
					this._eventSeries[run.name] = eventSeries;
				}else{
					delete this._eventSeries[run.name];
				}

				run.dirty = false;
			}
			this.dirty = false;
			// chart mirroring starts
			if(has("dojo-bidi")){
				this._checkOrientation(this.group, dim, offsets);
			}
			// chart mirroring ends
			return this;	//	dojox/charting/plot2d/Bubble
		},
		_animateBubble: function(shape, offset, size){
			fx.animateTransform(lang.delegate({
				shape: shape,
				duration: 1200,
				transform: [
					{name: "translate", start: [0, offset], end: [0, 0]},
					{name: "scale", start: [0, 1/size], end: [1, 1]},
					{name: "original"}
				]
			}, this.animate)).play();
		}
	});
});
