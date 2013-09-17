//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/stencil/Text"],function(_1,_2,_3){
_2.provide("dojox.drawing.tools.TextBlock");
_2.require("dojox.drawing.stencil.Text");
(function(){
var _4;
_2.addOnLoad(function(){
_4=_2.byId("conEdit");
if(!_4){
console.error("A contenteditable div is missing from the main document. See 'dojox.drawing.tools.TextBlock'");
}else{
_4.parentNode.removeChild(_4);
}
});
_3.drawing.tools.TextBlock=_3.drawing.util.oo.declare(_3.drawing.stencil.Text,function(_5){
if(_5.data){
var d=_5.data;
var _6=d.text?this.typesetter(d.text):d.text;
var w=!d.width?this.style.text.minWidth:d.width=="auto"?"auto":Math.max(d.width,this.style.text.minWidth);
var h=this._lineHeight;
if(_6&&w=="auto"){
var o=this.measureText(this.cleanText(_6,false),w);
w=o.w;
h=o.h;
}else{
this._text="";
}
this.points=[{x:d.x,y:d.y},{x:d.x+w,y:d.y},{x:d.x+w,y:d.y+h},{x:d.x,y:d.y+h}];
if(d.showEmpty||_6){
this.editMode=true;
_2.disconnect(this._postRenderCon);
this._postRenderCon=null;
this.connect(this,"render",this,"onRender",true);
if(d.showEmpty){
this._text=_6||"";
this.edit();
}else{
if(_6&&d.editMode){
this._text="";
this.edit();
}else{
if(_6){
this.render(_6);
}
}
}
setTimeout(_2.hitch(this,function(){
this.editMode=false;
}),100);
}else{
this.render();
}
}else{
this.connectMouse();
this._postRenderCon=_2.connect(this,"render",this,"_onPostRender");
}
},{draws:true,baseRender:false,type:"dojox.drawing.tools.TextBlock",_caretStart:0,_caretEnd:0,_blockExec:false,selectOnExec:true,showEmpty:false,onDrag:function(_7){
if(!this.parentNode){
this.showParent(_7);
}
var s=this._startdrag,e=_7.page;
this._box.left=(s.x<e.x?s.x:e.x);
this._box.top=s.y;
this._box.width=(s.x<e.x?e.x-s.x:s.x-e.x)+this.style.text.pad;
_2.style(this.parentNode,this._box.toPx());
},onUp:function(_8){
if(!this._downOnCanvas){
return;
}
this._downOnCanvas=false;
var c=_2.connect(this,"render",this,function(){
_2.disconnect(c);
this.onRender(this);
});
this.editMode=true;
this.showParent(_8);
this.created=true;
this.createTextField();
this.connectTextField();
},showParent:function(_9){
if(this.parentNode){
return;
}
var x=_9.pageX||10;
var y=_9.pageY||10;
this.parentNode=_2.doc.createElement("div");
this.parentNode.id=this.id;
var d=this.style.textMode.create;
this._box={left:x,top:y,width:_9.width||1,height:_9.height&&_9.height>8?_9.height:this._lineHeight,border:d.width+"px "+d.style+" "+d.color,position:"absolute",zIndex:500,toPx:function(){
var o={};
for(var nm in this){
o[nm]=typeof (this[nm])=="number"&&nm!="zIndex"?this[nm]+"px":this[nm];
}
return o;
}};
_2.style(this.parentNode,this._box);
document.body.appendChild(this.parentNode);
},createTextField:function(_a){
var d=this.style.textMode.edit;
this._box.border=d.width+"px "+d.style+" "+d.color;
this._box.height="auto";
this._box.width=Math.max(this._box.width,this.style.text.minWidth*this.mouse.zoom);
_2.style(this.parentNode,this._box.toPx());
this.parentNode.appendChild(_4);
_2.style(_4,{height:_a?"auto":this._lineHeight+"px",fontSize:(this.textSize/this.mouse.zoom)+"px",fontFamily:this.style.text.family});
_4.innerHTML=_a||"";
return _4;
},connectTextField:function(){
if(this._textConnected){
return;
}
var _b=_1.byId("greekPalette");
var _c=_b==undefined?false:true;
if(_c){
_2.mixin(_b,{_pushChangeTo:_4,_textBlock:this});
}
this._textConnected=true;
this._dropMode=false;
this.mouse.setEventMode("TEXT");
this.keys.editMode(true);
var _d,_e,_f,kc4,_10=this,_11=false,_12=function(){
if(_10._dropMode){
return;
}
_2.forEach([_d,_e,_f,kc4],function(c){
_2.disconnect(c);
});
_10._textConnected=false;
_10.keys.editMode(false);
_10.mouse.setEventMode();
_10.execText();
};
_d=_2.connect(_4,"keyup",this,function(evt){
if(_2.trim(_4.innerHTML)&&!_11){
_2.style(_4,"height","auto");
_11=true;
}else{
if(_2.trim(_4.innerHTML).length<2&&_11){
_2.style(_4,"height",this._lineHeight+"px");
_11=false;
}
}
if(!this._blockExec){
if(evt.keyCode==13||evt.keyCode==27){
_2.stopEvent(evt);
_12();
}
}else{
if(evt.keyCode==_2.keys.SPACE){
_2.stopEvent(evt);
_c&&_b.onCancel();
}
}
});
_e=_2.connect(_4,"keydown",this,function(evt){
if(evt.keyCode==13||evt.keyCode==27){
_2.stopEvent(evt);
}
if(evt.keyCode==220){
if(!_c){
return;
}
_2.stopEvent(evt);
this.getSelection(_4);
this.insertText(_4,"\\");
this._dropMode=true;
this._blockExec=true;
_b.show({around:this.parentNode,orient:{"BL":"TL"}});
}
if(!this._dropMode){
this._blockExec=false;
}else{
switch(evt.keyCode){
case _2.keys.UP_ARROW:
case _2.keys.DOWN_ARROW:
case _2.keys.LEFT_ARROW:
case _2.keys.RIGHT_ARROW:
_2.stopEvent(evt);
_b._navigateByArrow(evt);
break;
case _2.keys.ENTER:
_2.stopEvent(evt);
_b._onCellClick(evt);
break;
case _2.keys.BACKSPACE:
case _2.keys.DELETE:
_2.stopEvent(evt);
_b.onCancel();
break;
}
}
});
_f=_2.connect(document,"mouseup",this,function(evt){
if(!this._onAnchor&&evt.target.id!="conEdit"){
_2.stopEvent(evt);
_12();
}else{
if(evt.target.id=="conEdit"&&_4.innerHTML==""){
_4.blur();
setTimeout(function(){
_4.focus();
},200);
}
}
});
this.createAnchors();
kc4=_2.connect(this.mouse,"setZoom",this,function(evt){
_12();
});
_4.focus();
this.onDown=function(){
};
this.onDrag=function(){
};
setTimeout(_2.hitch(this,function(){
_4.focus();
this.onUp=function(){
if(!_10._onAnchor&&this.parentNode){
_10.disconnectMouse();
_12();
_10.onUp=function(){
};
}
};
}),500);
},execText:function(){
var d=_2.marginBox(this.parentNode);
var w=Math.max(d.w,this.style.text.minWidth);
var txt=this.cleanText(_4.innerHTML,true);
_4.innerHTML="";
_4.blur();
this.destroyAnchors();
txt=this.typesetter(txt);
var o=this.measureText(txt,w);
var sc=this.mouse.scrollOffset();
var org=this.mouse.origin;
var x=this._box.left+sc.left-org.x;
var y=this._box.top+sc.top-org.y;
x*=this.mouse.zoom;
y*=this.mouse.zoom;
w*=this.mouse.zoom;
o.h*=this.mouse.zoom;
this.points=[{x:x,y:y},{x:x+w,y:y},{x:x+w,y:y+o.h},{x:x,y:y+o.h}];
this.editMode=false;
if(!o.text){
this._text="";
this._textArray=[];
}
this.render(o.text);
this.onChangeText(this.getText());
},edit:function(){
this.editMode=true;
var _13=this.getText()||"";
if(this.parentNode||!this.points){
return;
}
var d=this.pointsToData();
var sc=this.mouse.scrollOffset();
var org=this.mouse.origin;
var obj={pageX:(d.x)/this.mouse.zoom-sc.left+org.x,pageY:(d.y)/this.mouse.zoom-sc.top+org.y,width:d.width/this.mouse.zoom,height:d.height/this.mouse.zoom};
this.remove(this.shape,this.hit);
this.showParent(obj);
this.createTextField(_13.replace("/n"," "));
this.connectTextField();
if(_13){
this.setSelection(_4,"end");
}
},cleanText:function(txt,_14){
var _15=function(str){
var _16={"&lt;":"<","&gt;":">","&amp;":"&"};
for(var nm in _16){
str=str.replace(new RegExp(nm,"gi"),_16[nm]);
}
return str;
};
if(_14){
_2.forEach(["<br>","<br/>","<br />","\\n","\\r"],function(br){
txt=txt.replace(new RegExp(br,"gi")," ");
});
}
txt=txt.replace(/&nbsp;/g," ");
txt=_15(txt);
txt=_2.trim(txt);
txt=txt.replace(/\s{2,}/g," ");
return txt;
},measureText:function(str,_17){
var r="(<br\\s*/*>)|(\\n)|(\\r)";
this.showParent({width:_17||"auto",height:"auto"});
this.createTextField(str);
var txt="";
var el=_4;
el.innerHTML="X";
var h=_2.marginBox(el).h;
el.innerHTML=str;
if(!_17||new RegExp(r,"gi").test(str)){
txt=str.replace(new RegExp(r,"gi"),"\n");
el.innerHTML=str.replace(new RegExp(r,"gi"),"<br/>");
}else{
if(_2.marginBox(el).h==h){
txt=str;
}else{
var ar=str.split(" ");
var _18=[[]];
var _19=0;
el.innerHTML="";
while(ar.length){
var _1a=ar.shift();
el.innerHTML+=_1a+" ";
if(_2.marginBox(el).h>h){
_19++;
_18[_19]=[];
el.innerHTML=_1a+" ";
}
_18[_19].push(_1a);
}
_2.forEach(_18,function(ar,i){
_18[i]=ar.join(" ");
});
txt=_18.join("\n");
el.innerHTML=txt.replace("\n","<br/>");
}
}
var dim=_2.marginBox(el);
_4.parentNode.removeChild(_4);
_2.destroy(this.parentNode);
this.parentNode=null;
return {h:dim.h,w:dim.w,text:txt};
},_downOnCanvas:false,onDown:function(obj){
this._startdrag={x:obj.pageX,y:obj.pageY};
_2.disconnect(this._postRenderCon);
this._postRenderCon=null;
this._downOnCanvas=true;
},createAnchors:function(){
this._anchors={};
var _1b=this;
var d=this.style.anchors,b=d.width,w=d.size-b*2,h=d.size-b*2,p=(d.size)/2*-1+"px";
var s={position:"absolute",width:w+"px",height:h+"px",backgroundColor:d.fill,border:b+"px "+d.style+" "+d.color};
if(_2.isIE){
s.paddingLeft=w+"px";
s.fontSize=w+"px";
}
var ss=[{top:p,left:p},{top:p,right:p},{bottom:p,right:p},{bottom:p,left:p}];
for(var i=0;i<4;i++){
var _1c=(i==0)||(i==3);
var id=this.util.uid(_1c?"left_anchor":"right_anchor");
var a=_2.create("div",{id:id},this.parentNode);
_2.style(a,_2.mixin(_2.clone(s),ss[i]));
var md,mm,mu;
var md=_2.connect(a,"mousedown",this,function(evt){
_1c=evt.target.id.indexOf("left")>-1;
_1b._onAnchor=true;
var _1d=evt.pageX;
var _1e=this._box.width;
_2.stopEvent(evt);
mm=_2.connect(document,"mousemove",this,function(evt){
var x=evt.pageX;
if(_1c){
this._box.left=x;
this._box.width=_1e+_1d-x;
}else{
this._box.width=x+_1e-_1d;
}
_2.style(this.parentNode,this._box.toPx());
});
mu=_2.connect(document,"mouseup",this,function(evt){
_1d=this._box.left;
_1e=this._box.width;
_2.disconnect(mm);
_2.disconnect(mu);
_1b._onAnchor=false;
_4.focus();
_2.stopEvent(evt);
});
});
this._anchors[id]={a:a,cons:[md]};
}
},destroyAnchors:function(){
for(var n in this._anchors){
_2.forEach(this._anchors[n].con,_2.disconnect,_2);
_2.destroy(this._anchors[n].a);
}
},setSavedCaret:function(val){
this._caretStart=this._caretEnd=val;
},getSavedCaret:function(){
return {start:this._caretStart,end:this._caretEnd};
},insertText:function(_1f,val){
var t,_20=_1f.innerHTML;
var _21=this.getSavedCaret();
_20=_20.replace(/&nbsp;/g," ");
t=_20.substr(0,_21.start)+val+_20.substr(_21.end);
t=this.cleanText(t,true);
this.setSavedCaret(Math.min(t.length,(_21.end+val.length)));
_1f.innerHTML=t;
this.setSelection(_1f,"stored");
},getSelection:function(_22){
var _23,end;
if(_2.doc.selection){
var r=_2.doc.selection.createRange();
var rs=_2.body().createTextRange();
rs.moveToElementText(_22);
var re=rs.duplicate();
rs.moveToBookmark(r.getBookmark());
re.setEndPoint("EndToStart",rs);
_23=this._caretStart=re.text.length;
end=this._caretEnd=re.text.length+r.text.length;
console.warn("Caret start: ",_23," end: ",end," length: ",re.text.length," text: ",re.text);
}else{
this._caretStart=_2.global.getSelection().getRangeAt(_22).startOffset;
this._caretEnd=_2.global.getSelection().getRangeAt(_22).endOffset;
}
},setSelection:function(_24,_25){
console.warn("setSelection:");
if(_2.doc.selection){
var rs=_2.body().createTextRange();
rs.moveToElementText(_24);
switch(_25){
case "end":
rs.collapse(false);
break;
case "beg"||"start":
rs.collapse();
break;
case "all":
rs.collapse();
rs.moveStart("character",0);
rs.moveEnd("character",_24.text.length);
break;
case "stored":
rs.collapse();
var dif=this._caretStart-this._caretEnd;
rs.moveStart("character",this._caretStart);
rs.moveEnd("character",dif);
break;
}
rs.select();
}else{
var _26=function(_27,_28){
_28=_28||[];
for(var i=0;i<_27.childNodes.length;i++){
var n=_27.childNodes[i];
if(n.nodeType==3){
_28.push(n);
}else{
if(n.tagName&&n.tagName.toLowerCase()=="img"){
_28.push(n);
}
}
if(n.childNodes&&n.childNodes.length){
_26(n,_28);
}
}
return _28;
};
_24.focus();
var _29=_2.global.getSelection();
_29.removeAllRanges();
var r=_2.doc.createRange();
var _2a=_26(_24);
switch(_25){
case "end":
undefined;
r.setStart(_2a[_2a.length-1],_2a[_2a.length-1].textContent.length);
r.setEnd(_2a[_2a.length-1],_2a[_2a.length-1].textContent.length);
break;
case "beg"||"start":
r.setStart(_2a[0],0);
r.setEnd(_2a[0],0);
break;
case "all":
r.setStart(_2a[0],0);
r.setEnd(_2a[_2a.length-1],_2a[_2a.length-1].textContent.length);
break;
case "stored":
undefined;
r.setStart(_2a[0],this._caretStart);
r.setEnd(_2a[0],this._caretEnd);
}
_29.addRange(r);
}
}});
_3.drawing.tools.TextBlock.setup={name:"dojox.drawing.tools.TextBlock",tooltip:"Text Tool",iconClass:"iconText"};
_3.drawing.register(_3.drawing.tools.TextBlock.setup,"tool");
})();
});
