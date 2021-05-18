//>>built
define("dojox/dgauges/components/default/SemiCircularLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../utils","../../CircularGauge","../../LinearScaler","../../CircularScale","../../CircularValueIndicator","../../TextIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.dgauges.components.default.SemiCircularLinearGauge",[_5,_a],{_radius:88,_width:200,_height:123,borderColor:"#C9DFF2",fillColor:"#FCFCFF",indicatorColor:"#F01E28",constructor:function(){
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
this.addElement("indicatorTextBorder",_1.hitch(this,this.drawTextBorder),"leading");
var _e=new _9();
_e.set("indicator",_d);
_e.set("x",100);
_e.set("y",115);
this.addElement("indicatorText",_e);
_4.genericCircularGauge(_c,_d,this._width/2,0.76*this._height,this._radius,166,14,null,null,"inside");
},drawBackground:function(g){
var w=this._width;
var h=this._height;
var _f=0;
var cr=3;
var _10=_4.createGradient([0,_4.brightness(this.borderColor,-20),0.1,_4.brightness(this.borderColor,-40)]);
g.createRect({x:0,y:0,width:w,height:h,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_10)).setStroke({color:"#A5A5A5",width:0.2});
_10=_4.createGradient([0,_4.brightness(this.borderColor,70),1,_4.brightness(this.borderColor,-50)]);
_f=4;
cr=2;
g.createRect({x:_f,y:_f,width:w-2*_f,height:h-2*_f,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_10));
_f=6;
cr=1;
_10=_4.createGradient([0,_4.brightness(this.borderColor,60),1,_4.brightness(this.borderColor,-40)]);
g.createRect({x:_f,y:_f,width:w-2*_f,height:h-2*_f,r:cr}).setFill(_1.mixin({type:"linear",x1:0,y1:0,x2:w,y2:h},_10));
_f=7;
cr=0;
_10=_4.createGradient([0,_4.brightness(this.borderColor,70),1,_4.brightness(this.borderColor,-40)]);
g.createRect({x:_f,y:_f,width:w-2*_f,height:h-2*_f,r:cr}).setFill(_1.mixin({type:"linear",x1:w,y1:0,x2:0,y2:h},_10));
_f=5;
cr=0;
_10=_4.createGradient([0,[255,255,255,220],0.8,_4.brightness(this.fillColor,-5),1,_4.brightness(this.fillColor,-30)]);
g.createRect({x:_f,y:_f,width:w-2*_f,height:h-2*_f,r:cr}).setFill(_1.mixin({type:"radial",cx:w/2,cy:h/2,r:h},_10)).setStroke({color:_4.brightness(this.fillColor,-40),width:0.4});
},drawForeground:function(g){
var r=0.07*this._radius;
var _11=_4.createGradient([0,this.borderColor,1,_4.brightness(this.borderColor,-20)]);
g.createEllipse({cx:this._width/2,cy:0.76*this._height,rx:r,ry:r}).setFill(_1.mixin({type:"radial",cx:this._width/2-5,cy:this._height*0.76-5,r:r},_11)).setStroke({color:_4.brightness(this.fillColor,-50),width:0.4});
},drawTextBorder:function(g){
return g.createRect({x:this._width/2-12,y:this._height-20,width:24,height:14}).setStroke({color:_4.brightness(this.fillColor,-20),width:0.3});
}});
});
