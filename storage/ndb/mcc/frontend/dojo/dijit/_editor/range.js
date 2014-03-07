//>>built
define("dijit/_editor/range",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window",".."],function(_1,_2,_3,_4,_5){
_5.range={};
_5.range.getIndex=function(_6,_7){
var _8=[],_9=[];
var _a=_6;
var _b,n;
while(_6!=_7){
var i=0;
_b=_6.parentNode;
while((n=_b.childNodes[i++])){
if(n===_6){
--i;
break;
}
}
_8.unshift(i);
_9.unshift(i-_b.childNodes.length);
_6=_b;
}
if(_8.length>0&&_a.nodeType==3){
n=_a.previousSibling;
while(n&&n.nodeType==3){
_8[_8.length-1]--;
n=n.previousSibling;
}
n=_a.nextSibling;
while(n&&n.nodeType==3){
_9[_9.length-1]++;
n=n.nextSibling;
}
}
return {o:_8,r:_9};
};
_5.range.getNode=function(_c,_d){
if(!_3.isArray(_c)||_c.length==0){
return _d;
}
var _e=_d;
_1.every(_c,function(i){
if(i>=0&&i<_e.childNodes.length){
_e=_e.childNodes[i];
}else{
_e=null;
return false;
}
return true;
});
return _e;
};
_5.range.getCommonAncestor=function(n1,n2,_f){
_f=_f||n1.ownerDocument.body;
var _10=function(n){
var as=[];
while(n){
as.unshift(n);
if(n!==_f){
n=n.parentNode;
}else{
break;
}
}
return as;
};
var _11=_10(n1);
var _12=_10(n2);
var m=Math.min(_11.length,_12.length);
var com=_11[0];
for(var i=1;i<m;i++){
if(_11[i]===_12[i]){
com=_11[i];
}else{
break;
}
}
return com;
};
_5.range.getAncestor=function(_13,_14,_15){
_15=_15||_13.ownerDocument.body;
while(_13&&_13!==_15){
var _16=_13.nodeName.toUpperCase();
if(_14.test(_16)){
return _13;
}
_13=_13.parentNode;
}
return null;
};
_5.range.BlockTagNames=/^(?:P|DIV|H1|H2|H3|H4|H5|H6|ADDRESS|PRE|OL|UL|LI|DT|DE)$/;
_5.range.getBlockAncestor=function(_17,_18,_19){
_19=_19||_17.ownerDocument.body;
_18=_18||_5.range.BlockTagNames;
var _1a=null,_1b;
while(_17&&_17!==_19){
var _1c=_17.nodeName.toUpperCase();
if(!_1a&&_18.test(_1c)){
_1a=_17;
}
if(!_1b&&(/^(?:BODY|TD|TH|CAPTION)$/).test(_1c)){
_1b=_17;
}
_17=_17.parentNode;
}
return {blockNode:_1a,blockContainer:_1b||_17.ownerDocument.body};
};
_5.range.atBeginningOfContainer=function(_1d,_1e,_1f){
var _20=false;
var _21=(_1f==0);
if(!_21&&_1e.nodeType==3){
if(/^[\s\xA0]+$/.test(_1e.nodeValue.substr(0,_1f))){
_21=true;
}
}
if(_21){
var _22=_1e;
_20=true;
while(_22&&_22!==_1d){
if(_22.previousSibling){
_20=false;
break;
}
_22=_22.parentNode;
}
}
return _20;
};
_5.range.atEndOfContainer=function(_23,_24,_25){
var _26=false;
var _27=(_25==(_24.length||_24.childNodes.length));
if(!_27&&_24.nodeType==3){
if(/^[\s\xA0]+$/.test(_24.nodeValue.substr(_25))){
_27=true;
}
}
if(_27){
var _28=_24;
_26=true;
while(_28&&_28!==_23){
if(_28.nextSibling){
_26=false;
break;
}
_28=_28.parentNode;
}
}
return _26;
};
_5.range.adjacentNoneTextNode=function(_29,_2a){
var _2b=_29;
var len=(0-_29.length)||0;
var _2c=_2a?"nextSibling":"previousSibling";
while(_2b){
if(_2b.nodeType!=3){
break;
}
len+=_2b.length;
_2b=_2b[_2c];
}
return [_2b,len];
};
_5.range._w3c=Boolean(window["getSelection"]);
_5.range.create=function(_2d){
if(_5.range._w3c){
return (_2d||_4.global).document.createRange();
}else{
return new _5.range.W3CRange;
}
};
_5.range.getSelection=function(win,_2e){
if(_5.range._w3c){
return win.getSelection();
}else{
var s=new _5.range.ie.selection(win);
if(!_2e){
s._getCurrentSelection();
}
return s;
}
};
if(!_5.range._w3c){
_5.range.ie={cachedSelection:{},selection:function(win){
this._ranges=[];
this.addRange=function(r,_2f){
this._ranges.push(r);
if(!_2f){
r._select();
}
this.rangeCount=this._ranges.length;
};
this.removeAllRanges=function(){
this._ranges=[];
this.rangeCount=0;
};
var _30=function(){
var r=win.document.selection.createRange();
var _31=win.document.selection.type.toUpperCase();
if(_31=="CONTROL"){
return new _5.range.W3CRange(_5.range.ie.decomposeControlRange(r));
}else{
return new _5.range.W3CRange(_5.range.ie.decomposeTextRange(r));
}
};
this.getRangeAt=function(i){
return this._ranges[i];
};
this._getCurrentSelection=function(){
this.removeAllRanges();
var r=_30();
if(r){
this.addRange(r,true);
this.isCollapsed=r.collapsed;
}else{
this.isCollapsed=true;
}
};
},decomposeControlRange:function(_32){
var _33=_32.item(0),_34=_32.item(_32.length-1);
var _35=_33.parentNode,_36=_34.parentNode;
var _37=_5.range.getIndex(_33,_35).o[0];
var _38=_5.range.getIndex(_34,_36).o[0]+1;
return [_35,_37,_36,_38];
},getEndPoint:function(_39,end){
var _3a=_39.duplicate();
_3a.collapse(!end);
var _3b="EndTo"+(end?"End":"Start");
var _3c=_3a.parentElement();
var _3d,_3e,_3f;
if(_3c.childNodes.length>0){
_1.every(_3c.childNodes,function(_40,i){
var _41;
if(_40.nodeType!=3){
_3a.moveToElementText(_40);
if(_3a.compareEndPoints(_3b,_39)>0){
if(_3f&&_3f.nodeType==3){
_3d=_3f;
_41=true;
}else{
_3d=_3c;
_3e=i;
return false;
}
}else{
if(i==_3c.childNodes.length-1){
_3d=_3c;
_3e=_3c.childNodes.length;
return false;
}
}
}else{
if(i==_3c.childNodes.length-1){
_3d=_40;
_41=true;
}
}
if(_41&&_3d){
var _42=_5.range.adjacentNoneTextNode(_3d)[0];
if(_42){
_3d=_42.nextSibling;
}else{
_3d=_3c.firstChild;
}
var _43=_5.range.adjacentNoneTextNode(_3d);
_42=_43[0];
var _44=_43[1];
if(_42){
_3a.moveToElementText(_42);
_3a.collapse(false);
}else{
_3a.moveToElementText(_3c);
}
_3a.setEndPoint(_3b,_39);
_3e=_3a.text.length-_44;
return false;
}
_3f=_40;
return true;
});
}else{
_3d=_3c;
_3e=0;
}
if(!end&&_3d.nodeType==1&&_3e==_3d.childNodes.length){
var _45=_3d.nextSibling;
if(_45&&_45.nodeType==3){
_3d=_45;
_3e=0;
}
}
return [_3d,_3e];
},setEndPoint:function(_46,_47,_48){
var _49=_46.duplicate(),_4a,len;
if(_47.nodeType!=3){
if(_48>0){
_4a=_47.childNodes[_48-1];
if(_4a){
if(_4a.nodeType==3){
_47=_4a;
_48=_4a.length;
}else{
if(_4a.nextSibling&&_4a.nextSibling.nodeType==3){
_47=_4a.nextSibling;
_48=0;
}else{
_49.moveToElementText(_4a.nextSibling?_4a:_47);
var _4b=_4a.parentNode;
var _4c=_4b.insertBefore(_4a.ownerDocument.createTextNode(" "),_4a.nextSibling);
_49.collapse(false);
_4b.removeChild(_4c);
}
}
}
}else{
_49.moveToElementText(_47);
_49.collapse(true);
}
}
if(_47.nodeType==3){
var _4d=_5.range.adjacentNoneTextNode(_47);
var _4e=_4d[0];
len=_4d[1];
if(_4e){
_49.moveToElementText(_4e);
_49.collapse(false);
if(_4e.contentEditable!="inherit"){
len++;
}
}else{
_49.moveToElementText(_47.parentNode);
_49.collapse(true);
}
_48+=len;
if(_48>0){
if(_49.move("character",_48)!=_48){
console.error("Error when moving!");
}
}
}
return _49;
},decomposeTextRange:function(_4f){
var _50=_5.range.ie.getEndPoint(_4f);
var _51=_50[0],_52=_50[1];
var _53=_50[0],_54=_50[1];
if(_4f.htmlText.length){
if(_4f.htmlText==_4f.text){
_54=_52+_4f.text.length;
}else{
_50=_5.range.ie.getEndPoint(_4f,true);
_53=_50[0],_54=_50[1];
}
}
return [_51,_52,_53,_54];
},setRange:function(_55,_56,_57,_58,_59,_5a){
var _5b=_5.range.ie.setEndPoint(_55,_56,_57);
_55.setEndPoint("StartToStart",_5b);
if(!_5a){
var end=_5.range.ie.setEndPoint(_55,_58,_59);
}
_55.setEndPoint("EndToEnd",end||_5b);
return _55;
}};
_2("dijit.range.W3CRange",null,{constructor:function(){
if(arguments.length>0){
this.setStart(arguments[0][0],arguments[0][1]);
this.setEnd(arguments[0][2],arguments[0][3]);
}else{
this.commonAncestorContainer=null;
this.startContainer=null;
this.startOffset=0;
this.endContainer=null;
this.endOffset=0;
this.collapsed=true;
}
},_updateInternal:function(){
if(this.startContainer!==this.endContainer){
this.commonAncestorContainer=_5.range.getCommonAncestor(this.startContainer,this.endContainer);
}else{
this.commonAncestorContainer=this.startContainer;
}
this.collapsed=(this.startContainer===this.endContainer)&&(this.startOffset==this.endOffset);
},setStart:function(_5c,_5d){
_5d=parseInt(_5d);
if(this.startContainer===_5c&&this.startOffset==_5d){
return;
}
delete this._cachedBookmark;
this.startContainer=_5c;
this.startOffset=_5d;
if(!this.endContainer){
this.setEnd(_5c,_5d);
}else{
this._updateInternal();
}
},setEnd:function(_5e,_5f){
_5f=parseInt(_5f);
if(this.endContainer===_5e&&this.endOffset==_5f){
return;
}
delete this._cachedBookmark;
this.endContainer=_5e;
this.endOffset=_5f;
if(!this.startContainer){
this.setStart(_5e,_5f);
}else{
this._updateInternal();
}
},setStartAfter:function(_60,_61){
this._setPoint("setStart",_60,_61,1);
},setStartBefore:function(_62,_63){
this._setPoint("setStart",_62,_63,0);
},setEndAfter:function(_64,_65){
this._setPoint("setEnd",_64,_65,1);
},setEndBefore:function(_66,_67){
this._setPoint("setEnd",_66,_67,0);
},_setPoint:function(_68,_69,_6a,ext){
var _6b=_5.range.getIndex(_69,_69.parentNode).o;
this[_68](_69.parentNode,_6b.pop()+ext);
},_getIERange:function(){
var r=(this._body||this.endContainer.ownerDocument.body).createTextRange();
_5.range.ie.setRange(r,this.startContainer,this.startOffset,this.endContainer,this.endOffset,this.collapsed);
return r;
},getBookmark:function(){
this._getIERange();
return this._cachedBookmark;
},_select:function(){
var r=this._getIERange();
r.select();
},deleteContents:function(){
var s=this.startContainer,r=this._getIERange();
if(s.nodeType===3&&!this.startOffset){
this.setStartBefore(s);
}
r.pasteHTML("");
this.endContainer=this.startContainer;
this.endOffset=this.startOffset;
this.collapsed=true;
},cloneRange:function(){
var r=new _5.range.W3CRange([this.startContainer,this.startOffset,this.endContainer,this.endOffset]);
r._body=this._body;
return r;
},detach:function(){
this._body=null;
this.commonAncestorContainer=null;
this.startContainer=null;
this.startOffset=0;
this.endContainer=null;
this.endOffset=0;
this.collapsed=true;
}});
}
return _5.range;
});
