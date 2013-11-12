//>>built
define("dojox/editor/plugins/PrettyPrint",["dojo","dijit","dojox","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare","dojox/html/format"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.PrettyPrint",_2._editor._Plugin,{indentBy:-1,lineLength:-1,useDefaultCommand:false,entityMap:null,_initButton:function(){
delete this.command;
},setToolbar:function(_4){
},setEditor:function(_5){
this.inherited(arguments);
var _6=this;
this.editor.onLoadDeferred.addCallback(function(){
_6.editor._prettyprint_getValue=_6.editor.getValue;
_6.editor.getValue=function(){
var _7=_6.editor._prettyprint_getValue(arguments);
return _3.html.format.prettyPrint(_7,_6.indentBy,_6.lineLength,_6.entityMap,_6.xhtml);
};
_6.editor._prettyprint_endEditing=_6.editor._endEditing;
_6.editor._prettyprint_onBlur=_6.editor._onBlur;
_6.editor._endEditing=function(_8){
var v=_6.editor._prettyprint_getValue(true);
_6.editor._undoedSteps=[];
_6.editor._steps.push({text:v,bookmark:_6.editor._getBookmark()});
};
_6.editor._onBlur=function(e){
this.inherited("_onBlur",arguments);
var _9=_6.editor._prettyprint_getValue(true);
if(_9!=_6.editor.savedContent){
_6.editor.onChange(_9);
_6.editor.savedContent=_9;
}
};
});
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _a=o.args.name.toLowerCase();
if(_a==="prettyprint"){
o.plugin=new _3.editor.plugins.PrettyPrint({indentBy:("indentBy" in o.args)?o.args.indentBy:-1,lineLength:("lineLength" in o.args)?o.args.lineLength:-1,entityMap:("entityMap" in o.args)?o.args.entityMap:_3.html.entities.html.concat([["¢","cent"],["£","pound"],["€","euro"],["¥","yen"],["©","copy"],["§","sect"],["…","hellip"],["®","reg"]]),xhtml:("xhtml" in o.args)?o.args.xhtml:false});
}
});
});
