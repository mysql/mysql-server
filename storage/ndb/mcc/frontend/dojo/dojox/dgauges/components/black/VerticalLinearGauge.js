//>>built
define("dojox/dgauges/components/black/VerticalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.dgauges.components.black.VerticalLinearGauge",[_4,_8],{borderColor:"#000000",fillColor:"#000000",indicatorColor:"#A4A4A4",constructor:function(){
this.orientation="vertical";
this.borderColor=new _3(this.borderColor);
this.fillColor=new _3(this.fillColor);
this.indicatorColor=new _3(this.indicatorColor);
this.addElement("background",_1.hitch(this,this.drawBackground));
var _9=new _5();
var _a=new _6();
_a.set("scaler",_9);
_a.set("labelPosition","trailing");
_a.set("paddingTop",30);
_a.set("paddingBottom",30);
_a.set("paddingLeft",15);
_a.set("font",{family:"Helvetica",weight:"bold",size:"7pt",color:"#CECECE"});
_a.set("tickShapeFunc",function(_b,_c,_d){
return _b.createCircle({r:_d.isMinor?0.5:3}).setFill("#CECECE");
});
this.addElement("scale",_a);
var _e=new _7();
_e.set("interactionArea","gauge");
_e.set("value",_9.minimum);
_e.set("paddingLeft",18);
_e.set("indicatorShapeFunc",_1.hitch(this,function(_f){
return _f.createPolyline([0,0,-10,-20,10,-20,0,0]).setFill(this.indicatorColor).setStroke({color:[69,69,69],width:1,style:"Solid",cap:"butt",join:20});
}));
_a.addIndicator("indicator",_e);
},drawBackground:function(g,w,h){
g.createRect({x:0,y:0,width:50,height:h,r:15}).setFill(this.borderColor);
g.createRect({x:4,y:4,width:42,height:h-8,r:12}).setFill({type:"linear",x1:5,y1:0,x2:20,y2:0,colors:[{offset:0,color:[100,100,100]},{offset:1,color:this.fillColor}]});
g.createPath().moveTo(25,4).hLineTo(36).smoothCurveTo(46,4,46,18).vLineTo(h-20).smoothCurveTo(46,h-4,36,h-4).closePath().setFill({type:"linear",x1:70,y1:0,x2:25,y2:0,colors:[{offset:0,color:[150,150,150]},{offset:1,color:this.fillColor}]});
g.createPath().moveTo(25,4).hLineTo(36).smoothCurveTo(46,4,46,18).vLineTo(h-20).smoothCurveTo(46,h-4,36,h-4).closePath().setFill([255,255,255,0.05]);
}});
});
