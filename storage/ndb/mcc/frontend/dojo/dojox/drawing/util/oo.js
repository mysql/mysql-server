//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.util.oo");
_3.drawing.util.oo={declare:function(){
var f,o,_4=0,a=arguments;
if(a.length<2){
console.error("drawing.util.oo.declare; not enough arguments");
}
if(a.length==2){
f=a[0];
o=a[1];
}else{
a=Array.prototype.slice.call(arguments);
o=a.pop();
f=a.pop();
_4=1;
}
for(var n in o){
f.prototype[n]=o[n];
}
if(_4){
a.unshift(f);
f=this.extend.apply(this,a);
}
return f;
},extend:function(){
var a=arguments,_5=a[0];
if(a.length<2){
console.error("drawing.util.oo.extend; not enough arguments");
}
var f=function(){
for(var i=1;i<a.length;i++){
a[i].prototype.constructor.apply(this,arguments);
}
_5.prototype.constructor.apply(this,arguments);
};
for(var i=1;i<a.length;i++){
for(var n in a[i].prototype){
f.prototype[n]=a[i].prototype[n];
}
}
for(n in _5.prototype){
f.prototype[n]=_5.prototype[n];
}
return f;
}};
});
