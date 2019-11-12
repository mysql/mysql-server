//>>built
define("dijit/layout/BorderContainer",["dojo/_base/array","dojo/cookie","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/on","dojo/touch","../_WidgetBase","../_Widget","../_TemplatedMixin","./_LayoutWidget","./utils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10){
var _11=_3("dijit.layout._Splitter",[_d,_e],{live:true,templateString:"<div class=\"dijitSplitter\" data-dojo-attach-event=\"onkeypress:_onKeyPress,press:_startDrag,onmouseenter:_onMouse,onmouseleave:_onMouse\" tabIndex=\"0\" role=\"separator\"><div class=\"dijitSplitterThumb\"></div></div>",constructor:function(){
this._handlers=[];
},postMixInProperties:function(){
this.inherited(arguments);
this.horizontal=/top|bottom/.test(this.region);
this._factor=/top|left/.test(this.region)?1:-1;
this._cookieName=this.container.id+"_"+this.region;
},buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"dijitSplitter"+(this.horizontal?"H":"V"));
if(this.container.persist){
var _12=_2(this._cookieName);
if(_12){
this.child.domNode.style[this.horizontal?"height":"width"]=_12;
}
}
},_computeMaxSize:function(){
var dim=this.horizontal?"h":"w",_13=_6.getMarginBox(this.child.domNode)[dim],_14=_1.filter(this.container.getChildren(),function(_15){
return _15.region=="center";
})[0],_16=_6.getMarginBox(_14.domNode)[dim];
return Math.min(this.child.maxSize,_13+_16);
},_startDrag:function(e){
if(!this.cover){
this.cover=_5.place("<div class=dijitSplitterCover></div>",this.child.domNode,"after");
}
_4.add(this.cover,"dijitSplitterCoverActive");
if(this.fake){
_5.destroy(this.fake);
}
if(!(this._resize=this.live)){
(this.fake=this.domNode.cloneNode(true)).removeAttribute("id");
_4.add(this.domNode,"dijitSplitterShadow");
_5.place(this.fake,this.domNode,"after");
}
_4.add(this.domNode,"dijitSplitterActive dijitSplitter"+(this.horizontal?"H":"V")+"Active");
if(this.fake){
_4.remove(this.fake,"dijitSplitterHover dijitSplitter"+(this.horizontal?"H":"V")+"Hover");
}
var _17=this._factor,_18=this.horizontal,_19=_18?"pageY":"pageX",_1a=e[_19],_1b=this.domNode.style,dim=_18?"h":"w",_1c=_6.getMarginBox(this.child.domNode)[dim],max=this._computeMaxSize(),min=this.child.minSize||20,_1d=this.region,_1e=_1d=="top"||_1d=="bottom"?"top":"left",_1f=parseInt(_1b[_1e],10),_20=this._resize,_21=_a.hitch(this.container,"_layoutChildren",this.child.id),de=this.ownerDocument;
this._handlers=this._handlers.concat([on(de,_b.move,this._drag=function(e,_22){
var _23=e[_19]-_1a,_24=_17*_23+_1c,_25=Math.max(Math.min(_24,max),min);
if(_20||_22){
_21(_25);
}
_1b[_1e]=_23+_1f+_17*(_25-_24)+"px";
}),on(de,"dragstart",_8.stop),on(this.ownerDocumentBody,"selectstart",_8.stop),on(de,_b.release,_a.hitch(this,"_stopDrag"))]);
_8.stop(e);
},_onMouse:function(e){
var o=(e.type=="mouseover"||e.type=="mouseenter");
_4.toggle(this.domNode,"dijitSplitterHover",o);
_4.toggle(this.domNode,"dijitSplitter"+(this.horizontal?"H":"V")+"Hover",o);
},_stopDrag:function(e){
try{
if(this.cover){
_4.remove(this.cover,"dijitSplitterCoverActive");
}
if(this.fake){
_5.destroy(this.fake);
}
_4.remove(this.domNode,"dijitSplitterActive dijitSplitter"+(this.horizontal?"H":"V")+"Active dijitSplitterShadow");
this._drag(e);
this._drag(e,true);
}
finally{
this._cleanupHandlers();
delete this._drag;
}
if(this.container.persist){
_2(this._cookieName,this.child.domNode.style[this.horizontal?"height":"width"],{expires:365});
}
},_cleanupHandlers:function(){
var h;
while(h=this._handlers.pop()){
h.remove();
}
},_onKeyPress:function(e){
this._resize=true;
var _26=this.horizontal;
var _27=1;
switch(e.charOrCode){
case _26?_9.UP_ARROW:_9.LEFT_ARROW:
_27*=-1;
case _26?_9.DOWN_ARROW:_9.RIGHT_ARROW:
break;
default:
return;
}
var _28=_6.getMarginSize(this.child.domNode)[_26?"h":"w"]+this._factor*_27;
this.container._layoutChildren(this.child.id,Math.max(Math.min(_28,this._computeMaxSize()),this.child.minSize));
_8.stop(e);
},destroy:function(){
this._cleanupHandlers();
delete this.child;
delete this.container;
delete this.cover;
delete this.fake;
this.inherited(arguments);
}});
var _29=_3("dijit.layout._Gutter",[_d,_e],{templateString:"<div class=\"dijitGutter\" role=\"presentation\"></div>",postMixInProperties:function(){
this.inherited(arguments);
this.horizontal=/top|bottom/.test(this.region);
},buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"dijitGutter"+(this.horizontal?"H":"V"));
}});
var _2a=_3("dijit.layout.BorderContainer",_f,{design:"headline",gutters:true,liveSplitters:true,persist:false,baseClass:"dijitBorderContainer",_splitterClass:_11,postMixInProperties:function(){
if(!this.gutters){
this.baseClass+="NoGutter";
}
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
_1.forEach(this.getChildren(),this._setupChild,this);
this.inherited(arguments);
},_setupChild:function(_2b){
var _2c=_2b.region;
if(_2c){
this.inherited(arguments);
_4.add(_2b.domNode,this.baseClass+"Pane");
var ltr=this.isLeftToRight();
if(_2c=="leading"){
_2c=ltr?"left":"right";
}
if(_2c=="trailing"){
_2c=ltr?"right":"left";
}
if(_2c!="center"&&(_2b.splitter||this.gutters)&&!_2b._splitterWidget){
var _2d=_2b.splitter?this._splitterClass:_29;
if(_a.isString(_2d)){
_2d=_a.getObject(_2d);
}
var _2e=new _2d({id:_2b.id+"_splitter",container:this,child:_2b,region:_2c,live:this.liveSplitters});
_2e.isSplitter=true;
_2b._splitterWidget=_2e;
_5.place(_2e.domNode,_2b.domNode,"after");
_2e.startup();
}
_2b.region=_2c;
}
},layout:function(){
this._layoutChildren();
},addChild:function(_2f,_30){
this.inherited(arguments);
if(this._started){
this.layout();
}
},removeChild:function(_31){
var _32=_31.region;
var _33=_31._splitterWidget;
if(_33){
_33.destroy();
delete _31._splitterWidget;
}
this.inherited(arguments);
if(this._started){
this._layoutChildren();
}
_4.remove(_31.domNode,this.baseClass+"Pane");
_7.set(_31.domNode,{top:"auto",bottom:"auto",left:"auto",right:"auto",position:"static"});
_7.set(_31.domNode,_32=="top"||_32=="bottom"?"width":"height","auto");
},getChildren:function(){
return _1.filter(this.inherited(arguments),function(_34){
return !_34.isSplitter;
});
},getSplitter:function(_35){
return _1.filter(this.getChildren(),function(_36){
return _36.region==_35;
})[0]._splitterWidget;
},resize:function(_37,_38){
if(!this.cs||!this.pe){
var _39=this.domNode;
this.cs=_7.getComputedStyle(_39);
this.pe=_6.getPadExtents(_39,this.cs);
this.pe.r=_7.toPixelValue(_39,this.cs.paddingRight);
this.pe.b=_7.toPixelValue(_39,this.cs.paddingBottom);
_7.set(_39,"padding","0px");
}
this.inherited(arguments);
},_layoutChildren:function(_3a,_3b){
if(!this._borderBox||!this._borderBox.h){
return;
}
var _3c=_1.map(this.getChildren(),function(_3d,idx){
return {pane:_3d,weight:[_3d.region=="center"?Infinity:0,_3d.layoutPriority,(this.design=="sidebar"?1:-1)*(/top|bottom/.test(_3d.region)?1:-1),idx]};
},this);
_3c.sort(function(a,b){
var aw=a.weight,bw=b.weight;
for(var i=0;i<aw.length;i++){
if(aw[i]!=bw[i]){
return aw[i]-bw[i];
}
}
return 0;
});
var _3e=[];
_1.forEach(_3c,function(_3f){
var _40=_3f.pane;
_3e.push(_40);
if(_40._splitterWidget){
_3e.push(_40._splitterWidget);
}
});
var dim={l:this.pe.l,t:this.pe.t,w:this._borderBox.w-this.pe.w,h:this._borderBox.h-this.pe.h};
_10.layoutChildren(this.domNode,dim,_3e,_3a,_3b);
},destroyRecursive:function(){
_1.forEach(this.getChildren(),function(_41){
var _42=_41._splitterWidget;
if(_42){
_42.destroy();
}
delete _41._splitterWidget;
});
this.inherited(arguments);
}});
_2a.ChildWidgetProperties={region:"",layoutPriority:0,splitter:false,minSize:0,maxSize:Infinity};
_a.extend(_c,_2a.ChildWidgetProperties);
_2a._Splitter=_11;
_2a._Gutter=_29;
return _2a;
});
