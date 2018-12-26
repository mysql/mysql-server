//>>built
define("dojox/editor/plugins/_SpellCheckParser",["dojo","dojox","dojo/_base/connect","dojo/_base/declare"],function(_1,_2){
_1.declare("dojox.editor.plugins._SpellCheckParser",null,{lang:"english",parseIntoWords:function(_3){
function _4(c){
var ch=c.charCodeAt(0);
return 48<=ch&&ch<=57||65<=ch&&ch<=90||97<=ch&&ch<=122;
};
var _5=this.words=[],_6=this.indices=[],_7=0,_8=_3&&_3.length,_9=0;
while(_7<_8){
var ch;
while(_7<_8&&!_4(ch=_3.charAt(_7))&&ch!="&"){
_7++;
}
if(ch=="&"){
while(++_7<_8&&(ch=_3.charAt(_7))!=";"&&_4(ch)){
}
}else{
_9=_7;
while(++_7<_8&&_4(_3.charAt(_7))){
}
if(_9<_8){
_5.push(_3.substring(_9,_7));
_6.push(_9);
}
}
}
return _5;
},getIndices:function(){
return this.indices;
}});
_1.subscribe(dijit._scopeName+".Editor.plugin.SpellCheck.getParser",null,function(sp){
if(sp.parser){
return;
}
sp.parser=new _2.editor.plugins._SpellCheckParser();
});
return _2.editor.plugins._SpellCheckParser;
});
