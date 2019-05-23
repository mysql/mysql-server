//>>built
define("dojox/geo/charting/widget/Legend",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/declare","dojo/_base/html","dojo/dom","dojo/dom-construct","dojo/dom-class","dojo/_base/window","dijit/_Widget"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _4("dojox.geo.charting.widget.Legend",_a,{horizontal:true,legendBody:null,swatchSize:18,map:null,postCreate:function(){
if(!this.map){
return;
}
this.series=this.map.series;
if(!this.domNode.parentNode){
_6.byId(this.map.container).appendChild(this.domNode);
}
this.refresh();
},buildRendering:function(){
this.domNode=_7.create("table",{role:"group","class":"dojoxLegendNode"});
this.legendBody=_7.create("tbody",null,this.domNode);
this.inherited(arguments);
},refresh:function(){
while(this.legendBody.lastChild){
_7.destroy(this.legendBody.lastChild);
}
if(this.horizontal){
_8.add(this.domNode,"dojoxLegendHorizontal");
this._tr=_9.doc.createElement("tr");
this.legendBody.appendChild(this._tr);
}
var s=this.series;
if(s.length==0){
return;
}
_3.forEach(s,function(x){
this._addLabel(x.color,x.name);
},this);
},_addLabel:function(_b,_c){
var _d=_9.doc.createElement("td");
var _e=_9.doc.createElement("td");
var _f=_9.doc.createElement("div");
_8.add(_d,"dojoxLegendIcon");
_8.add(_e,"dojoxLegendText");
_f.style.width=this.swatchSize+"px";
_f.style.height=this.swatchSize+"px";
_d.appendChild(_f);
if(this.horizontal){
this._tr.appendChild(_d);
this._tr.appendChild(_e);
}else{
var tr=_9.doc.createElement("tr");
this.legendBody.appendChild(tr);
tr.appendChild(_d);
tr.appendChild(_e);
}
_f.style.background=_b;
_e.innerHTML=String(_c);
}});
});
