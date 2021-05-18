//>>built
define("dojox/dgauges/components/green/HorizontalLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../utils","../../RectangularGauge","../../LinearScaler","../../RectangularScale","../../RectangularValueIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.dgauges.components.green.HorizontalLinearGauge",[_5,_9],{borderColor:[50,50,50],fillColor:[109,183,19],indicatorColor:[0,0,0],constructor:function(){
this.borderColor=new _3(this.borderColor);
this.fillColor=new _3(this.fillColor);
this.indicatorColor=new _3(this.indicatorColor);
this.addElement("background",_1.hitch(this,this.drawBackground));
var _a=new _6();
var _b=new _7();
_b.set("scaler",_a);
_b.set("labelPosition","leading");
_b.set("paddingLeft",30);
_b.set("paddingRight",30);
_b.set("paddingTop",28);
_b.set("labelGap",2);
_b.set("font",{family:"Helvetica",weight:"bold",size:"7pt"});
this.addElement("scale",_b);
var _c=new _8();
_c.set("interactionArea","gauge");
_c.set("value",_a.minimum);
_c.set("paddingTop",32);
_c.set("indicatorShapeFunc",_1.hitch(this,function(_d,_e){
return _d.createPolyline([0,0,-10,-20,10,-20,0,0]).setFill(this.indicatorColor);
}));
_b.addIndicator("indicator",_c);
},drawBackground:function(g,w,h){
var _f=_4.brightness(new _3(this.fillColor),100);
g.createRect({x:0,y:0,width:w,height:50,r:10}).setFill(this.borderColor);
g.createRect({x:3,y:3,width:w-6,height:44,r:7}).setFill({type:"linear",x1:0,y1:2,x2:0,y2:30,colors:[{offset:0,color:_f},{offset:1,color:this.fillColor}]});
g.createRect({x:6,y:6,width:w-12,height:38,r:5}).setFill({type:"linear",x1:0,y1:6,x2:0,y2:38,colors:[{offset:0,color:[226,226,221]},{offset:0.5,color:[239,239,236]},{offset:1,color:"white"}]});
}});
});
