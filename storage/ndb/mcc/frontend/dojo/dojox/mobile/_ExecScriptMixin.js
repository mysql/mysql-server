//>>built
define("dojox/mobile/_ExecScriptMixin",["dojo/_base/kernel","dojo/_base/declare","dojo/_base/window","dojo/dom-construct"],function(_1,_2,_3,_4){
return _2("dojox.mobile._ExecScriptMixin",null,{execScript:function(_5){
var s=_5.replace(/\f/g," ").replace(/<\/script>/g,"\f");
s=s.replace(/<script [^>]*src=['"]([^'"]+)['"][^>]*>([^\f]*)\f/ig,function(_6,_7){
_4.create("script",{type:"text/javascript",src:_7},_3.doc.getElementsByTagName("head")[0]);
return "";
});
s=s.replace(/<script>([^\f]*)\f/ig,function(_8,_9){
_1.eval(_9);
return "";
});
return s;
}});
});
