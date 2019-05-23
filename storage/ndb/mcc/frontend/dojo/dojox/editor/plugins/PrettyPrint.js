//>>built
define("dojox/editor/plugins/PrettyPrint",["dojo","dijit","dojox","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/html/format"],function(_1,_2,_3,_4){
_1.declare("dojox.editor.plugins.PrettyPrint",_4,{indentBy:-1,lineLength:-1,useDefaultCommand:false,entityMap:null,_initButton:function(){
delete this.command;
},setToolbar:function(_5){
},setEditor:function(_6){
this.inherited(arguments);
var _7=this;
this.editor.onLoadDeferred.addCallback(function(){
_7.editor._prettyprint_getValue=_7.editor.getValue;
_7.editor.getValue=function(){
var _8=_7.editor._prettyprint_getValue(arguments);
return _3.html.format.prettyPrint(_8,_7.indentBy,_7.lineLength,_7.entityMap,_7.xhtml);
};
_7.editor._prettyprint_endEditing=_7.editor._endEditing;
_7.editor._prettyprint_onBlur=_7.editor._onBlur;
_7.editor._endEditing=function(_9){
var v=_7.editor._prettyprint_getValue(true);
_7.editor._undoedSteps=[];
_7.editor._steps.push({text:v,bookmark:_7.editor._getBookmark()});
};
_7.editor._onBlur=function(e){
this.inherited("_onBlur",arguments);
var _a=_7.editor._prettyprint_getValue(true);
if(_a!=_7.editor.savedContent){
_7.editor.onChange(_a);
_7.editor.savedContent=_a;
}
};
});
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _b=o.args.name.toLowerCase();
if(_b==="prettyprint"){
o.plugin=new _3.editor.plugins.PrettyPrint({indentBy:("indentBy" in o.args)?o.args.indentBy:-1,lineLength:("lineLength" in o.args)?o.args.lineLength:-1,entityMap:("entityMap" in o.args)?o.args.entityMap:_3.html.entities.html.concat([["¢","cent"],["£","pound"],["€","euro"],["¥","yen"],["©","copy"],["§","sect"],["…","hellip"],["®","reg"]]),xhtml:("xhtml" in o.args)?o.args.xhtml:false});
}
});
return _3.editor.plugins.PrettyPrint;
});
