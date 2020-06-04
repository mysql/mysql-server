//>>built
define("dojox/charting/themes/ThreeD",["dojo/_base/lang","dojo/_base/array","../Theme","./gradientGenerator","./PrimaryColors","dojo/colors","./common"],function(_1,_2,_3,_4,_5,_6){
var _7=["#f00","#0f0","#00f","#ff0","#0ff","#f0f","./common"],_8={type:"linear",space:"shape",x1:0,y1:0,x2:100,y2:0},_9=[{o:0,i:174},{o:0.08,i:231},{o:0.18,i:237},{o:0.3,i:231},{o:0.39,i:221},{o:0.49,i:206},{o:0.58,i:187},{o:0.68,i:165},{o:0.8,i:128},{o:0.9,i:102},{o:1,i:174}],_a=2,_b=100,_c=_2.map(_7,function(c){
var _d=_1.delegate(_8),_7=_d.colors=_4.generateGradientByIntensity(c,_9),_e=_7[_a].color;
_e.r+=_b;
_e.g+=_b;
_e.b+=_b;
_e.sanitize();
return _d;
});
_6.ThreeD=_5.clone();
_6.ThreeD.series.shadow={dx:1,dy:1,width:3,color:[0,0,0,0.15]};
_6.ThreeD.next=function(_f,_10,_11){
if(_f=="bar"||_f=="column"){
var _12=this._current%this.seriesThemes.length,s=this.seriesThemes[_12],old=s.fill;
s.fill=_c[_12];
var _13=_3.prototype.next.apply(this,arguments);
s.fill=old;
return _13;
}
return _3.prototype.next.apply(this,arguments);
};
return _6.ThreeD;
});
