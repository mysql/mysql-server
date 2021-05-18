//>>built
define("dojox/dgauges/components/grey/HorizontalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.dgauges.components.grey.HorizontalLinearGauge",[_4,_8],{borderColor:[148,152,161],fillColor:[148,152,161],indicatorColor:[63,63,63],constructor:function(){
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
_a.set("paddingTop",28);
_a.set("labelGap",8);
_a.set("font",{family:"Helvetica",weight:"bold",size:"7pt"});
this.addElement("scale",_a);
var _b=new _7();
_b.set("interactionArea","gauge");
_b.set("value",_9.minimum);
_b.set("paddingTop",32);
_b.set("indicatorShapeFunc",_1.hitch(this,function(_c,_d){
return _c.createPolyline([0,0,-10,-20,10,-20,0,0]).setFill(this.indicatorColor).setStroke({color:[70,70,70],width:1,style:"Solid",cap:"butt",join:20});
}));
_a.addIndicator("indicator",_b);
},drawBackground:function(g,w,h){
g.createRect({x:0,y:0,width:w,height:50,r:13.5}).setFill(this.borderColor);
g.createRect({x:2,y:2,width:w-4,height:46,r:11.5}).setFill({type:"linear",x1:0,y1:-2,x2:0,y2:60,colors:[{offset:0,color:this.fillColor},{offset:1,color:"white"}]});
g.createPath().moveTo(2,25).vLineTo(12).smoothCurveTo(2,2,16,2).hLineTo(w-12).smoothCurveTo(w-2,2,w-2,16).closePath().setFill({type:"linear",x1:0,y1:-5,x2:0,y2:40,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
}});
});
