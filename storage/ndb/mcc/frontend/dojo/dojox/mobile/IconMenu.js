//>>built
define("dojox/mobile/IconMenu",["dojo/_base/declare","dojo/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","dojo/has!dojo-bidi?dojox/mobile/bidi/IconMenu","./IconMenuItem"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var _b=_1(_2("dojo-bidi")?"dojox.mobile.NonBidiIconMenu":"dojox.mobile.IconMenu",[_9,_8,_7],{transition:"slide",iconBase:"",iconPos:"",cols:3,tag:"ul",selectOne:false,baseClass:"mblIconMenu",childItemClass:"mblIconMenuItem",_createTerminator:false,buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_4.create(this.tag);
_6.set(this.domNode,"role","menu");
this.inherited(arguments);
if(this._createTerminator){
var t=this._terminator=_4.create("br");
t.className=this.childItemClass+"Terminator";
this.domNode.appendChild(t);
}
},startup:function(){
if(this._started){
return;
}
this.refresh();
this.inherited(arguments);
},refresh:function(){
var p=this.getParent();
if(p){
_3.remove(p.domNode,"mblSimpleDialogDecoration");
}
var _c=this.getChildren();
if(this.cols){
var _d=Math.ceil(_c.length/this.cols);
var w=Math.floor(100/this.cols);
var _e=100-w*this.cols;
var h=Math.floor(100/_d);
var _f=100-h*_d;
if(_2("ie")){
_e--;
_f--;
}
}
for(var i=0;i<_c.length;i++){
var _10=_c[i];
if(this.cols){
var _11=((i%this.cols)===0);
var _12=(((i+1)%this.cols)===0);
var _13=Math.floor(i/this.cols);
_5.set(_10.domNode,{width:w+(_12?_e:0)+"%",height:h+((_13+1===_d)?_f:0)+"%"});
_3.toggle(_10.domNode,this.childItemClass+"FirstColumn",_11);
_3.toggle(_10.domNode,this.childItemClass+"LastColumn",_12);
_3.toggle(_10.domNode,this.childItemClass+"FirstRow",_13===0);
_3.toggle(_10.domNode,this.childItemClass+"LastRow",_13+1===_d);
}
}
},addChild:function(_14,_15){
this.inherited(arguments);
this.refresh();
},hide:function(){
var p=this.getParent();
if(p&&p.hide){
p.hide();
}
}});
return _2("dojo-bidi")?_1("dojox.mobile.IconMenu",[_b,_a]):_b;
});
