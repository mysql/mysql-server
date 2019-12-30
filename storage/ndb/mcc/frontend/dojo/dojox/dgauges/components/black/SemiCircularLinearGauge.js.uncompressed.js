define("dojox/dgauges/components/black/SemiCircularLinearGauge", ["dojo/_base/lang", "dojo/_base/declare", "dojo/_base/Color", 
		"../../CircularGauge", 
		"../../LinearScaler", 
		"../../CircularScale", 
		"../../CircularValueIndicator", 
		"../../CircularRangeIndicator",
		"../DefaultPropertiesMixin"
	],
	function(lang, declare, Color, CircularGauge, LinearScaler, CircularScale, CircularValueIndicator, CircularRangeIndicator, DefaultPropertiesMixin){
	return declare("dojox.dgauges.components.black.SemiCircularLinearGauge", [CircularGauge, DefaultPropertiesMixin], {
		// summary:
		//		A semi circular gauge widget.

		// borderColor: Object|Array|int
		//		The border color. Default is "#000000".
		borderColor: "#000000",
		// fillColor: Object|Array|int
		//		The background color. Default is "#000000".
		fillColor: "#000000",
		// indicatorColor: Object|Array|int
		//		The indicator fill color. Default is "#A4A4A4".
		indicatorColor: "#A4A4A4",
		constructor: function(){
			// Base colors
			this.borderColor = new Color(this.borderColor);
			this.fillColor = new Color(this.fillColor);
			this.indicatorColor = new Color(this.indicatorColor);

			var scaler = new LinearScaler();
			this.addElement("background", lang.hitch(this, this.drawBackground));
			var scale = new CircularScale();
			scale.set("scaler", scaler);
			scale.set("originX", 186.46999);
			scale.set("originY", 184.74814);			
			scale.set("radius", 149.82183);
			scale.set("startAngle", -180);
			scale.set("endAngle", 0);
			scale.set("orientation", "clockwise");
			scale.set("labelGap", 8);
			scale.set("font", {
				family: "Helvetica",
				weight: "bold",
				size: "14pt",
				color: "#CECECE"
			});
			scale.set("tickShapeFunc", function(group, scale, tick){
				return group.createCircle({
					r: tick.isMinor ? 2 : 4
				}).setFill("#CECECE");
			});
			this.addElement("scale", scale);
			var indicator = new CircularValueIndicator();
			indicator.set("interactionArea", "gauge");
			indicator.set("value", scaler.minimum);
			indicator.set("indicatorShapeFunc", lang.hitch(this, function(group, indicator){
				return group.createPolyline([0, -12, indicator.scale.radius - 2, 0, 0, 12, 0, -12]).setStroke({
					color: [70, 70, 70],
					width: 1
				}).setFill(this.indicatorColor);
				
			}));
			scale.addIndicator("indicator", indicator);
			this.addElement("foreground", lang.hitch(this, this.drawForeground));
		},
		
		drawBackground: function(g){
			// summary:
			//		Draws the background shape of the gauge.
			// g: dojox/gfx/Group
			//		The group used to draw the background. 
			// tags:
			//		protected
			g.createPath({
				path: "M372.8838 205.5688 C372.9125 204.4538 372.93 194.135 372.94 185.6062 C372.4475 83.0063 289.1138 -0 186.4063 0.035 C83.7 0.0713 0.4225 83.1325 0 185.7325 C0.01 194.2175 0.0275 204.4638 0.0563 205.5763 C0.235 212.3488 5.7763 217.7462 12.5525 217.7462 L360.3888 217.7462 C367.1663 217.7462 372.71 212.3438 372.8838 205.5688"
			}).setFill(this.borderColor);
			g.createPath({
				path: "M358.6738 203.9965 C358.7188 202.3627 358.7463 188.224 358.745 186.579 L358.745 186.4627 C358.7138 91.3165 281.5575 14.2127 186.4113 14.244 C91.2675 14.2777 14.1625 91.4327 14.1938 186.579 C14.1938 186.6177 14.2213 202.4015 14.2663 203.9965 L358.6738 203.9965 Z"
			}).setFill({
				type: "linear",
				x1: 14.19376,
				y1: 260.92225,
				x2: 14.19376,
				y2: 156.55837,
				colors: [{
					offset: 0,
					color: [100, 100, 100]
				}, {
					offset: 1,
					color: this.fillColor
				}]
			});
			g.createPath({
				path: "M358.7038 182.9027 C356.775 89.4027 280.3713 14.2127 186.4163 14.244 C92.5013 14.2765 16.1713 89.4527 14.2438 182.9002 C66.8388 197.064 127.36 168.814 188.7525 168.814 C250.1638 168.814 306.2575 197.0703 358.7038 182.9027"
			}).setFill({
				type: "linear",
				x1: 14.24378,
				y1: 186.87786,
				x2: 14.24378,
				y2: 14.24398,
				colors: [{
					offset: 0,
					color: this.fillColor
				}, {
					offset: 1,
					color: [200, 200, 200]
				}]
			});
			g.createPath({
				path: "M358.953 183.1553 C357.0243 89.6553 280.6205 14.4653 186.6655 14.4966 C92.7505 14.5291 16.4205 89.7053 14.493 183.1528 C67.088 197.3166 127.6093 169.0666 189.0018 169.0666 C250.413 169.0666 306.5068 197.3228 358.953 183.1553"
			}).setFill([255, 255, 255, 0.12157]);
		},
		
		drawForeground: function(g){
			// summary:
			//		Draws the foreground shape of the gauge.
			// g: dojox/gfx/Group
			//		The group used to draw the foreground. 
			// tags:
			//		protected
			var g1 = g.createGroup();
			g1.createPath({
				path: "M214.9406 185.3295 C214.9456 201.0533 202.2044 213.8033 186.4806 213.8095 C170.7544 213.8145 158.0044 201.072 157.9994 185.3495 L157.9994 185.3295 C157.9931 169.6057 170.7369 156.8557 186.4619 156.8495 C202.1844 156.8445 214.9356 169.587 214.9406 185.3108 L214.9406 185.3295 Z"
			}).setFill(this.borderColor);
			g1.createPath({
				path: "M211.3563 185.329 C211.36 199.074 200.2238 210.2177 186.4787 210.2228 C172.735 210.2277 161.59 199.0902 161.585 185.3465 L161.585 185.329 C161.58 171.5852 172.7175 160.4402 186.4613 160.4352 C200.2063 160.4303 211.3513 171.569 211.3563 185.3128 L211.3563 185.329 Z"
			}).setFill({
				type: "linear",
				x1: 161.58503,
				y1: 210.22273,
				x2: 161.58503,
				y2: 185.32899,
				colors: [{
					offset: 0,
					color: [100, 100, 100]
				}, {
					offset: 1,
					color: this.fillColor
				}]
			});
			g1.createPath({
				path: "M211.35 184.799 C211.0713 171.2928 200.035 160.4303 186.4625 160.4352 C172.8963 160.4402 161.87 171.299 161.5925 184.799 C169.1888 186.844 177.93 182.764 186.8013 182.764 C195.6712 182.764 203.7738 186.8452 211.35 184.799"
			}).setFill({
				type: "linear",
				x1: 161.59251,
				y1: 185.37311,
				x2: 161.59251,
				y2: 160.43524,
				colors: [{
					offset: 0,
					color: this.fillColor
				}, {
					offset: 1,
					color: [150, 150, 150]
				}]
			});
			g1.createPath({
				path: "M211.3494 184.7985 C211.0706 171.2923 200.0344 160.431 186.4632 160.436 C172.8956 160.441 161.8707 171.2997 161.5919 184.7985 C169.1881 186.8435 177.9306 182.7635 186.8006 182.7635 C195.6706 182.7635 203.7731 186.846 211.3494 184.7985"
			}).setFill([255, 255, 255, 0.12157]);
		}
	});
});

