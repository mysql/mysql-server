//>>built
define("dijit/layout/BorderContainer",["dojo/_base/array","dojo/cookie","dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/on","dojo/touch","dojo/_base/window","../_WidgetBase","../_Widget","../_TemplatedMixin","./_LayoutWidget","./utils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f,_10,_11){
var _12=_3("dijit.layout._Splitter",[_e,_f],{live:true,templateString:"<div class=\"dijitSplitter\" data-dojo-attach-event=\"onkeypress:_onKeyPress,press:_startDrag,onmouseenter:_onMouse,onmouseleave:_onMouse\" tabIndex=\"0\" role=\"separator\"><div class=\"dijitSplitterThumb\"></div></div>",constructor:function(){
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
var _13=_2(this._cookieName);
if(_13){
this.child.domNode.style[this.horizontal?"height":"width"]=_13;
}
}
},_computeMaxSize:function(){
var dim=this.horizontal?"h":"w",_14=_6.getMarginBox(this.child.domNode)[dim],_15=_1.filter(this.container.getChildren(),function(_16){
return _16.region=="center";
})[0],_17=_6.getMarginBox(_15.domNode)[dim];
return Math.min(this.child.maxSize,_14+_17);
},_startDrag:function(e){
if(!this.cover){
this.cover=_c.doc.createElement("div");
_4.add(this.cover,"dijitSplitterCover");
_5.place(this.cover,this.child.domNode,"after");
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
var _18=this._factor,_19=this.horizontal,_1a=_19?"pageY":"pageX",_1b=e[_1a],_1c=this.domNode.style,dim=_19?"h":"w",_1d=_6.getMarginBox(this.child.domNode)[dim],max=this._computeMaxSize(),min=this.child.minSize||20,_1e=this.region,_1f=_1e=="top"||_1e=="bottom"?"top":"left",_20=parseInt(_1c[_1f],10),_21=this._resize,_22=_a.hitch(this.container,"_layoutChildren",this.child.id),de=_c.doc;
this._handlers=this._handlers.concat([on(de,_b.move,this._drag=function(e,_23){
var _24=e[_1a]-_1b,_25=_18*_24+_1d,_26=Math.max(Math.min(_25,max),min);
if(_21||_23){
_22(_26);
}
_1c[_1f]=_24+_20+_18*(_26-_25)+"px";
}),on(de,"dragstart",_8.stop),on(_c.body(),"selectstart",_8.stop),on(de,_b.release,_a.hitch(this,"_stopDrag"))]);
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
var _27=this.horizontal;
var _28=1;
switch(e.charOrCode){
case _27?_9.UP_ARROW:_9.LEFT_ARROW:
_28*=-1;
case _27?_9.DOWN_ARROW:_9.RIGHT_ARROW:
break;
default:
return;
}
var _29=_6.getMarginSize(this.child.domNode)[_27?"h":"w"]+this._factor*_28;
this.container._layoutChildren(this.child.id,Math.max(Math.min(_29,this._computeMaxSize()),this.child.minSize));
_8.stop(e);
},destroy:function(){
this._cleanupHandlers();
delete this.child;
delete this.container;
delete this.cover;
delete this.fake;
this.inherited(arguments);
}});
var _2a=_3("dijit.layout._Gutter",[_e,_f],{templateString:"<div class=\"dijitGutter\" role=\"presentation\"></div>",postMixInProperties:function(){
this.inherited(arguments);
this.horizontal=/top|bottom/.test(this.region);
},buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"dijitGutter"+(this.horizontal?"H":"V"));
}});
var _2b=_3("dijit.layout.BorderContainer",_10,{design:"headline",gutters:true,liveSplitters:true,persist:false,baseClass:"dijitBorderContainer",_splitterClass:_12,postMixInProperties:function(){
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
},_setupChild:function(_2c){
var _2d=_2c.region;
if(_2d){
this.inherited(arguments);
_4.add(_2c.domNode,this.baseClass+"Pane");
var ltr=this.isLeftToRight();
if(_2d=="leading"){
_2d=ltr?"left":"right";
}
if(_2d=="trailing"){
_2d=ltr?"right":"left";
}
if(_2d!="center"&&(_2c.splitter||this.gutters)&&!_2c._splitterWidget){
var _2e=_2c.splitter?this._splitterClass:_2a;
if(_a.isString(_2e)){
_2e=_a.getObject(_2e);
}
var _2f=new _2e({id:_2c.id+"_splitter",container:this,child:_2c,region:_2d,live:this.liveSplitters});
_2f.isSplitter=true;
_2c._splitterWidget=_2f;
_5.place(_2f.domNode,_2c.domNode,"after");
_2f.startup();
}
_2c.region=_2d;
}
},layout:function(){
this._layoutChildren();
},addChild:function(_30,_31){
this.inherited(arguments);
if(this._started){
this.layout();
}
},removeChild:function(_32){
var _33=_32.region;
var _34=_32._splitterWidget;
if(_34){
_34.destroy();
delete _32._splitterWidget;
}
this.inherited(arguments);
if(this._started){
this._layoutChildren();
}
_4.remove(_32.domNode,this.baseClass+"Pane");
_7.set(_32.domNode,{top:"auto",bottom:"auto",left:"auto",right:"auto",position:"static"});
_7.set(_32.domNode,_33=="top"||_33=="bottom"?"width":"height","auto");
},getChildren:function(){
return _1.filter(this.inherited(arguments),function(_35){
return !_35.isSplitter;
});
},getSplitter:function(_36){
return _1.filter(this.getChildren(),function(_37){
return _37.region==_36;
})[0]._splitterWidget;
},resize:function(_38,_39){
if(!this.cs||!this.pe){
var _3a=this.domNode;
this.cs=_7.getComputedStyle(_3a);
this.pe=_6.getPadExtents(_3a,this.cs);
this.pe.r=_7.toPixelValue(_3a,this.cs.paddingRight);
this.pe.b=_7.toPixelValue(_3a,this.cs.paddingBottom);
_7.set(_3a,"padding","0px");
}
this.inherited(arguments);
},_layoutChildren:function(_3b,_3c){
if(!this._borderBox||!this._borderBox.h){
return;
}
var _3d=_1.map(this.getChildren(),function(_3e,idx){
return {pane:_3e,weight:[_3e.region=="center"?Infinity:0,_3e.layoutPriority,(this.design=="sidebar"?1:-1)*(/top|bottom/.test(_3e.region)?1:-1),idx]};
},this);
_3d.sort(function(a,b){
var aw=a.weight,bw=b.weight;
for(var i=0;i<aw.length;i++){
if(aw[i]!=bw[i]){
return aw[i]-bw[i];
}
}
return 0;
});
var _3f=[];
_1.forEach(_3d,function(_40){
var _41=_40.pane;
_3f.push(_41);
if(_41._splitterWidget){
_3f.push(_41._splitterWidget);
}
});
var dim={l:this.pe.l,t:this.pe.t,w:this._borderBox.w-this.pe.w,h:this._borderBox.h-this.pe.h};
_11.layoutChildren(this.domNode,dim,_3f,_3b,_3c);
},destroyRecursive:function(){
_1.forEach(this.getChildren(),function(_42){
var _43=_42._splitterWidget;
if(_43){
_43.destroy();
}
delete _42._splitterWidget;
});
this.inherited(arguments);
}});
_a.extend(_d,{region:"",layoutPriority:0,splitter:false,minSize:0,maxSize:Infinity});
_2b._Splitter=_12;
_2b._Gutter=_2a;
return _2b;
});
