//>>built
define("dojox/dgauges/components/grey/CircularLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../utils","../../CircularGauge","../../LinearScaler","../../CircularScale","../../CircularValueIndicator","../../CircularRangeIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.dgauges.components.grey.CircularLinearGauge",[_5,_a],{borderColor:[148,152,161],fillColor:[148,152,161],indicatorColor:[63,63,63],constructor:function(_b,_c){
var _d=new _6();
this.addElement("background",_1.hitch(this,this.drawBackground));
var _e=new _7();
_e.set("scaler",_d);
_e.set("originX",73.4);
_e.set("originY",74.10297);
_e.set("radius",61.44239);
_e.set("startAngle",130.16044);
_e.set("endAngle",50.25444);
_e.set("orientation","clockwise");
_e.set("labelGap",2);
_e.set("font",{family:"Helvetica",weight:"bold",size:"8pt"});
this.addElement("scale",_e);
var _f=new _8();
_f.set("interactionArea","gauge");
_f.set("value",_d.minimum);
_f.set("indicatorShapeFunc",_1.hitch(this,function(_10,_11){
var l=_11.scale.radius-2;
_10.createPath().moveTo(0,0).lineTo(0,-5).lineTo(l,0).lineTo(0,0).closePath().setFill(this.indicatorColor);
var _12=_4.brightness(new _3(this.indicatorColor),70);
_10.createPath().moveTo(0,0).lineTo(0,5).lineTo(l,0).lineTo(0,0).closePath().setFill(_12);
return _10;
}));
_e.addIndicator("indicator",_f);
this.addElement("foreground",_1.hitch(this,this.drawForeground));
},drawBackground:function(g){
g.createEllipse({cx:73.5,cy:73.75,rx:73.5,ry:73.75}).setFill(this.borderColor);
g.createEllipse({cx:73.5,cy:73.75,rx:71.5,ry:71.75}).setFill({type:"linear",x1:2,y1:2,x2:2,y2:174.2,colors:[{offset:0,color:this.fillColor},{offset:1,color:"white"}]});
g.createPath({path:"M71.7134 2.3627 C35.3338 3.0547 6.0025 32.818 6.0025 69.3621 C6.0025 69.7225 5.9968 70.0836 6.0025 70.4427 C26.4442 78.2239 50.1913 82.6622 75.4956 82.6622 C98.7484 82.6622 120.6538 78.8779 139.918 72.2299 C139.9587 71.2717 140.0011 70.3303 140.0011 69.3621 C140.0011 32.3847 109.9791 2.3627 73.0018 2.3627 C72.5685 2.3627 72.1447 2.3545 71.7133 2.3627 Z"}).setFill({type:"linear",x1:6,y1:2.3591,x2:6,y2:150,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
},drawForeground:function(g){
g.createEllipse({cx:73.3,cy:73.8,rx:9.25,ry:9.25}).setFill({type:"radial",cx:73.30003,cy:70.10003,r:18.5,colors:[{offset:0,color:[149,149,149]},{offset:0.5,color:"black"},{offset:1,color:"black"}]}).setStroke({color:"black",width:0.1,style:"Solid",cap:"butt",join:4});
}});
});
