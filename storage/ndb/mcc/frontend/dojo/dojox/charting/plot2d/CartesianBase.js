define(["dojo/_base/lang", "dojo/_base/declare", "dojo/_base/connect", "dojo/has",
		"./Base", "../scaler/primitive", "dojox/gfx", "dojox/gfx/fx", "dojox/lang/utils"], 
	function(lang, declare, hub, has, Base, primitive, gfx, fx, du){
	/*=====
	declare("dojox.charting.plot2d.__CartesianCtorArgs", dojox.charting.plot2d.__PlotCtorArgs, {
		// hAxis: String?
		//		The horizontal axis name.
		hAxis: "x",

		// vAxis: String?
		//		The vertical axis name
		vAxis: "y",

		// labels: Boolean?
		//		For plots that support labels, whether or not to draw labels for each data item.  Default is false.
		labels:			false,

		// fixed: Boolean?
        //		Whether a fixed precision must be applied to data values for display. Default is true.
		fixed:			true,

		// precision: Number?
        //		The precision at which to round data values for display. Default is 0.
		precision:		1,

		// labelOffset: Number?
		//		The amount in pixels by which to offset labels when using "outside" labelStyle.  Default is 10.
		labelOffset:	10,

		// labelStyle: String?
		//		Options as to where to draw labels.  This must be either "inside" or "outside". By default
		//      the labels are drawn "inside" the shape representing the data point (a column rectangle for a Columns plot
		//      or a marker for a Line plot for instance). When "outside" is used the labels are drawn above the data point shape.
		labelStyle:		"inside",

		// htmlLabels: Boolean?
		//		Whether or not to use HTML to render slice labels. Default is true.
		htmlLabels:		true,

		// omitLabels: Boolean?
		//		Whether labels that do not fit in an item render are omitted or not.	This applies only when labelStyle
		//		is "inside".	Default is false.
		omitLabels: true,

		// labelFunc: Function?
		//		An optional function to use to compute label text. It takes precedence over
		//		the default text when available.
		//	|		function labelFunc(value, fixed, precision) {}
		//		`value` is the data value to display
		//		`fixed` is true if fixed precision must be applied.
		//		`precision` is the requested precision to be applied.
		labelFunc: null
	});
	=====*/

	var alwaysFalse = function(){ return false; }

	return declare("dojox.charting.plot2d.CartesianBase", Base, {
		baseParams: {
			hAxis: 			"x",
			vAxis: 			"y",
			labels:			false,
			labelOffset:    10,
			fixed:			true,
			precision:		1,
			labelStyle:		"inside",
			htmlLabels:		true,		// use HTML to draw labels
			omitLabels:		true,
			labelFunc:		null
        },

		// summary:
		//		Base class for cartesian plot types.
		constructor: function(chart, kwArgs){
			// summary:
			//		Create a cartesian base plot for cartesian charts.
			// chart: dojox/chart/Chart
			//		The chart this plot belongs to.
			// kwArgs: dojox.charting.plot2d.__CartesianCtorArgs?
			//		An optional arguments object to help define the plot.
			this.axes = ["hAxis", "vAxis"];
			this.zoom = null;
			this.zoomQueue = [];	// zooming action task queue
			this.lastWindow = {vscale: 1, hscale: 1, xoffset: 0, yoffset: 0};
			this.hAxis = (kwArgs && kwArgs.hAxis) || "x";
			this.vAxis = (kwArgs && kwArgs.vAxis) || "y";
			this.series = [];
			this.opt = lang.clone(this.baseParams);
			du.updateWithObject(this.opt, kwArgs);
		},
		clear: function(){
			// summary:
			//		Clear out all of the information tied to this plot.
			// returns: dojox/charting/plot2d/CartesianBase
			//		A reference to this plot for functional chaining.
			this.inherited(arguments);
			this._hAxis = null;
			this._vAxis = null;
			return this;	//	dojox/charting/plot2d/CartesianBase
		},
		cleanGroup: function(creator, noClip){
			this.inherited(arguments);
			if(!noClip && this.chart._nativeClip){
				var offsets = this.chart.offsets, dim = this.chart.dim;
				var w = Math.max(0, dim.width  - offsets.l - offsets.r),
					h = Math.max(0, dim.height - offsets.t - offsets.b);
				this.group.setClip({ x: offsets.l, y: offsets.t, width: w, height: h });
				if(!this._clippedGroup){
					this._clippedGroup = this.group.createGroup();
				}
			}
		},
		purgeGroup: function(){
			this.inherited(arguments);
			this._clippedGroup = null;
		},
		getGroup: function(){
			return this._clippedGroup || this.group;
		},
		setAxis: function(axis){
			// summary:
			//		Set an axis for this plot.
			// axis: dojox/charting/axis2d/Base
			//		The axis to set.
			// returns: dojox/charting/plot2d/CartesianBase
			//		A reference to this plot for functional chaining.
			if(axis){
				this[axis.vertical ? "_vAxis" : "_hAxis"] = axis;
			}
			return this;	//	dojox/charting/plot2d/CartesianBase
		},
		toPage: function(coord){
			// summary:
			//		Compute page coordinates from plot axis data coordinates.
			// coord: Object?
			//		The coordinates in plot axis data coordinate space. For cartesian charts that is of the following form:
			//		`{ hAxisName: 50, vAxisName: 200 }`
			//		If not provided return the transform method instead of the result of the transformation.
			// returns: Object
			//		The resulting page pixel coordinates. That is of the following form:
			//		`{ x: 50, y: 200 }`
			var ah = this._hAxis, av = this._vAxis,
				sh = ah.getScaler(), sv = av.getScaler(),
				th = sh.scaler.getTransformerFromModel(sh),
				tv = sv.scaler.getTransformerFromModel(sv),
				c = this.chart.getCoords(),
				o = this.chart.offsets, dim = this.chart.dim;
			var t = function(coord){
				var r = {};
				r.x = th(coord[ah.name]) + c.x + o.l;
				r.y = c.y + dim.height - o.b - tv(coord[av.name]);
				return r;
			};
			// if no coord return the function so that we can capture the current transforms
			// and reuse them later on
			return coord?t(coord):t; // Object
		},
		toData: function(coord){
			// summary:
			//		Compute plot axis data coordinates from page coordinates.
			// coord: Object
			//		The pixel coordinate in page coordinate space. That is of the following form:
			//		`{ x: 50, y: 200 }`
			//		If not provided return the tranform method instead of the result of the transformation.
			// returns: Object
			//		The resulting plot axis data coordinates. For cartesian charts that is of the following form:
			//		`{ hAxisName: 50, vAxisName: 200 }`
			var ah = this._hAxis, av = this._vAxis,
				sh = ah.getScaler(), sv = av.getScaler(),
				th = sh.scaler.getTransformerFromPlot(sh),
				tv = sv.scaler.getTransformerFromPlot(sv),
				c = this.chart.getCoords(),
				o = this.chart.offsets, dim = this.chart.dim;
			var t = function(coord){
				var r = {};
				r[ah.name] = th(coord.x - c.x - o.l);
				r[av.name] = tv(c.y + dim.height - coord.y  - o.b);
				return r;
			};
			// if no coord return the function so that we can capture the current transforms
			// and reuse them later on
			return coord?t(coord):t; // Object
		},
		isDirty: function(){
			// summary:
			//		Returns whether or not this plot needs to be rendered.
			// returns: Boolean
			//		The state of the plot.
			return this.dirty || this._hAxis && this._hAxis.dirty || this._vAxis && this._vAxis.dirty;	//	Boolean
		},
		createLabel: function(group, value, bbox, theme){
			if(this.opt.labels){
				var x, y, label = this.opt.labelFunc?this.opt.labelFunc.apply(this, [value, this.opt.fixed, this.opt.precision]):
					this._getLabel(isNaN(value.y)?value:value.y);
				if(this.opt.labelStyle == "inside"){
					var lbox = gfx._base._getTextBox(label, { font: theme.series.font } );
					x = bbox.x + bbox.width / 2;
					y = bbox.y + bbox.height / 2 + lbox.h / 4;
					if(lbox.w > bbox.width || lbox.h > bbox.height){
						return;
					}

				}else{
					x = bbox.x + bbox.width / 2;
					y = bbox.y - this.opt.labelOffset;
				}
				this.renderLabel(group, x, y, label, theme, this.opt.labelStyle == "inside");
			}
		},
		performZoom: function(dim, offsets){
			// summary:
			//		Create/alter any zooming windows on this plot.
			// dim: Object
			//		An object of the form { width, height }.
			// offsets: Object
			//		An object of the form { l, r, t, b }.
			// returns: dojox/charting/plot2d/CartesianBase
			//		A reference to this plot for functional chaining.

			// get current zooming various
			var vs = this._vAxis.scale || 1,
				hs = this._hAxis.scale || 1,
				vOffset = dim.height - offsets.b,
				hBounds = this._hScaler.bounds,
				xOffset = (hBounds.from - hBounds.lower) * hBounds.scale,
				vBounds = this._vScaler.bounds,
				yOffset = (vBounds.from - vBounds.lower) * vBounds.scale,
				// get incremental zooming various
				rVScale = vs / this.lastWindow.vscale,
				rHScale = hs / this.lastWindow.hscale,
				rXOffset = (this.lastWindow.xoffset - xOffset)/
					((this.lastWindow.hscale == 1)? hs : this.lastWindow.hscale),
				rYOffset = (yOffset - this.lastWindow.yoffset)/
					((this.lastWindow.vscale == 1)? vs : this.lastWindow.vscale),

				shape = this.getGroup(),
				anim = fx.animateTransform(lang.delegate({
					shape: shape,
					duration: 1200,
					transform:[
						{name:"translate", start:[0, 0], end: [offsets.l * (1 - rHScale), vOffset * (1 - rVScale)]},
						{name:"scale", start:[1, 1], end: [rHScale, rVScale]},
						{name:"original"},
						{name:"translate", start: [0, 0], end: [rXOffset, rYOffset]}
					]}, this.zoom));

			lang.mixin(this.lastWindow, {vscale: vs, hscale: hs, xoffset: xOffset, yoffset: yOffset});
			//add anim to zooming action queue,
			//in order to avoid several zooming action happened at the same time
			this.zoomQueue.push(anim);
			//perform each anim one by one in zoomQueue
			hub.connect(anim, "onEnd", this, function(){
				this.zoom = null;
				this.zoomQueue.shift();
				if(this.zoomQueue.length > 0){
					this.zoomQueue[0].play();
				}
			});
			if(this.zoomQueue.length == 1){
				this.zoomQueue[0].play();
			}
			return this;	//	dojox/charting/plot2d/CartesianBase
		},
		initializeScalers: function(dim, stats){
			// summary:
			//		Initializes scalers using attached axes.
			// dim: Object
			//		Size of a plot area in pixels as {width, height}.
			// stats: Object
			//		Min/max of data in both directions as {hmin, hmax, vmin, vmax}.
			// returns: dojox/charting/plot2d/CartesianBase
			//		A reference to this plot for functional chaining.
			if(this._hAxis){
				if(!this._hAxis.initialized()){
					this._hAxis.calculate(stats.hmin, stats.hmax, dim.width);
				}
				this._hScaler = this._hAxis.getScaler();
			}else{
				this._hScaler = primitive.buildScaler(stats.hmin, stats.hmax, dim.width);
			}
			if(this._vAxis){
				if(!this._vAxis.initialized()){
					this._vAxis.calculate(stats.vmin, stats.vmax, dim.height);
				}
				this._vScaler = this._vAxis.getScaler();
			}else{
				this._vScaler = primitive.buildScaler(stats.vmin, stats.vmax, dim.height);
			}
			return this;	//	dojox/charting/plot2d/CartesianBase
		},
		isNullValue: function(value){
			if(value === null || typeof value == "undefined"){
				return true;
			}
			var h = this._hAxis ? this._hAxis.isNullValue : alwaysFalse,
				v = this._vAxis ? this._vAxis.isNullValue : alwaysFalse;
			if(typeof value == "number"){
				return h(1) || v(value);
			}
			return h(isNaN(value.x) ? 1 : value.x) || value.y === null || v(value.y);
		}		
	});
});
