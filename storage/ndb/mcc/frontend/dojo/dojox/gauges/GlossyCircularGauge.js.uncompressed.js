//>>built
define("dojox/gauges/GlossyCircularGauge", ["dojo/_base/declare","dojo/_base/Color","./GlossyCircularGaugeBase"],
  function(declare, Color, GlossyCircularGaugeBase) {

/*=====
	GlossyCircularGaugeBase = dojox.gauges.GlossyCircularGaugeBase;
=====*/

return declare("dojox.gauges.GlossyCircularGauge", [GlossyCircularGaugeBase], {
	// summary:
	//		Represents a circular gauge with a glossy appearance.
	//
	// example:
	//	|	<div	dojoType="dojox.gauges.GlossyCircularGauge"
	//	|		id="testGauge"
	//	|		width="300"
	//	|		height="300"
	//	|		min="0"
	//	|		max="100"
	//	|		value="0" 
	//	|		majorTicksInterval="10"
	//	|		majorTicksColor="#c4c4c4"
	//	|		minorTicksInterval="5"
	//	|		minorTicksColor="#c4c4c4"
	//	|		color="black" 
	//	|		needleColor="#c4c4c4"
	//	|		font="normal normal normal 10pt sans-serif"
	//	|		textIndicatorFont="normal normal normal 20pt sans-serif"
	//	|		textIndicatorVisible="true" 
	//	|		textIndicatorColor="#c4c4c4" 
	//	|		majorTicksLabelPlacement="inside"|"outside"
	//	|		noChange="true"
	//	|		title="title"
	//	|		scalePrecision="0"
	//	|		textIndicatorPrecision="0">
	//	|	</div>

	_designWidth : 376.25,
	_designHeight : 382.5,
	_designCx : 187.19173,
	_designCy : 187.81589,
	_designTextIndicatorX :	187.19173,
	_designTextIndicatorY :	267.81589,	
	

	constructor: function(){
		// summary:
		//		Creates a new GlossyCircularGauge.
		this.startAngle= -135;
		this.endAngle= 135;
		this.min = 0;
		this.max = 100;
	},

	drawBackground: function(group){
		// summary:
		//		Draws the background of the gauge.
		// group: dojox.gfx.Group
		//		The GFX group where the background must be drawn

		var scale = Math.min((this.width / this._designWidth), (this.height / this._designHeight));
		var transform = {
				xx: scale,
				xy: 0,
				yx: 0,
				yy: scale,
				dx: (-161) * scale + (this.width - scale * this._designWidth) / 2,
				dy: (-263.5) * scale + (this.height - scale * this._designHeight) / 2

		};
		
		var lighterColor1 = Color.blendColors(new Color(this.color), new Color('white'), 0.4 );
		var lighterColor2 = Color.blendColors(new Color(this.color), new Color('white'), 0.8 );		
		
		
		if (this._gaugeBackground){
			this._gaugeBackground.setTransform(transform);
			return;
		}
		this._gaugeBackground = group.createGroup();
		this._gaugeBackground.setTransform(transform);

		this._gaugeBackground.createPath({
			path: "M0 0 C0.028 -82.393 -66.741 -149.207 -149.134 -149.235 C-231.526 -149.264 -298.341 -82.494 -298.369 -0.101 L-298.369 0 C-298.397 82.393 -231.627 149.207 -149.235 149.235 C-66.843 149.264 -0.028 82.494 0 0.102 L0 0 Z"
		}).setTransform({
			xx: 1.25,
			xy: 0,
			yx: 0,
			yy: -1.25,
			dx: 535.09494,
			dy: 452.53442
		}).setFill(this.color);

		this._gaugeBackground.createPath({
			path: "M451.354 450.434 C451.382 374.317 389.698 312.593 313.581 312.566 C237.464 312.54 175.739 374.224 175.713 450.341 L175.713 450.434 C175.688 526.551 237.372 588.275 313.487 588.302 C389.604 588.327 451.329 526.644 451.354 450.527 L451.354 450.434 Z"
		}).setTransform({
			xx: 1.25,
			xy: 0,
			yx: 0,
			yy: -1.25,
			dx: -43.30358,
			dy: 1015.57642
		}).setFill({
			type: "linear",
			x1: 175.688,
			y1: 312.54001,
			x2: 175.688,
			y2: 422.85482,
			colors: [
			         {offset: 0, color: lighterColor1},
			         {offset: 1, color: this.color}
			         ]
		});

		this._gaugeBackground.createPath({
			path: "M451.321 453.375 C449.778 528.175 388.655 588.327 313.491 588.302 C238.359 588.276 177.295 528.135 175.753 453.377 C217.829 442.046 266.246 464.646 315.36 464.646 C364.489 464.646 409.364 442.041 451.321 453.375"
		}).setTransform({
			xx: 1.25,
			xy: 0,
			yx: 0,
			yy: -1.25,
			dx: -43.30358,
			dy: 1015.57642
		}).setFill({
			type: "linear",
			x1: 175.75301,
			y1: 442.04099,
			x2: 175.75301,
			y2: 588.32703,
			colors: [
			         {offset: 0, color: this.color},
			         {offset: 1, color: lighterColor2}
			         ]
		});

		this._gaugeBackground.createPath({
			path: "M0 0 C-1.543 74.8 -62.666 134.952 -137.83 134.927 C-212.962 134.901 -274.026 74.76 -275.568 0.002 C-233.492 -11.329 -185.075 11.271 -135.961 11.271 C-86.832 11.271 -41.957 -11.334 0 0"
		}).setTransform({
			xx: 1.25,
			xy: 0,
			yx: 0,
			yy: -1.25,
			dx: 520.84441,
			dy: 448.85767
		}).setFill([255,255,255,0.12157]);

	},

	drawForeground: function(group){
		// summary:
		//		Draws the foreground of the gauge.
		// group: dojox.gfx.Group
		//		The GFX group where the foreground must be drawn

		var scale = Math.min((this.width / this._designWidth), (this.height / this._designHeight));
		var transform = {
				xx: scale,
				xy: 0,
				yx: 0,
				yy: scale,
				dx: (-160) * scale + (this.width - scale * this._designWidth) / 2,
				dy: (-263.5) * scale + (this.height - scale * this._designHeight) / 2
		};
		
		var lighterColor1 = Color.blendColors(new Color(this.color), new Color('white'), 0.4 );
		var lighterColor2 = Color.blendColors(new Color(this.color), new Color('white'), 0.8 );		
				
		if (this._foreground){
			this._foreground.setTransform(transform);
			return;
		}
		this._foreground = group.createGroup();
		this._foreground.setTransform(transform);

		var group1 = this._foreground.createGroup();
		group1.setTransform({
			xx: 1.25,
			xy: 0,
			yx: 0,
			yy: -1.25,
			dx: -43.30358,
			dy: 1015.57642
		});
		group1.createPath({
			path: "M0 0 C0.004 -12.579 -10.189 -22.779 -22.768 -22.784 C-35.349 -22.788 -45.549 -12.594 -45.553 -0.016 L-45.553 0 C-45.558 12.579 -35.363 22.779 -22.783 22.784 C-10.205 22.788 -0.004 12.594 0 0.015 L0 0 Z"
		}).setTransform({
			xx: 1,
			xy: 0,
			yx: 0,
			yy: 1,
			dx: 336.31049,
			dy: 451.43359
		}).setFill(this.color);

		group1.createPath({
			path: "M333.443 451.434 C333.446 440.438 324.537 431.523 313.541 431.519 C302.546 431.515 293.63 440.425 293.626 451.42 L293.626 451.434 C293.622 462.429 302.532 471.345 313.527 471.349 C324.523 471.353 333.439 462.442 333.443 451.447 L333.443 451.434 Z"
		}).setFill({
			type: "linear",
			x1: 293.62201,
			y1: 431.51501,
			x2: 293.62201,
			y2: 451.43401,
			colors: [
			         {offset: 0, color: lighterColor1},
			         {offset: 1, color: this.color}
			         ]
		});

		group1.createPath({
			path: "M333.438 451.858 C333.215 462.663 324.386 471.353 313.528 471.349 C302.675 471.345 293.854 462.658 293.632 451.858 C299.709 450.222 306.702 453.486 313.799 453.486 C320.895 453.486 327.377 450.221 333.438 451.858"
		}).setFill({
			type: "linear",
			x1: 293.63199,
			y1: 450.22101,
			x2: 293.63199,
			y2: 471.353,
			colors: [
			         {offset: 0, color: this.color},
			         {offset: 1, color: lighterColor2}
			         ]
		});

		group1.createPath({
			path: "M0 0 C-0.223 10.805 -9.052 19.494 -19.909 19.49 C-30.763 19.486 -39.583 10.799 -39.806 0 C-33.729 -1.636 -26.735 1.628 -19.639 1.628 C-12.543 1.628 -6.061 -1.638 0 0"
		}).setTransform({
			xx: 1,
			xy: 0,
			yx: 0,
			yy: 1,
			dx: 333.4375,
			dy: 451.8584
		}).setFill([255,255,255,0.12157]);

	}

});
});
