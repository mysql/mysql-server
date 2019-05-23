//>>built
define("dojox/mobile/IconMenu",["dojo/_base/declare","dojo/_base/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./IconMenuItem"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.mobile.IconMenu",[_8,_7,_6],{transition:"slide",iconBase:"",iconPos:"",cols:3,tag:"ul",selectOne:false,baseClass:"mblIconMenu",childItemClass:"mblIconMenuItem",_createTerminator:false,buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_4.create(this.tag);
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
var _9=this.getChildren();
if(this.cols){
var _a=Math.ceil(_9.length/this.cols);
var w=Math.floor(100/this.cols);
var _b=100-w*this.cols;
var h=Math.floor(100/_a);
var _c=100-h*_a;
if(_2("ie")){
_b--;
_c--;
}
}
for(var i=0;i<_9.length;i++){
var _d=_9[i];
if(this.cols){
var _e=((i%this.cols)===0);
var _f=(((i+1)%this.cols)===0);
var _10=Math.floor(i/this.cols);
_5.set(_d.domNode,{width:w+(_f?_b:0)+"%",height:h+((_10+1===_a)?_c:0)+"%"});
_3.toggle(_d.domNode,this.childItemClass+"FirstColumn",_e);
_3.toggle(_d.domNode,this.childItemClass+"LastColumn",_f);
_3.toggle(_d.domNode,this.childItemClass+"FirstRow",_10===0);
_3.toggle(_d.domNode,this.childItemClass+"LastRow",_10+1===_a);
}
}
},addChild:function(_11,_12){
this.inherited(arguments);
this.refresh();
},hide:function(){
var p=this.getParent();
if(p&&p.hide){
p.hide();
}
}});
});
