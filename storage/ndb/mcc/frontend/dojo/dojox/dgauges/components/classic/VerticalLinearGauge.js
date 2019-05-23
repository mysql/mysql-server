//>>built
define("dojox/dgauges/components/classic/VerticalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.dgauges.components.classic.VerticalLinearGauge",[_4,_8],{borderColor:[121,126,134],fillColor:[148,152,161],indicatorColor:"#FFFFFF",constructor:function(){
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
_a.set("paddingLeft",17);
_a.set("font",{family:"Helvetica",weight:"bold",size:"7pt"});
_a.set("tickShapeFunc",function(_b,_c,_d){
return _b.createCircle({r:_d.isMinor?0.5:2}).setFill("black");
});
this.addElement("scale",_a);
var _e=new _7();
_e.set("interactionArea","gauge");
_e.set("value",_9.minimum);
_e.set("paddingLeft",18);
_e.set("indicatorShapeFunc",_1.hitch(this,function(_f){
return _f.createPolyline([0,0,-10,-20,10,-20,0,0]).setFill(this.indicatorColor).setStroke({color:[121,126,134],width:1,style:"Solid",cap:"butt",join:20});
}));
_a.addIndicator("indicator",_e);
},drawBackground:function(g,w,h){
g.createRect({x:0,y:0,width:50,height:h,r:8}).setFill(this.borderColor);
g.createRect({x:2,y:2,width:46,height:h/2,r:6}).setFill({type:"linear",x1:0,y1:2,x2:0,y2:h/2,colors:[{offset:0,color:[235,235,235]},{offset:1,color:this.borderColor}]});
g.createRect({x:6,y:6,width:38,height:h-12,r:5}).setFill({type:"linear",x1:6,y1:0,x2:38,y2:0,colors:[{offset:0,color:this.fillColor},{offset:1,color:[220,220,220]}]});
g.createRect({x:7,y:7,width:36,height:h-14,r:3}).setFill({type:"linear",x1:7,y1:0,x2:36,y2:0,colors:[{offset:0,color:[220,220,220]},{offset:1,color:this.fillColor}]});
}});
});
