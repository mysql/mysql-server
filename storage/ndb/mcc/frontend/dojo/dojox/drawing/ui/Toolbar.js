//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/library/icons"],function(_1,_2,_3){
_2.provide("dojox.drawing.ui.Toolbar");
_2.require("dojox.drawing.library.icons");
_2.declare("dojox.drawing.ui.Toolbar",[],{constructor:function(_4,_5){
this.util=_3.drawing.util.common;
if(_4.drawing){
this.toolDrawing=_4.drawing;
this.drawing=this.toolDrawing;
this.width=this.toolDrawing.width;
this.height=this.toolDrawing.height;
this.strSelected=_4.selected;
this.strTools=_4.tools;
this.strPlugs=_4.plugs;
this._mixprops(["padding","margin","size","radius"],_4);
this.addBack();
this.orient=_4.orient?_4.orient:false;
}else{
var _6=_2.marginBox(_5);
this.width=_6.w;
this.height=_6.h;
this.strSelected=_2.attr(_5,"selected");
this.strTools=_2.attr(_5,"tools");
this.strPlugs=_2.attr(_5,"plugs");
this._mixprops(["padding","margin","size","radius"],_5);
this.toolDrawing=new _3.drawing.Drawing({mode:"ui"},_5);
this.orient=_2.attr(_5,"orient");
}
this.horizontal=this.orient?this.orient=="H":this.width>this.height;
if(this.toolDrawing.ready){
this.makeButtons();
if(!this.strSelected&&this.drawing.defaults.clickMode){
this.drawing.mouse.setCursor("default");
}
}else{
var c=_2.connect(this.toolDrawing,"onSurfaceReady",this,function(){
_2.disconnect(c);
this.drawing=_3.drawing.getRegistered("drawing",_2.attr(_5,"drawingId"));
this.makeButtons();
if(!this.strSelected&&this.drawing.defaults.clickMode){
var c=_2.connect(this.drawing,"onSurfaceReady",this,function(){
_2.disconnect(c);
this.drawing.mouse.setCursor("default");
});
}
});
}
},padding:10,margin:5,size:30,radius:3,toolPlugGap:20,strSelected:"",strTools:"",strPlugs:"",makeButtons:function(){
this.buttons=[];
this.plugins=[];
var x=this.padding,y=this.padding,w=this.size,h=this.size,r=this.radius,g=this.margin,_7=_3.drawing.library.icons,s={place:"BR",size:2,mult:4};
if(this.strTools){
var _8=[];
var _9=_3.drawing.getRegistered("tool");
var _a={};
for(var nm in _9){
var _b=this.util.abbr(nm);
_a[_b]=_9[nm];
if(this.strTools=="all"){
_8.push(_b);
var _c=_3.drawing.getRegistered("tool",nm);
if(_c.secondary){
_8.push(_c.secondary.name);
}
}
}
if(this.strTools!="all"){
var _d=this.strTools.split(",");
_2.forEach(_d,function(_e){
_e=_2.trim(_e);
_8.push(_e);
var _f=_3.drawing.getRegistered("tool",_a[_e].name);
if(_f.secondary){
_8.push(_f.secondary.name);
}
},this);
}
_2.forEach(_8,function(t){
t=_2.trim(t);
var _10=false;
if(t.indexOf("Secondary")>-1){
var _11=t.substring(0,t.indexOf("Secondary"));
var sec=_3.drawing.getRegistered("tool",_a[_11].name).secondary;
var _12=sec.label;
this[t]=sec.funct;
if(sec.setup){
_2.hitch(this,sec.setup)();
}
var btn=this.toolDrawing.addUI("button",{data:{x:x,y:y,width:w,height:h/2,r:r},toolType:t,secondary:true,text:_12,shadow:s,scope:this,callback:this[t]});
if(sec.postSetup){
_2.hitch(this,sec.postSetup,btn)();
}
_10=true;
}else{
var btn=this.toolDrawing.addUI("button",{data:{x:x,y:y,width:w,height:h,r:r},toolType:t,icon:_7[t],shadow:s,scope:this,callback:"onToolClick"});
}
_3.drawing.register(btn,"button");
this.buttons.push(btn);
if(this.strSelected==t){
btn.select();
this.selected=btn;
this.drawing.setTool(btn.toolType);
}
if(this.horizontal){
x+=h+g;
}else{
var _13=_10?h/2+g:h+g;
y+=_13;
}
},this);
}
if(this.horizontal){
x+=this.toolPlugGap;
}else{
y+=this.toolPlugGap;
}
if(this.strPlugs){
var _14=[];
var _15=_3.drawing.getRegistered("plugin");
var _16={};
for(var nm in _15){
var _17=this.util.abbr(nm);
_16[_17]=_15[nm];
if(this.strPlugs=="all"){
_14.push(_17);
}
}
if(this.strPlugs!="all"){
_14=this.strPlugs.split(",");
_2.map(_14,function(p){
return _2.trim(p);
});
}
_2.forEach(_14,function(p){
var t=_2.trim(p);
if(_16[p].button!=false){
var btn=this.toolDrawing.addUI("button",{data:{x:x,y:y,width:w,height:h,r:r},toolType:t,icon:_7[t],shadow:s,scope:this,callback:"onPlugClick"});
_3.drawing.register(btn,"button");
this.plugins.push(btn);
if(this.horizontal){
x+=h+g;
}else{
y+=h+g;
}
}
var _18={};
_16[p].button==false?_18={name:this.drawing.stencilTypeMap[p]}:_18={name:this.drawing.stencilTypeMap[p],options:{button:btn}};
this.drawing.addPlugin(_18);
},this);
}
_2.connect(this.drawing,"onRenderStencil",this,"onRenderStencil");
},onRenderStencil:function(_19){
if(this.drawing.defaults.clickMode){
this.drawing.mouse.setCursor("default");
this.selected&&this.selected.deselect();
this.selected=null;
}
},addTool:function(){
},addPlugin:function(){
},addBack:function(){
this.toolDrawing.addUI("rect",{data:{x:0,y:0,width:this.width,height:this.size+(this.padding*2),fill:"#ffffff",borderWidth:0}});
},onToolClick:function(_1a){
if(this.drawing.defaults.clickMode){
this.drawing.mouse.setCursor("crosshair");
}
_2.forEach(this.buttons,function(b){
if(b.id==_1a.id){
b.select();
this.selected=b;
this.drawing.setTool(_1a.toolType);
}else{
if(!b.secondary){
b.deselect();
}
}
},this);
},onPlugClick:function(_1b){
},_mixprops:function(_1c,_1d){
_2.forEach(_1c,function(p){
this[p]=_1d.tagName?_2.attr(_1d,p)===null?this[p]:_2.attr(_1d,p):_1d[p]===undefined?this[p]:_1d[p];
},this);
}});
});
