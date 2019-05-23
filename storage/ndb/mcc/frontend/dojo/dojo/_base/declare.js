/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/declare",["./kernel","../has","./lang"],function(_1,_2,_3){
var _4=_3.mixin,op=Object.prototype,_5=op.toString,_6=new Function,_7=0,_8="constructor";
function _9(_a,_b){
throw new Error("declare"+(_b?" "+_b:"")+": "+_a);
};
function _c(_d,_e){
var _f=[],_10=[{cls:0,refs:[]}],_11={},_12=1,l=_d.length,i=0,j,lin,_13,top,_14,rec,_15,_16;
for(;i<l;++i){
_13=_d[i];
if(!_13){
_9("mixin #"+i+" is unknown. Did you use dojo.require to pull it in?",_e);
}else{
if(_5.call(_13)!="[object Function]"){
_9("mixin #"+i+" is not a callable constructor.",_e);
}
}
lin=_13._meta?_13._meta.bases:[_13];
top=0;
for(j=lin.length-1;j>=0;--j){
_14=lin[j].prototype;
if(!_14.hasOwnProperty("declaredClass")){
_14.declaredClass="uniqName_"+(_7++);
}
_15=_14.declaredClass;
if(!_11.hasOwnProperty(_15)){
_11[_15]={count:0,refs:[],cls:lin[j]};
++_12;
}
rec=_11[_15];
if(top&&top!==rec){
rec.refs.push(top);
++top.count;
}
top=rec;
}
++top.count;
_10[0].refs.push(top);
}
while(_10.length){
top=_10.pop();
_f.push(top.cls);
--_12;
while(_16=top.refs,_16.length==1){
top=_16[0];
if(!top||--top.count){
top=0;
break;
}
_f.push(top.cls);
--_12;
}
if(top){
for(i=0,l=_16.length;i<l;++i){
top=_16[i];
if(!--top.count){
_10.push(top);
}
}
}
}
if(_12){
_9("can't build consistent linearization",_e);
}
_13=_d[0];
_f[0]=_13?_13._meta&&_13===_f[_f.length-_13._meta.bases.length]?_13._meta.bases.length:1:0;
return _f;
};
function _17(_18,a,f){
var _19,_1a,_1b,_1c,_1d,_1e,_1f,opf,pos,_20=this._inherited=this._inherited||{};
if(typeof _18=="string"){
_19=_18;
_18=a;
a=f;
}
f=0;
_1c=_18.callee;
_19=_19||_1c.nom;
if(!_19){
_9("can't deduce a name to call inherited()",this.declaredClass);
}
_1d=this.constructor._meta;
_1b=_1d.bases;
pos=_20.p;
if(_19!=_8){
if(_20.c!==_1c){
pos=0;
_1e=_1b[0];
_1d=_1e._meta;
if(_1d.hidden[_19]!==_1c){
_1a=_1d.chains;
if(_1a&&typeof _1a[_19]=="string"){
_9("calling chained method with inherited: "+_19,this.declaredClass);
}
do{
_1d=_1e._meta;
_1f=_1e.prototype;
if(_1d&&(_1f[_19]===_1c&&_1f.hasOwnProperty(_19)||_1d.hidden[_19]===_1c)){
break;
}
}while(_1e=_1b[++pos]);
pos=_1e?pos:-1;
}
}
_1e=_1b[++pos];
if(_1e){
_1f=_1e.prototype;
if(_1e._meta&&_1f.hasOwnProperty(_19)){
f=_1f[_19];
}else{
opf=op[_19];
do{
_1f=_1e.prototype;
f=_1f[_19];
if(f&&(_1e._meta?_1f.hasOwnProperty(_19):f!==opf)){
break;
}
}while(_1e=_1b[++pos]);
}
}
f=_1e&&f||op[_19];
}else{
if(_20.c!==_1c){
pos=0;
_1d=_1b[0]._meta;
if(_1d&&_1d.ctor!==_1c){
_1a=_1d.chains;
if(!_1a||_1a.constructor!=="manual"){
_9("calling chained constructor with inherited",this.declaredClass);
}
while(_1e=_1b[++pos]){
_1d=_1e._meta;
if(_1d&&_1d.ctor===_1c){
break;
}
}
pos=_1e?pos:-1;
}
}
while(_1e=_1b[++pos]){
_1d=_1e._meta;
f=_1d?_1d.ctor:_1e;
if(f){
break;
}
}
f=_1e&&f;
}
_20.c=f;
_20.p=pos;
if(f){
return a===true?f:f.apply(this,a||_18);
}
};
function _21(_22,_23){
if(typeof _22=="string"){
return this.__inherited(_22,_23,true);
}
return this.__inherited(_22,true);
};
function _24(_25,a1,a2){
var f=this.getInherited(_25,a1);
if(f){
return f.apply(this,a2||a1||_25);
}
};
var _26=_1.config.isDebug?_24:_17;
function _27(cls){
var _28=this.constructor._meta.bases;
for(var i=0,l=_28.length;i<l;++i){
if(_28[i]===cls){
return true;
}
}
return this instanceof cls;
};
function _29(_2a,_2b){
for(var _2c in _2b){
if(_2c!=_8&&_2b.hasOwnProperty(_2c)){
_2a[_2c]=_2b[_2c];
}
}
if(_2("bug-for-in-skips-shadowed")){
for(var _2d=_3._extraNames,i=_2d.length;i;){
_2c=_2d[--i];
if(_2c!=_8&&_2b.hasOwnProperty(_2c)){
_2a[_2c]=_2b[_2c];
}
}
}
};
function _2e(_2f,_30){
var _31,t;
for(_31 in _30){
t=_30[_31];
if((t!==op[_31]||!(_31 in op))&&_31!=_8){
if(_5.call(t)=="[object Function]"){
t.nom=_31;
}
_2f[_31]=t;
}
}
if(_2("bug-for-in-skips-shadowed")){
for(var _32=_3._extraNames,i=_32.length;i;){
_31=_32[--i];
t=_30[_31];
if((t!==op[_31]||!(_31 in op))&&_31!=_8){
if(_5.call(t)=="[object Function]"){
t.nom=_31;
}
_2f[_31]=t;
}
}
}
return _2f;
};
function _33(_34){
_35.safeMixin(this.prototype,_34);
return this;
};
function _36(_37){
return _35([this].concat(_37));
};
function _38(_39,_3a){
return function(){
var a=arguments,_3b=a,a0=a[0],f,i,m,l=_39.length,_3c;
if(!(this instanceof a.callee)){
return _3d(a);
}
if(_3a&&(a0&&a0.preamble||this.preamble)){
_3c=new Array(_39.length);
_3c[0]=a;
for(i=0;;){
a0=a[0];
if(a0){
f=a0.preamble;
if(f){
a=f.apply(this,a)||a;
}
}
f=_39[i].prototype;
f=f.hasOwnProperty("preamble")&&f.preamble;
if(f){
a=f.apply(this,a)||a;
}
if(++i==l){
break;
}
_3c[i]=a;
}
}
for(i=l-1;i>=0;--i){
f=_39[i];
m=f._meta;
f=m?m.ctor:f;
if(f){
f.apply(this,_3c?_3c[i]:a);
}
}
f=this.postscript;
if(f){
f.apply(this,_3b);
}
};
};
function _3e(_3f,_40){
return function(){
var a=arguments,t=a,a0=a[0],f;
if(!(this instanceof a.callee)){
return _3d(a);
}
if(_40){
if(a0){
f=a0.preamble;
if(f){
t=f.apply(this,t)||t;
}
}
f=this.preamble;
if(f){
f.apply(this,t);
}
}
if(_3f){
_3f.apply(this,a);
}
f=this.postscript;
if(f){
f.apply(this,a);
}
};
};
function _41(_42){
return function(){
var a=arguments,i=0,f,m;
if(!(this instanceof a.callee)){
return _3d(a);
}
for(;f=_42[i];++i){
m=f._meta;
f=m?m.ctor:f;
if(f){
f.apply(this,a);
break;
}
}
f=this.postscript;
if(f){
f.apply(this,a);
}
};
};
function _43(_44,_45,_46){
return function(){
var b,m,f,i=0,_47=1;
if(_46){
i=_45.length-1;
_47=-1;
}
for(;b=_45[i];i+=_47){
m=b._meta;
f=(m?m.hidden:b.prototype)[_44];
if(f){
f.apply(this,arguments);
}
}
};
};
function _48(_49){
_6.prototype=_49.prototype;
var t=new _6;
_6.prototype=null;
return t;
};
function _3d(_4a){
var _4b=_4a.callee,t=_48(_4b);
_4b.apply(t,_4a);
return t;
};
function _35(_4c,_4d,_4e){
if(typeof _4c!="string"){
_4e=_4d;
_4d=_4c;
_4c="";
}
_4e=_4e||{};
var _4f,i,t,_50,_51,_52,_53,_54=1,_55=_4d;
if(_5.call(_4d)=="[object Array]"){
_52=_c(_4d,_4c);
t=_52[0];
_54=_52.length-t;
_4d=_52[_54];
}else{
_52=[0];
if(_4d){
if(_5.call(_4d)=="[object Function]"){
t=_4d._meta;
_52=_52.concat(t?t.bases:_4d);
}else{
_9("base class is not a callable constructor.",_4c);
}
}else{
if(_4d!==null){
_9("unknown base class. Did you use dojo.require to pull it in?",_4c);
}
}
}
if(_4d){
for(i=_54-1;;--i){
_4f=_48(_4d);
if(!i){
break;
}
t=_52[i];
(t._meta?_29:_4)(_4f,t.prototype);
_50=new Function;
_50.superclass=_4d;
_50.prototype=_4f;
_4d=_4f.constructor=_50;
}
}else{
_4f={};
}
_35.safeMixin(_4f,_4e);
t=_4e.constructor;
if(t!==op.constructor){
t.nom=_8;
_4f.constructor=t;
}
for(i=_54-1;i;--i){
t=_52[i]._meta;
if(t&&t.chains){
_53=_4(_53||{},t.chains);
}
}
if(_4f["-chains-"]){
_53=_4(_53||{},_4f["-chains-"]);
}
if(_4d&&_4d.prototype&&_4d.prototype["-chains-"]){
_53=_4(_53||{},_4d.prototype["-chains-"]);
}
t=!_53||!_53.hasOwnProperty(_8);
_52[0]=_50=(_53&&_53.constructor==="manual")?_41(_52):(_52.length==1?_3e(_4e.constructor,t):_38(_52,t));
_50._meta={bases:_52,hidden:_4e,chains:_53,parents:_55,ctor:_4e.constructor};
_50.superclass=_4d&&_4d.prototype;
_50.extend=_33;
_50.createSubclass=_36;
_50.prototype=_4f;
_4f.constructor=_50;
_4f.getInherited=_21;
_4f.isInstanceOf=_27;
_4f.inherited=_26;
_4f.__inherited=_17;
if(_4c){
_4f.declaredClass=_4c;
_3.setObject(_4c,_50);
}
if(_53){
for(_51 in _53){
if(_4f[_51]&&typeof _53[_51]=="string"&&_51!=_8){
t=_4f[_51]=_43(_51,_52,_53[_51]==="after");
t.nom=_51;
}
}
}
return _50;
};
_1.safeMixin=_35.safeMixin=_2e;
_1.declare=_35;
return _35;
});
