define("dojox/dgauges/components/classic/HorizontalLinearGauge", [
		"dojo/_base/lang", 
		"dojo/_base/declare",
		"dojo/_base/Color",
		"../../RectangularGauge", 
		"../../LinearScaler", 
		"../../RectangularScale", 
		"../../RectangularValueIndicator",
		"../DefaultPropertiesMixin"
	], 
	function(lang, declare, Color, RectangularGauge, LinearScaler, RectangularScale, RectangularValueIndicator, DefaultPropertiesMixin){
		return declare("dojox.dgauges.components.classic.HorizontalLinearGauge", [RectangularGauge, DefaultPropertiesMixin], {
			// summary:
			//		A horizontal gauge widget.

			// borderColor: Object|Array|int
			//		The border color. Default is "#797E86".
			borderColor: [121,126,134],
			// fillColor: Object|Array|int
			//		The background color. Default is "#9498A1".
			fillColor: [148,152,161],
			// indicatorColor: Object|Array|int
			//		The indicator fill color. Default is "#FFFFFF".
			indicatorColor: "#FFFFFF",
			constructor: function(){
				// Base colors
				this.borderColor = new Color(this.borderColor);
				this.fillColor = new Color(this.fillColor);
				this.indicatorColor = new Color(this.indicatorColor);

				this.addElement("background", lang.hitch(this, this.drawBackground));

				// Scaler
				var scaler = new LinearScaler();
				
				// Scale
				var scale = new RectangularScale();
				scale.set("scaler", scaler);
				scale.set("labelPosition", "leading");
				scale.set("paddingLeft", 30);
				scale.set("paddingRight", 30);
				scale.set("paddingTop", 32);
				scale.set("labelGap", 8);
				scale.set("font", {
					family: "Helvetica",
					weight: "bold",
					size: "7pt"
				});
				scale.set("tickShapeFunc", function(group, scale, tick){
					return group.createCircle({
						r: tick.isMinor ? 0.5 : 2
					}).setFill("black");
				});
				this.addElement("scale", scale);
				
				var indicator = new RectangularValueIndicator();
				indicator.set("interactionArea", "gauge");
				indicator.set("value", scaler.minimum);
				indicator.set("paddingTop", 30);
				indicator.set("indicatorShapeFunc", lang.hitch(this, function(group, indicator){
					
					return group.createPolyline([0, 0, -10, -20, 10, -20, 0, 0]).setFill(this.indicatorColor).setStroke({
						color: [121,126,134],
						width: 1,
						style: "Solid",
						cap: "butt",
						join: 20.0
					});

				}));
				scale.addIndicator("indicator", indicator);
			},

			drawBackground: function(g, w, h){
				// summary:
				//		Draws the background shape of the gauge.
				// g: dojox/gfx/Group
				//		The group used to draw the background. 
				// w: Number
				//		The width of the gauge.
				// h: Number
				//		The height of the gauge.
				// tags:
				//		protected
				g.createRect({
					x: 0,
					y: 0,
					width: w,
					height: 50,
					r: 8
				}).setFill(this.borderColor);
				g.createRect({
					x: 2,
					y: 2,
					width: w - 4,
					height: 32,
					r: 6
				}).setFill({
					type: "linear",
					x1: 0,
					y1: 2,
					x2: 0,
					y2: 15,
					colors: [
						{offset: 0, color: [235,235,235]},
						{offset: 1, color: this.borderColor}
					]
				});
				g.createRect({
					x: 6,
					y: 6,
					width: w - 12,
					height: 38,
					r: 5
				}).setFill({
					type: "linear",
					x1: 0,
					y1: 6,
					x2: 0,
					y2: 38,
					colors: [
						{offset: 0, color: [220,220,220]},
						{offset: 1, color: this.fillColor}
					]
				});
				g.createRect({
					x: 7,
					y: 7,
					width: w - 14,
					height: 36,
					r: 3
				}).setFill({
					type: "linear",
					x1: 0,
					y1: 7,
					x2: 0,
					y2: 36,
					colors: [
						{offset: 0, color: this.fillColor},
						{offset: 1, color: [220,220,220]}
					]
				});
			}
		});
	}
);

