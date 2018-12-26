//>>built
define("dojox/mobile/PageIndicator",["dojo/_base/connect","dojo/_base/declare","dojo/_base/window","dojo/dom","dojo/dom-class","dojo/dom-construct","dijit/registry","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.PageIndicator",[_9,_8],{refId:"",buildRendering:function(){
this.domNode=this.srcNodeRef||_3.doc.createElement("DIV");
this.domNode.className="mblPageIndicator";
this._tblNode=_6.create("TABLE",{className:"mblPageIndicatorContainer"},this.domNode);
this._tblNode.insertRow(-1);
this.connect(this.domNode,"onclick","onClick");
_1.subscribe("/dojox/mobile/viewChanged",this,function(_a){
this.reset();
});
},startup:function(){
var _b=this;
setTimeout(function(){
_b.reset();
},0);
},reset:function(){
var r=this._tblNode.rows[0];
var i,c,a=[],_c;
var _d=(this.refId&&_4.byId(this.refId))||this.domNode;
var _e=_d.parentNode.childNodes;
for(i=0;i<_e.length;i++){
c=_e[i];
if(this.isView(c)){
a.push(c);
}
}
if(r.cells.length!==a.length){
_6.empty(r);
for(i=0;i<a.length;i++){
c=a[i];
_c=_6.create("DIV",{className:"mblPageIndicatorDot"});
r.insertCell(-1).appendChild(_c);
}
}
if(a.length===0){
return;
}
var _f=_7.byNode(a[0]).getShowingView();
for(i=0;i<r.cells.length;i++){
_c=r.cells[i].firstChild;
if(a[i]===_f.domNode){
_5.add(_c,"mblPageIndicatorDotSelected");
}else{
_5.remove(_c,"mblPageIndicatorDotSelected");
}
}
},isView:function(_10){
return (_10&&_10.nodeType===1&&_5.contains(_10,"mblView"));
},onClick:function(e){
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
}});
});
