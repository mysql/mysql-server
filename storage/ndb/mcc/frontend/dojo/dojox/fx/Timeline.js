//>>built
define("dojox/fx/Timeline",["dojo/_base/lang","dojo/fx/easing","dojo/_base/fx","dojo/dom","./_base","dojo/_base/connect","dojo/_base/html","dojo/_base/array","dojo/_base/Color"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
_5.animateTimeline=function(_a,_b){
var _c=new _d(_a.keys);
var _e=_3.animateProperty({node:_4.byId(_b||_a.node),duration:_a.duration||1000,properties:_c._properties,easing:_2.linear,onAnimate:function(v){
}});
_6.connect(_e,"onEnd",function(_f){
var sty=_e.curve.getValue(_e.reversed?0:1);
_7.style(_f,sty);
});
_6.connect(_e,"beforeBegin",function(){
if(_e.curve){
delete _e.curve;
}
_e.curve=_c;
_c.ani=_e;
});
return _e;
};
var _d=function(_10){
this.keys=_1.isArray(_10)?this.flatten(_10):_10;
};
_d.prototype.flatten=function(_11){
var _12=function(str,idx){
if(str=="from"){
return 0;
}
if(str=="to"){
return 1;
}
if(str===undefined){
return idx==0?0:idx/(_11.length-1);
}
return parseInt(str,10)*0.01;
};
var p={},o={};
_8.forEach(_11,function(k,i){
var _13=_12(k.step,i);
var _14=_2[k.ease]||_2.linear;
for(var nm in k){
if(nm=="step"||nm=="ease"||nm=="from"||nm=="to"){
continue;
}
if(!o[nm]){
o[nm]={steps:[],values:[],eases:[],ease:_14};
p[nm]={};
if(!/#/.test(k[nm])){
p[nm].units=o[nm].units=/\D{1,}/.exec(k[nm]).join("");
}else{
p[nm].units=o[nm].units="isColor";
}
}
o[nm].eases.push(_2[k.ease||"linear"]);
o[nm].steps.push(_13);
if(p[nm].units=="isColor"){
o[nm].values.push(new _9(k[nm]));
}else{
o[nm].values.push(parseInt(/\d{1,}/.exec(k[nm]).join("")));
}
if(p[nm].start===undefined){
p[nm].start=o[nm].values[o[nm].values.length-1];
}else{
p[nm].end=o[nm].values[o[nm].values.length-1];
}
}
});
this._properties=p;
return o;
};
_d.prototype.getValue=function(p){
p=this.ani._reversed?1-p:p;
var o={},_15=this;
var _16=function(nm,i){
return _15._properties[nm].units!="isColor"?_15.keys[nm].values[i]+_15._properties[nm].units:_15.keys[nm].values[i].toCss();
};
for(var nm in this.keys){
var k=this.keys[nm];
for(var i=0;i<k.steps.length;i++){
var _17=k.steps[i];
var ns=k.steps[i+1];
var _18=i<k.steps.length?true:false;
var _19=k.eases[i]||function(n){
return n;
};
if(p==_17){
o[nm]=_16(nm,i);
if(!_18||(_18&&this.ani._reversed)){
break;
}
}else{
if(p>_17){
if(_18&&p<k.steps[i+1]){
var end=k.values[i+1];
var beg=k.values[i];
var seg=(1/(ns-_17))*(p-_17);
seg=_19(seg);
if(beg instanceof _9){
o[nm]=_9.blendColors(beg,end,seg).toCss(false);
}else{
var df=end-beg;
o[nm]=beg+seg*df+this._properties[nm].units;
}
break;
}else{
o[nm]=_16(nm,i);
}
}else{
if((_18&&!this.ani._reversed)||(!_18&&this.ani._reversed)){
o[nm]=_16(nm,i);
}
}
}
}
}
return o;
};
_5._Timeline=_d;
return _5;
});
