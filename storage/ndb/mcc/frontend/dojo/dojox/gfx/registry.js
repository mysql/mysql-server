//>>built
define("dojox/gfx/registry",["dojo/has","./shape"],function(_1,_2){
_1.add("gfxRegistry",1);
var _3={};
var _4={};
var _5={};
_3.register=_2.register=function(s){
var t=s.declaredClass.split(".").pop();
var i=t in _4?++_4[t]:((_4[t]=0));
var _6=t+i;
_5[_6]=s;
return _6;
};
_3.byId=_2.byId=function(id){
return _5[id];
};
_3.dispose=_2.dispose=function(s,_7){
if(_7&&s.children){
for(var i=0;i<s.children.length;++i){
_3.dispose(s.children[i],true);
}
}
var _8=s.getUID();
_5[_8]=null;
delete _5[_8];
};
return _3;
});
