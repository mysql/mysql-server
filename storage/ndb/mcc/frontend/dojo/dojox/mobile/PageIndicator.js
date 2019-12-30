//>>built
define("dojox/mobile/PageIndicator",["dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-class","dojo/dom-construct","dijit/registry","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _2("dojox.mobile.PageIndicator",[_8,_7],{refId:"",baseClass:"mblPageIndicator",buildRendering:function(){
this.inherited(arguments);
this._tblNode=_5.create("table",{className:"mblPageIndicatorContainer"},this.domNode);
this._tblNode.insertRow(-1);
this._clickHandle=this.connect(this.domNode,"onclick","_onClick");
this.subscribe("/dojox/mobile/viewChanged",function(_9){
this.reset();
});
},startup:function(){
var _a=this;
setTimeout(function(){
_a.reset();
},0);
},reset:function(){
var r=this._tblNode.rows[0];
var i,c,a=[],_b;
var _c=(this.refId&&_3.byId(this.refId))||this.domNode;
var _d=_c.parentNode.childNodes;
for(i=0;i<_d.length;i++){
c=_d[i];
if(this.isView(c)){
a.push(c);
}
}
if(r.cells.length!==a.length){
_5.empty(r);
for(i=0;i<a.length;i++){
c=a[i];
_b=_5.create("div",{className:"mblPageIndicatorDot"});
r.insertCell(-1).appendChild(_b);
}
}
if(a.length===0){
return;
}
var _e=_6.byNode(a[0]).getShowingView();
for(i=0;i<r.cells.length;i++){
_b=r.cells[i].firstChild;
if(a[i]===_e.domNode){
_4.add(_b,"mblPageIndicatorDotSelected");
}else{
_4.remove(_b,"mblPageIndicatorDotSelected");
}
}
},isView:function(_f){
return (_f&&_f.nodeType===1&&_4.contains(_f,"mblView"));
},_onClick:function(e){
if(this.onClick(e)===false){
return;
}
if(e.target!==this.domNode){
return;
}
if(e.layerX<this._tblNode.offsetLeft){
_1.publish("/dojox/mobile/prevPage",[this]);
}else{
if(e.layerX>this._tblNode.offsetLeft+this._tblNode.offsetWidth){
_1.publish("/dojox/mobile/nextPage",[this]);
}
}
},onClick:function(){
}});
});
