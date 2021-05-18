//>>built
define("dojox/dgauges/components/grey/VerticalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.dgauges.components.grey.VerticalLinearGauge",[_4,_8],{borderColor:[148,152,161],fillColor:[148,152,161],indicatorColor:[63,63,63],constructor:function(){
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
_a.set("labelGap",4);
_a.set("font",{family:"Helvetica",weight:"bold",size:"7pt"});
this.addElement("scale",_a);
var _b=new _7();
_b.set("interactionArea","gauge");
_b.set("value",_9.minimum);
_b.set("paddingLeft",22);
_b.set("indicatorShapeFunc",_1.hitch(this,function(_c){
_c.createPath().moveTo(0,0).lineTo(-10,-20).lineTo(10,-20).lineTo(0,0).closePath().setFill(this.indicatorColor);
return _c;
}));
_a.addIndicator("indicator",_b);
},drawBackground:function(g,w,h){
g.createRect({x:0,y:0,width:50,height:h,r:13.5}).setFill(this.borderColor);
g.createRect({x:2,y:2,width:46,height:h-4,r:11.5}).setFill({type:"linear",x1:-2,y1:0,x2:60,y2:0,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
g.createPath().moveTo(25,2).hLineTo(38).smoothCurveTo(48,2,48,18).vLineTo(h-16).smoothCurveTo(48,h-2,38,h-2).closePath().setFill({type:"linear",x1:-10,y1:0,x2:60,y2:0,colors:[{offset:0,color:this.fillColor},{offset:1,color:"white"}]});
}});
});
