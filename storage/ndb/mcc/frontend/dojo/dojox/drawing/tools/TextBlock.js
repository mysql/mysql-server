//>>built
define("dojox/drawing/tools/TextBlock",["dojo","dijit/registry","../util/oo","../manager/_registry","../stencil/Text"],function(_1,_2,oo,_3,_4){
var _5;
_1.addOnLoad(function(){
_5=_1.byId("conEdit");
if(!_5){
console.error("A contenteditable div is missing from the main document. See 'dojox.drawing.tools.TextBlock'");
}else{
_5.parentNode.removeChild(_5);
}
});
var _6=oo.declare(_4,function(_7){
if(_7.data){
var d=_7.data;
var _8=d.text?this.typesetter(d.text):d.text;
var w=!d.width?this.style.text.minWidth:d.width=="auto"?"auto":Math.max(d.width,this.style.text.minWidth);
var h=this._lineHeight;
if(_8&&w=="auto"){
var o=this.measureText(this.cleanText(_8,false),w);
w=o.w;
h=o.h;
}else{
this._text="";
}
this.points=[{x:d.x,y:d.y},{x:d.x+w,y:d.y},{x:d.x+w,y:d.y+h},{x:d.x,y:d.y+h}];
if(d.showEmpty||_8){
this.editMode=true;
_1.disconnect(this._postRenderCon);
this._postRenderCon=null;
this.connect(this,"render",this,"onRender",true);
if(d.showEmpty){
this._text=_8||"";
this.edit();
}else{
if(_8&&d.editMode){
this._text="";
this.edit();
}else{
if(_8){
this.render(_8);
}
}
}
setTimeout(_1.hitch(this,function(){
this.editMode=false;
}),100);
}else{
this.render();
}
}else{
this.connectMouse();
this._postRenderCon=_1.connect(this,"render",this,"_onPostRender");
}
},{draws:true,baseRender:false,type:"dojox.drawing.tools.TextBlock",_caretStart:0,_caretEnd:0,_blockExec:false,selectOnExec:true,showEmpty:false,onDrag:function(_9){
if(!this.parentNode){
this.showParent(_9);
}
var s=this._startdrag,e=_9.page;
this._box.left=(s.x<e.x?s.x:e.x);
this._box.top=s.y;
this._box.width=(s.x<e.x?e.x-s.x:s.x-e.x)+this.style.text.pad;
_1.style(this.parentNode,this._box.toPx());
},onUp:function(_a){
if(!this._downOnCanvas){
return;
}
this._downOnCanvas=false;
var c=_1.connect(this,"render",this,function(){
_1.disconnect(c);
this.onRender(this);
});
this.editMode=true;
this.showParent(_a);
this.created=true;
this.createTextField();
this.connectTextField();
},showParent:function(_b){
if(this.parentNode){
return;
}
var x=_b.pageX||10;
var y=_b.pageY||10;
this.parentNode=_1.doc.createElement("div");
this.parentNode.id=this.id;
var d=this.style.textMode.create;
this._box={left:x,top:y,width:_b.width||1,height:_b.height&&_b.height>8?_b.height:this._lineHeight,border:d.width+"px "+d.style+" "+d.color,position:"absolute",zIndex:500,toPx:function(){
var o={};
for(var nm in this){
o[nm]=typeof (this[nm])=="number"&&nm!="zIndex"?this[nm]+"px":this[nm];
}
return o;
}};
_1.style(this.parentNode,this._box);
document.body.appendChild(this.parentNode);
},createTextField:function(_c){
var d=this.style.textMode.edit;
this._box.border=d.width+"px "+d.style+" "+d.color;
this._box.height="auto";
this._box.width=Math.max(this._box.width,this.style.text.minWidth*this.mouse.zoom);
_1.style(this.parentNode,this._box.toPx());
this.parentNode.appendChild(_5);
_1.style(_5,{height:_c?"auto":this._lineHeight+"px",fontSize:(this.textSize/this.mouse.zoom)+"px",fontFamily:this.style.text.family});
_5.innerHTML=_c||"";
return _5;
},connectTextField:function(){
if(this._textConnected){
return;
}
var _d=_2.byId("greekPalette");
var _e=_d==undefined?false:true;
if(_e){
_1.mixin(_d,{_pushChangeTo:_5,_textBlock:this});
}
this._textConnected=true;
this._dropMode=false;
this.mouse.setEventMode("TEXT");
this.keys.editMode(true);
var _f,kc2,kc3,kc4,_10=this,_11=false,_12=function(){
if(_10._dropMode){
return;
}
_1.forEach([_f,kc2,kc3,kc4],function(c){
_1.disconnect(c);
});
_10._textConnected=false;
_10.keys.editMode(false);
_10.mouse.setEventMode();
_10.execText();
};
_f=_1.connect(_5,"keyup",this,function(evt){
if(_1.trim(_5.innerHTML)&&!_11){
_1.style(_5,"height","auto");
_11=true;
}else{
if(_1.trim(_5.innerHTML).length<2&&_11){
_1.style(_5,"height",this._lineHeight+"px");
_11=false;
}
}
if(!this._blockExec){
if(evt.keyCode==13||evt.keyCode==27){
_1.stopEvent(evt);
_12();
}
}else{
if(evt.keyCode==_1.keys.SPACE){
_1.stopEvent(evt);
_e&&_d.onCancel();
}
}
});
kc2=_1.connect(_5,"keydown",this,function(evt){
if(evt.keyCode==13||evt.keyCode==27){
_1.stopEvent(evt);
}
if(evt.keyCode==220){
if(!_e){
return;
}
_1.stopEvent(evt);
this.getSelection(_5);
this.insertText(_5,"\\");
this._dropMode=true;
this._blockExec=true;
_d.show({around:this.parentNode,orient:{"BL":"TL"}});
}
if(!this._dropMode){
this._blockExec=false;
}else{
switch(evt.keyCode){
case _1.keys.UP_ARROW:
case _1.keys.DOWN_ARROW:
case _1.keys.LEFT_ARROW:
case _1.keys.RIGHT_ARROW:
_1.stopEvent(evt);
_d._navigateByArrow(evt);
break;
case _1.keys.ENTER:
_1.stopEvent(evt);
_d._onCellClick(evt);
break;
case _1.keys.BACKSPACE:
case _1.keys.DELETE:
_1.stopEvent(evt);
_d.onCancel();
break;
}
}
});
kc3=_1.connect(document,"mouseup",this,function(evt){
if(!this._onAnchor&&evt.target.id!="conEdit"){
_1.stopEvent(evt);
_12();
}else{
if(evt.target.id=="conEdit"&&_5.innerHTML==""){
_5.blur();
setTimeout(function(){
_5.focus();
},200);
}
}
});
this.createAnchors();
kc4=_1.connect(this.mouse,"setZoom",this,function(evt){
_12();
});
_5.focus();
this.onDown=function(){
};
this.onDrag=function(){
};
setTimeout(_1.hitch(this,function(){
_5.focus();
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
var d=_1.marginBox(this.parentNode);
var w=Math.max(d.w,this.style.text.minWidth);
var txt=this.cleanText(_5.innerHTML,true);
_5.innerHTML="";
_5.blur();
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
this.setSelection(_5,"end");
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
_1.forEach(["<br>","<br/>","<br />","\\n","\\r"],function(br){
txt=txt.replace(new RegExp(br,"gi")," ");
});
}
txt=txt.replace(/&nbsp;/g," ");
txt=_15(txt);
txt=_1.trim(txt);
txt=txt.replace(/\s{2,}/g," ");
return txt;
},measureText:function(str,_17){
var r="(<br\\s*/*>)|(\\n)|(\\r)";
this.showParent({width:_17||"auto",height:"auto"});
this.createTextField(str);
var txt="";
var el=_5;
el.innerHTML="X";
var h=_1.marginBox(el).h;
el.innerHTML=str;
if(!_17||new RegExp(r,"gi").test(str)){
txt=str.replace(new RegExp(r,"gi"),"\n");
el.innerHTML=str.replace(new RegExp(r,"gi"),"<br/>");
}else{
if(_1.marginBox(el).h==h){
txt=str;
}else{
var ar=str.split(" ");
var _18=[[]];
var _19=0;
el.innerHTML="";
while(ar.length){
var _1a=ar.shift();
el.innerHTML+=_1a+" ";
if(_1.marginBox(el).h>h){
_19++;
_18[_19]=[];
el.innerHTML=_1a+" ";
}
_18[_19].push(_1a);
}
_1.forEach(_18,function(ar,i){
_18[i]=ar.join(" ");
});
txt=_18.join("\n");
el.innerHTML=txt.replace("\n","<br/>");
}
}
var dim=_1.marginBox(el);
_5.parentNode.removeChild(_5);
_1.destroy(this.parentNode);
this.parentNode=null;
return {h:dim.h,w:dim.w,text:txt};
},_downOnCanvas:false,onDown:function(obj){
this._startdrag={x:obj.pageX,y:obj.pageY};
_1.disconnect(this._postRenderCon);
this._postRenderCon=null;
this._downOnCanvas=true;
},createAnchors:function(){
this._anchors={};
var _1b=this;
var d=this.style.anchors,b=d.width,w=d.size-b*2,h=d.size-b*2,p=(d.size)/2*-1+"px";
var s={position:"absolute",width:w+"px",height:h+"px",backgroundColor:d.fill,border:b+"px "+d.style+" "+d.color};
if(_1.isIE){
s.paddingLeft=w+"px";
s.fontSize=w+"px";
}
var ss=[{top:p,left:p},{top:p,right:p},{bottom:p,right:p},{bottom:p,left:p}];
for(var i=0;i<4;i++){
var _1c=(i==0)||(i==3);
var id=this.util.uid(_1c?"left_anchor":"right_anchor");
var a=_1.create("div",{id:id},this.parentNode);
_1.style(a,_1.mixin(_1.clone(s),ss[i]));
var md,mm,mu;
var md=_1.connect(a,"mousedown",this,function(evt){
_1c=evt.target.id.indexOf("left")>-1;
_1b._onAnchor=true;
var _1d=evt.pageX;
var _1e=this._box.width;
_1.stopEvent(evt);
mm=_1.connect(document,"mousemove",this,function(evt){
var x=evt.pageX;
if(_1c){
this._box.left=x;
this._box.width=_1e+_1d-x;
}else{
this._box.width=x+_1e-_1d;
}
_1.style(this.parentNode,this._box.toPx());
});
mu=_1.connect(document,"mouseup",this,function(evt){
_1d=this._box.left;
_1e=this._box.width;
_1.disconnect(mm);
_1.disconnect(mu);
_1b._onAnchor=false;
_5.focus();
_1.stopEvent(evt);
});
});
this._anchors[id]={a:a,cons:[md]};
}
},destroyAnchors:function(){
for(var n in this._anchors){
_1.forEach(this._anchors[n].con,_1.disconnect,_1);
_1.destroy(this._anchors[n].a);
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
if(_1.doc.selection){
var r=_1.doc.selection.createRange();
var rs=_1.body().createTextRange();
rs.moveToElementText(_22);
var re=rs.duplicate();
rs.moveToBookmark(r.getBookmark());
re.setEndPoint("EndToStart",rs);
_23=this._caretStart=re.text.length;
end=this._caretEnd=re.text.length+r.text.length;
console.warn("Caret start: ",_23," end: ",end," length: ",re.text.length," text: ",re.text);
}else{
this._caretStart=_1.global.getSelection().getRangeAt(_22).startOffset;
this._caretEnd=_1.global.getSelection().getRangeAt(_22).endOffset;
}
},setSelection:function(_24,_25){
console.warn("setSelection:");
if(_1.doc.selection){
var rs=_1.body().createTextRange();
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
var _29=_1.global.getSelection();
_29.removeAllRanges();
var r=_1.doc.createRange();
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
_1.setObject("dojox.drawing.tools.TextBlock",_6);
_6.setup={name:"dojox.drawing.tools.TextBlock",tooltip:"Text Tool",iconClass:"iconText"};
_3.register(_6.setup,"tool");
return _6;
});
