//>>built
define("dojox/mobile/PageIndicator",["dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-class","dojo/dom-construct","dijit/registry","dijit/_Contained","dijit/_WidgetBase","dojo/i18n!dojox/mobile/nls/messages"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.PageIndicator",[_8,_7],{refId:"",baseClass:"mblPageIndicator",buildRendering:function(){
this.inherited(arguments);
this.domNode.setAttribute("role","img");
this._tblNode=_5.create("table",{className:"mblPageIndicatorContainer"},this.domNode);
this._tblNode.insertRow(-1);
this.connect(this.domNode,"onclick","_onClick");
this.subscribe("/dojox/mobile/viewChanged",function(_a){
this.reset();
});
},startup:function(){
var _b=this;
_b.defer(function(){
_b.reset();
});
},reset:function(){
var r=this._tblNode.rows[0];
var i,c,a=[],_c,_d=0;
var _e=(this.refId&&_3.byId(this.refId))||this.domNode;
var _f=_e.parentNode.childNodes;
for(i=0;i<_f.length;i++){
c=_f[i];
if(this.isView(c)){
a.push(c);
}
}
if(r.cells.length!==a.length){
_5.empty(r);
for(i=0;i<a.length;i++){
c=a[i];
_c=_5.create("div",{className:"mblPageIndicatorDot"});
r.insertCell(-1).appendChild(_c);
}
}
if(a.length===0){
return;
}
var _10=_6.byNode(a[0]).getShowingView();
for(i=0;i<r.cells.length;i++){
_c=r.cells[i].firstChild;
if(a[i]===_10.domNode){
_d=i+1;
_4.add(_c,"mblPageIndicatorDotSelected");
}else{
_4.remove(_c,"mblPageIndicatorDotSelected");
}
}
if(r.cells.length){
this.domNode.setAttribute("alt",_9["PageIndicatorLabel"].replace("$0",_d).replace("$1",r.cells.length));
}else{
this.domNode.removeAttribute("alt");
}
},isView:function(_11){
return (_11&&_11.nodeType===1&&_4.contains(_11,"mblView"));
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
