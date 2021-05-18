define("dojox/charting/widget/SelectableLegend", ["dojo/_base/array", 
		"dojo/_base/declare", 
		"dojo/query",
		"dojo/_base/connect", 
		"dojo/_base/Color", 
		"./Legend", 
		"dijit/form/CheckBox", 
		"../action2d/Highlight",
		"dojox/lang/functional", 
		"dojox/gfx/fx", 
		"dojo/keys", 
		"dojo/dom-construct",
		"dojo/dom-prop",
		"dijit/registry"
], function(arrayUtil, declare, query, hub, Color, Legend, CheckBox, Highlight, df, fx, keys, dom, domProp, registry){

	var FocusManager = declare(null, {
		// summary:
		//		It will take legend as a tab stop, and using
		//		cursor keys to navigate labels within the legend.
		// tags:
		//		private
		constructor: function(legend){
			this.legend = legend;
			this.index = 0;
			this.horizontalLength = this._getHrizontalLength();
			arrayUtil.forEach(legend.legends, function(item, i){
				if(i > 0){
					query("input", item).attr("tabindex", -1);
				}
			});
			this.firstLabel = query("input", legend.legends[0])[0];
			hub.connect(this.firstLabel, "focus", this, function(){this.legend.active = true;});
			hub.connect(this.legend.domNode, "keydown", this, "_onKeyEvent");
		},
		_getHrizontalLength: function(){
			var horizontal = this.legend.horizontal;
			if(typeof horizontal == "number"){
				return Math.min(horizontal, this.legend.legends.length);
			}else if(!horizontal){
				return 1;
			}else{
				return this.legend.legends.length;
			}
		},
		_onKeyEvent: function(e){
			//	if not focused
			if(!this.legend.active){
				return;
			}
			//	lose focus
			if(e.keyCode == keys.TAB){
				this.legend.active = false;
				return;
			}
			//	handle with arrow keys
			var max = this.legend.legends.length;
			switch(e.keyCode){
				case keys.LEFT_ARROW:
					this.index--;
					if(this.index < 0){
						this.index += max;
					}
					break;
				case keys.RIGHT_ARROW:
					this.index++;
					if(this.index >= max){
						this.index -= max;
					}
					break;
				case keys.UP_ARROW:
					if(this.index - this.horizontalLength >= 0){
						this.index -= this.horizontalLength;
					}
					break;
				case keys.DOWN_ARROW:
					if(this.index + this.horizontalLength < max){
						this.index += this.horizontalLength;
					}
					break;
				default:
					return;
			}
			this._moveToFocus();
			Event.stop(e);
		},
		_moveToFocus: function(){
			query("input", this.legend.legends[this.index])[0].focus();
		}
	});
	
	var FakeHighlight = declare(Highlight, {
		connect: function(){}
	});
	
	var SelectableLegend = declare("dojox.charting.widget.SelectableLegend", Legend, {
		// summary:
		//		An enhanced chart legend supporting interactive events on data series
		
		//	theme component
		outline:			false,	//	outline of vanished data series
		transitionFill:		null,	//	fill of deselected data series
		transitionStroke:	null,	//	stroke of deselected data series

		// autoScale: Boolean
		//		Whether the scales of the chart are recomputed when selecting/unselecting a series in the legend. Default is false.
		autoScale: false,
		
		postCreate: function(){
			this.legends = [];
			this.legendAnim = {};
			this._cbs = [];
			this.inherited(arguments);
		},
		refresh: function(){
			this.legends = [];
			this._clearLabels();
			this.inherited(arguments);
			this._applyEvents();
			new FocusManager(this);
		},
		_clearLabels: function(){
			var cbs = this._cbs;
			while(cbs.length){
				cbs.pop().destroyRecursive();
			}
		},
		_addLabel: function(dyn, label){
			this.inherited(arguments);
			//	create checkbox
			var legendNodes = query("td", this.legendBody);
			var currentLegendNode = legendNodes[legendNodes.length - 1];
			this.legends.push(currentLegendNode);
			var checkbox = new CheckBox({checked: true});
			this._cbs.push(checkbox);
			dom.place(checkbox.domNode, currentLegendNode, "first");
			// connect checkbox and existed label
			var clabel = query("label", currentLegendNode)[0];
			domProp.set(clabel, "for", checkbox.id);
		},
		_applyEvents: function(){
			// summary:
			//		Apply click-event on checkbox and hover-event on legend icon,
			//		highlight data series or toggle it.
			
			// if the chart has not yet been refreshed it will crash here (targetData.group == null)
			if(this.chart.dirty){
				return;
			}
			arrayUtil.forEach(this.legends, function(legend, i){
				var targetData, plotName, seriesName;
				if(this._isPie()){
					targetData = this.chart.stack[0];
					plotName = targetData.name;
					seriesName = this.chart.series[0].name;
				}else{
					targetData = this.chart.series[i];
					plotName = targetData.plot;
					seriesName = targetData.name;
				}
				//	toggle action
				var legendCheckBox = registry.byNode(query(".dijitCheckBox", legend)[0]);
				legendCheckBox.set("checked", !this._isHidden(plotName, i));
				hub.connect(legendCheckBox, "onClick", this, function(e){
					this.toogle(plotName, i, !legendCheckBox.get("checked"));
					e.stopPropagation();
				});
				//	highlight action
				var legendIcon = query(".dojoxLegendIcon", legend)[0],
					iconShape = this._getFilledShape(this._surfaces[i].children);
				arrayUtil.forEach(["onmouseenter", "onmouseleave"], function(event){
					hub.connect(legendIcon, event, this, function(e){
						this._highlight(e, iconShape, i, !legendCheckBox.get("checked"), seriesName, plotName);
					});
				}, this);
			},this);
		},
		_isHidden: function(plotName, index){
			if(this._isPie()){
				return arrayUtil.indexOf(this.chart.getPlot(plotName).runFilter, index) != -1;
			}else{
				return this.chart.series[index].hidden
			}
		},
		toogle: function(plotName, index, hide){
			var plot =  this.chart.getPlot(plotName);
			if(this._isPie()){
				if(arrayUtil.indexOf(plot.runFilter, index) != -1){
					if(!hide){
						plot.runFilter = arrayUtil.filter(plot.runFilter, function(item){
							return item != index;
						});
					}
				}else{
					if(hide){
						plot.runFilter.push(index);
					}
				}
			}else{ 
				this.chart.series[index].hidden = hide;
			}
			this.autoScale ? this.chart.dirty = true: plot.dirty = true;
			this.chart.render();
		},
		_highlight: function(e, iconShape, index, isOff, seriesName, plotName){
			if(!isOff){
				var anim = this._getAnim(plotName),
					isPie = this._isPie(),
					type = formatEventType(e.type);
				// highlight the label icon,
				var label = {
					shape: iconShape,
					index: isPie ? "legend" + index : "legend",
					run: {name: seriesName},
					type: type
				};
				anim.process(label);
				//	highlight the data items
				arrayUtil.forEach(this._getShapes(index, plotName), function(shape, i){
					var o = {
						shape: shape,
						index: isPie ? index : i,
						run: {name: seriesName},
						type: type
					};
					anim.duration = 100;
					anim.process(o);
				});
			}
		},
		_getShapes: function(i, plotName){
			var shapes = [];
			if(this._isPie()){
				var decrease = 0;
				arrayUtil.forEach(this.chart.getPlot(plotName).runFilter, function(item){
					if(i > item){
						decrease++;
					}
				});
				shapes.push(this.chart.stack[0].group.children[i-decrease]);
			}else if(this._isCandleStick(plotName)){
				arrayUtil.forEach(this.chart.series[i].group.children, function(group){
					arrayUtil.forEach(group.children, function(candle){
						arrayUtil.forEach(candle.children, function(shape){
							if(shape.shape.type !="line"){
								shapes.push(shape);
							}
						});
					});
				});
			}else{
				shapes = this.chart.series[i].group.children;
			}
			return shapes;
		},
		_getAnim: function(plotName){
			if(!this.legendAnim[plotName]){
				this.legendAnim[plotName] = new FakeHighlight(this.chart, plotName);
			}
			return this.legendAnim[plotName];
		},
		_getTransitionFill: function(plotName){
			// Since series of stacked charts all start from the base line,
			// fill the "front" series with plotarea color to make it disappear .
			if(this.chart.stack[this.chart.plots[plotName]].declaredClass.indexOf("dojox.charting.plot2d.Stacked") != -1){
				return this.chart.theme.plotarea.fill;
			}
			return null;
		},
		_getFilledShape: function(shapes){
			// summary:
			//		Get filled shape in legend icon which would be highlighted when hovered
			var i = 0;
			while(shapes[i]){
				if(shapes[i].getFill())return shapes[i];
				i++;
			}
			return null;
		},
		_isPie: function(){
			return this.chart.stack[0].declaredClass == "dojox.charting.plot2d.Pie";
		},
		_isCandleStick: function(plotName){
			return this.chart.stack[this.chart.plots[plotName]].declaredClass == "dojox.charting.plot2d.Candlesticks";
		},
		destroy: function(){
			this._clearLabels();
			this.inherited(arguments);
		}
	});
	
	function formatEventType(type){
		if(type == "mouseenter")return "onmouseover";
		if(type == "mouseleave")return "onmouseout";
		return "on" + type;
	}

	return SelectableLegend;
});
