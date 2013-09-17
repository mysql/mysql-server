//>>built
define("dojox/editor/plugins/ToolbarLineBreak",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dojo/_base/connect","dojo/_base/declare"],function(_1,_2,_3){
_1.declare("dojox.editor.plugins.ToolbarLineBreak",[_2._Widget,_2._TemplatedMixin],{templateString:"<span class='dijit dijitReset'><br></span>",postCreate:function(){
_1.setSelectable(this.domNode,false);
},isFocusable:function(){
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _4=o.args.name.toLowerCase();
if(_4==="||"||_4==="toolbarlinebreak"){
o.plugin=new _2._editor._Plugin({button:new _3.editor.plugins.ToolbarLineBreak(),setEditor:function(_5){
this.editor=_5;
}});
}
});
return _3.editor.plugins.ToolbarLineBreak;
});
