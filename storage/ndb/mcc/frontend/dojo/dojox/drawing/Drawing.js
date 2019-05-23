//>>built
define("dojox/drawing/Drawing",["dojo","./defaults","./manager/_registry","./manager/keys","./manager/Mouse","./manager/Canvas","./manager/Undo","./manager/Anchors","./manager/Stencil","./manager/StencilUI","./util/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1.declare("dojox.drawing.Drawing",[],{ready:false,mode:"",width:0,height:0,constructor:function(_c,_d){
var _e=_1.attr(_d,"defaults");
this.defaults=_e?(typeof _e==="string"?_1.getObject(_e):_e):_2;
this.id=_d.id||dijit.getUniqueId("dojox_drawing_Drawing");
_3.register(this,"drawing");
this.mode=(_c.mode||_1.attr(_d,"mode")||"").toLowerCase();
var _f=_1.contentBox(_d);
this.width=_c.width||_f.w;
this.height=_c.height||_f.h;
_b.register(this);
this.mouse=new _5({util:_b,keys:_4,id:this.mode=="ui"?"MUI":"mse"});
this.mouse.setEventMode(this.mode);
this.tools={};
this.stencilTypes={};
this.stencilTypeMap={};
this.srcRefNode=_d;
this.domNode=_d;
if(_c.plugins){
this.plugins=eval(_c.plugins);
}else{
this.plugins=[];
}
this.widgetId=this.id;
_1.attr(this.domNode,"widgetId",this.widgetId);
if(dijit&&dijit.registry){
dijit.registry.add(this);
}else{
dijit.registry={objs:{},add:function(obj){
this.objs[obj.id]=obj;
}};
dijit.byId=function(id){
return dijit.registry.objs[id];
};
dijit.registry.add(this);
}
var _10=_3.getRegistered("stencil");
for(var nm in _10){
this.registerTool(_10[nm].name);
}
var _11=_3.getRegistered("tool");
for(nm in _11){
this.registerTool(_11[nm].name);
}
var _12=_3.getRegistered("plugin");
for(nm in _12){
this.registerTool(_12[nm].name);
}
this._createCanvas();
},_createCanvas:function(){
this.canvas=new _6({srcRefNode:this.domNode,util:_b,mouse:this.mouse,width:this.width,height:this.height,callback:_1.hitch(this,"onSurfaceReady")});
this.initPlugins();
},resize:function(box){
box&&_1.style(this.domNode,{width:box.w+"px",height:box.h+"px"});
if(!this.canvas){
this._createCanvas();
}else{
if(box){
this.canvas.resize(box.w,box.h);
}
}
},startup:function(){
},getShapeProps:function(_13,_14){
var _15=_13.stencilType;
var ui=this.mode=="ui"||_14=="ui";
return _1.mixin({container:ui&&!_15?this.canvas.overlay.createGroup():this.canvas.surface.createGroup(),util:_b,keys:_4,mouse:this.mouse,drawing:this,drawingType:ui&&!_15?"ui":"stencil",style:this.defaults.copy()},_13||{});
},addPlugin:function(_16){
this.plugins.push(_16);
if(this.canvas.surfaceReady){
this.initPlugins();
}
},initPlugins:function(){
if(!this.canvas||!this.canvas.surfaceReady){
var c=_1.connect(this,"onSurfaceReady",this,function(){
_1.disconnect(c);
this.initPlugins();
});
return;
}
_1.forEach(this.plugins,function(p,i){
var _17=_1.mixin({util:_b,keys:_4,mouse:this.mouse,drawing:this,stencils:this.stencils,anchors:this.anchors,canvas:this.canvas},p.options||{});
this.registerTool(p.name,_1.getObject(p.name));
try{
this.plugins[i]=new this.tools[p.name](_17);
}
catch(e){
console.error("Failed to initilaize plugin:\t"+p.name+". Did you require it?");
}
},this);
this.plugins=[];
this.mouse.setCanvas();
},onSurfaceReady:function(){
this.ready=true;
this.mouse.init(this.canvas.domNode);
this.undo=new _7({keys:_4});
this.anchors=new _8({drawing:this,mouse:this.mouse,undo:this.undo,util:_b});
if(this.mode=="ui"){
this.uiStencils=new _a({canvas:this.canvas,surface:this.canvas.surface,mouse:this.mouse,keys:_4});
}else{
this.stencils=new _9({canvas:this.canvas,surface:this.canvas.surface,mouse:this.mouse,undo:this.undo,keys:_4,anchors:this.anchors});
this.uiStencils=new _a({canvas:this.canvas,surface:this.canvas.surface,mouse:this.mouse,keys:_4});
}
if(dojox.gfx.renderer=="silverlight"){
try{
new dojox.drawing.plugins.drawing.Silverlight({util:_b,mouse:this.mouse,stencils:this.stencils,anchors:this.anchors,canvas:this.canvas});
}
catch(e){
throw new Error("Attempted to install the Silverlight plugin, but it was not found.");
}
}
_1.forEach(this.plugins,function(p){
p.onSurfaceReady&&p.onSurfaceReady();
});
},addUI:function(_18,_19){
if(!this.ready){
var c=_1.connect(this,"onSurfaceReady",this,function(){
_1.disconnect(c);
this.addUI(_18,_19);
});
return false;
}
if(_19&&!_19.data&&!_19.points){
_19={data:_19};
}
if(!this.stencilTypes[_18]){
if(_18!="tooltip"){
console.warn("Not registered:",_18);
}
return null;
}
var s=this.uiStencils.register(new this.stencilTypes[_18](this.getShapeProps(_19,"ui")));
return s;
},addStencil:function(_1a,_1b){
if(!this.ready){
var c=_1.connect(this,"onSurfaceReady",this,function(){
_1.disconnect(c);
this.addStencil(_1a,_1b);
});
return false;
}
if(_1b&&!_1b.data&&!_1b.points){
_1b={data:_1b};
}
var s=this.stencils.register(new this.stencilTypes[_1a](this.getShapeProps(_1b)));
this.currentStencil&&this.currentStencil.moveToFront();
return s;
},removeStencil:function(_1c){
this.stencils.unregister(_1c);
_1c.destroy();
},removeAll:function(){
this.stencils.removeAll();
},selectAll:function(){
this.stencils.selectAll();
},toSelected:function(_1d){
this.stencils.toSelected.apply(this.stencils,arguments);
},exporter:function(){
return this.stencils.exporter();
},importer:function(_1e){
_1.forEach(_1e,function(m){
this.addStencil(m.type,m);
},this);
},changeDefaults:function(_1f,_20){
if(_20!=undefined&&_20){
for(var nm in _1f){
this.defaults[nm]=_1f[nm];
}
}else{
for(var nm in _1f){
for(var n in _1f[nm]){
this.defaults[nm][n]=_1f[nm][n];
}
}
}
if(this.currentStencil!=undefined&&(!this.currentStencil.created||this.defaults.clickMode)){
this.unSetTool();
this.setTool(this.currentType);
}
},onRenderStencil:function(_21){
this.stencils.register(_21);
this.unSetTool();
if(!this.defaults.clickMode){
this.setTool(this.currentType);
}else{
this.defaults.clickable=true;
}
},onDeleteStencil:function(_22){
this.stencils.unregister(_22);
},registerTool:function(_23){
if(this.tools[_23]){
return;
}
var _24=_1.getObject(_23);
this.tools[_23]=_24;
var _25=_b.abbr(_23);
this.stencilTypes[_25]=_24;
this.stencilTypeMap[_25]=_23;
},getConstructor:function(_26){
return this.stencilTypes[_26];
},setTool:function(_27){
if(this.mode=="ui"){
return;
}
if(!this.canvas||!this.canvas.surface){
var c=_1.connect(this,"onSurfaceReady",this,function(){
_1.disconnect(c);
this.setTool(_27);
});
return;
}
if(this.currentStencil){
this.unSetTool();
}
this.currentType=this.tools[_27]?_27:this.stencilTypeMap[_27];
try{
this.currentStencil=new this.tools[this.currentType]({container:this.canvas.surface.createGroup(),util:_b,mouse:this.mouse,keys:_4});
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
},set:function(_28,_29){
},get:function(_2a){
return;
},unSetTool:function(){
if(!this.currentStencil.created){
this.currentStencil.destroy();
}
}});
});
