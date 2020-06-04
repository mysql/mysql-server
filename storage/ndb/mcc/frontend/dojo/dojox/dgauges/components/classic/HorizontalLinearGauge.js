//>>built
define("dojox/dgauges/components/classic/HorizontalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.dgauges.components.classic.HorizontalLinearGauge",[_4,_8],{borderColor:[121,126,134],fillColor:[148,152,161],indicatorColor:"#FFFFFF",constructor:function(){
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
_a.set("paddingTop",32);
_a.set("labelGap",8);
_a.set("font",{family:"Helvetica",weight:"bold",size:"7pt"});
_a.set("tickShapeFunc",function(_b,_c,_d){
return _b.createCircle({r:_d.isMinor?0.5:2}).setFill("black");
});
this.addElement("scale",_a);
var _e=new _7();
_e.set("interactionArea","gauge");
_e.set("value",_9.minimum);
_e.set("paddingTop",30);
_e.set("indicatorShapeFunc",_1.hitch(this,function(_f,_10){
return _f.createPolyline([0,0,-10,-20,10,-20,0,0]).setFill(this.indicatorColor).setStroke({color:[121,126,134],width:1,style:"Solid",cap:"butt",join:20});
}));
_a.addIndicator("indicator",_e);
},drawBackground:function(g,w,h){
g.createRect({x:0,y:0,width:w,height:50,r:8}).setFill(this.borderColor);
g.createRect({x:2,y:2,width:w-4,height:32,r:6}).setFill({type:"linear",x1:0,y1:2,x2:0,y2:15,colors:[{offset:0,color:[235,235,235]},{offset:1,color:this.borderColor}]});
g.createRect({x:6,y:6,width:w-12,height:38,r:5}).setFill({type:"linear",x1:0,y1:6,x2:0,y2:38,colors:[{offset:0,color:[220,220,220]},{offset:1,color:this.fillColor}]});
g.createRect({x:7,y:7,width:w-14,height:36,r:3}).setFill({type:"linear",x1:0,y1:7,x2:0,y2:36,colors:[{offset:0,color:this.fillColor},{offset:1,color:[220,220,220]}]});
}});
});
