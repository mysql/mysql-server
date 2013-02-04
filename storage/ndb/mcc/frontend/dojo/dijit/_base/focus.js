//>>built
define("dijit/_base/focus",["dojo/_base/array","dojo/dom","dojo/_base/lang","dojo/topic","dojo/_base/window","../focus",".."],function(_1,_2,_3,_4,_5,_6,_7){
_3.mixin(_7,{_curFocus:null,_prevFocus:null,isCollapsed:function(){
return _7.getBookmark().isCollapsed;
},getBookmark:function(){
var bm,rg,tg,_8=_5.doc.selection,cf=_6.curNode;
if(_5.global.getSelection){
_8=_5.global.getSelection();
if(_8){
if(_8.isCollapsed){
tg=cf?cf.tagName:"";
if(tg){
tg=tg.toLowerCase();
if(tg=="textarea"||(tg=="input"&&(!cf.type||cf.type.toLowerCase()=="text"))){
_8={start:cf.selectionStart,end:cf.selectionEnd,node:cf,pRange:true};
return {isCollapsed:(_8.end<=_8.start),mark:_8};
}
}
bm={isCollapsed:true};
if(_8.rangeCount){
bm.mark=_8.getRangeAt(0).cloneRange();
}
}else{
rg=_8.getRangeAt(0);
bm={isCollapsed:false,mark:rg.cloneRange()};
}
}
}else{
if(_8){
tg=cf?cf.tagName:"";
tg=tg.toLowerCase();
if(cf&&tg&&(tg=="button"||tg=="textarea"||tg=="input")){
if(_8.type&&_8.type.toLowerCase()=="none"){
return {isCollapsed:true,mark:null};
}else{
rg=_8.createRange();
return {isCollapsed:rg.text&&rg.text.length?false:true,mark:{range:rg,pRange:true}};
}
}
bm={};
try{
rg=_8.createRange();
bm.isCollapsed=!(_8.type=="Text"?rg.htmlText.length:rg.length);
}
catch(e){
bm.isCollapsed=true;
return bm;
}
if(_8.type.toUpperCase()=="CONTROL"){
if(rg.length){
bm.mark=[];
var i=0,_9=rg.length;
while(i<_9){
bm.mark.push(rg.item(i++));
}
}else{
bm.isCollapsed=true;
bm.mark=null;
}
}else{
bm.mark=rg.getBookmark();
}
}else{
console.warn("No idea how to store the current selection for this browser!");
}
}
return bm;
},moveToBookmark:function(_a){
var _b=_5.doc,_c=_a.mark;
if(_c){
if(_5.global.getSelection){
var _d=_5.global.getSelection();
if(_d&&_d.removeAllRanges){
if(_c.pRange){
var n=_c.node;
n.selectionStart=_c.start;
n.selectionEnd=_c.end;
}else{
_d.removeAllRanges();
_d.addRange(_c);
}
}else{
console.warn("No idea how to restore selection for this browser!");
}
}else{
if(_b.selection&&_c){
var rg;
if(_c.pRange){
rg=_c.range;
}else{
if(_3.isArray(_c)){
rg=_b.body.createControlRange();
_1.forEach(_c,function(n){
rg.addElement(n);
});
}else{
rg=_b.body.createTextRange();
rg.moveToBookmark(_c);
}
}
rg.select();
}
}
}
},getFocus:function(_e,_f){
var _10=!_6.curNode||(_e&&_2.isDescendant(_6.curNode,_e.domNode))?_7._prevFocus:_6.curNode;
return {node:_10,bookmark:_10&&(_10==_6.curNode)&&_5.withGlobal(_f||_5.global,_7.getBookmark),openedForWindow:_f};
},_activeStack:[],registerIframe:function(_11){
return _6.registerIframe(_11);
},unregisterIframe:function(_12){
_12&&_12.remove();
},registerWin:function(_13,_14){
return _6.registerWin(_13,_14);
},unregisterWin:function(_15){
_15&&_15.remove();
}});
_6.focus=function(_16){
if(!_16){
return;
}
var _17="node" in _16?_16.node:_16,_18=_16.bookmark,_19=_16.openedForWindow,_1a=_18?_18.isCollapsed:false;
if(_17){
var _1b=(_17.tagName.toLowerCase()=="iframe")?_17.contentWindow:_17;
if(_1b&&_1b.focus){
try{
_1b.focus();
}
catch(e){
}
}
_6._onFocusNode(_17);
}
if(_18&&_5.withGlobal(_19||_5.global,_7.isCollapsed)&&!_1a){
if(_19){
_19.focus();
}
try{
_5.withGlobal(_19||_5.global,_7.moveToBookmark,null,[_18]);
}
catch(e2){
}
}
};
_6.watch("curNode",function(_1c,_1d,_1e){
_7._curFocus=_1e;
_7._prevFocus=_1d;
if(_1e){
_4.publish("focusNode",_1e);
}
});
_6.watch("activeStack",function(_1f,_20,_21){
_7._activeStack=_21;
});
_6.on("widget-blur",function(_22,by){
_4.publish("widgetBlur",_22,by);
});
_6.on("widget-focus",function(_23,by){
_4.publish("widgetFocus",_23,by);
});
return _7;
});
