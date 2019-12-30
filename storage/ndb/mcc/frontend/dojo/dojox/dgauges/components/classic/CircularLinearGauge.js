//>>built
define("dojox/dgauges/components/classic/CircularLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../CircularGauge","../../LinearScaler","../../CircularScale","../../CircularValueIndicator","../../CircularRangeIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.dgauges.components.classic.CircularLinearGauge",[_4,_9],{borderColor:[121,126,134],fillColor:[148,152,161],indicatorColor:"#FFFFFF",constructor:function(){
this.borderColor=new _3(this.borderColor);
this.fillColor=new _3(this.fillColor);
this.indicatorColor=new _3(this.indicatorColor);
var _a=new _5();
this.addElement("background",_1.hitch(this,this.drawBackground));
var _b=new _6();
_b.set("scaler",_a);
_b.set("originX",81.94991);
_b.set("originY",87.99015);
_b.set("radius",66.34219);
_b.set("startAngle",115.9);
_b.set("endAngle",61.6);
_b.set("orientation","clockwise");
_b.set("labelGap",2);
_b.set("font",{family:"Helvetica",weight:"bold",size:"6pt"});
this.addElement("scale",_b);
var _c=new _7();
_c.set("interactionArea","gauge");
_c.set("value",_a.minimum);
_c.set("indicatorShapeFunc",_1.hitch(this,function(_d,_e){
var l=_e.scale.radius-2;
return _d.createPath().moveTo(0,0).smoothCurveTo(l/2,-10,l,0).lineTo(l,0).smoothCurveTo(l/2,10,0,0).closePath().setStroke({color:this.borderColor,width:1,join:10}).setFill({type:"linear",x1:0,y1:0,x2:l,y2:0,colors:[{offset:0,color:[208,208,208]},{offset:1,color:this.indicatorColor}]});
}));
_b.addIndicator("indicator",_c);
this.addElement("foreground",_1.hitch(this,this.drawForeground));
},drawBackground:function(g){
g.createPath({path:"M81.9213 6.4012 C36.7458 6.4012 0 43.1469 0 88.3225 C0 133.498 36.7458 170.2438 81.9213 170.2438 C127.0968 170.2438 163.8426 133.498 163.8425 88.3225 C163.8425 43.147 127.0968 6.4012 81.9213 6.4012 ZM81.9213 14.6771 C122.6195 14.6771 155.5666 47.6241 155.5666 88.3225 C155.5667 129.0207 122.6195 161.9678 81.9213 161.9678 C41.223 161.9678 8.2759 129.0207 8.2759 88.3225 C8.2759 47.6242 41.223 14.6771 81.9213 14.6771 Z"}).setFill(this.borderColor);
g.createPath({path:"M131.7007 23.859 C123.1609 16.836 112.7669 11.9131 100.5479 9.0902 C61.2014 0 20.5795 20.6702 9.8522 55.1976 C9.3592 56.9339 12.7501 58.0358 13.6957 55.5238 C24.6274 24.4073 64.5764 6.1932 100.6316 14.523 C118.2575 18.5951 131.7906 27.3347 141.2184 40.7415 C143.0075 43.6629 146.9334 42.1265 145.2492 39.0652 C141.4153 33.222 136.9106 28.1434 131.7007 23.859 Z"}).setFill({type:"linear",x1:9.8035,y1:6.94738,x2:9.8035,y2:31.97231,colors:[{offset:0,color:[235,235,235]},{offset:1,color:this.borderColor}]});
g.createPath({path:"M128.7453 148.5681 C120.6736 155.591 110.8493 160.5139 99.3 163.3368 C62.1102 172.427 23.715 151.7568 13.5757 117.2294 C13.1097 115.4931 16.3147 114.3912 17.2085 116.9032 C27.541 148.0197 65.3002 166.2338 99.3792 157.904 C116.0389 153.8319 128.8303 145.0923 137.7413 131.6855 C139.4323 128.7641 143.143 130.3005 141.5511 133.3618 C137.9274 139.205 133.6696 144.2836 128.7453 148.5681 Z"}).setFill({type:"linear",x1:13.52963,y1:165.47966,x2:13.52963,y2:140.45474,colors:[{offset:0,color:[235,235,235]},{offset:1,color:this.borderColor}]});
g.createPath({path:"M155.481 88.3136 C155.481 129.2951 122.5479 162.5169 81.9228 162.5169 C41.2978 162.5169 8.3647 129.2951 8.3647 88.3136 C8.3647 47.3323 41.2978 14.1102 81.9228 14.1102 C122.5479 14.1102 155.481 47.3323 155.481 88.3136 Z"}).setFill({type:"linear",x1:8.3647,y1:14.11022,x2:155.48103,y2:162.51695,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
g.createPath({path:"M81.9229 13.2351 C40.8398 13.2351 7.4925 46.8859 7.4925 88.3295 C7.4925 129.7729 40.8398 163.3921 81.9229 163.3921 C123.006 163.3921 156.3532 129.7729 156.3532 88.3295 C156.3532 46.8859 123.006 13.2351 81.9229 13.2351 ZM81.9229 14.7211 C122.1911 14.7211 154.8672 47.708 154.8672 88.3295 C154.8672 128.951 122.1911 161.906 81.9229 161.906 C41.6546 161.906 8.9786 128.951 8.9786 88.3295 C8.9786 47.708 41.6546 14.7211 81.9229 14.7211 Z"}).setFill({type:"linear",x1:7.4925,y1:13.23515,x2:7.4925,y2:163.39214,colors:[{offset:0,color:"white"},{offset:1,color:[148,152,161]}]});
},drawForeground:function(g){
g.createEllipse({cx:81.85091,cy:87.72405,rx:9.25,ry:9.25}).setFill({type:"radial",cx:81.85093,cy:84.02408,r:18.5,colors:[{offset:0,color:[149,149,149]},{offset:0.5,color:"black"},{offset:1,color:"black"}]}).setStroke({color:"black",width:0.1,style:"Solid",cap:"butt",join:4});
}});
});
