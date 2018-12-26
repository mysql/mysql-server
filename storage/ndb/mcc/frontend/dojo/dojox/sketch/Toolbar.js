//>>built
define("dojox/sketch/Toolbar",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","./Annotation","dijit/Toolbar","dijit/form/Button"],function(_1){
_1.getObject("sketch",true,dojox);
_1.declare("dojox.sketch.ButtonGroup",null,{constructor:function(){
this._childMaps={};
this._children=[];
},add:function(_2){
this._childMaps[_2]=_2.connect(_2,"onActivate",_1.hitch(this,"_resetGroup",_2));
this._children.push(_2);
},_resetGroup:function(p){
var cs=this._children;
_1.forEach(cs,function(c){
if(p!=c&&c["attr"]){
c.attr("checked",false);
}
});
}});
_1.declare("dojox.sketch.Toolbar",dijit.Toolbar,{figure:null,plugins:null,postCreate:function(){
this.inherited(arguments);
this.shapeGroup=new dojox.sketch.ButtonGroup;
if(!this.plugins){
this.plugins=["Lead","SingleArrow","DoubleArrow","Underline","Preexisting","Slider"];
}
this._plugins=[];
_1.forEach(this.plugins,function(_3){
var _4=_1.isString(_3)?_3:_3.name;
var p=new dojox.sketch.tools[_4](_3.args||{});
this._plugins.push(p);
p.setToolbar(this);
if(!this._defaultTool&&p.button){
this._defaultTool=p;
}
},this);
},setFigure:function(f){
this.figure=f;
this.connect(f,"onLoad","reset");
_1.forEach(this._plugins,function(p){
p.setFigure(f);
});
},destroy:function(){
_1.forEach(this._plugins,function(p){
p.destroy();
});
this.inherited(arguments);
delete this._defaultTool;
delete this._plugins;
},addGroupItem:function(_5,_6){
if(_6!="toolsGroup"){
console.error("not supported group "+_6);
return;
}
this.shapeGroup.add(_5);
},reset:function(){
this._defaultTool.activate();
},_setShape:function(s){
if(!this.figure.surface){
return;
}
if(this.figure.hasSelections()){
for(var i=0;i<this.figure.selected.length;i++){
var _7=this.figure.selected[i].serialize();
this.figure.convert(this.figure.selected[i],s);
this.figure.history.add(dojox.sketch.CommandTypes.Convert,this.figure.selected[i],_7);
}
}
}});
dojox.sketch.makeToolbar=function(_8,_9){
var _a=new dojox.sketch.Toolbar();
_a.setFigure(_9);
_8.appendChild(_a.domNode);
return _a;
};
return dojox.sketch.Toolbar;
});
