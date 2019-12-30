//>>built
define("dojox/dgauges/components/classic/SemiCircularLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../../CircularGauge","../../LinearScaler","../../CircularScale","../../CircularValueIndicator","../../CircularRangeIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.dgauges.components.classic.SemiCircularLinearGauge",[_4,_9],{borderColor:[121,126,134],fillColor:[148,152,161],indicatorColor:"#FFFFFF",constructor:function(){
this.borderColor=new _3(this.borderColor);
this.fillColor=new _3(this.fillColor);
this.indicatorColor=new _3(this.indicatorColor);
var _a=new _5();
this.addElement("background",_1.hitch(this,this.drawBackground));
var _b=new _6();
_b.set("scaler",_a);
_b.set("originX",82.5469);
_b.set("originY",86.56135);
_b.set("radius",68.2182);
_b.set("startAngle",-179.9);
_b.set("endAngle",-0.1);
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
g.createPath({path:"M82.4061 4.2361 C36.9308 4.2361 0 41.1357 0 86.6111 C0 92.8402 0.719 98.9014 2.0311 104.7361 L162.75 104.7361 C164.0621 98.9014 164.7812 92.8402 164.7812 86.6111 C164.7812 41.1357 127.8816 4.2361 82.4061 4.2361 Z"}).setFill([121,126,134,1]);
g.createPath({path:"M132.1809 21.8379 C109.1471 3.3761 75.6533 -0 48.8591 11.9106 C31.1385 19.6122 16.0526 34.4656 10.3325 53.1765 C13.128 58.4642 15.6567 49.646 16.9008 47.1352 C27.4881 26.5761 49.7822 14.215 72.1729 11.227 C95.4142 8.1002 121.0126 14.95 136.9788 32.7778 C139.4356 34.8999 143.5455 43.9915 146.1132 38.1149 C142.8197 31.7999 137.5776 26.4561 132.1809 21.8379 Z"}).setFill({type:"linear",x1:10.3325,y1:4.93862,x2:10.3325,y2:29.8873,colors:[{offset:0,color:[235,235,235]},{offset:1,color:this.borderColor}]});
g.createPath({path:"M82.2469 11.4202 C54.147 11.0778 26.9953 28.2183 14.7503 53.4303 C7.835 67.2286 5.3186 83.1515 7.5166 98.4202 C57.3367 98.4202 107.1569 98.4202 156.9772 98.4202 C161.2359 70.7787 148.7651 41.4452 125.8445 25.4041 C113.2386 16.3396 97.7752 11.3718 82.2469 11.4202 Z"}).setFill({type:"linear",x1:6.74398,y1:11.41516,x2:6.74398,y2:98.4202,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
g.createPath({path:"M82.2469 12.4202 C53.2167 12.031 25.1734 30.4906 13.8879 57.2103 C8.3968 69.7668 6.5444 83.863 8.5064 97.4202 C57.6668 97.4202 106.827 97.4202 155.9873 97.4202 C160.2416 70.0807 147.542 41.1134 124.6241 25.6326 C112.2794 17.0413 97.2852 12.377 82.2469 12.4202 L82.2469 12.4202 Z"}).setFill({type:"linear",x1:7.74645,y1:12.41416,x2:156.8018,y2:97.4202,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
},drawForeground:function(g){
g.createEllipse({cx:82.2187,cy:86.0486,rx:9.25,ry:9.25}).setFill({type:"radial",cx:82.21872,cy:82.34862,r:18.5,colors:[{offset:0,color:[149,149,149]},{offset:0.5,color:"black"},{offset:1,color:"black"}]}).setStroke({color:"black",width:0.1,style:"Solid",cap:"butt",join:4});
}});
});
