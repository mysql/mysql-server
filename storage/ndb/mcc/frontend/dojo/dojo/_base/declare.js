/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
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
function _36(_37,_38){
return function(){
var a=arguments,_39=a,a0=a[0],f,i,m,l=_37.length,_3a;
if(!(this instanceof a.callee)){
return _3b(a);
}
if(_38&&(a0&&a0.preamble||this.preamble)){
_3a=new Array(_37.length);
_3a[0]=a;
for(i=0;;){
a0=a[0];
if(a0){
f=a0.preamble;
if(f){
a=f.apply(this,a)||a;
}
}
f=_37[i].prototype;
f=f.hasOwnProperty("preamble")&&f.preamble;
if(f){
a=f.apply(this,a)||a;
}
if(++i==l){
break;
}
_3a[i]=a;
}
}
for(i=l-1;i>=0;--i){
f=_37[i];
m=f._meta;
f=m?m.ctor:f;
if(f){
f.apply(this,_3a?_3a[i]:a);
}
}
f=this.postscript;
if(f){
f.apply(this,_39);
}
};
};
function _3c(_3d,_3e){
return function(){
var a=arguments,t=a,a0=a[0],f;
if(!(this instanceof a.callee)){
return _3b(a);
}
if(_3e){
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
if(_3d){
_3d.apply(this,a);
}
f=this.postscript;
if(f){
f.apply(this,a);
}
};
};
function _3f(_40){
return function(){
var a=arguments,i=0,f,m;
if(!(this instanceof a.callee)){
return _3b(a);
}
for(;f=_40[i];++i){
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
function _41(_42,_43,_44){
return function(){
var b,m,f,i=0,_45=1;
if(_44){
i=_43.length-1;
_45=-1;
}
for(;b=_43[i];i+=_45){
m=b._meta;
f=(m?m.hidden:b.prototype)[_42];
if(f){
f.apply(this,arguments);
}
}
};
};
function _46(_47){
_6.prototype=_47.prototype;
var t=new _6;
_6.prototype=null;
return t;
};
function _3b(_48){
var _49=_48.callee,t=_46(_49);
_49.apply(t,_48);
return t;
};
function _35(_4a,_4b,_4c){
if(typeof _4a!="string"){
_4c=_4b;
_4b=_4a;
_4a="";
}
_4c=_4c||{};
var _4d,i,t,_4e,_4f,_50,_51,_52=1,_53=_4b;
if(_5.call(_4b)=="[object Array]"){
_50=_c(_4b,_4a);
t=_50[0];
_52=_50.length-t;
_4b=_50[_52];
}else{
_50=[0];
if(_4b){
if(_5.call(_4b)=="[object Function]"){
t=_4b._meta;
_50=_50.concat(t?t.bases:_4b);
}else{
_9("base class is not a callable constructor.",_4a);
}
}else{
if(_4b!==null){
_9("unknown base class. Did you use dojo.require to pull it in?",_4a);
}
}
}
if(_4b){
for(i=_52-1;;--i){
_4d=_46(_4b);
if(!i){
break;
}
t=_50[i];
(t._meta?_29:_4)(_4d,t.prototype);
_4e=new Function;
_4e.superclass=_4b;
_4e.prototype=_4d;
_4b=_4d.constructor=_4e;
}
}else{
_4d={};
}
_35.safeMixin(_4d,_4c);
t=_4c.constructor;
if(t!==op.constructor){
t.nom=_8;
_4d.constructor=t;
}
for(i=_52-1;i;--i){
t=_50[i]._meta;
if(t&&t.chains){
_51=_4(_51||{},t.chains);
}
}
if(_4d["-chains-"]){
_51=_4(_51||{},_4d["-chains-"]);
}
t=!_51||!_51.hasOwnProperty(_8);
_50[0]=_4e=(_51&&_51.constructor==="manual")?_3f(_50):(_50.length==1?_3c(_4c.constructor,t):_36(_50,t));
_4e._meta={bases:_50,hidden:_4c,chains:_51,parents:_53,ctor:_4c.constructor};
_4e.superclass=_4b&&_4b.prototype;
_4e.extend=_33;
_4e.prototype=_4d;
_4d.constructor=_4e;
_4d.getInherited=_21;
_4d.isInstanceOf=_27;
_4d.inherited=_26;
_4d.__inherited=_17;
if(_4a){
_4d.declaredClass=_4a;
_3.setObject(_4a,_4e);
}
if(_51){
for(_4f in _51){
if(_4d[_4f]&&typeof _51[_4f]=="string"&&_4f!=_8){
t=_4d[_4f]=_41(_4f,_50,_51[_4f]==="after");
t.nom=_4f;
}
}
}
return _4e;
};
_1.safeMixin=_35.safeMixin=_2e;
_1.declare=_35;
return _35;
});
