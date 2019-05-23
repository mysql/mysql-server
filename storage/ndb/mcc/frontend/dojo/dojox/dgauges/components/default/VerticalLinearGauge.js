//>>built
define("dojox/dgauges/components/default/VerticalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/connect","dojo/_base/Color","../utils","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../../RectangularRangeIndicator","../../TextIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _2("dojox.dgauges.components.default.VerticalLinearGauge",[_6,_c],{borderColor:"#C9DFF2",fillColor:"#FCFCFF",indicatorColor:"#F01E28",constructor:function(){
this.orientation="vertical";
this.borderColor=new _4(this.borderColor);
this.fillColor=new _4(this.fillColor);
this.indicatorColor=new _4(this.indicatorColor);
this.addElement("background",_1.hitch(this,this.drawBackground));
var _d=new _7();
var _e=new _8();
_e.set("scaler",_d);
_e.set("labelPosition","leading");
_e.set("paddingBottom",20);
_e.set("paddingLeft",25);
this.addElement("scale",_e);
var _f=new _9();
_f.indicatorShapeFunc=_1.hitch(this,function(_10){
var _11=_10.createPolyline([0,0,10,0,0,10,-10,0,0,0]).setStroke({color:"blue",width:0.25}).setFill(this.indicatorColor);
return _11;
});
_f.set("paddingLeft",45);
_f.set("interactionArea","gauge");
_e.addIndicator("indicator",_f);
this.addElement("indicatorTextBorder",_1.hitch(this,this.drawTextBorder),"leading");
var _12=new _b();
_12.set("indicator",_f);
_12.set("x",22.5);
_12.set("y",30);
this.addElement("indicatorText",_12);
},drawBackground:function(g,w,h){
w=49;
var gap=0;
var cr=3;
var _13=_5.createGradient([0,_5.brightness(this.borderColor,-20),0.1,_5.brightness(this.borderColor,-40)]);
g.createRect({x:0,y:0,width:w,height:h,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_13)).setStroke({color:"#A5A5A5",width:0.2});
_13=_5.createGradient([0,_5.brightness(this.borderColor,70),1,_5.brightness(this.borderColor,-50)]);
gap=4;
cr=2;
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_13));
gap=6;
cr=1;
_13=_5.createGradient([0,_5.brightness(this.borderColor,60),1,_5.brightness(this.borderColor,-40)]);
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_13));
gap=7;
cr=0;
_13=_5.createGradient([0,_5.brightness(this.borderColor,70),1,_5.brightness(this.borderColor,-40)]);
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"linear",x1:w,y1:0,x2:0,y2:h},_13));
gap=5;
cr=0;
_13=_5.createGradient([0,[255,255,255,220],0.8,_5.brightness(this.fillColor,-5),1,_5.brightness(this.fillColor,-30)]);
g.createRect({x:gap,y:gap,width:w-2*gap,height:h-2*gap,r:cr}).setFill(_1.mixin({type:"radial",cx:0,cy:h/2,r:h},_13)).setStroke({color:_5.brightness(this.fillColor,-40),width:0.4});
},drawTextBorder:function(g){
return g.createRect({x:5,y:5,width:40,height:40}).setStroke({color:"#CECECE",width:1});
}});
});
