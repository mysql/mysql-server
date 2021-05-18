//>>built
define("dojox/editor/plugins/ResizeTableColumn",["dojo","dijit","dojox","./TablePlugins"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.ResizeTableColumn",_4,{constructor:function(){
this.isLtr=this.dir?(this.dir=="ltr"):_1._isBodyLtr();
this.ruleDiv=_1.create("div",{style:"top: -10000px; z-index: 10001"},_1.body(),"last");
},setEditor:function(_6){
var _7=this.ruleDiv;
this.editor=_6;
this.editor.customUndo=true;
this.onEditorLoaded();
_6.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.connect(this.editor.editNode,"onmousemove",function(_8){
var _9=_1.position(_6.iframe,true),ex=_9.x,cx=_8.clientX;
if(!this.isDragging){
var _a=_8.target;
if(_a.tagName&&_a.tagName.toLowerCase()=="td"){
var _b=_1.position(_a),ox=_b.x,ow=_b.w,_c=ex+_b.x-2;
if(this.isLtr){
_7.headerColumn=true;
if(!_23(_a,"first")||cx>ox+ow/2){
_c+=ow;
_7.headerColumn=false;
}
}else{
_7.headerColumn=false;
if(_23(_a,"first")&&cx>ox+ow/2){
_c+=ow;
_7.headerColumn=true;
}
}
_1.style(_7,{position:"absolute",cursor:"col-resize",display:"block",width:"4px",backgroundColor:"transparent",top:_9.y+_b.y+"px",left:_c+"px",height:_b.h+"px"});
this.activeCell=_a;
}else{
_1.style(_7,{display:"none",top:"-10000px"});
}
}else{
var _d=this.activeCell,_e=_1.position(_d),ax=_e.x,aw=_e.w,_f=_17(_d),_10,sx,sw,_11=_1.position(_1b(_d).parentNode),ctx=_11.x,ctw=_11.w;
if(_f){
_10=_1.position(_f);
sx=_10.x;
sw=_10.w;
}
if(this.isLtr&&((_7.headerColumn&&_f&&ctx<cx&&cx<ax+aw)||((!_f&&ax<cx&&cx<ctx+ctw)||(_f&&ax<cx&&cx<sx+sw)))||!this.isLtr&&((_7.headerColumn&&_f&&ctx>cx&&cx>ax)||((!_f&&ax+aw>cx&&cx>ctx)||(_f&&ax+aw>cx&&cx>sx)))){
_1.style(_7,{left:ex+cx+"px"});
}
}
});
this.connect(_7,"onmousedown",function(evt){
var _12=_1.position(_6.iframe,true),_13=_1.position(_1b(this.activeCell));
this.isDragging=true;
_1.style(_6.editNode,{cursor:"col-resize"});
_1.style(_7,{width:"1px",left:evt.clientX+"px",top:_12.y+_13.y+"px",height:_13.h+"px",backgroundColor:"#777"});
});
this.connect(_7,"onmouseup",function(evt){
var _14=this.activeCell,_15=_1.position(_14),aw=_15.w,ax=_15.x,_16=_17(_14),_18,sx,sw,_19=_1.position(_6.iframe),ex=_19.x,_1a=_1b(_14),_1c=_1.position(_1a),cs=_1a.getAttribute("cellspacing"),cx=evt.clientX,_1d=_1e(_14),_1f,_20,_21;
if(!cs||(cs=parseInt(cs,10))<0){
cs=2;
}
if(_16){
_18=_1.position(_16);
sx=_18.x;
sw=_18.w;
_1f=_1e(_16);
}
if(this.isLtr){
if(_7.headerColumn){
_20=ex+ax+aw-cx;
}else{
_20=cx-ex-ax;
if(_16){
_21=ex+sx+sw-cx-cs;
}
}
}else{
if(_7.headerColumn){
_20=cx-ex-ax;
}else{
_20=ex+ax+aw-cx;
if(_16){
_21=cx-ex-sx-cs;
}
}
}
this.isDragging=false;
_22(_1d,_20);
if(_16){
if(!_7.headerColumn){
_22(_1f,_21);
}
}
if(_7.headerColumn&&_23(_14,"first")||_23(_14,"last")){
_1.marginBox(_1a,{w:_1c.w+_20-aw});
}
_22(_1d,_1.position(_14).w);
if(_16){
_22(_1f,_1.position(_16).w);
}
_1.style(_6.editNode,{cursor:"auto"});
_1.style(_7,{display:"none",top:"-10000px"});
this.activeCell=null;
});
}));
function _23(n,b){
var _24=_1.query("> td",n.parentNode);
switch(b){
case "first":
return _24[0]==n;
case "last":
return _24[_24.length-1]==n;
default:
return false;
}
};
function _17(_25){
_25=_25.nextSibling;
while(_25){
if(_25.tagName&&_25.tagName.toLowerCase()=="td"){
break;
}
_25=_25.nextSibling;
}
return _25;
};
function _1b(t){
while((t=t.parentNode)&&t.tagName.toLowerCase()!="table"){
}
return t;
};
function _1e(t){
var tds=_1.query("td",_1b(t)),len=tds.length;
for(var i=0;i<len;i++){
if(_1.position(tds[i]).x==_1.position(t).x){
return tds[i];
}
}
return null;
};
function _22(_26,_27){
if(_1.isIE){
var s=_26.currentStyle,bl=px(_26,s.borderLeftWidth),br=px(_26,s.borderRightWidth),pl=px(_26,s.paddingLeft),pr=px(_26,s.paddingRight);
_26.style.width=_27-bl-br-pl-pr;
}else{
_1.marginBox(_26,{w:_27});
}
function px(_28,_29){
if(!_29){
return 0;
}
if(_29=="medium"){
return 1;
}
if(_29.slice&&_29.slice(-2)=="px"){
return parseFloat(_29);
}
with(_28){
var _2a=style.left;
var _2b=runtimeStyle.left;
runtimeStyle.left=currentStyle.left;
try{
style.left=_29;
_29=style.pixelLeft;
}
catch(e){
_29=0;
}
style.left=_2a;
runtimeStyle.left=_2b;
}
return _29;
};
};
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
if(o.args&&o.args.command){
var cmd=o.args.command.charAt(0).toLowerCase()+o.args.command.substring(1,o.args.command.length);
if(cmd=="resizeTableColumn"){
o.plugin=new _5({commandName:cmd});
}
}
});
return _5;
});
