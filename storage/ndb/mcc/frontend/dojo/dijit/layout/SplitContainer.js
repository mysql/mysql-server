//>>built
define("dijit/layout/SplitContainer",["dojo/_base/array","dojo/cookie","dojo/_base/declare","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/_base/kernel","dojo/_base/lang","dojo/on","dojo/_base/sniff","dojo/_base/window","../registry","../_WidgetBase","./_LayoutWidget"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,on,_c,_d,_e,_f,_10){
_b.extend(_f,{sizeMin:10,sizeShare:10});
return _3("dijit.layout.SplitContainer",_10,{constructor:function(){
_a.deprecated("dijit.layout.SplitContainer is deprecated","use BorderContainer with splitter instead",2);
},activeSizing:false,sizerWidth:7,orientation:"horizontal",persist:true,baseClass:"dijitSplitContainer",postMixInProperties:function(){
this.inherited("postMixInProperties",arguments);
this.isHorizontal=(this.orientation=="horizontal");
},postCreate:function(){
this.inherited(arguments);
this.sizers=[];
if(_c("mozilla")){
this.domNode.style.overflow="-moz-scrollbars-none";
}
if(typeof this.sizerWidth=="object"){
try{
this.sizerWidth=parseInt(this.sizerWidth.toString());
}
catch(e){
this.sizerWidth=7;
}
}
var _11=_d.doc.createElement("div");
this.virtualSizer=_11;
_11.style.position="relative";
_11.style.zIndex=10;
_11.className=this.isHorizontal?"dijitSplitContainerVirtualSizerH":"dijitSplitContainerVirtualSizerV";
this.domNode.appendChild(_11);
_4.setSelectable(_11,false);
},destroy:function(){
delete this.virtualSizer;
if(this._ownconnects){
var h;
while(h=this._ownconnects.pop()){
h.remove();
}
}
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
_1.forEach(this.getChildren(),function(_12,i,_13){
this._setupChild(_12);
if(i<_13.length-1){
this._addSizer();
}
},this);
if(this.persist){
this._restoreState();
}
this.inherited(arguments);
},_setupChild:function(_14){
this.inherited(arguments);
_14.domNode.style.position="absolute";
_5.add(_14.domNode,"dijitSplitPane");
},_onSizerMouseDown:function(e){
if(e.target.id){
for(var i=0;i<this.sizers.length;i++){
if(this.sizers[i].id==e.target.id){
break;
}
}
if(i<this.sizers.length){
this.beginSizing(e,i);
}
}
},_addSizer:function(_15){
_15=_15===undefined?this.sizers.length:_15;
var _16=_d.doc.createElement("div");
_16.id=_e.getUniqueId("dijit_layout_SplitterContainer_Splitter");
this.sizers.splice(_15,0,_16);
this.domNode.appendChild(_16);
_16.className=this.isHorizontal?"dijitSplitContainerSizerH":"dijitSplitContainerSizerV";
var _17=_d.doc.createElement("div");
_17.className="thumb";
_16.appendChild(_17);
this.connect(_16,"onmousedown","_onSizerMouseDown");
_4.setSelectable(_16,false);
},removeChild:function(_18){
if(this.sizers.length){
var i=_1.indexOf(this.getChildren(),_18);
if(i!=-1){
if(i==this.sizers.length){
i--;
}
_6.destroy(this.sizers[i]);
this.sizers.splice(i,1);
}
}
this.inherited(arguments);
if(this._started){
this.layout();
}
},addChild:function(_19,_1a){
this.inherited(arguments);
if(this._started){
var _1b=this.getChildren();
if(_1b.length>1){
this._addSizer(_1a);
}
this.layout();
}
},layout:function(){
this.paneWidth=this._contentBox.w;
this.paneHeight=this._contentBox.h;
var _1c=this.getChildren();
if(!_1c.length){
return;
}
var _1d=this.isHorizontal?this.paneWidth:this.paneHeight;
if(_1c.length>1){
_1d-=this.sizerWidth*(_1c.length-1);
}
var _1e=0;
_1.forEach(_1c,function(_1f){
_1e+=_1f.sizeShare;
});
var _20=_1d/_1e;
var _21=0;
_1.forEach(_1c.slice(0,_1c.length-1),function(_22){
var _23=Math.round(_20*_22.sizeShare);
_22.sizeActual=_23;
_21+=_23;
});
_1c[_1c.length-1].sizeActual=_1d-_21;
this._checkSizes();
var pos=0;
var _24=_1c[0].sizeActual;
this._movePanel(_1c[0],pos,_24);
_1c[0].position=pos;
pos+=_24;
if(!this.sizers){
return;
}
_1.some(_1c.slice(1),function(_25,i){
if(!this.sizers[i]){
return true;
}
this._moveSlider(this.sizers[i],pos,this.sizerWidth);
this.sizers[i].position=pos;
pos+=this.sizerWidth;
_24=_25.sizeActual;
this._movePanel(_25,pos,_24);
_25.position=pos;
pos+=_24;
},this);
},_movePanel:function(_26,pos,_27){
var box;
if(this.isHorizontal){
_26.domNode.style.left=pos+"px";
_26.domNode.style.top=0;
box={w:_27,h:this.paneHeight};
if(_26.resize){
_26.resize(box);
}else{
_7.setMarginBox(_26.domNode,box);
}
}else{
_26.domNode.style.left=0;
_26.domNode.style.top=pos+"px";
box={w:this.paneWidth,h:_27};
if(_26.resize){
_26.resize(box);
}else{
_7.setMarginBox(_26.domNode,box);
}
}
},_moveSlider:function(_28,pos,_29){
if(this.isHorizontal){
_28.style.left=pos+"px";
_28.style.top=0;
_7.setMarginBox(_28,{w:_29,h:this.paneHeight});
}else{
_28.style.left=0;
_28.style.top=pos+"px";
_7.setMarginBox(_28,{w:this.paneWidth,h:_29});
}
},_growPane:function(_2a,_2b){
if(_2a>0){
if(_2b.sizeActual>_2b.sizeMin){
if((_2b.sizeActual-_2b.sizeMin)>_2a){
_2b.sizeActual=_2b.sizeActual-_2a;
_2a=0;
}else{
_2a-=_2b.sizeActual-_2b.sizeMin;
_2b.sizeActual=_2b.sizeMin;
}
}
}
return _2a;
},_checkSizes:function(){
var _2c=0;
var _2d=0;
var _2e=this.getChildren();
_1.forEach(_2e,function(_2f){
_2d+=_2f.sizeActual;
_2c+=_2f.sizeMin;
});
if(_2c<=_2d){
var _30=0;
_1.forEach(_2e,function(_31){
if(_31.sizeActual<_31.sizeMin){
_30+=_31.sizeMin-_31.sizeActual;
_31.sizeActual=_31.sizeMin;
}
});
if(_30>0){
var _32=this.isDraggingLeft?_2e.reverse():_2e;
_1.forEach(_32,function(_33){
_30=this._growPane(_30,_33);
},this);
}
}else{
_1.forEach(_2e,function(_34){
_34.sizeActual=Math.round(_2d*(_34.sizeMin/_2c));
});
}
},beginSizing:function(e,i){
var _35=this.getChildren();
this.paneBefore=_35[i];
this.paneAfter=_35[i+1];
this.isSizing=true;
this.sizingSplitter=this.sizers[i];
if(!this.cover){
this.cover=_6.create("div",{style:{position:"absolute",zIndex:5,top:0,left:0,width:"100%",height:"100%"}},this.domNode);
}else{
this.cover.style.zIndex=5;
}
this.sizingSplitter.style.zIndex=6;
this.originPos=_7.position(_35[0].domNode,true);
var _36,_37;
if(this.isHorizontal){
_36=e.layerX||e.offsetX||0;
_37=e.pageX;
this.originPos=this.originPos.x;
}else{
_36=e.layerY||e.offsetY||0;
_37=e.pageY;
this.originPos=this.originPos.y;
}
this.startPoint=this.lastPoint=_37;
this.screenToClientOffset=_37-_36;
this.dragOffset=this.lastPoint-this.paneBefore.sizeActual-this.originPos-this.paneBefore.position;
if(!this.activeSizing){
this._showSizingLine();
}
this._ownconnects=[on(_d.doc.documentElement,"mousemove",_b.hitch(this,"changeSizing")),on(_d.doc.documentElement,"mouseup",_b.hitch(this,"endSizing"))];
_9.stop(e);
},changeSizing:function(e){
if(!this.isSizing){
return;
}
this.lastPoint=this.isHorizontal?e.pageX:e.pageY;
this.movePoint();
if(this.activeSizing){
this._updateSize();
}else{
this._moveSizingLine();
}
_9.stop(e);
},endSizing:function(){
if(!this.isSizing){
return;
}
if(this.cover){
this.cover.style.zIndex=-1;
}
if(!this.activeSizing){
this._hideSizingLine();
}
this._updateSize();
this.isSizing=false;
if(this.persist){
this._saveState(this);
}
var h;
while(h=this._ownconnects.pop()){
h.remove();
}
},movePoint:function(){
var p=this.lastPoint-this.screenToClientOffset;
var a=p-this.dragOffset;
a=this.legaliseSplitPoint(a);
p=a+this.dragOffset;
this.lastPoint=p+this.screenToClientOffset;
},legaliseSplitPoint:function(a){
a+=this.sizingSplitter.position;
this.isDraggingLeft=!!(a>0);
if(!this.activeSizing){
var min=this.paneBefore.position+this.paneBefore.sizeMin;
if(a<min){
a=min;
}
var max=this.paneAfter.position+(this.paneAfter.sizeActual-(this.sizerWidth+this.paneAfter.sizeMin));
if(a>max){
a=max;
}
}
a-=this.sizingSplitter.position;
this._checkSizes();
return a;
},_updateSize:function(){
var pos=this.lastPoint-this.dragOffset-this.originPos;
var _38=this.paneBefore.position;
var _39=this.paneAfter.position+this.paneAfter.sizeActual;
this.paneBefore.sizeActual=pos-_38;
this.paneAfter.position=pos+this.sizerWidth;
this.paneAfter.sizeActual=_39-this.paneAfter.position;
_1.forEach(this.getChildren(),function(_3a){
_3a.sizeShare=_3a.sizeActual;
});
if(this._started){
this.layout();
}
},_showSizingLine:function(){
this._moveSizingLine();
_7.setMarginBox(this.virtualSizer,this.isHorizontal?{w:this.sizerWidth,h:this.paneHeight}:{w:this.paneWidth,h:this.sizerWidth});
this.virtualSizer.style.display="block";
},_hideSizingLine:function(){
this.virtualSizer.style.display="none";
},_moveSizingLine:function(){
var pos=(this.lastPoint-this.startPoint)+this.sizingSplitter.position;
_8.set(this.virtualSizer,(this.isHorizontal?"left":"top"),pos+"px");
},_getCookieName:function(i){
return this.id+"_"+i;
},_restoreState:function(){
_1.forEach(this.getChildren(),function(_3b,i){
var _3c=this._getCookieName(i);
var _3d=_2(_3c);
if(_3d){
var pos=parseInt(_3d);
if(typeof pos=="number"){
_3b.sizeShare=pos;
}
}
},this);
},_saveState:function(){
if(!this.persist){
return;
}
_1.forEach(this.getChildren(),function(_3e,i){
_2(this._getCookieName(i),_3e.sizeShare,{expires:365});
},this);
}});
});
