//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/editor/plugins/TablePlugins"],function(_1,_2,_3){
_2.provide("dojox.editor.plugins.ResizeTableColumn");
_2.require("dojox.editor.plugins.TablePlugins");
_2.declare("dojox.editor.plugins.ResizeTableColumn",_3.editor.plugins.TablePlugins,{constructor:function(){
this.isLtr=this.dir?(this.dir=="ltr"):_2._isBodyLtr();
this.ruleDiv=_2.create("div",{style:"top: -10000px; z-index: 10001"},_2.body(),"last");
},setEditor:function(_4){
var _5=this.ruleDiv;
this.editor=_4;
this.editor.customUndo=true;
this.onEditorLoaded();
_4.onLoadDeferred.addCallback(_2.hitch(this,function(){
this.connect(this.editor.editNode,"onmousemove",function(_6){
var _7=_2.coords(_4.iframe,true),ex=_7.x,cx=_6.clientX;
if(!this.isDragging){
var _8=_6.target;
if(_8.tagName&&_8.tagName.toLowerCase()=="td"){
var _9=_2.coords(_8),ox=_9.x,ow=_9.w,_a=ex+_9.x-2;
if(this.isLtr){
_5.headerColumn=true;
if(!_21(_8,"first")||cx>ox+ow/2){
_a+=ow;
_5.headerColumn=false;
}
}else{
_5.headerColumn=false;
if(_21(_8,"first")&&cx>ox+ow/2){
_a+=ow;
_5.headerColumn=true;
}
}
_2.style(_5,{position:"absolute",cursor:"col-resize",display:"block",width:"4px",backgroundColor:"transparent",top:_7.y+_9.y+"px",left:_a+"px",height:_9.h+"px"});
this.activeCell=_8;
}else{
_2.style(_5,{display:"none",top:"-10000px"});
}
}else{
var _b=this.activeCell,_c=_2.coords(_b),ax=_c.x,aw=_c.w,_d=_15(_b),_e,sx,sw,_f=_2.coords(_19(_b).parentNode),ctx=_f.x,ctw=_f.w;
if(_d){
_e=_2.coords(_d);
sx=_e.x;
sw=_e.w;
}
if(this.isLtr&&((_5.headerColumn&&_d&&ctx<cx&&cx<ax+aw)||((!_d&&ax<cx&&cx<ctx+ctw)||(_d&&ax<cx&&cx<sx+sw)))||!this.isLtr&&((_5.headerColumn&&_d&&ctx>cx&&cx>ax)||((!_d&&ax+aw>cx&&cx>ctx)||(_d&&ax+aw>cx&&cx>sx)))){
_2.style(_5,{left:ex+cx+"px"});
}
}
});
this.connect(_5,"onmousedown",function(evt){
var _10=_2.coords(_4.iframe,true),_11=_2.coords(_19(this.activeCell));
this.isDragging=true;
_2.style(_4.editNode,{cursor:"col-resize"});
_2.style(_5,{width:"1px",left:evt.clientX+"px",top:_10.y+_11.y+"px",height:_11.h+"px",backgroundColor:"#777"});
});
this.connect(_5,"onmouseup",function(evt){
var _12=this.activeCell,_13=_2.coords(_12),aw=_13.w,ax=_13.x,_14=_15(_12),_16,sx,sw,_17=_2.coords(_4.iframe),ex=_17.x,_18=_19(_12),_1a=_2.coords(_18),cs=_18.getAttribute("cellspacing"),cx=evt.clientX,_1b=_1c(_12),_1d,_1e,_1f;
if(!cs||(cs=parseInt(cs,10))<0){
cs=2;
}
if(_14){
_16=_2.coords(_14);
sx=_16.x;
sw=_16.w;
_1d=_1c(_14);
}
if(this.isLtr){
if(_5.headerColumn){
_1e=ex+ax+aw-cx;
}else{
_1e=cx-ex-ax;
if(_14){
_1f=ex+sx+sw-cx-cs;
}
}
}else{
if(_5.headerColumn){
_1e=cx-ex-ax;
}else{
_1e=ex+ax+aw-cx;
if(_14){
_1f=cx-ex-sx-cs;
}
}
}
this.isDragging=false;
_20(_1b,_1e);
if(_14){
if(!_5.headerColumn){
_20(_1d,_1f);
}
}
if(_5.headerColumn&&_21(_12,"first")||_21(_12,"last")){
_2.marginBox(_18,{w:_1a.w+_1e-aw});
}
_20(_1b,_2.coords(_12).w);
if(_14){
_20(_1d,_2.coords(_14).w);
}
_2.style(_4.editNode,{cursor:"auto"});
_2.style(_5,{display:"none",top:"-10000px"});
this.activeCell=null;
});
}));
function _21(n,b){
var _22=_2.withGlobal(_4.window,"query",_2,["> td",n.parentNode]);
switch(b){
case "first":
return _22[0]==n;
case "last":
return _22[_22.length-1]==n;
default:
return false;
}
};
function _15(_23){
_23=_23.nextSibling;
while(_23){
if(_23.tagName&&_23.tagName.toLowerCase()=="td"){
break;
}
_23=_23.nextSibling;
}
return _23;
};
function _19(t){
while((t=t.parentNode)&&t.tagName.toLowerCase()!="table"){
}
return t;
};
function _1c(t){
var tds=_2.withGlobal(_4.window,"query",_2,["td",_19(t)]),len=tds.length;
for(var i=0;i<len;i++){
if(_2.coords(tds[i]).x==_2.coords(t).x){
return tds[i];
}
}
return null;
};
function _20(_24,_25){
if(_2.isIE){
var s=_24.currentStyle,bl=px(_24,s.borderLeftWidth),br=px(_24,s.borderRightWidth),pl=px(_24,s.paddingLeft),pr=px(_24,s.paddingRight);
_24.style.width=_25-bl-br-pl-pr;
}else{
_2.marginBox(_24,{w:_25});
}
function px(_26,_27){
if(!_27){
return 0;
}
if(_27=="medium"){
return 1;
}
if(_27.slice&&_27.slice(-2)=="px"){
return parseFloat(_27);
}
with(_26){
var _28=style.left;
var _29=runtimeStyle.left;
runtimeStyle.left=currentStyle.left;
try{
style.left=_27;
_27=style.pixelLeft;
}
catch(e){
_27=0;
}
style.left=_28;
runtimeStyle.left=_29;
}
return _27;
};
};
}});
_2.subscribe(_1._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
if(o.args&&o.args.command){
var cmd=o.args.command.charAt(0).toLowerCase()+o.args.command.substring(1,o.args.command.length);
if(cmd=="resizeTableColumn"){
o.plugin=new _3.editor.plugins.ResizeTableColumn({commandName:cmd});
}
}
});
});
