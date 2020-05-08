define(["dojo/_base/lang", "dojo/_base/array", "dojo/_base/declare", "dojo/has", "./CartesianBase", "./_PlotEvents", "./common",
	"dojox/gfx/fx", "dojox/lang/utils", "dojox/lang/functional"],
	function(lang, arr, declare, has, CartesianBase, _PlotEvents, dc, fx, du, df){

	/*=====
	declare("dojox.charting.plot2d.__BarCtorArgs", dojox.charting.plot2d.__DefaultCtorArgs, {
		// summary:
		//		Additional keyword arguments for bar charts.

		// minBarSize: Number?
		//		The minimum size for a bar in pixels.  Default is 1.
		minBarSize: 1,

		// maxBarSize: Number?
		//		The maximum size for a bar in pixels.  Default is 1.
		maxBarSize: 1,

		// stroke: dojox.gfx.Stroke?
		//		An optional stroke to use for any series on the plot.
		stroke:		{},

		// outline: dojox.gfx.Stroke?
		//		An optional stroke used to outline any series on the plot.
		outline:	{},

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

		// font: String?
		//		A font definition to be used for labels and other text-based elements on the plot.
		font:		"",

		// fontColor: String|dojo.Color?
		//		The color to be used for any text-based elements on the plot.
		fontColor:	"",

		// enableCache: Boolean?
		//		Whether the bars rect are cached from one rendering to another. This improves the rendering performance of
		//		successive rendering but penalize the first rendering.  Default false.
		enableCache: false
	});
	=====*/

	var alwaysFalse = function(){ return false; }

	return declare("dojox.charting.plot2d.Bars", [CartesianBase, _PlotEvents], {
		// summary:
		//		The plot object representing a bar chart (horizontal bars).
		defaultParams: {
			gap:	0,		// gap between columns in pixels
			animate: null,   // animate bars into place
			enableCache: false
		},
		optionalParams: {
			minBarSize:	1,	// minimal bar width in pixels
			maxBarSize:	1,	// maximal bar width in pixels
			// theme component
			stroke:		{},
			outline:	{},
			shadow:		{},
			fill:		{},
			filter:	    {},
			styleFunc:  null,
			font:		"",
			fontColor:	""
		},

		constructor: function(chart, kwArgs){
			// summary:
			//		The constructor for a bar chart.
			// chart: dojox/charting/Chart
			//		The chart this plot belongs to.
			// kwArgs: dojox.charting.plot2d.__BarCtorArgs?
			//		An optional keyword arguments object to help define the plot.
			this.opt = lang.clone(lang.mixin(this.opt, this.defaultParams));
			du.updateWithObject(this.opt, kwArgs);
			du.updateWithPattern(this.opt, kwArgs, this.optionalParams);
			this.animate = this.opt.animate;
			this.renderingOptions = { "shape-rendering": "crispEdges" };
		},

		getSeriesStats: function(){
			// summary:
			//		Calculate the min/max on all attached series in both directions.
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			var stats = dc.collectSimpleStats(this.series, lang.hitch(this, "isNullValue")), t;
			stats.hmin -= 0.5;
			stats.hmax += 0.5;
			t = stats.hmin, stats.hmin = stats.vmin, stats.vmin = t;
			t = stats.hmax, stats.hmax = stats.vmax, stats.vmax = t;
			return stats; // Object
		},

		createRect: function(run, creator, params){
			var rect;
			if(this.opt.enableCache && run._rectFreePool.length > 0){
				rect = run._rectFreePool.pop();
				rect.setShape(params);
				// was cleared, add it back
				creator.add(rect);
			}else{
				rect = creator.createRect(params);
			}
			if(this.opt.enableCache){
				run._rectUsePool.push(rect);
			}
			return rect;
		},

		createLabel: function(group, value, bbox, theme){
			if(this.opt.labels && this.opt.labelStyle == "outside"){
				var y = bbox.y + bbox.height / 2;
				var x = bbox.x + bbox.width + this.opt.labelOffset;
				this.renderLabel(group, x, y, this._getLabel(isNaN(value.y)?value:value.y), theme, "start");
          	}else{
				this.inherited(arguments);
			}
		},

		render: function(dim, offsets){
			// summary:
			//		Run the calculations for any axes for this plot.
			// dim: Object
			//		An object in the form of { width, height }
			// offsets: Object
			//		An object of the form { l, r, t, b}.
			// returns: dojox/charting/plot2d/Bars
			//		A reference to this plot for functional chaining.
			if(this.zoom && !this.isDataDirty()){
				return this.performZoom(dim, offsets); // dojox/charting/plot2d/Bars
			}
			this.dirty = this.isDirty();
			this.resetEvents();
			var s;
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
				baseline = Math.max(this._hScaler.bounds.lower,
					this._hAxis ? this._hAxis.naturalBaseline : 0),
				baselineWidth = ht(baseline),
				events = this.events();
			var bar = this.getBarProperties();

			var actualLength = this.series.length;
			arr.forEach(this.series, function(serie){if(serie.hidden){actualLength--;}});
			var z = actualLength;

			// Collect and calculate all values
			var extractedValues = this.extractValues(this._vScaler);
			extractedValues = this.rearrangeValues(extractedValues, ht, baselineWidth);

			for(var i = 0; i < this.series.length; i++){
				var run = this.series[i];
				if(!this.dirty && !run.dirty){
					t.skip();
					this._reconnectEvents(run.name);
					continue;
				}
				run.cleanGroup();
				if(this.opt.enableCache){
					run._rectFreePool = (run._rectFreePool?run._rectFreePool:[]).concat(run._rectUsePool?run._rectUsePool:[]);
					run._rectUsePool = [];
				}
				var theme = t.next("bar", [this.opt, run]);
				if(run.hidden){
					run.dyn.fill = theme.series.fill;
					run.dyn.stroke = theme.series.stroke;
					continue;
				}
				z--;

				var	eventSeries = new Array(run.data.length);
				s = run.group;
				var indexed = arr.some(run.data, function(item){
					return typeof item == "number" || (item && !item.hasOwnProperty("x"));
				});
				// on indexed charts we can easily just interate from the first visible to the last visible
				// data point to save time
				var min = indexed?Math.max(0, Math.floor(this._vScaler.bounds.from - 1)):0;
				var max = indexed?Math.min(run.data.length, Math.ceil(this._vScaler.bounds.to)):run.data.length;
				for(var j = min; j < max; ++j){
					var value = run.data[j];
					if(!this.isNullValue(value)){
						var val = this.getValue(value, j, i, indexed),
							w = extractedValues[i][j], finalTheme, sshape;
						if(this.opt.styleFunc || typeof value != "number"){
							var tMixin = typeof value != "number" ? [value] : [];
							if(this.opt.styleFunc){
								tMixin.push(this.opt.styleFunc(value));
							}
							finalTheme = t.addMixin(theme, "bar", tMixin, true);
						}else{
							finalTheme = t.post(theme, "bar");
						}
						if(w && bar.height >= 1){
							var rect = {
								x: offsets.l + baselineWidth + Math.min(w, 0),
								y: dim.height - offsets.b - vt(val.x + 1.5) + bar.gap + bar.thickness * (actualLength - z - 1),
								width: Math.abs(w),
								height: bar.height
							};
							if(finalTheme.series.shadow){
								var srect = lang.clone(rect);
								srect.x += finalTheme.series.shadow.dx;
								srect.y += finalTheme.series.shadow.dy;
								sshape = this.createRect(run, s, srect).setFill(finalTheme.series.shadow.color).setStroke(finalTheme.series.shadow);
								if(this.animate){
									this._animateBar(sshape, offsets.l + baselineWidth, -w);
								}
							}

							var specialFill = this._plotFill(finalTheme.series.fill, dim, offsets);
							specialFill = this._shapeFill(specialFill, rect);
							var shape = this.createRect(run, s, rect).setFill(specialFill).setStroke(finalTheme.series.stroke);
							if(shape.setFilter && finalTheme.series.filter){
								shape.setFilter(finalTheme.series.filter);
							}
							run.dyn.fill   = shape.getFill();
							run.dyn.stroke = shape.getStroke();
							if(events){
								var o = {
									element: "bar",
									index:   j,
									run:     run,
									shape:   shape,
									shadow:	 sshape,
									cx:      val.y,
									cy:      val.x + 1.5,
									x:	     indexed?j:run.data[j].x,
									y:	 	 indexed?run.data[j]:run.data[j].y
								};
								this._connectEvents(o);
								eventSeries[j] = o;
							}
							if(!isNaN(val.py) && val.py > baseline){
								rect.x += ht(val.py);
								rect.width -= ht(val.py);
							}
							this.createLabel(s, value, rect, finalTheme);
							if(this.animate){
								this._animateBar(shape, offsets.l + baselineWidth, -Math.abs(w));
							}
						}
					}
				}
				this._eventSeries[run.name] = eventSeries;
				run.dirty = false;
			}
			this.dirty = false;
			// chart mirroring starts
			if(has("dojo-bidi")){
				this._checkOrientation(this.group, dim, offsets);
			}
			// chart mirroring ends
			return this;	//	dojox/charting/plot2d/Bars
		},
		getValue: function(value, j, seriesIndex, indexed){
			var y, x;
			if(indexed){
				if(typeof value == "number"){
					y = value;
				}else{
					y = value.y;
				}
				x = j;
			}else{
				y = value.y;
				x = value.x -1;
			}
			return {y:y, x:x};
		},
		extractValues: function(scaler){
			var extracted = [];
			for(var i = this.series.length - 1; i >= 0; --i){
				var run = this.series[i];
				if(!this.dirty && !run.dirty){
					continue;
				}
				// on indexed charts we can easily just interate from the first visible to the last visible
				// data point to save time
				var indexed = arr.some(run.data, function(item){
						return typeof item == "number" || (item && !item.hasOwnProperty("x"));
					}),
					min = indexed ? Math.max(0, Math.floor(scaler.bounds.from - 1)) : 0,
					max = indexed ? Math.min(run.data.length, Math.ceil(scaler.bounds.to)) : run.data.length,
					extractedSet = extracted[i] = [];
				extractedSet.min = min;
				extractedSet.max = max;
				for(var j = min; j < max; ++j){
					var value = run.data[j];
					extractedSet[j] = this.isNullValue(value) ? 0 :
						(typeof value == "number" ? value : value.y);
				}
			}
			return extracted;
		},
		rearrangeValues: function(values, transform, baseline){
			// transform to pixels
			for(var i = 0, n = values.length; i < n; ++i){
				var extractedSet = values[i];
				if(extractedSet){
					for(var j = extractedSet.min, k = extractedSet.max; j < k; ++j){
						var value = extractedSet[j];
						extractedSet[j] = this.isNullValue(value) ? 0 : transform(value) - baseline;
					}
				}
			}
			return values;
		},
		isNullValue: function(value){
			if(value === null || typeof value == "undefined"){
				return true;
			}
			var h = this._hAxis ? this._hAxis.isNullValue : alwaysFalse,
				v = this._vAxis ? this._vAxis.isNullValue : alwaysFalse;
			if(typeof value == "number"){
				return v(0.5) || h(value);
			}
			return v(isNaN(value.x) ? 0.5 : value.x + 0.5) || value.y === null || h(value.y);
		},
		getBarProperties: function(){
			var f = dc.calculateBarSize(this._vScaler.bounds.scale, this.opt);
			return {gap: f.gap, height: f.size, thickness: 0};
		},
		_animateBar: function(shape, hoffset, hsize){
			if(hsize==0){
				hsize = 1;
			}
			fx.animateTransform(lang.delegate({
				shape: shape,
				duration: 1200,
				transform: [
					{name: "translate", start: [hoffset - (hoffset/hsize), 0], end: [0, 0]},
					{name: "scale", start: [1/hsize, 1], end: [1, 1]},
					{name: "original"}
				]
			}, this.animate)).play();
		}
	});
});
