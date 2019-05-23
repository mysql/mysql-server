//>>built
define("dijit/_base/focus",["dojo/_base/array","dojo/dom","dojo/_base/lang","dojo/topic","dojo/_base/window","../focus","../main"],function(_1,_2,_3,_4,_5,_6,_7){
var _8={_curFocus:null,_prevFocus:null,isCollapsed:function(){
return _7.getBookmark().isCollapsed;
},getBookmark:function(){
var bm,rg,tg,_9=_5.doc.selection,cf=_6.curNode;
if(_5.global.getSelection){
_9=_5.global.getSelection();
if(_9){
if(_9.isCollapsed){
tg=cf?cf.tagName:"";
if(tg){
tg=tg.toLowerCase();
if(tg=="textarea"||(tg=="input"&&(!cf.type||cf.type.toLowerCase()=="text"))){
_9={start:cf.selectionStart,end:cf.selectionEnd,node:cf,pRange:true};
return {isCollapsed:(_9.end<=_9.start),mark:_9};
}
}
bm={isCollapsed:true};
if(_9.rangeCount){
bm.mark=_9.getRangeAt(0).cloneRange();
}
}else{
rg=_9.getRangeAt(0);
bm={isCollapsed:false,mark:rg.cloneRange()};
}
}
}else{
if(_9){
tg=cf?cf.tagName:"";
tg=tg.toLowerCase();
if(cf&&tg&&(tg=="button"||tg=="textarea"||tg=="input")){
if(_9.type&&_9.type.toLowerCase()=="none"){
return {isCollapsed:true,mark:null};
}else{
rg=_9.createRange();
return {isCollapsed:rg.text&&rg.text.length?false:true,mark:{range:rg,pRange:true}};
}
}
bm={};
try{
rg=_9.createRange();
bm.isCollapsed=!(_9.type=="Text"?rg.htmlText.length:rg.length);
}
catch(e){
bm.isCollapsed=true;
return bm;
}
if(_9.type.toUpperCase()=="CONTROL"){
if(rg.length){
bm.mark=[];
var i=0,_a=rg.length;
while(i<_a){
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
},moveToBookmark:function(_b){
var _c=_5.doc,_d=_b.mark;
if(_d){
if(_5.global.getSelection){
var _e=_5.global.getSelection();
if(_e&&_e.removeAllRanges){
if(_d.pRange){
var n=_d.node;
n.selectionStart=_d.start;
n.selectionEnd=_d.end;
}else{
_e.removeAllRanges();
_e.addRange(_d);
}
}else{
console.warn("No idea how to restore selection for this browser!");
}
}else{
if(_c.selection&&_d){
var rg;
if(_d.pRange){
rg=_d.range;
}else{
if(_3.isArray(_d)){
rg=_c.body.createControlRange();
_1.forEach(_d,function(n){
rg.addElement(n);
});
}else{
rg=_c.body.createTextRange();
rg.moveToBookmark(_d);
}
}
rg.select();
}
}
}
},getFocus:function(_f,_10){
var _11=!_6.curNode||(_f&&_2.isDescendant(_6.curNode,_f.domNode))?_7._prevFocus:_6.curNode;
return {node:_11,bookmark:_11&&(_11==_6.curNode)&&_5.withGlobal(_10||_5.global,_7.getBookmark),openedForWindow:_10};
},_activeStack:[],registerIframe:function(_12){
return _6.registerIframe(_12);
},unregisterIframe:function(_13){
_13&&_13.remove();
},registerWin:function(_14,_15){
return _6.registerWin(_14,_15);
},unregisterWin:function(_16){
_16&&_16.remove();
}};
_6.focus=function(_17){
if(!_17){
return;
}
var _18="node" in _17?_17.node:_17,_19=_17.bookmark,_1a=_17.openedForWindow,_1b=_19?_19.isCollapsed:false;
if(_18){
var _1c=(_18.tagName.toLowerCase()=="iframe")?_18.contentWindow:_18;
if(_1c&&_1c.focus){
try{
_1c.focus();
}
catch(e){
}
}
_6._onFocusNode(_18);
}
if(_19&&_5.withGlobal(_1a||_5.global,_7.isCollapsed)&&!_1b){
if(_1a){
_1a.focus();
}
try{
_5.withGlobal(_1a||_5.global,_7.moveToBookmark,null,[_19]);
}
catch(e2){
}
}
};
_6.watch("curNode",function(_1d,_1e,_1f){
_7._curFocus=_1f;
_7._prevFocus=_1e;
if(_1f){
_4.publish("focusNode",_1f);
}
});
_6.watch("activeStack",function(_20,_21,_22){
_7._activeStack=_22;
});
_6.on("widget-blur",function(_23,by){
_4.publish("widgetBlur",_23,by);
});
_6.on("widget-focus",function(_24,by){
_4.publish("widgetFocus",_24,by);
});
_3.mixin(_7,_8);
return _7;
});
