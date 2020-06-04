//>>built
define("dojox/editor/plugins/_SpellCheckParser",["dojo","dojox","dojo/_base/connect","dojo/_base/declare"],function(_1,_2){
var _3=_1.declare("dojox.editor.plugins._SpellCheckParser",null,{lang:"english",parseIntoWords:function(_4){
function _5(c){
var ch=c.charCodeAt(0);
return 48<=ch&&ch<=57||65<=ch&&ch<=90||97<=ch&&ch<=122;
};
var _6=this.words=[],_7=this.indices=[],_8=0,_9=_4&&_4.length,_a=0;
while(_8<_9){
var ch;
while(_8<_9&&!_5(ch=_4.charAt(_8))&&ch!="&"){
_8++;
}
if(ch=="&"){
while(++_8<_9&&(ch=_4.charAt(_8))!=";"&&_5(ch)){
}
}else{
_a=_8;
while(++_8<_9&&_5(_4.charAt(_8))){
}
if(_a<_9){
_6.push(_4.substring(_a,_8));
_7.push(_a);
}
}
}
return _6;
},getIndices:function(){
return this.indices;
}});
_1.subscribe(dijit._scopeName+".Editor.plugin.SpellCheck.getParser",null,function(sp){
if(sp.parser){
return;
}
sp.parser=new _3();
});
return _3;
});
