//>>built
define("dojox/fx/ext-dojo/reverse",["dojo/_base/fx","dojo/fx","dojo/_base/lang","dojo/fx/easing","dojox/fx"],function(_1,_2,_3,_4,_5){
var _6={_reversed:false,reverse:function(_7,_8){
var _9=this.status()=="playing";
this.pause();
this._reversed=!this._reversed;
var d=this.duration,_a=d*this._percent,_b=d-_a,_c=new Date().valueOf(),cp=this.curve._properties,p=this.properties,nm;
this._endTime=_c+_a;
this._startTime=_c-_b;
if(_9){
this.gotoPercent(_b/d);
}
for(nm in p){
var _d=p[nm].start;
p[nm].start=cp[nm].start=p[nm].end;
p[nm].end=cp[nm].end=_d;
}
if(this._reversed){
if(!this.rEase){
this.fEase=this.easing;
if(_8){
this.rEase=_8;
}else{
var de=_4,_e,_f;
for(nm in de){
if(this.easing==de[nm]){
_e=nm;
break;
}
}
if(_e){
if(/InOut/.test(nm)||!/In|Out/i.test(nm)){
this.rEase=this.easing;
}else{
if(/In/.test(nm)){
_f=nm.replace("In","Out");
}else{
_f=nm.replace("Out","In");
}
}
if(_f){
this.rEase=_4[_f];
}
}else{
this.rEase=this.easing;
}
}
}
this.easing=this.rEase;
}else{
this.easing=this.fEase;
}
if(!_7&&this.status()!="playing"){
this.play();
}
return this;
}};
_3.extend(_1.Animation,_6);
return _1.Animation;
});
