define(["dojo/_base/lang", "dojo/_base/array", "dojo/_base/declare", "./CartesianBase", "./_PlotEvents", "./common",
    "../axis2d/common", "dojox/gfx", "dojox/lang/utils", "dojox/gfx/fx", "dojo/has"],
	function(lang, array, declare, CartesianBase, _PlotEvents, dcpc, dcac, gfx, du, fx, has){

	// all the code below should be removed when http://trac.dojotoolkit.org/ticket/11299 will be available
	var getBoundingBox = function(shape){
		return getTextBBox(shape, shape.getShape().text);
	};
	var getTextBBox = function(s, t){
		var c = s.declaredClass;
		var w, h;
		if(c.indexOf("svg")!=-1){
			// try/catch the FF native getBBox error. cheaper than walking up in the DOM
			// hierarchy to check the conditions (bench show /10 )
			try {
				return lang.mixin({}, s.rawNode.getBBox());
			}catch (e){
				return null;
			}
		}else if(c.indexOf("vml")!=-1){
			var rawNode = s.rawNode, _display = rawNode.style.display;
			rawNode.style.display = "inline";
			w = gfx.pt2px(parseFloat(rawNode.currentStyle.width));
			h = gfx.pt2px(parseFloat(rawNode.currentStyle.height));
			var sz = {x: 0, y: 0, width: w, height: h};
			// in VML, the width/height we get are in view coordinates
			// in our case we don't zoom the view so that is ok
			// It's impossible to get the x/y from the currentStyle.left/top,
			// because all negative coordinates are 'clipped' to 0.
			// (x:0 + translate(-100) -> x=0
			computeLocation(s, sz);
			rawNode.style.display = _display;
			return sz;
		}else if(c.indexOf("silverlight")!=-1){
			var bb = {width: s.rawNode.actualWidth, height: s.rawNode.actualHeight};
			return computeLocation(s, bb, 0.75);
		}else if(s.getTextWidth){
			// canvas
			w = s.getTextWidth();
			var font = s.getFont();
			var fz = font ? font.size : gfx.defaultFont.size;
			h = gfx.normalizedLength(fz);
			sz = {width: w, height: h};
			computeLocation(s, sz, 0.75);
			return sz;
		}
		return null;
	};
	var computeLocation =  function(s, sz, coef){
		var width = sz.width, height = sz.height, sh = s.getShape(), align = sh.align;
		switch (align) {
		case "end":
			sz.x = sh.x - width;
			break;
		case "middle":
			sz.x = sh.x - width / 2;
			break;
		case "start":
		default:
			sz.x = sh.x;
		break;
		}
		coef = coef || 1;
		sz.y = sh.y - height*coef; // rough approximation of the ascent!...
		return sz;
	};

	/*=====
	declare("dojox.charting.plot2d.__IndicatorCtorArgs", dojox.charting.plot2d.__CartesianCtorArgs, {
		// summary:
		//		A special keyword arguments object that is specific to a indicator "plot".

		// lines: Boolean?
		//		Whether the indicator lines are visible or not. The lines are displayed for each of the
		//		'values' of the indicator. Default is true.
		lines: true,

		// labels: String?
		//		Describes how the indicator labels are displayed.
		//		Possible values are:
		//		`"line"` for displaying the value of each indicator line,
		//		`"marker"` for displaying the values of the markers of each indicator line,
		//		`"trend"` for displaying the percentage variation from the first to the last indicator line,
		//		`"none"` to prevent any label to display.
		//		Default is "line".
		labels: "line",

		// markers: Boolean?
		//		Whether the markers on the indicator lines are visible or not. The markers are displayed for each
		//		of the indicator lines using the series attached to the indicator plot. Default is true.
		markers: true,

		// values: Boolean
		//		The data values at which each of the indicator line is drawn. For a single value a Number can be provided
		//		otherwise an Array of Number is required. fault is [].
		values: [],

		// offset: {x, y}?
		//		A pair of (x, y) pixel coordinate to specifiy the offset between the end of the indicator line and the
		//		position at which the labels are rendered. Default is no offset.
		offset: {},

		// start: Boolean?
		//		Whether the label is rendered at the start or end of the indicator line. Default is false meaning end of
		//		the line.
		start: false,

		// animate: Boolean?|Number?
		//		Whether or not to animate the chart to place. When a Number it specifies the duration of the animation.
		//		Default is false.
		animate: false,

		// vertical: Boolean?
		//		Whether the indicator is vertical or not. Default is true.
		vertical: true,

		// fixed: Boolean?
		//		Whether a fixed precision must be applied to data values for display. Default is true.
		fixed: true,

		// precision: Number?
		//		The precision at which to round data values for display. Default is 0.
		precision: 0,

		// lineStroke: dojo/gfx/Stroke?
		//		An optional stroke to use for indicator line.
		lineStroke: {},

		// lineOutline: dojo/gfx/Stroke?
		//		An optional outline to use for indicator line.
		lineOutline: {},

		// lineShadow: dojo/gfx/Stroke?
		//		An optional shadow to use for indicator line.
		lineShadow: {},

		// stroke: dojo.gfx.Stroke?
		//		An optional stroke to use for indicator label background.
		stroke: {},

		// outline: dojo.gfx.Stroke?
		//		An optional outline to use for indicator label background.
		outline: {},

		// shadow: dojo.gfx.Stroke?
		//		An optional shadow to use for indicator label background.
		shadow: {},

		// fill: dojo.gfx.Fill?
		//		An optional fill to use for indicator label background.
		fill: {},

		// fillFunc: Function?
		//		An optional function to use to compute label background fill. It takes precedence over
		//		fill property when available.
		//	|		function fillFunc(index, values, data) {}
		//		`index` is the index in the values array of the label being drawn.
		//		`values` is the entire array of values.
		//		`data` is the entire array of marker values.
		fillFunc: null,

		// labelFunc: Function?
		//		An optional function to use to compute label text. It takes precedence over
		//		the default text when available.
		//	|		function labelFunc(index, values, data, fixed, precision, labels) {}
		//		`index` is the index in the values array of the label being drawn.
		//		`values` is the entire array of values.
		//		`data` is the entire array of marker values.
		//		`fixed` is true if fixed precision must be applied.
		//		`precision` is the requested precision to be applied.
		//		`labels` is the labels mode of the indicator.
		labelFunc: null,

		// font: String?
		//		A font definition to use for indicator label background.
		font: "",

		// fontColor: String|dojo.Color?
		//		The color to use for indicator label background.
		fontColor: "",

		// markerStroke: dojo.gfx.Stroke?
		//		An optional stroke to use for indicator marker.
		markerStroke: {},

		// markerOutline: dojo.gfx.Stroke?
		//		An optional outline to use for indicator marker.
		markerOutline: {},

		// markerShadow: dojo.gfx.Stroke?
		//		An optional shadow to use for indicator marker.
		markerShadow: {},

		// markerFill: dojo.gfx.Fill?
		//		An optional fill to use for indicator marker.
		markerFill: {},

		// markerSymbol: String?
		//		An optional symbol string to use for indicator marker.
		markerSymbol: "",
	});
	=====*/

	var Indicator = declare("dojox.charting.plot2d.Indicator", [CartesianBase, _PlotEvents], {
		// summary:
		//		A "faux" plot that can be placed behind or above other plots to represent a line or multi-line
		//		threshold on the chart.
		defaultParams: {
			vertical: true,
			fixed: true,
			precision: 0,
			lines: true,
			labels: "line", // "line" | "trend" | "markers" | "none"
			markers: true
		},
		optionalParams: {
			lineStroke: {},
			outlineStroke: {},
			shadowStroke: {},
			lineFill: {},
			stroke:		{},
			outline:	{},
			shadow:		{},
			fill:		{},
			fillFunc:  null,
			labelFunc: null,
			font:		"",
			fontColor:	"",
			markerStroke:		{},
			markerOutline:		{},
			markerShadow:		{},
			markerFill:			{},
			markerSymbol:		"",
			values: [],
			offset: {},
			start: false,
			animate: false
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
			if(typeof kwArgs.values == "number"){
				kwArgs.values = [ kwArgs.values ];
			}
			du.updateWithPattern(this.opt, kwArgs, this.optionalParams);
			this.animate = this.opt.animate;
		},
		render: function(dim, offsets){
			if(this.zoom){
				return this.performZoom(dim, offsets);
			}

			if(!this.isDirty()){
				return this;
			}

			this.cleanGroup(null, true);

			if(!this.opt.values){
				return this;
			}

			this._updateIndicator();
			return this;
		},
		_updateIndicator: function(){
			var t = this.chart.theme;
			var hn = this._hAxis.name, vn = this._vAxis.name,
				hb = this._hAxis.getScaler().bounds, vb = this._vAxis.getScaler().bounds;
			var o = {};
			o[hn] = hb.from;
			o[vn] = vb.from;
			var min = this.toPage(o);
			o[hn] = hb.to;
			o[vn] = vb.to;
			var max = this.toPage(o);
			var events = this.events();
			var results = array.map(this.opt.values, function(value, index){
				return this._renderIndicator(value, index, hn, vn, min, max, events, this.animate);
			}, this);
			var length = results.length;
			if(this.opt.labels == "trend"){
				var v = this.opt.vertical;
				var first = this._data[0][0];
				var last = this._data[length - 1][0];
				var delta = last-first;
				var text = this.opt.labelFunc?this.opt.labelFunc(-1, this.values, this._data, this.opt.fixed, this.opt.precision):
						(dcpc.getLabel(delta, this.opt.fixed, this.opt.precision)+" ("+dcpc.getLabel(100*delta/first, true, 2)+"%)");
				this._renderText(this.getGroup(), text, this.chart.theme, v?(results[0].x+results[length - 1].x)/2:results[1].x,
					v?results[0].y:(results[1].y+results[length - 1].y)/2, -1, this.opt.values, this._data);
			}
			var lineFill = this.opt.lineFill!=undefined?this.opt.lineFill:t.indicator.lineFill;
			if(lineFill && length > 1){
				var x0 = Math.min(results[0].x1, results[length - 1].x1);
				var y0 =  Math.min(results[0].y1, results[length - 1].y1);
				var r = this.getGroup().createRect({x: x0, y: y0, width: Math.max(results[0].x2, results[length - 1].x2) - x0,
															 height: Math.max(results[0].y2, results[length - 1].y2) - y0}).
					setFill(lineFill);
				r.moveToBack();
			}
		},
		_renderIndicator: function(coord, index, hn, vn, min, max, events, animate){
			var t = this.chart.theme, c = this.chart.getCoords(), v = this.opt.vertical;

			var g = this.getGroup().createGroup();
			var mark = {};
			mark[hn] = v?coord:0;
			mark[vn] = v?0:coord;
			if(has("dojo-bidi")){
				mark.x = this._getMarkX(mark.x);
			}
			mark = this.toPage(mark);
			var visible = v?mark.x >= min.x && mark.x <= max.x:mark.y >= max.y && mark.y <= min.y;

			var cx = mark.x - c.x, cy = mark.y - c.y;
			var x1 = v?cx:min.x - c.x, y1 = v?min.y - c.y:cy, x2 = v?x1:max.x - c.x, y2 = v?max.y - c.y:y1;

			if(this.opt.lines && visible){
				var sh = this.opt.hasOwnProperty("lineShadow")?this.opt.lineShadow:t.indicator.lineShadow,
					ls = this.opt.hasOwnProperty("lineStroke")?this.opt.lineStroke:t.indicator.lineStroke,
					ol = this.opt.hasOwnProperty("lineOutline")?this.opt.lineOutline:t.indicator.lineOutline;
				if(sh){
					g.createLine({x1: x1 + sh.dx, y1: y1 + sh.dy, x2: x2 + sh.dx, y2: y2 + sh.dy}).setStroke(sh);
				}
				if(ol){
					ol = dcpc.makeStroke(ol);
					ol.width = 2 * ol.width + (ls?ls.width:0);
					g.createLine({x1: x1, y1: y1, x2: x2, y2: y2}).setStroke(ol);
				}
				g.createLine({x1: x1, y1: y1, x2: x2, y2: y2}).setStroke(ls);
			}

			// series items represent markers on the indicator
			var data;
			if(this.opt.markers && visible){
				var d = this._data[index];
				var self = this;
				if(d){
					data = array.map(d, function(value, index){
						mark[hn] = v?coord:value;
						mark[vn] = v?value:coord;
						if(has("dojo-bidi")){
							mark.x = self._getMarkX(mark.x);
						}
						mark = this.toPage(mark);
						if(v?mark.y <= min.y && mark.y >= max.y:mark.x >= min.x && mark.x <= max.x){
							cx = mark.x - c.x
							cy = mark.y - c.y;
							var ms = this.opt.markerSymbol?this.opt.markerSymbol:t.indicator.markerSymbol,
								path = "M" + cx + " " + cy + " " + ms;
							sh = this.opt.markerShadow!=undefined?this.opt.markerShadow:t.indicator.markerShadow;
							ls = this.opt.markerStroke!=undefined?this.opt.markerStroke:t.indicator.markerStroke;
							ol = this.opt.markerOutline!=undefined?this.opt.markerOutline:t.indicator.markerOutline;
							if(sh){
								var sp = "M" + (cx + sh.dx) + " " + (cy + sh.dy) + " " + ms;
								g.createPath(sp).setFill(sh.color).setStroke(sh);
							}
							if(ol){
								ol = dcpc.makeStroke(ol);
								ol.width = 2 * ol.width + (ls?ls.width:0);
								g.createPath(path).setStroke(ol);
							}

							var shape = g.createPath(path);
							var sf = this._shapeFill(this.opt.markerFill != undefined?this.opt.markerFill:t.indicator.markerFill, shape.getBoundingBox());
							shape.setFill(sf).setStroke(ls);
						}
						return value;
					}, this);
				}
			}
			var ctext;
			if(this.opt.start){
				ctext = {
					x: v?x1:x1,
					y: v?y1:y2
				};
			}else{
				ctext = {
					x: v?x1:x2,
					y: v?y2:y1
				};
			}

			if(this.opt.labels && this.opt.labels != "trend" && visible){
				var text;
				if(this.opt.labelFunc){
					text = this.opt.labelFunc(index, this.opt.values, this._data,
						this.opt.fixed, this.opt.precision, this.opt.labels);
				}else{
					if(this.opt.labels == "markers"){
						text = array.map(data, function(value){
							return dcpc.getLabel(value, this.opt.fixed, this.opt.precision);
						}, this);
						text = text.length != 1 ? "[ "+text.join(", ")+" ]" : text[0];
					}else{
						text = dcpc.getLabel(coord, this.opt.fixed, this.opt.precision);
					}
				}
				this._renderText(g, text, t, ctext.x, ctext.y, index, this.opt.values, this._data);
			}

			if(events){
				this._connectEvents({
					element: "indicator",
					run:     this.run?this.run[index]:undefined,
					shape:   g,
					value:   coord
				});
			}

			if(animate){
				this._animateIndicator(g, v, v?y1:x1, v?(y1 + y2):(x1 + x2), animate);
			}

			return lang.mixin(ctext, {x1: x1, y1: y1, x2: x2, y2: y2});
		},
		_animateIndicator: function(shape, vertical, offset, size, animate){
			var transStart = vertical ? [0, offset] : [offset, 0];
			var scaleStart = vertical ? [1, 1 / size] : [1 / size, 1];
			fx.animateTransform(lang.delegate({
				shape: shape,
				duration: 1200,
				transform: [
					{name: "translate", start: transStart, end: [0, 0]},
					{name: "scale", start: scaleStart, end: [1, 1]},
					{name: "original"}
				]
			}, animate)).play();
		},
		clear: function(){
			this.inherited(arguments);
			this._data = [];
		},
		addSeries: function(run){
			this.inherited(arguments);
			this._data.push(run.data);
		},
		_renderText: function(g, text, t, x, y, index, values, data){
			if(this.opt.offset){
				x += this.opt.offset.x;
				y += this.opt.offset.y;
			}
			var label = dcac.createText.gfx(
				this.chart,
				g,
				x, y,
				this.opt.vertical? "middle" : (this.opt.start ? "start":"end"),
				text, 
                this.opt.font?this.opt.font:t.indicator.font, 
                this.opt.fontColor?this.opt.fontColor:t.indicator.fontColor);
			var b = getBoundingBox(label);
            if(this.opt.vertical && !this.opt.start) {
                b.y += b.height/2;
                label.setShape({y: y+b.height/2});
            }
			b.x-=2; b.y-=1; b.width+=4; b.height+=2; 
            b.r = this.opt.radius?this.opt.radius:t.indicator.radius;
			var sh = this.opt.shadow!=undefined?this.opt.shadow:t.indicator.shadow,
				ls = this.opt.stroke!=undefined?this.opt.stroke:t.indicator.stroke,
				ol = this.opt.outline!=undefined?this.opt.outline:t.indicator.outline;
			if(sh){
				g.createRect(b).setFill(sh.color).setStroke(sh);
			}
			if(ol){
				ol = dcpc.makeStroke(ol);
				ol.width = 2 * ol.width + (ls?ls.width:0);
				g.createRect(b).setStroke(ol);
			}
			var f = this.opt.fillFunc?this.opt.fillFunc(index, values, data):(this.opt.fill!=undefined?this.opt.fill:t.indicator.fill);
			g.createRect(b).setFill(this._shapeFill(f, b)).setStroke(ls);
			label.moveToFront();
		},
		getSeriesStats: function(){
			// summary:
			//		Returns default stats (irrelevant for this type of plot).
			// returns: Object
			//		{hmin, hmax, vmin, vmax} min/max in both directions.
			return lang.delegate(dcpc.defaultStats);
		}
	});
	if(has("dojo-bidi")){
		Indicator.extend({
			_getMarkX: function(x){
				if(this.chart.isRightToLeft()){
					return this.chart.axes.x.scaler.bounds.to + this.chart.axes.x.scaler.bounds.from - x;
				}
				return x;
			}			
		});
	}
	return Indicator;
});
