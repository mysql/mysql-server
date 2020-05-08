define(["dojo/_base/lang", "dojo/_base/array", "dojo/_base/declare", "../plot2d/Indicator",
        "dojo/has", "../plot2d/common", "../axis2d/common", "dojox/gfx"],
	function(lang, array, declare, Indicator, has){

	var getXYCoordinates = function(v, values, data){
		var c2, c1 = v?{ x: values[0], y : data[0][0] } :
					   { x : data[0][0], y : values[0] };
		if(values.length > 1){
			c2 = v?{ x: values[1], y : data[1][0] } :
				   { x : data[1][0], y : values[1] };
		}
		return [c1, c2];
	};

	var _IndicatorElement = declare("dojox.charting.action2d._IndicatorElement", Indicator, {
		// summary:
		//		Internal element used by indicator actions.
		// tags:
		//		private
		constructor: function(chart, kwArgs){
			if(!kwArgs){ kwArgs = {}; }
			this.inter = kwArgs.inter;
		},
		_updateVisibility: function(cp, limit, attr){
			var axis = attr=="x"?this._hAxis:this._vAxis;
			var scale = axis.getWindowScale();
			this.chart.setAxisWindow(axis.name, scale, axis.getWindowOffset() + (cp[attr] - limit[attr]) / scale);
			this._noDirty = true;
			this.chart.render();
			this._noDirty = false;
			this._initTrack();
		},
		_trackMove: function(){
			// let's update the selector
			this._updateIndicator(this.pageCoord);
			// if we reached that point once, then we don't stop until mouse up
 			// use a recursive setTimeout to avoid intervals that might get backed up
			this._tracker = setTimeout(lang.hitch(this, this._trackMove), 100);
		},
		_initTrack: function(){
			if(!this._tracker){
				this._tracker = setTimeout(lang.hitch(this, this._trackMove), 500);
			}
		},
		stopTrack: function(){
			if(this._tracker){
				clearTimeout(this._tracker);
				this._tracker = null;
			}
		},
		render: function(){
			if(!this.isDirty()){
				return;
			}

			var inter = this.inter, plot = inter.plot, v = inter.opt.vertical;

			this.opt.offset = inter.opt.offset || (v?{ x:0 , y: 5}: { x: 5, y: 0});

			if(inter.opt.labelFunc){
				// adapt to indicator labelFunc format
				this.opt.labelFunc = function(index, values, data, fixed, precision){
					var coords = getXYCoordinates(v, values, data);
					return inter.opt.labelFunc(coords[0], coords[1], fixed, precision);
				};
			}
			if(inter.opt.fillFunc){
				// adapt to indicator fillFunc format
				this.opt.fillFunc = function(index, values, data){
					var coords = getXYCoordinates(v, values, data);
					return inter.opt.fillFunc(coords[0], coords[1]);
				};
			}

			this.opt = lang.delegate(inter.opt, this.opt);

			if(!this.pageCoord){
				this.opt.values = null;
				this.inter.onChange({});
			}else{
				// let's create a fake coordinate to not block parent render method
				// actual coordinate will be computed in _updateCoordinates
				this.opt.values = [];
				this.opt.labels = this.secondCoord?"trend":"markers";
			}

			// take axis on the interactor plot and forward them onto the indicator plot
			this.hAxis = plot.hAxis;
			this.vAxis = plot.vAxis;

			this.inherited(arguments);
		},
		_updateIndicator: function(){
			var coordinates = this._updateCoordinates(this.pageCoord, this.secondCoord);
			if(coordinates.length > 1){
				var v = this.opt.vertical;
				this._data= [];
				this.opt.values = [];
				array.forEach(coordinates, function(value){
					if(value){
						this.opt.values.push(v?value.x:value.y);
						this._data.push([v?value.y:value.x]);
					}
				}, this);
			}else{
				this.inter.onChange({});
				return;
			}
			this.inherited(arguments);
		},
		_renderText: function(g, text, t, x, y, index, values, data){
			// render only if labels is true
			if(this.inter.opt.labels){
				this.inherited(arguments);
			}
			// send the event in all cases
			var coords = getXYCoordinates(this.opt.vertical, values, data);
			this.inter.onChange({
				start: coords[0],
				end: coords[1],
				label: text
			});
		},
		_updateCoordinates: function(cp1, cp2){
			// chart mirroring starts
			if(has("dojo-bidi")){
				this._checkXCoords(cp1, cp2);
			}
			// chart mirroring ends
			var inter = this.inter, plot = inter.plot, v = inter.opt.vertical;
			var hAxis = this.chart.getAxis(plot.hAxis), vAxis = this.chart.getAxis(plot.vAxis);
			var hn = hAxis.name, vn = vAxis.name, hb = hAxis.getScaler().bounds, vb = vAxis.getScaler().bounds;
			var attr = v?"x":"y", n = v?hn:vn, bounds = v?hb:vb;

			// sort data point
			if(cp2){
				var tmp;
				if(v){
					if(cp1.x > cp2.x){
						tmp = cp2;
						cp2 = cp1;
						cp1 = tmp;
					}
				}else{
					if(cp1.y > cp2.y){
						tmp = cp2;
						cp2 = cp1;
						cp1 = tmp;
					}
				}
			}

			var cd1 = plot.toData(cp1), cd2;
			if(cp2){
				cd2 = plot.toData(cp2);
			}

			var o = {};
			o[hn] = hb.from;
			o[vn] = vb.from;
			var min = plot.toPage(o);
			o[hn] = hb.to;
			o[vn] = vb.to;
			var max = plot.toPage(o);

			if(cd1[n] < bounds.from){
				// do not autoscroll if dual indicator
				if(!cd2 && inter.opt.autoScroll && !inter.opt.mouseOver){
					this._updateVisibility(cp1, min, attr);
					return [];
				}else{
					if(inter.opt.mouseOver){
						return[];
					}
					cp1[attr] = min[attr];
				}
				// cp1 might have changed, let's update cd1
				cd1 = plot.toData(cp1);
			}else if(cd1[n] > bounds.to){
				if(!cd2 && inter.opt.autoScroll && !inter.opt.mouseOver){
					this._updateVisibility(cp1, max, attr);
					return [];
				}else{
					if(inter.opt.mouseOver){
						return[];
					}
					cp1[attr] = max[attr];
				}
				// cp1 might have changed, let's update cd1
				cd1 = plot.toData(cp1);
			}

			var c1 = this._snapData(cd1, attr, v), c2;

			if(c1.y == null){
				// we have no data for that point let's just return
				return [];
			}

			if(cp2){
				if(cd2[n] < bounds.from){
					cp2[attr] = min[attr];
					cd2 = plot.toData(cp2);
				}else if(cd2[n] > bounds.to){
					cp2[attr] = max[attr];
					cd2 = plot.toData(cp2);
				}
				c2 = this._snapData(cd2, attr, v);
				if(c2.y == null){
					// we have no data for that point let's pretend we have a single touch point
					c2 = null;
				}
			}

			return [c1, c2];
		},
		_snapData: function(cd, attr, v){
			// we need to find which actual data point is "close" to the data value
			var data = this.chart.getSeries(this.inter.opt.series).data;
			// let's consider data are sorted because anyway rendering will be "weird" with unsorted data
			// i is an index in the array, which is different from a x-axis value even for index based data
			var i, r, l = data.length;
			// first let's find which data index we are in
			for (i = 0; i < l; ++i){
				r = data[i];
				if(r == null){
					// move to next item
				}else if(typeof r == "number"){
					if(i + 1 > cd[attr]){
						break;
					}
				}else if(r[attr] > cd[attr]){
					break;
				}
			}

			var x, y, px, py;
			if(typeof r == "number"){
				x = i+1;
				y = r;
				if(i > 0){
					px = i;
					py = data[i-1];
				}
			}else{
				x = r.x;
				y = r.y;
				if(i > 0){
					px = data[i-1].x;
					py = data[i-1].y;
				}
			}
			if(i > 0){
				var m = v?(x+px)/2:(y+py)/2;
				if(cd[attr]<=m){
					x = px;
					y = py;
				}
			}
			return {x: x, y: y};
		},
		cleanGroup: function(creator){
			// summary:
			//		Clean any elements (HTML or GFX-based) out of our group, and create a new one.
			// creator: dojox/gfx/Surface?
			//		An optional surface to work with.
			// returns: dojox/charting/Element
			//		A reference to this object for functional chaining.
			this.inherited(arguments);
			// we always want to be above regular plots and not clipped
			this.group.moveToFront();
			return this;	//	dojox/charting/Element
		},
		isDirty: function(){
			// summary:
			//		Return whether or not this plot needs to be redrawn.
			// returns: Boolean
			//		If this plot needs to be rendered, this will return true.
			return !this._noDirty && (this.dirty || this.inter.plot.isDirty());
		}
	});
	if(has("dojo-bidi")){
		_IndicatorElement.extend({
			_checkXCoords: function(cp1, cp2){
				if(this.chart.isRightToLeft() && this.isDirty()){
					var offset = this.chart.node.offsetLeft;
					function transform(plot, cp) {
						var x = cp.x - offset;
						var shift = (plot.chart.offsets.l - plot.chart.offsets.r);
						var transformed_x = plot.chart.dim.width + shift - x;

						return transformed_x + offset;
					}
					if(cp1){
						cp1.x = transform(this, cp1);
					}
					if(cp2){
						cp2.x = transform(this, cp2);
					}
				}
			}
		});
	}
	return _IndicatorElement;
});
