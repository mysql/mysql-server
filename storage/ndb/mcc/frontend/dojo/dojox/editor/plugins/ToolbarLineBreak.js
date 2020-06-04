//>>built
define("dojox/editor/plugins/ToolbarLineBreak",["dojo","dijit","dojox","dijit/_Widget","dijit/_TemplatedMixin","dijit/_editor/_Plugin","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_7("dojox.editor.plugins.ToolbarLineBreak",[_4,_5],{templateString:"<span class='dijit dijitReset'><br></span>",postCreate:function(){
_1.setSelectable(this.domNode,false);
},isFocusable:function(){
return false;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _9=o.args.name.toLowerCase();
if(_9==="||"||_9==="toolbarlinebreak"){
o.plugin=new _6({button:new _8(),setEditor:function(_a){
this.editor=_a;
}});
}
});
return _8;
});
