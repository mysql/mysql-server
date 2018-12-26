//>>built
define("dojox/gauges/GlossySemiCircularGauge",["dojo/_base/declare","dojo/_base/Color","./GlossyCircularGaugeBase"],function(_1,_2,_3){
return _1("dojox.gauges.GlossySemiCircularGauge",[_3],{_designWidth:381.25,_designHeight:221.25,_designCx:190.6675,_designCy:185.87665,_designTextIndicatorX:190.6675,_designTextIndicatorY:145.87665,constructor:function(){
this.min=0;
this.max=100;
this.startAngle=-91.5;
this.endAngle=91.5;
},drawBackground:function(_4){
var _5=_2.blendColors(new _2(this.color),new _2("white"),0.4);
var _6=_2.blendColors(new _2(this.color),new _2("white"),0.8);
var _7=Math.min((this.width/this._designWidth),(this.height/this._designHeight));
var _8={xx:_7,xy:0,yx:0,yy:_7,dx:-23.33928*_7+(this.width-_7*this._designWidth)/2,dy:-483.30859*_7+(this.height-_7*this._designHeight)/2};
if(this._gaugeBackground){
this._gaugeBackground.setTransform(_8);
return;
}
this._gaugeBackground=_4.createGroup();
this._gaugeBackground.setTransform(_8);
this._gaugeBackground.createPath({path:"M0 0 C0.023 0.892 0.037 9.147 0.045 15.97 C-0.349 98.05 -67.016 164.455 -149.182 164.427 C-231.347 164.398 -297.969 97.949 -298.307 15.869 C-298.299 9.081 -298.285 0.884 -298.262 -0.006 C-298.119 -5.424 -293.686 -9.742 -288.265 -9.742 L-9.996 -9.742 C-4.574 -9.742 -0.139 -5.42 0 0"}).setTransform({xx:1.25,xy:0,yx:0,yy:-1.25,dx:400.74198,dy:690.00586}).setFill(this.color);
this._gaugeBackground.createPath({path:"M451.297 436.5 C451.333 437.807 451.355 449.118 451.354 450.434 L451.354 450.527 C451.329 526.644 389.604 588.327 313.487 588.302 C237.372 588.275 175.688 526.551 175.713 450.434 C175.713 450.403 175.735 437.776 175.771 436.5 L451.297 436.5 Z"}).setTransform({xx:1.25,xy:0,yx:0,yy:-1.25,dx:-177.58928,dy:1234.05859}).setFill({type:"linear",x1:175.688,y1:390.95189,x2:175.688,y2:474.45676,colors:[{offset:0,color:_5},{offset:1,color:this.color}]});
this._gaugeBackground.createPath({path:"M451.321 453.375 C449.778 528.175 388.655 588.327 313.491 588.302 C238.359 588.276 177.295 528.135 175.753 453.377 C217.829 442.046 266.246 464.646 315.36 464.646 C364.489 464.646 409.364 442.041 451.321 453.375"}).setTransform({xx:1.25,xy:0,yx:0,yy:-1.25,dx:-177.58928,dy:1234.05859}).setFill({type:"linear",x1:175.75301,y1:442.04099,x2:175.75301,y2:588.32703,colors:[{offset:0,color:this.color},{offset:1,color:_6}]});
this._gaugeBackground.createPath({path:"M0 0 C-1.543 74.8 -62.666 134.952 -137.83 134.927 C-212.962 134.901 -274.026 74.76 -275.568 0.002 C-233.492 -11.329 -185.075 11.271 -135.961 11.271 C-86.832 11.271 -41.957 -11.334 0 0"}).setTransform({xx:1.25,xy:0,yx:0,yy:-1.25,dx:386.81123,dy:667.59241}).setFill([255,255,255,0.12157]);
},drawForeground:function(_9){
var _a=Math.min((this.width/this._designWidth),(this.height/this._designHeight));
var _b={xx:_a,xy:0,yx:0,yy:_a,dx:(-160)*_a+(this.width-_a*this._designWidth)/2,dy:(-264.5)*_a+(this.height-_a*this._designHeight)/2};
var _c=_2.blendColors(new _2(this.color),new _2("white"),0.4);
var _d=_2.blendColors(new _2(this.color),new _2("white"),0.8);
if(this._foreground){
this._foreground.setTransform(_b);
return;
}
this._foreground=_9.createGroup();
this._foreground.setTransform(_b);
var _e=this._foreground.createGroup().setTransform({xx:1.25,xy:0,yx:0,yy:-1.25,dx:-43.30358,dy:1015.57642});
_e.createPath({path:"M0 0 C0.004 -12.579 -10.189 -22.779 -22.768 -22.784 C-35.349 -22.788 -45.549 -12.594 -45.553 -0.016 L-45.553 0 C-45.558 12.579 -35.363 22.779 -22.783 22.784 C-10.205 22.788 -0.004 12.594 0 0.015 L0 0 Z"}).setTransform({xx:1,xy:0,yx:0,yy:1,dx:336.31049,dy:451.43359}).setFill(this.color);
_e.createPath({path:"M333.443 451.434 C333.446 440.438 324.537 431.523 313.541 431.519 C302.546 431.515 293.63 440.425 293.626 451.42 L293.626 451.434 C293.622 462.429 302.532 471.345 313.527 471.349 C324.523 471.353 333.439 462.442 333.443 451.447 L333.443 451.434 Z"}).setFill({type:"linear",x1:293.62201,y1:431.51501,x2:293.62201,y2:451.43401,colors:[{offset:0,color:_c},{offset:1,color:this.color}]});
_e.createPath({path:"M333.438 451.858 C333.215 462.663 324.386 471.353 313.528 471.349 C302.675 471.345 293.854 462.658 293.632 451.858 C299.709 450.222 306.702 453.486 313.799 453.486 C320.895 453.486 327.377 450.221 333.438 451.858"}).setFill({type:"linear",x1:293.63199,y1:450.22101,x2:293.63199,y2:471.353,colors:[{offset:0,color:this.color},{offset:1,color:_d}]});
_e.createPath({path:"M0 0 C-0.223 10.805 -9.052 19.494 -19.909 19.49 C-30.763 19.486 -39.583 10.799 -39.806 0 C-33.729 -1.636 -26.735 1.628 -19.639 1.628 C-12.543 1.628 -6.061 -1.638 0 0"}).setTransform({xx:1,xy:0,yx:0,yy:1,dx:333.4375,dy:451.8584}).setFill([255,255,255,0.12157]);
}});
});
