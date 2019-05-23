//>>built
define("dojox/dgauges/components/default/HorizontalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/Color","../utils","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../../TextIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _2("dojox.dgauges.components.default.HorizontalLinearGauge",[_6,_b],{borderColor:"#C9DFF2",fillColor:"#FCFCFF",indicatorColor:"#F01E28",constructor:function(){
this.borderColor=new _4(this.borderColor);
this.fillColor=new _4(this.fillColor);
this.indicatorColor=new _4(this.indicatorColor);
this.addElement("background",_1.hitch(this,this.drawBackground));
var _c=new _7();
var _d=new _8();
_d.set("scaler",_c);
_d.set("labelPosition","trailing");
_d.set("paddingTop",15);
_d.set("paddingRight",23);
this.addElement("scale",_d);
var _e=new _9();
_e.indicatorShapeFunc=_1.hitch(this,function(_f){
var _10=_f.createPolyline([0,0,10,0,0,10,-10,0,0,0]).setStroke({color:"blue",width:0.25}).setFill(this.indicatorColor);
return _10;
});
_e.set("paddingTop",5);
_e.set("interactionArea","gauge");
_d.addIndicator("indicator",_e);
this.addElement("indicatorTextBorder",_1.hitch(this,this.drawTextBorder),"leading");
var _11=new _a();
_11.set("indicator",_e);
_11.set("x",32.5);
_11.set("y",30);
this.addElement("indicatorText",_11);
},drawBackground:function(g,w,h){
h=49;
var gap=0;
var cr=3;
var _12=_5.createGradient([0,_5.brightness(this.borderColor,-20),0.1,_5.brightness(this.borderColor,-40)]);
g.createRect({x:0,y:0,width:w,height:h,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_12)).setStroke({color:"#A5A5A5",width:0.2});
_12=_5.createGradient([0,_5.brightness(this.borderColor,70),1,_5.brightness(this.borderColor,-50)]);
gap=4;
cr=2;
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_12));
gap=6;
cr=1;
_12=_5.createGradient([0,_5.brightness(this.borderColor,60),1,_5.brightness(this.borderColor,-40)]);
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_12));
gap=7;
cr=0;
_12=_5.createGradient([0,_5.brightness(this.borderColor,70),1,_5.brightness(this.borderColor,-40)]);
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"linear",x1:w,y1:0,x2:0,y2:h},_12));
gap=5;
cr=0;
_12=_5.createGradient([0,[255,255,255,220],0.8,_5.brightness(this.fillColor,-5),1,_5.brightness(this.fillColor,-30)]);
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"radial",cx:w/2,cy:0,r:w},_12)).setStroke({color:_5.brightness(this.fillColor,-40),width:0.4});
},drawTextBorder:function(g){
return g.createRect({x:5,y:5,width:60,height:39}).setStroke({color:"#CECECE",width:1});
}});
});
