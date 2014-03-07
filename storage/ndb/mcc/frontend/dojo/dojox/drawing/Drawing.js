//>>built
define(["dijit","dojo","dojox"],function(_1,_2,_3){
_2.provide("dojox.drawing.Drawing");
(function(){
var _4=false;
_2.declare("dojox.drawing.Drawing",[],{ready:false,mode:"",width:0,height:0,constructor:function(_5,_6){
var _7=_2.attr(_6,"defaults");
if(_7){
_3.drawing.defaults=_2.getObject(_7);
}
this.defaults=_3.drawing.defaults;
this.id=_6.id;
_3.drawing.register(this,"drawing");
this.mode=(_5.mode||_2.attr(_6,"mode")||"").toLowerCase();
var _8=_2.contentBox(_6);
this.width=_8.w;
this.height=_8.h;
this.util=_3.drawing.util.common;
this.util.register(this);
this.keys=_3.drawing.manager.keys;
this.mouse=new _3.drawing.manager.Mouse({util:this.util,keys:this.keys,id:this.mode=="ui"?"MUI":"mse"});
this.mouse.setEventMode(this.mode);
this.tools={};
this.stencilTypes={};
this.stencilTypeMap={};
this.srcRefNode=_6;
this.domNode=_6;
if(_5.plugins){
this.plugins=eval(_5.plugins);
}else{
this.plugins=[];
}
this.widgetId=this.id;
_2.attr(this.domNode,"widgetId",this.widgetId);
if(_1&&_1.registry){
_1.registry.add(this);
}else{
_1.registry={objs:{},add:function(_9){
this.objs[_9.id]=_9;
}};
_1.byId=function(id){
return _1.registry.objs[id];
};
_1.registry.add(this);
}
var _a=_3.drawing.getRegistered("stencil");
for(var nm in _a){
this.registerTool(_a[nm].name);
}
var _b=_3.drawing.getRegistered("tool");
for(nm in _b){
this.registerTool(_b[nm].name);
}
var _c=_3.drawing.getRegistered("plugin");
for(nm in _c){
this.registerTool(_c[nm].name);
}
this._createCanvas();
},_createCanvas:function(){
this.canvas=new _3.drawing.manager.Canvas({srcRefNode:this.domNode,util:this.util,mouse:this.mouse,callback:_2.hitch(this,"onSurfaceReady")});
this.initPlugins();
},resize:function(_d){
_d&&_2.style(this.domNode,{width:_d.w+"px",height:_d.h+"px"});
if(!this.canvas){
this._createCanvas();
}else{
if(_d){
this.canvas.resize(_d.w,_d.h);
}
}
},startup:function(){
},getShapeProps:function(_e,_f){
var _10=_e.stencilType;
var ui=this.mode=="ui"||_f=="ui";
return _2.mixin({container:ui&&!_10?this.canvas.overlay.createGroup():this.canvas.surface.createGroup(),util:this.util,keys:this.keys,mouse:this.mouse,drawing:this,drawingType:ui&&!_10?"ui":"stencil",style:this.defaults.copy()},_e||{});
},addPlugin:function(_11){
this.plugins.push(_11);
if(this.canvas.surfaceReady){
this.initPlugins();
}
},initPlugins:function(){
if(!this.canvas||!this.canvas.surfaceReady){
var c=_2.connect(this,"onSurfaceReady",this,function(){
_2.disconnect(c);
this.initPlugins();
});
return;
}
_2.forEach(this.plugins,function(p,i){
var _12=_2.mixin({util:this.util,keys:this.keys,mouse:this.mouse,drawing:this,stencils:this.stencils,anchors:this.anchors,canvas:this.canvas},p.options||{});
this.registerTool(p.name,_2.getObject(p.name));
try{
this.plugins[i]=new this.tools[p.name](_12);
}
catch(e){
console.error("Failed to initilaize plugin:\t"+p.name+". Did you require it?");
}
},this);
this.plugins=[];
_4=true;
this.mouse.setCanvas();
},onSurfaceReady:function(){
this.ready=true;
this.mouse.init(this.canvas.domNode);
this.undo=new _3.drawing.manager.Undo({keys:this.keys});
this.anchors=new _3.drawing.manager.Anchors({drawing:this,mouse:this.mouse,undo:this.undo,util:this.util});
if(this.mode=="ui"){
this.uiStencils=new _3.drawing.manager.StencilUI({canvas:this.canvas,surface:this.canvas.surface,mouse:this.mouse,keys:this.keys});
}else{
this.stencils=new _3.drawing.manager.Stencil({canvas:this.canvas,surface:this.canvas.surface,mouse:this.mouse,undo:this.undo,keys:this.keys,anchors:this.anchors});
this.uiStencils=new _3.drawing.manager.StencilUI({canvas:this.canvas,surface:this.canvas.surface,mouse:this.mouse,keys:this.keys});
}
if(_3.gfx.renderer=="silverlight"){
try{
new _3.drawing.plugins.drawing.Silverlight({util:this.util,mouse:this.mouse,stencils:this.stencils,anchors:this.anchors,canvas:this.canvas});
}
catch(e){
throw new Error("Attempted to install the Silverlight plugin, but it was not found.");
}
}
_2.forEach(this.plugins,function(p){
p.onSurfaceReady&&p.onSurfaceReady();
});
},addUI:function(_13,_14){
if(!this.ready){
var c=_2.connect(this,"onSurfaceReady",this,function(){
_2.disconnect(c);
this.addUI(_13,_14);
});
return false;
}
if(_14&&!_14.data&&!_14.points){
_14={data:_14};
}
if(!this.stencilTypes[_13]){
if(_13!="tooltip"){
console.warn("Not registered:",_13);
}
return null;
}
var s=this.uiStencils.register(new this.stencilTypes[_13](this.getShapeProps(_14,"ui")));
return s;
},addStencil:function(_15,_16){
if(!this.ready){
var c=_2.connect(this,"onSurfaceReady",this,function(){
_2.disconnect(c);
this.addStencil(_15,_16);
});
return false;
}
if(_16&&!_16.data&&!_16.points){
_16={data:_16};
}
var s=this.stencils.register(new this.stencilTypes[_15](this.getShapeProps(_16)));
this.currentStencil&&this.currentStencil.moveToFront();
return s;
},removeStencil:function(_17){
this.stencils.unregister(_17);
_17.destroy();
},removeAll:function(){
this.stencils.removeAll();
},selectAll:function(){
this.stencils.selectAll();
},toSelected:function(_18){
this.stencils.toSelected.apply(this.stencils,arguments);
},exporter:function(){
return this.stencils.exporter();
},importer:function(_19){
_2.forEach(_19,function(m){
this.addStencil(m.type,m);
},this);
},changeDefaults:function(_1a,_1b){
if(_1b!=undefined&&_1b){
for(var nm in _1a){
this.defaults[nm]=_1a[nm];
}
}else{
for(var nm in _1a){
for(var n in _1a[nm]){
this.defaults[nm][n]=_1a[nm][n];
}
}
}
if(this.currentStencil!=undefined&&(!this.currentStencil.created||this.defaults.clickMode)){
this.unSetTool();
this.setTool(this.currentType);
}
},onRenderStencil:function(_1c){
this.stencils.register(_1c);
this.unSetTool();
if(!this.defaults.clickMode){
this.setTool(this.currentType);
}else{
this.defaults.clickable=true;
}
},onDeleteStencil:function(_1d){
this.stencils.unregister(_1d);
},registerTool:function(_1e){
if(this.tools[_1e]){
return;
}
var _1f=_2.getObject(_1e);
this.tools[_1e]=_1f;
var _20=this.util.abbr(_1e);
this.stencilTypes[_20]=_1f;
this.stencilTypeMap[_20]=_1e;
},getConstructor:function(_21){
return this.stencilTypes[_21];
},setTool:function(_22){
if(this.mode=="ui"){
return;
}
if(!this.canvas||!this.canvas.surface){
var c=_2.connect(this,"onSurfaceReady",this,function(){
_2.disconnect(c);
this.setTool(_22);
});
return;
}
if(this.currentStencil){
this.unSetTool();
}
this.currentType=this.tools[_22]?_22:this.stencilTypeMap[_22];
try{
this.currentStencil=new this.tools[this.currentType]({container:this.canvas.surface.createGroup(),util:this.util,mouse:this.mouse,keys:this.keys});
if(this.defaults.clickMode){
this.defaults.clickable=false;
}
this.currentStencil.connect(this.currentStencil,"onRender",this,"onRenderStencil");
this.currentStencil.connect(this.currentStencil,"destroy",this,"onDeleteStencil");
}
catch(e){
console.error("dojox.drawing.setTool Error:",e);
console.error(this.currentType+" is not a constructor: ",this.tools[this.currentType]);
}
},set:function(_23,_24){
},unSetTool:function(){
if(!this.currentStencil.created){
this.currentStencil.destroy();
}
}});
})();
});
