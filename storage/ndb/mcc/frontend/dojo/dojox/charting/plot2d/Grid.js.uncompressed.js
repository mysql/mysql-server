define("dojox/charting/plot2d/Grid", ["dojo/_base/lang", "dojo/_base/declare", "dojo/_base/array", "dojo/sniff",
		"./CartesianBase", "./common", "dojox/lang/utils", "dojox/gfx/fx"],
	function(lang, declare, arr, has, CartesianBase, dc, du, fx){

	var sortTicks = function(a,b){return a.value - b.value};

	/*=====
	declare("dojox.charting.plot2d.__GridCtorArgs", dojox.charting.plot2d.__CartesianCtorArgs, {
		// summary:
		//		A special keyword arguments object that is specific to a grid "plot".

		// majorHLine: dojox.gfx.Stroke?
		//		An optional dojox.gfx.Stroke for a major horizontal line. By default major lines use major tick stroke.
		majorHLine:undefined,

		// minorHLine: dojox.gfx.Stroke?
		//		An optional dojox.gfx.Stroke for a minor horizontal line. By default minor lines use minor tick stroke.
		minorHLine:undefined,

		// majorVLine: dojox.gfx.Stroke?
		//		An optional dojox.gfx.Stroke for a major vertical line. By default major lines use major tick stroke.
		majorVLine:undefined,

		// minorVLine: dojox.gfx.Stroke?
		//		An optional dojox.gfx.Stroke for a minor vertical line. By default major lines use major tick stroke.
		minorVLine:undefined,

		// hFill: dojox.gfx.Fill?
		//		An optional dojox.gfx.Fill used to fill every other horizontal stripe created by grid lines.
		hFill: undefined,

		// hAlternateFill: dojox.gfx.Fill?
		//		An optional dojox.gfx.Fill used to fill alternating horizontal stripe created by grid lines not filled by `hFill`.
		hAlternateFill: undefined,

		// vFill: dojox.gfx.Fill?
		//		An optional dojox.gfx.Fill used to fill every other vertical stripe created by grid lines.
		vFill: undefined,

		// vAlternateFill: dojox.gfx.Fill?
		//		An optional dojox.gfx.Fill used to fill alternating vertical stripe created by grid lines not filled by `vFill`.
		vAlternateFill: undefined,

		// hMajorLines: Boolean?
		//		Whether to show lines at the major ticks along the horizontal axis. Default is true.
		hMajorLines: true,

		// hMinorLines: Boolean?
		//		Whether to show lines at the minor ticks along the horizontal axis. Default is false.
		hMinorLines: false,

		// vMajorLines: Boolean?
		//		Whether to show lines at the major ticks along the vertical axis. Default is true.
		vMajorLines: true,

		// vMinorLines: Boolean?
		//		Whether to show lines at the major ticks along the vertical axis. Default is false.
		vMinorLines: false,

		// hStripes: Boolean?
		//		Whether to show horizontal stripes. Default is false.
		hStripes: false,

		// vStripes: Boolean?
		//		Whether to show vertical stripes. Default is false.
		vStripes: false,

		// enableCache: Boolean?
		//		Whether the grid lines are cached from one rendering to another. This improves the rendering performance of
		//		successive rendering but penalize the first rendering.  Default false.
		enableCache: false,

		// renderOnAxis: Boolean?
		//		Whether or not the grid is rendered when drawn at horizontal or vertical axis position. Default is true.
		renderOnAxis: true
	});
	=====*/

	return declare("dojox.charting.plot2d.Grid", CartesianBase, {
		// summary:
		//		A "faux" plot that can be placed behind other plots to represent
		//		a grid against which other plots can be easily measured.
		defaultParams: {
			hMajorLines: true,	// draw horizontal major lines
			hMinorLines: false,	// draw horizontal minor lines
			vMajorLines: true,	// draw vertical major lines
			vMinorLines: false,	// draw vertical minor lines
			hStripes: false,	// draw vertical stripes
			vStripes: false,	// draw vertical stripes
			animate: null,   // animate bars into place
			enableCache: false,
			renderOnAxis: true
		},

		optionalParams: {
			majorHLine: {},
			minorHLine: {},
			majorVLine: {},
			minorVLine: {},
			hFill: {},
			vFill: {},
			hAlternateFill: {},
			vAlternateFill: {}
		},

		constructor: function(chart, kwArgs){
			// summary:
			//		Create the faux Grid plot.
			// chart: dojox/charting/Chart
			//		The chart this plot belongs to.
			// kwArgs: dojox.charting.plot2d.__GridCtorArgs?
			//		An optional keyword arguments object to help define the parameters of the underlying grid.
			this.opt = lang.clone(this.defaultParams);
			du.updateWithObject(this.opt, kwArgs);
			du.updateWithPattern(this.opt, kwArgs, this.optionalParams);
			this.animate = this.opt.animate;
			if(this.opt.enableCache){
				this._lineFreePool = [];
				this._lineUsePool = [];
				this._rectFreePool = [];
				this._rectUsePool = [];
			}
		},
		addSeries: function(run){
			// summary:
			//		Ignored but included as a dummy method.
			// returns: dojox/charting/plot2d/Grid
			//		The reference to this plot for functional chaining.
			return this;	//	dojox/charting/plot2d/Grid
		},
		getSeriesStats: function(){
			// summary:
			//		Returns default stats (irrelevant for this type of plot).
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			return lang.delegate(dc.defaultStats); // Object
		},
		cleanGroup: function(){
			this.inherited(arguments);
			if(this.opt.enableCache){
				this._lineFreePool = this._lineFreePool.concat(this._lineUsePool);
				this._lineUsePool = [];
				this._rectFreePool = this._rectFreePool.concat(this._rectUsePool);
				this._rectUsePool = [];
			}
		},
		createLine: function(creator, params){
			var line;
			if(this.opt.enableCache && this._lineFreePool.length > 0){
				line = this._lineFreePool.pop();
				line.setShape(params);
				// was cleared, add it back
				creator.add(line);
			}else{
				line = creator.createLine(params);
			}
			if(this.opt.enableCache){
				this._lineUsePool.push(line);
			}
			return line;
		},
		createRect: function(creator, params){
			var rect;
			if(this.opt.enableCache && this._rectFreePool.length > 0){
				rect = this._rectFreePool.pop();
				rect.setShape(params);
				// was cleared, add it back
				creator.add(rect);
			}else{
				rect = creator.createRect(params);
			}
			if(this.opt.enableCache){
				this._rectUsePool.push(rect);
			}
			return rect;
		},
		
		render: function(dim, offsets){
			// summary:
			//		Render the plot on the chart.
			// dim: Object
			//		An object of the form { width, height }.
			// offsets: Object
			//		An object of the form { l, r, t, b }.
			// returns: dojox/charting/plot2d/Grid
			//		A reference to this plot for functional chaining.
			if(this.zoom){
				return this.performZoom(dim, offsets);
			}
			this.dirty = this.isDirty();
			if(!this.dirty){ return this; }
			this.cleanGroup();
			var s = this.getGroup(), ta = this.chart.theme, lineStroke, ticks;
			if((has("ios") && has("ios") < 6) || has("android") || (has("safari") && !has("ios"))){
				// clipping seems buggy in some mobile Webkit browser and Safari desktop
				// it does not clip correctly if only lines are present => create a invisible rectangle...
				var w = Math.max(0, dim.width  - offsets.l - offsets.r),
					h = Math.max(0, dim.height - offsets.t - offsets.b);
				s.createRect({ x: offsets.l, y: offsets.t, width: w, height: h});
			}
			if(this._vAxis){
				// draw horizontal stripes and lines
				ticks = this._vAxis.getTicks();
				var vScaler = this._vAxis.getScaler();
				if(ticks != null && vScaler != null){
					var vt = vScaler.scaler.getTransformerFromModel(vScaler);
					if(this.opt.hStripes){
						this._renderHRect(ticks, ta.grid, dim, offsets, vScaler, vt);
					}
					if(this.opt.hMinorLines){
						lineStroke = this.opt.minorHLine || (ta.grid && ta.grid.minorLine) || ta.axis.minorTick;
						this._renderHLines(ticks.minor, lineStroke, dim, offsets, vScaler, vt);
					}
					if(this.opt.hMajorLines){
						lineStroke = this.opt.majorHLine || (ta.grid && ta.grid.majorLine) || ta.axis.majorTick;
						this._renderHLines(ticks.major, lineStroke, dim, offsets, vScaler, vt);
					}
				}
				
			}
			if(this._hAxis){
				// draw vertical stripes and lines
				ticks = this._hAxis.getTicks();
				var hScaler = this._hAxis.getScaler();
				if(ticks != null && hScaler != null){
					var ht = hScaler.scaler.getTransformerFromModel(hScaler);
					if(this.opt.vStripes){
						this._renderVRect(ticks, ta.grid, dim, offsets, hScaler, ht);
					}
					if(ticks && this.opt.vMinorLines){
						lineStroke = this.opt.minorVLine || (ta.grid && ta.grid.minorLine) || ta.axis.minorTick;
						this._renderVLines(ticks.minor, lineStroke, dim, offsets, hScaler, ht);
					}
					if(ticks && this.opt.vMajorLines){
						lineStroke = this.opt.majorVLine || (ta.grid && ta.grid.majorLine) || ta.axis.majorTick;
						this._renderVLines(ticks.major, lineStroke, dim, offsets, hScaler, ht);
					}

				}
			}
			this.dirty = false;
			return this;	//	dojox/charting/plot2d/Grid
		},
		_renderHLines: function(ticks, lineStroke, dim, offsets, vScaler, vt){
			var s = this.getGroup();
			arr.forEach(ticks, function(tick){
				if(!this.opt.renderOnAxis && tick.value == (this._vAxis.opt.leftBottom?vScaler.bounds.from:vScaler.bounds.to)){
					return;
				}
				var y = dim.height - offsets.b - vt(tick.value);
				var hLine = this.createLine(s, {
					x1: offsets.l,
					y1: y,
					x2: dim.width - offsets.r,
					y2: y
				}).setStroke(lineStroke);
				if(this.animate){
					this._animateGrid(hLine, "h", offsets.l, offsets.r + offsets.l - dim.width);
				}
			}, this);
		},
		_renderVLines: function(ticks, lineStroke, dim, offsets, hScaler, ht){
			var s = this.getGroup();
			arr.forEach(ticks, function(tick){
				if(!this.opt.renderOnAxis && tick.value == (this._hAxis.opt.leftBottom?hScaler.bounds.from:hScaler.bounds.to)){
					return;
				}
				var x = offsets.l + ht(tick.value);
				var vLine = this.createLine(s, {
					x1: x,
					y1: offsets.t,
					x2: x,
					y2: dim.height - offsets.b
				}).setStroke(lineStroke);
				if(this.animate){
					this._animateGrid(vLine, "v", dim.height - offsets.b, dim.height - offsets.b - offsets.t);
				}
			}, this);
		},
		_renderHRect: function(ticks, theme, dim, offsets, vScaler, vt){
			var fill, tick, y, y2, hStripe;
			var allTicks = ticks.major.concat(ticks.minor);
			allTicks.sort(sortTicks);
			if(allTicks[0].value > vScaler.bounds.from){
				allTicks.splice(0, 0, {value: vScaler.bounds.from});
			}
			if(allTicks[allTicks.length - 1].value < vScaler.bounds.to){
				allTicks.push({value: vScaler.bounds.to});
			}
			var s = this.getGroup();
			for(var j = 0; j < allTicks.length - 1; j++){
				tick = allTicks[j];
				y = dim.height - offsets.b - vt(tick.value);
				y2 = dim.height - offsets.b - vt(allTicks[j+1].value);

				fill = (j%2 == 0)?(this.opt.hAlternateFill ||(theme && theme.alternateFill)):
					(this.opt.hFill || (theme && theme.fill));
				if(fill){
					hStripe = this.createRect(s, {
						x: offsets.l,
						y: y,
						width: dim.width - offsets.r,
						height: y - y2
					}).setFill(fill);
					if(this.animate){
						this._animateGrid(hStripe, "h", offsets.l, offsets.r + offsets.l - dim.width);
					}
				}
			}
		},
		_renderVRect: function(ticks, theme, dim, offsets, hScaler, ht){
			var fill, tick, x, x2, vStripe;
			var allTicks = ticks.major.concat(ticks.minor);
			allTicks.sort(sortTicks);
			if(allTicks[0].value > hScaler.bounds.from){
				allTicks.splice(0, 0, {value: hScaler.bounds.from});
			}
			if(allTicks[allTicks.length - 1].value < hScaler.bounds.to){
				allTicks.push({value: hScaler.bounds.to});
			}
			var s = this.getGroup();
			for(var j = 0; j < allTicks.length - 1; j++){
				tick = allTicks[j];
				x = offsets.l + ht(tick.value);
				x2 = offsets.l + ht(allTicks[j+1].value);

				fill = (j%2 == 0)?(this.opt.vAlternateFill ||(theme && theme.alternateFill)):
					(this.opt.vFill || (theme && theme.fill));
				if(fill){
					vStripe = this.createRect(s, {
						x: x,
						y: offsets.t,
						width: x2 - x,
						height: dim.width - offsets.r
					}).setFill(fill);
					if(this.animate){
						this._animateGrid(vStripe, "v", dim.height - offsets.b, dim.height - offsets.b - offsets.t);
					}
				}
			}
		},
		_animateGrid: function(shape, type, offset, size){
			var transStart = type == "h" ? [offset, 0] : [0, offset];
			var scaleStart = type == "h" ? [1/size, 1] : [1, 1/size];
			fx.animateTransform(lang.delegate({
				shape: shape,
				duration: 1200,
				transform: [
					{name: "translate", start: transStart, end: [0, 0]},
					{name: "scale", start: scaleStart, end: [1, 1]},
					{name: "original"}
				]
			}, this.animate)).play();
		}
	});
});
