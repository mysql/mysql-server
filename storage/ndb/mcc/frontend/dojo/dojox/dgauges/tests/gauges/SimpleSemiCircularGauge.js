define(["dojo/_base/lang", "dojo/_base/declare", "dojox/dgauges/CircularGauge", "dojox/dgauges/LinearScaler",
	"dojox/dgauges/CircularScale", "dojox/dgauges/CircularValueIndicator", "dojox/dgauges/CircularRangeIndicator",
	"dojox/dgauges/TextIndicator"],
	function(lang, declare, CircularGauge, LinearScaler, CircularScale, CircularValueIndicator, CircularRangeIndicator,
			 TextIndicator){
		return declare("dojox.dgauges.tests.gauges.SimpleSemiCircularGauge", CircularGauge, {
			constructor: function(){
				// Changes the font
				this.font = {
					family: "Helvetica",
					style: "normal",
					size: "10pt",
					color: "black"
				};

				// Draws a transparent bounding box
				this.addElement("background", function(g){
					g.createRect({
						width: 200,
						height: 100
					});
				});

				// The scaler
				var scaler = new LinearScaler({
					minimum: 0,
					maximum: 100
				});

				// The scale
				var scale = new CircularScale({
					scaler: scaler,
					originX: 100,
					originY: 85,
					startAngle: 180,
					endAngle: 0,
					tickLabelFunc: function(){return null;},
					tickShapeFunc: function(){return null;}
				});
				this.addElement("scale", scale);

				// The background range indicator
				var backgroundRange = new CircularRangeIndicator({
					start: 0,
					value: 100,
					radius: 85,
					startThickness:30,
					endThickness: 30,
					fill:{
						type: "radial",
						cx: 100,
						cy: 85,
						colors: [
							{ offset: 0,   color: "black" },
							{ offset: 0.8, color: "#FAFAFA" },
							{ offset: 1,   color: "#AAAAAA" }
						]
					},
					stroke: null,
					interactionMode: "none"
				});
				scale.addIndicator("backgroundIndicator", backgroundRange);

				// The value range indicator
				var rangeIndicator = new CircularRangeIndicator({
					start: 0,
					value: 65,
					radius: 85,
					startThickness:30,
					endThickness: 30,
					fill:{
						type: "radial",
						cx: 100,
						cy: 85,
						rx: 85,
						ry: 85,
						colors: [
							{ offset: 0,   color: "FF0000" },
							{ offset: 0.8, color: "red" },
							{ offset: 1,   color: "#FFFAFA" }
						]
					},
					stroke: null,
					interactionMode: "mouse"
				});
				scale.addIndicator("rangeIndicator", rangeIndicator);


				// Labels
				this.addElement("text", new TextIndicator({
					indicator: rangeIndicator, x:100, y:75, font: {family: "Helvetica", color: "black", size:"16pt"}
				}));

				this.addElement("text2", new TextIndicator({
					value: "units", x:100, y:82, fill: "#CECECE", font: {family: "Helvetica", color: "#CECECE", size:"5pt"}
				}));

				this.addElement("text3", new TextIndicator({
					value: "0", x:30, y:97
				}));

				this.addElement("text4", new TextIndicator({
					value: "100", x:170, y:97
				}));
			}
		});
	});
