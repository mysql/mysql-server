define(["dojo/_base/lang", "dojo/_base/array" ,"dojo/_base/declare", "dojo/dom-geometry", "dojo/_base/Color",
		"./Base", "./_PlotEvents", "./common",
		"dojox/gfx", "dojox/gfx/matrix", "dojox/lang/functional", "dojox/lang/utils","dojo/has"],
	function(lang, arr, declare, domGeom, Color, Base, PlotEvents, dc, g, m, df, du, has){

	/*=====
	declare("dojox.charting.plot2d.__PieCtorArgs", dojox.charting.plot2d.__DefaultCtorArgs, {
		// summary:
		//		Specialized keyword arguments object for use in defining parameters on a Pie chart.

		// labels: Boolean?
		//		Whether or not to draw labels for each pie slice.  Default is true.
		labels:			true,

		// ticks: Boolean?
		//		Whether or not to draw ticks to labels within each slice. Default is false.
		ticks:			false,

		// fixed: Boolean?
		//		Whether a fixed precision must be applied to data values for display. Default is true.
		fixed:			true,

		// precision: Number?
		//		The precision at which to round data values for display. Default is 0.
		precision:		1,

		// labelOffset: Number?
		//		The amount in pixels by which to offset labels.  Default is 20.
		labelOffset:	20,

		// labelStyle: String?
		//		Options as to where to draw labels.  Values include "default", and "columns".	Default is "default".
		labelStyle:		"default",	// default/columns

		// omitLabels: Boolean?
		//		Whether labels of slices small to the point of not being visible are omitted.	Default false.
		omitLabels: false,

		// htmlLabels: Boolean?
		//		Whether or not to use HTML to render slice labels. Default is true.
		htmlLabels:		true,

		// radGrad: String?
		//		The type of radial gradient to use in rendering.  Default is "native".
		radGrad:        "native",

		// fanSize: Number?
		//		The amount for a radial gradient.  Default is 5.
		fanSize:		5,

		// startAngle: Number?
		//		Where to being rendering gradients in slices, in degrees.  Default is 0.
		startAngle:     0,

		// radius: Number?
		//		The size of the radial gradient.  Default is 0.
		radius:		0,

		// shadow: dojox.gfx.Stroke?
		//		An optional stroke to use to draw any shadows for a series on a plot.
		shadow:		{},

		// fill: dojox.gfx.Fill?
		//		Any fill to be used for elements on the plot.
		fill:		{},

		// filter: dojox.gfx.Filter?
		//		An SVG filter to be used for elements on the plot. gfx SVG renderer must be used and dojox/gfx/svgext must
		//		be required for this to work.
		filter:		{},

		// styleFunc: Function?
		//		A function that returns a styling object for the a given data item.
		styleFunc:	null,

		// innerRadius: Number?
		//		The inner radius of a ring in percent (0-100).  If value < 0
		//		then it is assumed to be pixels, not percent.
		innerRadius:	0,

		//  minWidth: Number?
		//      The minimum width of a pie slice at its chord. The default is 10px.
		minWidth:   10

	});
	=====*/

	var FUDGE_FACTOR = 0.2; // use to overlap fans

	return declare("dojox.charting.plot2d.Pie", [Base, PlotEvents], {
		// summary:
		//		The plot that represents a typical pie chart.
		defaultParams: {
			labels:			true,
			ticks:			false,
			fixed:			true,
			precision:		1,
			labelOffset:	20,
			labelStyle:		"default",	// default/columns
			htmlLabels:		true,		// use HTML to draw labels
			radGrad:       "native",	// or "linear", or "fan"
			fanSize:		   5,			// maximum fan size in degrees
			startAngle:    0,			// start angle for slices in degrees
			innerRadius:	0,			// inner radius in pixels
			minWidth:      0,			// minimal width of degenerated slices
			zeroDataMessage: ""     // The message to display when there is no data, if provided by the user.
		},
		optionalParams: {
			radius:		0,
			omitLabels: false,
			// theme components
			stroke:		{},
			outline:	{},
			shadow:		{},
			fill:		{},
			filter:     {},
			styleFunc:	null,
			font:		"",
			fontColor:	"",
			labelWiring: {}
		},

		constructor: function(chart, kwArgs){
			// summary:
			//		Create a pie plot.
			this.opt = lang.clone(this.defaultParams);
			du.updateWithObject(this.opt, kwArgs);
			du.updateWithPattern(this.opt, kwArgs, this.optionalParams);
			this.axes = [];
			this.run = null;
			this.dyn = [];
			this.runFilter = [];
			if(kwArgs && kwArgs.hasOwnProperty("innerRadius")){
				this._plotSetInnerRadius = true;
			}
		},
		clear: function(){
			// summary:
			//		Clear out all of the information tied to this plot.
			// returns: dojox/charting/plot2d/Pie
			//		A reference to this plot for functional chaining.
			this.inherited(arguments);
			this.dyn = [];
			this.run = null;
			return this;	//	dojox/charting/plot2d/Pie
		},
		setAxis: function(axis){
			// summary:
			//		Dummy method, since axes are irrelevant with a Pie chart.
			// returns: dojox/charting/plot2d/Pie
			//		The reference to this plot for functional chaining.
			return this;	//	dojox/charting/plot2d/Pie
		},
		addSeries: function(run){
			// summary:
			//		Add a series of data to this plot.
			// returns: dojox/charting/plot2d/Pie
			//		The reference to this plot for functional chaining.
			this.run = run;
			return this;	//	dojox/charting/plot2d/Pie
		},
		getSeriesStats: function(){
			// summary:
			//		Returns default stats (irrelevant for this type of plot).
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			return lang.delegate(dc.defaultStats); // Object
		},
		getRequiredColors: function(){
			// summary:
			//		Return the number of colors needed to draw this plot.
			return this.run ? this.run.data.length : 0;
		},

		render: function(dim, offsets){
			//	summary:
			//		Render the plot on the chart.
			//	dim: Object
			//		An object of the form { width, height }.
			//	offsets: Object
			//		An object of the form { l, r, t, b }.
			//	returns: dojox/charting/plot2d/Pie
			//		A reference to this plot for functional chaining.
			if(!this.dirty){ return this; }
			this.resetEvents();
			this.dirty = false;
			this._eventSeries = {};
			this.cleanGroup();
			var s = this.group, t = this.chart.theme;

			if(!this._plotSetInnerRadius && t && t.pieInnerRadius){
				this.opt.innerRadius = t.pieInnerRadius;
			}

			// calculate the geometry
			var rx = (dim.width  - offsets.l - offsets.r) / 2,
				ry = (dim.height - offsets.t - offsets.b) / 2,
				r  = Math.min(rx, ry),
				taFont = "font" in this.opt ? this.opt.font : t.axis.tick.titleFont || "",
				size = taFont ? g.normalizedLength(g.splitFontString(taFont).size) : 0,
				taFontColor = this.opt.hasOwnProperty("fontColor") ? this.opt.fontColor : t.axis.tick.fontColor,
				startAngle = m._degToRad(this.opt.startAngle),
				start = startAngle, filteredRun, slices, labels, shift, labelR,
				run = this.run.data,
				events = this.events();

			/* Added to handle no data case */
			var noDataFunc = lang.hitch(this, function(){
				var ct = t.clone();
				var themes = df.map(run, function(v){
					var tMixin = [this.opt, this.run];
					if(v !== null && typeof v != "number"){
						tMixin.push(v);
					}
					if(this.opt.styleFunc){
						tMixin.push(this.opt.styleFunc(v));
					}
					return ct.next("slice", tMixin, true);
				}, this);

				// Draw initial pie, with text in it noting 0 data.
				if("radius" in this.opt){
					r = this.opt.radius < r ? this.opt.radius : r;
				}

				var circle = {
					cx: offsets.l + rx,
					cy: offsets.t + ry,
					r:  r
				};
				var rColor = new Color(taFontColor);
				// If we have a radius, we'll need to fade the ring some
				if(this.opt.innerRadius){
					rColor.a = 0.1;
				}
				var ring = this._createRing(s, circle).setStroke(rColor);
				if(this.opt.innerRadius){
					// If we have a radius, fill it with the faded color.
					ring.setFill(rColor);
				}
				if(this.opt.zeroDataMessage){
					this.renderLabel(s, circle.cx, circle.cy + size/3, this.opt.zeroDataMessage, {
						series: {
							font: taFont,
							fontColor: taFontColor
						}
					},	null, "middle");
				}
				this.dyn = [];
				arr.forEach(run, function(item, i){
					this.dyn.push({
						fill: this._plotFill(themes[i].series.fill, dim, offsets),
						stroke: themes[i].series.stroke});
				}, this);
			});
			/* END Added to handle no data case */

			// Draw over circle!
			if(!this.run && !this.run.data.ength){
				noDataFunc();
				return this;
			}
			if(typeof run[0] == "number"){
				filteredRun = df.map(run, "x ? Math.max(x, 0) : 0");
				if(df.every(filteredRun, "<= 0")){
					noDataFunc();
					return this;
				}
				slices = df.map(filteredRun, "/this", df.foldl(filteredRun, "+", 0));
				if(this.opt.labels){
					labels = arr.map(slices, function(x){
						return x > 0 ? this._getLabel(x * 100) + "%" : "";
					}, this);
				}
			}else{
				filteredRun = df.map(run, "x ? Math.max(x.y, 0) : 0");
				if(!filteredRun.length || df.every(filteredRun, "<= 0")){
					noDataFunc();
					return this;
				}
				slices = df.map(filteredRun, "/this", df.foldl(filteredRun, "+", 0));
				if(this.opt.labels){
					labels = arr.map(slices, function(x, i){
						if(x < 0){ return ""; }
						var v = run[i];
						return v.hasOwnProperty("text") ? v.text : this._getLabel(x * 100) + "%";
					}, this);
				}
			}
			var themes = df.map(run, function(v){
				var tMixin = [this.opt, this.run];
				if(v !== null && typeof v != "number"){
					tMixin.push(v);
				}
				if(this.opt.styleFunc){
					tMixin.push(this.opt.styleFunc(v));
				}
				return t.next("slice", tMixin, true);
			}, this);

			if(this.opt.labels) {
				shift = df.foldl1(df.map(labels, function(label, i){
					var font = themes[i].series.font;
					return g._base._getTextBox(label, {font: font}).w;
				}, this), "Math.max(a, b)") / 2;

				if(this.opt.labelOffset < 0){
					r = Math.min(rx - 2 * shift, ry - size) + this.opt.labelOffset;
				}
			}
			if(this.opt.hasOwnProperty("radius")){
				r = this.opt.radius < r * 0.9 ? this.opt.radius : r * 0.9;
			}

			if (!this.opt.radius && this.opt.labels && this.opt.labelStyle == "columns") {
				r = r / 2;
				if (rx > ry && rx > r * 2) {
					r *= rx / (r * 2);
				}
				if (r >= ry * 0.8) {
					r = ry * 0.8;
				}
			} else {
				if (r >= ry * 0.9) {
					r = ry * 0.9;
				}
			}

			labelR = r - this.opt.labelOffset;

			var circle = {
					cx: offsets.l + rx,
					cy: offsets.t + ry,
					r:  r
				};

			this.dyn = [];
			// draw slices
			var eventSeries = new Array(slices.length);

			// Calulate primarily size for each slice
			var slicesSteps = [], localStart = start;
			var minWidth = this.opt.minWidth;
			arr.forEach(slices, function(slice, i){
				if(slice === 0){
					slicesSteps[i] = {
						step: 0,
						end: localStart,
						start: localStart,
						weak: false
					};
					return;
				}
				var end = localStart + slice * 2 * Math.PI;
				if(i === slices.length - 1){
					end = startAngle + 2 * Math.PI;
				}
				var step = end - localStart,
					dist = step * r;
				slicesSteps[i] = {
					step:  step,
					start: localStart,
					end:   end,
					weak: dist < minWidth
				};
				localStart = end;
			});

			if(minWidth > 0){
				var weakCount = 0, weakCoef = minWidth / r, oldWeakCoefSum = 0, i;
				for(i = slicesSteps.length - 1; i >= 0; i--){
					if(slicesSteps[i].weak){
						++weakCount;
						oldWeakCoefSum += slicesSteps[i].step;
						slicesSteps[i].step = weakCoef;
					}
				}
				// make sure that our steps are small enough
				var weakCoefSum = weakCount * weakCoef;
				if(weakCoefSum > Math.PI){
					weakCoef = Math.PI / weakCount;
					for(i = 0; i < slicesSteps.length; ++i){
						if(slicesSteps[i].weak){
							slicesSteps[i].step = weakCoef;
						}
					}
					weakCoefSum = Math.PI;
				}
				// now let's redistribute percentage
				if(weakCount > 0){
					weakCoef = 1 - (weakCoefSum - oldWeakCoefSum) / 2 / Math.PI;
					for(i = 0; i < slicesSteps.length; ++i){
						if(!slicesSteps[i].weak){
							slicesSteps[i].step = weakCoef * slicesSteps[i].step;
						}
					}
				}
				// now let's update start and end values
				for(i = 0; i < slicesSteps.length; ++i){
					slicesSteps[i].start = i ? slicesSteps[i].end : localStart;
					slicesSteps[i].end = slicesSteps[i].start + slicesSteps[i].step;
				}
				// let's make sure that our last end is exactly 2 * Math.PI
				for(i = slicesSteps.length - 1; i >= 0; --i){
					if(slicesSteps[i].step !== 0){
						slicesSteps[i].end = localStart + 2 * Math.PI;
						break;
					}
				}
			}

			localStart = start;
			var o, specialFill;
			arr.some(slices, function(slice, i){
				var shape;
				var v = run[i], theme = themes[i];

				if(slice >= 1){
					// whole pie
					specialFill = this._plotFill(theme.series.fill, dim, offsets);
					specialFill = this._shapeFill(specialFill,
						{
							x: circle.cx - circle.r, y: circle.cy - circle.r,
							width: 2 * circle.r, height: 2 * circle.r
						});
					specialFill = this._pseudoRadialFill(specialFill, {x: circle.cx, y: circle.cy}, circle.r);
					shape = this._createRing(s, circle).setFill(specialFill).setStroke(theme.series.stroke);
					this.dyn.push({fill: specialFill, stroke: theme.series.stroke});

					if(events){
						o = {
							element: "slice",
							index:   i,
							run:     this.run,
							shape:   shape,
							x:       i,
							y:       typeof v == "number" ? v : v.y,
							cx:      circle.cx,
							cy:      circle.cy,
							cr:      r
						};
						this._connectEvents(o);
						eventSeries[i] = o;
					}

					var k;
					for(k = i + 1; k < slices.length; k++){
						theme = themes[k];
						this.dyn.push({fill: theme.series.fill, stroke: theme.series.stroke});
					}
					return true;	// stop iteration
				}

				if(slicesSteps[i].step === 0){
					// degenerated slice
					// But we still want a fill since this will be skipped and we need the fill
					// for the label.
					this.dyn.push({fill: theme.series.fill, stroke: theme.series.stroke});
					return false;	// continue
				}

				// calculate the geometry of the slice
				var step = slicesSteps[i].step,
					x1 = circle.cx + r * Math.cos(localStart),
					y1 = circle.cy + r * Math.sin(localStart),
					x2 = circle.cx + r * Math.cos(localStart + step),
					y2 = circle.cy + r * Math.sin(localStart + step);
				// draw the slice
				var fanSize = m._degToRad(this.opt.fanSize), stroke;
				if(theme.series.fill && theme.series.fill.type === "radial" && this.opt.radGrad === "fan" && step > fanSize){
					var group = s.createGroup(), nfans = Math.ceil(step / fanSize), delta = step / nfans;
					specialFill = this._shapeFill(theme.series.fill,
						{x: circle.cx - circle.r, y: circle.cy - circle.r, width: 2 * circle.r, height: 2 * circle.r});
					var j, alpha, beta, fansx, fansy, fanex, faney;
					for(j = 0; j < nfans; ++j){
						alpha = localStart + (j - FUDGE_FACTOR) * delta;
						beta  = localStart + (j + 1 + FUDGE_FACTOR) * delta;
						fansx = j == 0 ? x1 : circle.cx + r * Math.cos(alpha);
						fansy = j == 0 ? y1 : circle.cy + r * Math.sin(alpha);
						fanex = j == nfans - 1 ? x2 : circle.cx + r * Math.cos(beta);
						faney = j == nfans - 1 ? y2 : circle.cy + r * Math.sin(beta);
						this._createSlice(group, circle, r, fansx, fansy, fanex, faney, alpha, delta).
							setFill(this._pseudoRadialFill(specialFill, {x: circle.cx, y: circle.cy}, r,
								localStart + (j + 0.5) * delta, localStart + (j + 0.5) * delta));
					}
					stroke = theme.series.stroke;
					this._createSlice(group, circle, r, x1, y1, x2, y2, localStart, step).setStroke(stroke);
					shape = group;
				}else{
					stroke = theme.series.stroke;

					shape = this._createSlice(s, circle, r, x1, y1, x2, y2, localStart, step).setStroke(stroke);

					specialFill = theme.series.fill;
					if(specialFill && specialFill.type === "radial"){
						specialFill = this._shapeFill(specialFill, {x: circle.cx - circle.r, y: circle.cy - circle.r, width: 2 * circle.r, height: 2 * circle.r});
						if(this.opt.radGrad === "linear"){
							specialFill = this._pseudoRadialFill(specialFill, {x: circle.cx, y: circle.cy}, r, localStart, localStart + step);
						}
					}else if(specialFill && specialFill.type === "linear"){
						var bbox = lang.clone(shape.getBoundingBox());
						if(g.renderer === "svg"){
							// Try to fix the bounding box calculations for
							// height.  Only really works for SVG.
							var pos = {w: 0, h: 0};
							try{
								pos = domGeom.position(shape.rawNode);
							}catch(ignore){}
							if(pos.h > bbox.height){
								bbox.height = pos.h;
							}
							if(pos.w > bbox.width){
								bbox.width = pos.w;
							}
						}
						specialFill = this._plotFill(specialFill, dim, offsets);
						specialFill = this._shapeFill(specialFill, bbox);
					}
					shape.setFill(specialFill);
				}
				this.dyn.push({fill: specialFill, stroke: theme.series.stroke});

				if(events){
					o = {
						element: "slice",
						index:   i,
						run:     this.run,
						shape:   shape,
						x:       i,
						y:       typeof v == "number" ? v : v.y,
						cx:      circle.cx,
						cy:      circle.cy,
						cr:      r
					};
					this._connectEvents(o);
					eventSeries[i] = o;
				}

				localStart = localStart + step;

				return false;	// continue
			}, this);
			// draw labels
			if(this.opt.labels){
				var isRtl = has("dojo-bidi") && this.chart.isRightToLeft();
				if(this.opt.labelStyle == "default"){
					start = startAngle;
					localStart = start;
					arr.some(slices, function(slice, i){
						if(slice <= 0 && !this.opt.minWidth){
							// degenerated slice
							return false;	// continue
						}
						var theme = themes[i];
						if(slice >= 1){
							// whole pie
							this.renderLabel(s, circle.cx, circle.cy + size / 2, labels[i], theme, this.opt.labelOffset > 0);
							return true;	// stop iteration
						}
						// calculate the geometry of the slice
						var end = start + slice * 2 * Math.PI;
						if(i + 1 == slices.length){
							end = startAngle + 2 * Math.PI;
						}

						if(this.opt.omitLabels && end-start < 0.001){
							return false;	// continue
						}

						var labelAngle = localStart + (slicesSteps[i].step / 2),//(start + end) / 2,
							x = circle.cx + labelR * Math.cos(labelAngle),
							y = circle.cy + labelR * Math.sin(labelAngle) + size / 2;
						// draw the label
						this.renderLabel(s, isRtl ? dim.width - x : x, y, labels[i], theme, this.opt.labelOffset > 0);
						localStart += slicesSteps[i].step;
						start = end;
						return false;	// continue
					}, this);
				}else if(this.opt.labelStyle == "columns"){
					//calculate label angles
					var omitLabels = this.opt.omitLabels;
					start = startAngle;
					localStart = start;
					var labeledSlices = [],
						significantCount = 0, k;
					for(k = slices.length - 1; k >= 0; --k){
						if(slices[k]){
							++significantCount;
						}
					}
					arr.forEach(slices, function(slice, i){
						var end = start + slice * 2 * Math.PI;
						if(i + 1 == slices.length){
							end = startAngle + 2 * Math.PI;
						}
						if(this.minWidth !== 0 || end - start >= 0.001){
							// var labelAngle = (start + end) / 2;
							var labelAngle = localStart + (slicesSteps[i].step / 2);//(start + end) / 2,
							if(significantCount === 1 && !this.opt.minWidth){
								labelAngle = (start + end) / 2;
							}
							labeledSlices.push({
								angle: labelAngle,
								left:  Math.cos(labelAngle) < 0,
								theme: themes[i],
								index: i,
								omit: omitLabels? end - start < 0.001:false
							});
						}
						start = end;
						localStart += slicesSteps[i].step;
					}, this);

					//calculate label radius to each slice
					var labelHeight = g._base._getTextBox("a", {font:taFont, whiteSpace: "nowrap"}).h;
					this._getProperLabelRadius(labeledSlices, labelHeight, circle.r * 1.1);

					//draw label and wiring
					var leftColumn  = circle.cx - circle.r * 2,
						rightColumn = circle.cx + circle.r * 2;
					arr.forEach(labeledSlices, function(slice){
						if(slice.omit){
							return;
						}
						var cTheme = themes[slice.index], lrPadding = 0;
						if(cTheme && cTheme.axis && cTheme.axis.tick && cTheme.axis.tick.labelGap){
							// Try to pad the lable a bit, the same as a tick gap.
							lrPadding = cTheme.axis.tick.labelGap;
						}
						var labelWidth = g._base._getTextBox(labels[slice.index],
								{font: cTheme.series.font, whiteSpace: "nowrap", paddingLeft: lrPadding + "px"}).w,
							x = circle.cx + slice.labelR * Math.cos(slice.angle),
							y = circle.cy + slice.labelR * Math.sin(slice.angle),
							jointX = (slice.left) ? (leftColumn + labelWidth) : (rightColumn - labelWidth),
							labelX = (slice.left) ? leftColumn : jointX + lrPadding,
							newRadius = circle.r,
							wiring = s.createPath().moveTo(circle.cx + newRadius * Math.cos(slice.angle),
								circle.cy + newRadius * Math.sin(slice.angle));
						if(Math.abs(slice.labelR * Math.cos(slice.angle)) < circle.r * 2 - labelWidth){
							wiring.lineTo(x, y);
						}
						wiring.lineTo(jointX, y).setStroke(slice.theme.series.labelWiring);
						// Push the wiring to the back so that highlight/magnify actions don't bleed the wire.
						wiring.moveToBack();
						// Try to adjust the wiring position here.  The browser always adds a bit
						// of padding on height, so divide by 3 instead of 2.
						var mid = labelHeight/3 + y;
						var elem = this.renderLabel(s, labelX, mid || 0, labels[slice.index], cTheme, false, "left");

						if(events && !this.opt.htmlLabels){
							var fontWidth  = g._base._getTextBox(labels[slice.index], {font: slice.theme.series.font}).w || 0,
								fontHeight = g.normalizedLength(g.splitFontString(slice.theme.series.font).size);
							o = {
								element: "labels",
								index:   slice.index,
								run:     this.run,
								shape:   elem,
								x:       labelX,
								y:       y,
								label:   labels[slice.index]
							};

							var shp = elem.getShape(),
								lt = domGeom.position(this.chart.node, true),
								aroundRect = lang.mixin({ type : 'rect' }, {
									x: shp.x,
									y: shp.y - 2 * fontHeight
								});

							aroundRect.x += lt.x;
							aroundRect.y += lt.y;
							aroundRect.x = Math.round(aroundRect.x);
							aroundRect.y = Math.round(aroundRect.y);
							aroundRect.width  = Math.ceil(fontWidth);
							aroundRect.height = Math.ceil(fontHeight);

							o.aroundRect = aroundRect;

							this._connectEvents(o);
							eventSeries[slices.length + slice.index] = o;
						}
					}, this);
				}
			}
			// post-process events to restore the original indexing
			var esi = 0;
			this._eventSeries[this.run.name] = df.map(run, function(v){
				return v <= 0 ? null : eventSeries[esi++];
			});

			// chart mirroring starts
			if(has("dojo-bidi")){
				this._checkOrientation(this.group, dim, offsets);
			}

			return this;	//	dojox/charting/plot2d/Pie
		},

		_getProperLabelRadius: function(slices, labelHeight, minRadius){
			if(slices.length == 1){
				slices[0].labelR = minRadius;
				return;
			}
			var leftCenterSlice = {}, rightCenterSlice = {}, leftMinSIN = 2, rightMinSIN = 2, i;
			var tempSIN;
			for(i = 0; i < slices.length; ++i){
				tempSIN = Math.abs(Math.sin(slices[i].angle));
				if(slices[i].left){
					if(leftMinSIN > tempSIN){
						leftMinSIN = tempSIN;
						leftCenterSlice = slices[i];
					}
				}else{
					if(rightMinSIN > tempSIN){
						rightMinSIN = tempSIN;
						rightCenterSlice = slices[i];
					}
				}
			}
			leftCenterSlice.labelR = rightCenterSlice.labelR = minRadius;
			this._caculateLabelR(leftCenterSlice,  slices, labelHeight);
			this._caculateLabelR(rightCenterSlice, slices, labelHeight);
		},

		_caculateLabelR: function(firstSlice, slices, labelHeight){
			var i, j, k, length = slices.length, currentLabelR = firstSlice.labelR, nextLabelR,
				step = slices[firstSlice.index].left ? -labelHeight : labelHeight;
			for(k = 0, i = firstSlice.index, j = (i + 1) % length; k < length && slices[i].left === slices[j].left; ++k){
				nextLabelR = (Math.sin(slices[i].angle) * currentLabelR + step) / Math.sin(slices[j].angle);
				currentLabelR = Math.max(firstSlice.labelR, nextLabelR);
				slices[j].labelR = currentLabelR;
				i = (i + 1) % length;
				j = (j + 1) % length;
			}
			if(k >= length){
				slices[0].labelR = firstSlice.labelR;
			}
			for(k = 0, i = firstSlice.index, j = (i || length) - 1; k < length && slices[i].left === slices[j].left; ++k){
				nextLabelR = (Math.sin(slices[i].angle) * currentLabelR - step) / Math.sin(slices[j].angle);
				currentLabelR = Math.max(firstSlice.labelR, nextLabelR);
				slices[j].labelR = currentLabelR;
				i = (i || length) - 1;
				j = (j || length) - 1;
			}
		},

		_createRing: function(group, circle){
			var r = this.opt.innerRadius;
			if(r > 0){
				// Percentage, use circle.  Anything < 0 for innerRadius
				// is assumed to be a multiple of the radius.  So 0.25 innerRadius value
				// is computed to be 25% of the outer radius.
				r = circle.r * (r/100);
			}else if(r < 0){
				r = -r; // Assume it is pixels, fixed size hole.
			}
			if(r){
				return group.createPath({}).setAbsoluteMode(true).
					moveTo(circle.cx, circle.cy - circle.r).
					arcTo(circle.r, circle.r, 0, false, true, circle.cx + circle.r, circle.cy).
					arcTo(circle.r, circle.r, 0,  true, true, circle.cx, circle.cy - circle.r).
					closePath().
					moveTo(circle.cx, circle.cy - r).
					arcTo(r, r, 0, false, true, circle.cx + r, circle.cy).
					arcTo(r, r, 0,  true, true, circle.cx, circle.cy - r).
					closePath();
			}
			return group.createCircle(circle);
		},
		_createSlice: function(group, circle, R, x1, y1, x2, y2, fromAngle, stepAngle){
			var r = this.opt.innerRadius;
			if(r > 0){
				// Percentage, use circle.  Anything < 0 for innerRadius
				// is assumed to be a multiple of the radius.  So 0.25 innerRadius value
				// is computed to be 25% of the outer radius.
				r = circle.r * (r/100);
			}else if(r < 0){
				r = -r; // Assume it is pixels, fixed size hole.
			}
			if(r){
				var innerX1 = circle.cx + r * Math.cos(fromAngle),
					innerY1 = circle.cy + r * Math.sin(fromAngle),
					innerX2 = circle.cx + r * Math.cos(fromAngle + stepAngle),
					innerY2 = circle.cy + r * Math.sin(fromAngle + stepAngle);
				return group.createPath({}).setAbsoluteMode(true).
					moveTo(innerX1, innerY1).
					lineTo(x1, y1).
					arcTo(R, R, 0, stepAngle > Math.PI, true, x2, y2).
					lineTo(innerX2, innerY2).
					arcTo(r, r, 0, stepAngle > Math.PI, false, innerX1, innerY1).
					closePath();
			}
			return group.createPath({}).setAbsoluteMode(true).
				moveTo(circle.cx, circle.cy).
				lineTo(x1, y1).
				arcTo(R, R, 0, stepAngle > Math.PI, true, x2, y2).
				lineTo(circle.cx, circle.cy).
				closePath();
		}
	});
});
