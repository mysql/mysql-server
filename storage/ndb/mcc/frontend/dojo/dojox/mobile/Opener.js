//>>built
define("dojox/mobile/Opener",["dojo/_base/declare","dojo/_base/Deferred","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-geometry","./Tooltip","./Overlay","./lazyLoadUtils"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
var _c=_5.contains(_4.doc.documentElement,"dj_phone");
var _d=_1("dojox.mobile.Opener",_c?_a:_9,{lazy:false,requires:"",buildRendering:function(){
this.inherited(arguments);
this.cover=_6.create("div",{onclick:_3.hitch(this,"_onBlur"),"class":"mblOpenerUnderlay",style:{position:_c?"absolute":"fixed",backgroundColor:"transparent",overflow:"hidden",zIndex:"-1"}},this.domNode,"first");
},onShow:function(_e){
},onHide:function(_f,v){
},show:function(_10,_11){
if(this.lazy){
this.lazy=false;
var _12=this;
return _2.when(_b.instantiateLazyWidgets(this.domNode,this.requires),function(){
return _12.show(_10,_11);
});
}
this.node=_10;
this.onShow(_10);
_7.set(this.cover,{top:"0px",left:"0px",width:"0px",height:"0px"});
this._resizeCover(_8.position(this.domNode,false));
return this.inherited(arguments);
},hide:function(val){
this.inherited(arguments);
this.onHide(this.node,val);
},_reposition:function(){
var _13=this.inherited(arguments);
this._resizeCover(_13);
return _13;
},_resizeCover:function(_14){
if(_c){
if(parseInt(_7.get(this.cover,"top"))!=-_14.y||parseInt(_7.get(this.cover,"height"))!=_14.y){
var x=Math.max(_14.x,0);
_7.set(this.cover,{top:-_14.y+"px",left:-x+"px",width:_14.w+x+"px",height:_14.y+"px"});
}
}else{
_7.set(this.cover,{width:Math.max(_4.doc.documentElement.scrollWidth||_4.body().scrollWidth||_4.doc.documentElement.clientWidth)+"px",height:Math.max(_4.doc.documentElement.scrollHeight||_4.body().scrollHeight||_4.doc.documentElement.clientHeight)+"px"});
}
},_onBlur:function(e){
var ret=this.onBlur(e);
if(ret!==false){
this.hide(e);
}
return ret;
}});
_d.prototype.baseClass+=" mblOpener";
return _d;
});
