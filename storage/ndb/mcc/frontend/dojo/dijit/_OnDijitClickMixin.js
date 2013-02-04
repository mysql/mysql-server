//>>built
define("dijit/_OnDijitClickMixin",["dojo/on","dojo/_base/array","dojo/keys","dojo/_base/declare","dojo/_base/sniff","dojo/_base/unload","dojo/_base/window"],function(on,_1,_2,_3,_4,_5,_6){
var _7=null;
if(_4("ie")){
(function(){
var _8=function(_9){
_7=_9.srcElement;
};
_6.doc.attachEvent("onkeydown",_8);
_5.addOnWindowUnload(function(){
_6.doc.detachEvent("onkeydown",_8);
});
})();
}else{
_6.doc.addEventListener("keydown",function(_a){
_7=_a.target;
},true);
}
var _b=function(_c,_d){
if(/input|button/i.test(_c.nodeName)){
return on(_c,"click",_d);
}else{
function _e(e){
return (e.keyCode==_2.ENTER||e.keyCode==_2.SPACE)&&!e.ctrlKey&&!e.shiftKey&&!e.altKey&&!e.metaKey;
};
var _f=[on(_c,"keypress",function(e){
if(_e(e)){
_7=e.target;
e.preventDefault();
}
}),on(_c,"keyup",function(e){
if(_e(e)&&e.target==_7){
_7=null;
_d.call(this,e);
}
}),on(_c,"click",function(e){
_d.call(this,e);
})];
return {remove:function(){
_1.forEach(_f,function(h){
h.remove();
});
}};
}
};
return _3("dijit._OnDijitClickMixin",null,{connect:function(obj,_10,_11){
return this.inherited(arguments,[obj,_10=="ondijitclick"?_b:_10,_11]);
}});
});
