//>>built
define("dojox/charting/themes/ThreeD",["dojox","dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","../Theme","./gradientGenerator","./PrimaryColors","dojo/colors","./common"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=["#f00","#0f0","#00f","#ff0","#0ff","#f0f","./common"],_a={type:"linear",space:"shape",x1:0,y1:0,x2:100,y2:0},_b=[{o:0,i:174},{o:0.08,i:231},{o:0.18,i:237},{o:0.3,i:231},{o:0.39,i:221},{o:0.49,i:206},{o:0.58,i:187},{o:0.68,i:165},{o:0.8,i:128},{o:0.9,i:102},{o:1,i:174}],_c=2,_d=100,_e=50,_f=_4.map(_9,function(c){
var _10=_3.delegate(_a),_9=_10.colors=_6.generateGradientByIntensity(c,_b),_11=_9[_c].color;
_11.r+=_d;
_11.g+=_d;
_11.b+=_d;
_11.sanitize();
return _10;
});
_8.ThreeD=_7.clone();
_8.ThreeD.series.shadow={dx:1,dy:1,width:3,color:[0,0,0,0.15]};
_8.ThreeD.next=function(_12,_13,_14){
if(_12=="bar"||_12=="column"){
var _15=this._current%this.seriesThemes.length,s=this.seriesThemes[_15],old=s.fill;
s.fill=_f[_15];
var _16=_5.prototype.next.apply(this,arguments);
s.fill=old;
return _16;
}
return _5.prototype.next.apply(this,arguments);
};
return _8.ThreeD;
});
