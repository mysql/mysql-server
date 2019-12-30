//>>built
define("dojox/mobile/lazyLoadUtils",["dojo/_base/kernel","dojo/_base/array","dojo/_base/config","dojo/_base/window","dojo/_base/Deferred","dojo/ready"],function(_1,_2,_3,_4,_5,_6){
var _7=function(){
this._lazyNodes=[];
var _8=this;
if(_3.parseOnLoad){
_6(90,function(){
var _9=_2.filter(_4.body().getElementsByTagName("*"),function(n){
return n.getAttribute("lazy")==="true"||(n.getAttribute("data-dojo-props")||"").match(/lazy\s*:\s*true/);
});
var i,j,_a,s,n;
for(i=0;i<_9.length;i++){
_2.forEach(["dojoType","data-dojo-type"],function(a){
_a=_2.filter(_9[i].getElementsByTagName("*"),function(n){
return n.getAttribute(a);
});
for(j=0;j<_a.length;j++){
n=_a[j];
n.setAttribute("__"+a,n.getAttribute(a));
n.removeAttribute(a);
_8._lazyNodes.push(n);
}
});
}
});
}
_6(function(){
for(var i=0;i<_8._lazyNodes.length;i++){
var n=_8._lazyNodes[i];
_2.forEach(["dojoType","data-dojo-type"],function(a){
if(n.getAttribute("__"+a)){
n.setAttribute(a,n.getAttribute("__"+a));
n.removeAttribute("__"+a);
}
});
}
delete _8._lazyNodes;
});
this.instantiateLazyWidgets=function(_b,_c,_d){
var d=new _5();
var _e=_c?_c.split(/,/):[];
var _f=_b.getElementsByTagName("*");
var len=_f.length;
for(var i=0;i<len;i++){
var s=_f[i].getAttribute("dojoType")||_f[i].getAttribute("data-dojo-type");
if(s){
_e.push(s);
var m=_f[i].getAttribute("data-dojo-mixins"),_10=m?m.split(/, */):[];
_e=_e.concat(_10);
}
}
if(_e.length===0){
return true;
}
if(_1.require){
_2.forEach(_e,function(c){
_1["require"](c);
});
_1.parser.parse(_b);
if(_d){
_d(_b);
}
return true;
}else{
_e=_2.map(_e,function(s){
return s.replace(/\./g,"/");
});
require(_e,function(){
_1.parser.parse(_b);
if(_d){
_d(_b);
}
d.resolve(true);
});
}
return d;
};
};
return new _7();
});
