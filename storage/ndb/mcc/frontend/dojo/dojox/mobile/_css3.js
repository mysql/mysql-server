//>>built
define("dojox/mobile/_css3",["dojo/_base/window","dojo/_base/array","dojo/has"],function(_1,_2,_3){
var _4=[],_5=[];
var _6=_1.doc.createElement("div").style;
var _7=["webkit"];
_3.add("css3-animations",function(_8,_9,_a){
var _b=_a.style;
return (_b["animation"]!==undefined&&_b["transition"]!==undefined)||_2.some(_7,function(p){
return _b[p+"Animation"]!==undefined&&_b[p+"Transition"]!==undefined;
});
});
_3.add("t17164",function(_c,_d,_e){
return (_e.style["transition"]!==undefined)&&!("TransitionEvent" in window);
});
var _f={name:function(p,_10){
var n=(_10?_5:_4)[p];
if(!n){
if(/End|Start/.test(p)){
var idx=p.length-(p.match(/End/)?3:5);
var s=p.substr(0,idx);
var pp=this.name(s);
if(pp==s){
n=p.toLowerCase();
}else{
n=pp+p.substr(idx);
}
}else{
if(p=="keyframes"){
var pk=this.name("animation",_10);
if(pk=="animation"){
n=p;
}else{
if(_10){
n=pk.replace(/animation/,"keyframes");
}else{
n=pk.replace(/Animation/,"Keyframes");
}
}
}else{
var cn=_10?p.replace(/-(.)/g,function(_11,p1){
return p1.toUpperCase();
}):p;
if(_6[cn]!==undefined&&!_3("t17164")){
n=p;
}else{
cn=cn.charAt(0).toUpperCase()+cn.slice(1);
_2.some(_7,function(_12){
if(_6[_12+cn]!==undefined){
if(_10){
n="-"+_12+"-"+p;
}else{
n=_12+cn;
}
}
});
}
}
}
if(!n){
n=p;
}
(_10?_5:_4)[p]=n;
}
return n;
},add:function(_13,_14){
for(var p in _14){
if(_14.hasOwnProperty(p)){
_13[_f.name(p)]=_14[p];
}
}
return _13;
}};
return _f;
});
