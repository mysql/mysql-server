//>>built
define("dojox/dgauges/components/grey/SemiCircularLinearGauge",["dojo/_base/lang","dojo/_base/declare","dojo/_base/Color","../utils","../../CircularGauge","../../LinearScaler","../../CircularScale","../../CircularValueIndicator","../../CircularRangeIndicator","../DefaultPropertiesMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _2("dojox.dgauges.components.grey.SemiCircularLinearGauge",[_5,_a],{borderColor:[148,152,161],fillColor:[148,152,161],indicatorColor:[63,63,63],constructor:function(_b,_c){
var _d=new _6({majorTickInterval:25,minorTickInterval:5});
this.addElement("background",_1.hitch(this,this.drawBackground));
var _e=new _7();
_e.set("scaler",_d);
_e.set("originX",118.80925);
_e.set("originY",172.98112);
_e.set("radius",160.62904);
_e.set("startAngle",-127.30061);
_e.set("endAngle",-53);
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
g.createPath({path:"M116.4449 0.014 C82.013 -0 48.0666 7.3389 14.6499 22.4862 C6.1025 25.6372 0 33.8658 0 43.5028 C0 49.8823 2.677 55.6345 6.9637 59.713 L99.9834 180.9853 C99.9859 180.9893 99.9914 180.9917 99.9939 180.9957 C103.9488 187.329 110.9778 191.5406 118.9895 191.5406 C126.8965 191.5406 133.8563 187.4321 137.8385 181.2366 C137.8426 181.2301 137.8448 181.222 137.849 181.2157 L231.3295 59.4197 C235.4368 55.3611 237.9789 49.7288 237.9789 43.5028 C237.9789 33.7053 231.6713 25.3703 222.8998 22.3395 C186.9226 7.6322 151.4291 0.0282 116.4449 0.014 Z"}).setFill(this.borderColor);
g.createPath({path:"M116.224 1.014 C82.301 1.0002 48.8563 8.2306 15.9334 23.1541 C7.5123 26.2585 1.5 34.3655 1.5 43.8601 C1.5 50.1453 4.1374 55.8125 8.3608 59.8307 L100.0058 179.3108 C100.0083 179.3148 100.0137 179.3171 100.0162 179.3211 C103.9126 185.5608 110.8377 189.7102 118.731 189.7102 C126.5212 189.7102 133.3781 185.6624 137.3015 179.5584 C137.3055 179.552 137.3077 179.5441 137.3118 179.5378 L229.4108 59.5418 C233.4574 55.5432 235.962 49.9941 235.962 43.8601 C235.962 34.2074 229.7476 25.9956 221.1057 23.0096 C185.6602 8.5196 150.6912 1.028 116.224 1.014 Z"}).setFill({type:"linear",x1:1.5,y1:1.01397,x2:1.5,y2:227.44943,colors:[{offset:0,color:this.fillColor},{offset:1,color:"white"}]});
g.createPath({path:"M117.848 3.0148 C83.3161 3.3066 42.2685 15.8301 16.4018 27.4452 C5.957 32.4174 9.5019 48.0401 18.3827 57.3539 C46.55 86.8947 80.5357 90.0379 118.4979 90.0379 C118.5432 90.0379 118.5868 90.0379 118.6321 90.0379 C118.7622 90.0379 118.8943 90.038 119.0241 90.0379 C119.0686 90.0379 119.1137 90.0379 119.1582 90.0379 C153.5091 90.0379 191.1062 86.8947 219.2735 57.3539 C228.1543 48.0401 231.6992 32.4174 221.2543 27.4452 C195.2041 15.7477 153.7583 3.1333 119.0757 3.0148 C119.0485 3.0148 119.0203 3.0148 118.9932 3.0148 C118.8843 3.0145 118.7717 3.0148 118.663 3.0148 C118.6351 3.0149 118.6084 3.0147 118.5805 3.0148 C118.3362 3.0156 118.093 3.0127 117.848 3.0148 Z"}).setFill({type:"linear",x1:10.0001,y1:3.01406,x2:10.0001,y2:150,colors:[{offset:0,color:"white"},{offset:1,color:this.fillColor}]});
},drawForeground:function(g){
g.createEllipse({cx:118.80001,cy:172.81399,rx:9.25,ry:9.25}).setFill({type:"radial",cx:118.80003,cy:169.11402,r:18.5,colors:[{offset:0,color:[149,149,149]},{offset:0.5,color:"black"},{offset:1,color:"black"}]}).setStroke({color:"black",width:0.1,style:"Solid",cap:"butt",join:4});
}});
});
