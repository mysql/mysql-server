//>>built
define("dojox/dgauges/components/black/HorizontalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.dgauges.components.black.HorizontalLinearGauge",[_4,_8],{borderColor:"#000000",fillColor:"#000000",indicatorColor:"#A4A4A4",constructor:function(){
this.borderColor=new _3(this.borderColor);
this.fillColor=new _3(this.fillColor);
this.indicatorColor=new _3(this.indicatorColor);
this.addElement("background",_1.hitch(this,this.drawBackground));
var _9=new _5();
var _a=new _6();
_a.set("scaler",_9);
_a.set("labelPosition","leading");
_a.set("paddingLeft",30);
_a.set("paddingRight",30);
_a.set("paddingTop",34);
_a.set("labelGap",8);
_a.set("font",{family:"Helvetica",weight:"bold",size:"7pt",color:"#CECECE"});
_a.set("tickShapeFunc",function(_b,_c,_d){
return _b.createCircle({r:_d.isMinor?0.5:3}).setFill("#CECECE");
});
this.addElement("scale",_a);
var _e=new _7();
_e.set("interactionArea","gauge");
_e.set("value",_9.minimum);
_e.set("paddingTop",30);
_e.set("indicatorShapeFunc",_1.hitch(this,function(_f,_10){
return _f.createPolyline([0,0,-10,-20,10,-20,0,0]).setFill(this.indicatorColor).setStroke({color:[70,70,70],width:1,style:"Solid",cap:"butt",join:20});
}));
_a.addIndicator("indicator",_e);
},drawBackground:function(g,w,h){
g.createRect({x:0,y:0,width:w,height:50,r:15}).setFill(this.borderColor);
g.createRect({x:4,y:4,width:w-8,height:42,r:12}).setFill({type:"linear",x1:0,y1:50,x2:0,y2:30,colors:[{offset:0,color:[100,100,100]},{offset:1,color:this.fillColor}]});
g.createPath().moveTo(4,25).vLineTo(14).smoothCurveTo(4,4,18,4).hLineTo(w-16).smoothCurveTo(w-4,4,w-4,16).closePath().setFill({type:"linear",x1:0,y1:0,x2:0,y2:20,colors:[{offset:0,color:[150,150,150]},{offset:1,color:this.fillColor}]});
g.createPath().moveTo(4,25).vLineTo(14).smoothCurveTo(4,4,18,4).hLineTo(w-16).smoothCurveTo(w-4,4,w-4,16).closePath().setFill([255,255,255,0.05]);
}});
});
