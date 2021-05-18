//>>built
define("dojox/drawing/ui/Toolbar",["dojo","../library/icons","../util/common","../Drawing","../manager/_registry"],function(_1,_2,_3,_4,_5){
return _1.declare("dojox.drawing.ui.Toolbar",[],{constructor:function(_6,_7){
if(_6.drawing){
this.toolDrawing=_6.drawing;
this.drawing=this.toolDrawing;
this.width=this.toolDrawing.width;
this.height=this.toolDrawing.height;
this.strSelected=_6.selected;
this.strTools=_6.tools;
this.strPlugs=_6.plugs;
this._mixprops(["padding","margin","size","radius"],_6);
this.addBack();
this.orient=_6.orient?_6.orient:false;
}else{
var _8=_1.marginBox(_7);
this.width=_8.w;
this.height=_8.h;
this.strSelected=_1.attr(_7,"selected");
this.strTools=_1.attr(_7,"tools");
this.strPlugs=_1.attr(_7,"plugs");
this._mixprops(["padding","margin","size","radius"],_7);
this.toolDrawing=new _4({mode:"ui"},_7);
this.orient=_1.attr(_7,"orient");
}
this.horizontal=this.orient?this.orient=="H":this.width>this.height;
if(this.toolDrawing.ready){
this.makeButtons();
if(!this.strSelected&&this.drawing.defaults.clickMode){
this.drawing.mouse.setCursor("default");
}
}else{
var c=_1.connect(this.toolDrawing,"onSurfaceReady",this,function(){
_1.disconnect(c);
this.drawing=_5.getRegistered("drawing",_1.attr(_7,"drawingId"));
this.makeButtons();
if(!this.strSelected&&this.drawing.defaults.clickMode){
var c=_1.connect(this.drawing,"onSurfaceReady",this,function(){
_1.disconnect(c);
this.drawing.mouse.setCursor("default");
});
}
});
}
},padding:10,margin:5,size:30,radius:3,toolPlugGap:20,strSelected:"",strTools:"",strPlugs:"",makeButtons:function(){
this.buttons=[];
this.plugins=[];
var x=this.padding,y=this.padding,w=this.size,h=this.size,r=this.radius,g=this.margin,_9=_2,s={place:"BR",size:2,mult:4};
if(this.strTools){
var _a=[];
var _b=_5.getRegistered("tool");
var _c={};
for(var nm in _b){
var _d=_3.abbr(nm);
_c[_d]=_b[nm];
if(this.strTools=="all"){
_a.push(_d);
var _e=_5.getRegistered("tool",nm);
if(_e.secondary){
_a.push(_e.secondary.name);
}
}
}
if(this.strTools!="all"){
var _f=this.strTools.split(",");
_1.forEach(_f,function(_10){
_10=_1.trim(_10);
_a.push(_10);
var _11=_5.getRegistered("tool",_c[_10].name);
if(_11.secondary){
_a.push(_11.secondary.name);
}
},this);
}
_1.forEach(_a,function(t){
t=_1.trim(t);
var _12=false;
if(t.indexOf("Secondary")>-1){
var _13=t.substring(0,t.indexOf("Secondary"));
var sec=_5.getRegistered("tool",_c[_13].name).secondary;
var _14=sec.label;
this[t]=sec.funct;
if(sec.setup){
_1.hitch(this,sec.setup)();
}
var btn=this.toolDrawing.addUI("button",{data:{x:x,y:y,width:w,height:h/2,r:r},toolType:t,secondary:true,text:_14,shadow:s,scope:this,callback:this[t]});
if(sec.postSetup){
_1.hitch(this,sec.postSetup,btn)();
}
_12=true;
}else{
var btn=this.toolDrawing.addUI("button",{data:{x:x,y:y,width:w,height:h,r:r},toolType:t,icon:_9[t],shadow:s,scope:this,callback:"onToolClick"});
}
_5.register(btn,"button");
this.buttons.push(btn);
if(this.strSelected==t){
btn.select();
this.selected=btn;
this.drawing.setTool(btn.toolType);
}
if(this.horizontal){
x+=h+g;
}else{
var _15=_12?h/2+g:h+g;
y+=_15;
}
},this);
}
if(this.horizontal){
x+=this.toolPlugGap;
}else{
y+=this.toolPlugGap;
}
if(this.strPlugs){
var _16=[];
var _17=_5.getRegistered("plugin");
var _18={};
for(var nm in _17){
var _19=_3.abbr(nm);
_18[_19]=_17[nm];
if(this.strPlugs=="all"){
_16.push(_19);
}
}
if(this.strPlugs!="all"){
_16=this.strPlugs.split(",");
_1.map(_16,function(p){
return _1.trim(p);
});
}
_1.forEach(_16,function(p){
var t=_1.trim(p);
if(_18[p].button!=false){
var btn=this.toolDrawing.addUI("button",{data:{x:x,y:y,width:w,height:h,r:r},toolType:t,icon:_9[t],shadow:s,scope:this,callback:"onPlugClick"});
_5.register(btn,"button");
this.plugins.push(btn);
if(this.horizontal){
x+=h+g;
}else{
y+=h+g;
}
}
var _1a={};
_18[p].button==false?_1a={name:this.drawing.stencilTypeMap[p]}:_1a={name:this.drawing.stencilTypeMap[p],options:{button:btn}};
this.drawing.addPlugin(_1a);
},this);
}
_1.connect(this.drawing,"onRenderStencil",this,"onRenderStencil");
},onRenderStencil:function(_1b){
if(this.drawing.defaults.clickMode){
this.drawing.mouse.setCursor("default");
this.selected&&this.selected.deselect();
this.selected=null;
}
},addTool:function(){
},addPlugin:function(){
},addBack:function(){
this.toolDrawing.addUI("rect",{data:{x:0,y:0,width:this.width,height:this.size+(this.padding*2),fill:"#ffffff",borderWidth:0}});
},onToolClick:function(_1c){
if(this.drawing.defaults.clickMode){
this.drawing.mouse.setCursor("crosshair");
}
_1.forEach(this.buttons,function(b){
if(b.id==_1c.id){
b.select();
this.selected=b;
this.drawing.setTool(_1c.toolType);
}else{
if(!b.secondary){
b.deselect();
}
}
},this);
},onPlugClick:function(_1d){
},_mixprops:function(_1e,_1f){
_1.forEach(_1e,function(p){
this[p]=_1f.tagName?_1.attr(_1f,p)===null?this[p]:_1.attr(_1f,p):_1f[p]===undefined?this[p]:_1f[p];
},this);
}});
});
