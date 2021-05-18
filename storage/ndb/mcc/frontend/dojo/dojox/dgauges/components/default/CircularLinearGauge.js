//>>built
define("dojox/dgauges/components/default/CircularLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../utils","../../CircularGauge","../../LinearScaler","../../CircularScale","../../CircularValueIndicator","../../TextIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.dgauges.components.default.CircularLinearGauge",[_5,_a],{_radius:100,borderColor:"#C9DFF2",fillColor:"#FCFCFF",indicatorColor:"#F01E28",constructor:function(){
this.borderColor=new _3(this.borderColor);
this.fillColor=new _3(this.fillColor);
this.indicatorColor=new _3(this.indicatorColor);
this.addElement("background",_1.hitch(this,this.drawBackground));
var _b=new _6();
var _c=new _7();
_c.set("scaler",_b);
this.addElement("scale",_c);
var _d=new _8();
_c.addIndicator("indicator",_d);
this.addElement("foreground",_1.hitch(this,this.drawForeground));
var _e=new _9();
_e.set("indicator",_d);
_e.set("x",100);
_e.set("y",150);
this.addElement("indicatorText",_e);
_4.genericCircularGauge(_c,_d,this._radius,this._radius,0.65*this._radius,130,50,null,null,"outside");
},drawBackground:function(g){
var r=this._radius;
var w=2*r;
var h=w;
var _f=_4.createGradient([0,_4.brightness(this.borderColor,70),1,_4.brightness(this.borderColor,-40)]);
g.createEllipse({cx:r,cy:r,rx:r,ry:r}).setFill(_1.mixin({type:"linear",x1:w,y1:0,x2:0,y2:h},_f)).setStroke({color:"#A5A5A5",width:0.2});
_f=_4.createGradient([0,_4.brightness(this.borderColor,70),1,_4.brightness(this.borderColor,-50)]);
g.createEllipse({cx:r,cy:r,rx:r*0.99,ry:r*0.99}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_f));
_f=_4.createGradient([0,_4.brightness(this.borderColor,60),1,_4.brightness(this.borderColor,-40)]);
g.createEllipse({cx:r,cy:r,rx:r*0.92,ry:r*0.92}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_f));
_f=_4.createGradient([0,_4.brightness(this.borderColor,70),1,_4.brightness(this.borderColor,-40)]);
g.createEllipse({cx:r,cy:r,rx:r*0.9,ry:r*0.9}).setFill(_1.mixin({type:"linear",x1:w,y1:0,x2:0,y2:h},_f));
_f=_4.createGradient([0,[255,255,255,220],0.8,_4.brightness(this.fillColor,-5),1,_4.brightness(this.fillColor,-30)]);
g.createEllipse({cx:r,cy:r,rx:r*0.9,ry:r*0.9}).setFill(_1.mixin({type:"radial",cx:r,cy:r,r:r},_f)).setStroke({color:_4.brightness(this.fillColor,-40),width:0.4});
},drawForeground:function(g){
var r=0.07*this._radius;
var _10=_4.createGradient([0,this.borderColor,1,_4.brightness(this.borderColor,-20)]);
g.createEllipse({cx:this._radius,cy:this._radius,rx:r,ry:r}).setFill(_1.mixin({type:"radial",cx:0.96*this._radius,cy:0.96*this._radius,r:r},_10)).setStroke({color:_4.brightness(this.fillColor,-50),width:0.4});
}});
});
