//>>built
define("dojox/editor/plugins/PrettyPrint",["dojo","dijit","dojox","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/html/format"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.PrettyPrint",_4,{indentBy:-1,lineLength:-1,useDefaultCommand:false,entityMap:null,_initButton:function(){
delete this.command;
},setToolbar:function(_6){
},setEditor:function(_7){
this.inherited(arguments);
var _8=this;
this.editor.onLoadDeferred.addCallback(function(){
_8.editor._prettyprint_getValue=_8.editor.getValue;
_8.editor.getValue=function(){
var _9=_8.editor._prettyprint_getValue(arguments);
return _3.html.format.prettyPrint(_9,_8.indentBy,_8.lineLength,_8.entityMap,_8.xhtml);
};
_8.editor._prettyprint_endEditing=_8.editor._endEditing;
_8.editor._prettyprint_onBlur=_8.editor._onBlur;
_8.editor._endEditing=function(_a){
var v=_8.editor._prettyprint_getValue(true);
_8.editor._undoedSteps=[];
_8.editor._steps.push({text:v,bookmark:_8.editor._getBookmark()});
};
_8.editor._onBlur=function(e){
this.inherited("_onBlur",arguments);
var _b=_8.editor._prettyprint_getValue(true);
if(_b!=_8.editor.savedContent){
_8.editor.onChange(_b);
_8.editor.savedContent=_b;
}
};
});
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _c=o.args.name.toLowerCase();
if(_c==="prettyprint"){
o.plugin=new _5({indentBy:("indentBy" in o.args)?o.args.indentBy:-1,lineLength:("lineLength" in o.args)?o.args.lineLength:-1,entityMap:("entityMap" in o.args)?o.args.entityMap:_3.html.entities.html.concat([["¢","cent"],["£","pound"],["€","euro"],["¥","yen"],["©","copy"],["§","sect"],["…","hellip"],["®","reg"]]),xhtml:("xhtml" in o.args)?o.args.xhtml:false});
}
});
return _5;
});
