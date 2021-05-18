//>>built
define("dojox/lang/functional/listcomp",["dojo","dijit","dojox"],function(_1,_2,_3){
_1.provide("dojox.lang.functional.listcomp");
(function(){
var _4=/\bfor\b|\bif\b/gm;
var _5=function(s){
var _6=s.split(_4),_7=s.match(_4),_8=["var r = [];"],_9=[],i=0,l=_7.length;
while(i<l){
var a=_7[i],f=_6[++i];
if(a=="for"&&!/^\s*\(\s*(;|var)/.test(f)){
f=f.replace(/^\s*\(/,"(var ");
}
_8.push(a,f,"{");
_9.push("}");
}
return _8.join("")+"r.push("+_6[0]+");"+_9.join("")+"return r;";
};
_1.mixin(_3.lang.functional,{buildListcomp:function(s){
return "function(){"+_5(s)+"}";
},compileListcomp:function(s){
return new Function([],_5(s));
},listcomp:function(s){
return (new Function([],_5(s)))();
}});
})();
});
