//>>built
define("dojox/dtl/contrib/dijit",["dojo/_base/lang","dojo/_base/connect","dojo/_base/array","dojo/query","../_base","../dom","dojo/parser","dojo/_base/sniff"],function(_1,_2,_3,_4,dd,_5,_6,_7){
_1.getObject("dojox.dtl.contrib.dijit",true);
var _8=dd.contrib.dijit;
_8.AttachNode=_1.extend(function(_9,_a){
this._keys=_9;
this._object=_a;
},{render:function(_b,_c){
if(!this._rendered){
this._rendered=true;
for(var i=0,_d;_d=this._keys[i];i++){
_b.getThis()[_d]=this._object||_c.getParent();
}
}
return _c;
},unrender:function(_e,_f){
if(this._rendered){
this._rendered=false;
for(var i=0,key;key=this._keys[i];i++){
if(_e.getThis()[key]===(this._object||_f.getParent())){
delete _e.getThis()[key];
}
}
}
return _f;
},clone:function(_10){
return new this.constructor(this._keys,this._object);
}});
_8.EventNode=_1.extend(function(_11,obj){
this._command=_11;
var _12,_13=_11.split(/\s*,\s*/);
var _14=_1.trim;
var _15=[];
var fns=[];
while(_12=_13.pop()){
if(_12){
var fn=null;
if(_12.indexOf(":")!=-1){
var _16=_12.split(":");
_12=_14(_16[0]);
fn=_14(_16.slice(1).join(":"));
}else{
_12=_14(_12);
}
if(!fn){
fn=_12;
}
_15.push(_12);
fns.push(fn);
}
}
this._types=_15;
this._fns=fns;
this._object=obj;
this._rendered=[];
},{_clear:false,render:function(_17,_18){
for(var i=0,_19;_19=this._types[i];i++){
if(!this._clear&&!this._object){
_18.getParent()[_19]=null;
}
var fn=this._fns[i];
var _1a;
if(fn.indexOf(" ")!=-1){
if(this._rendered[i]){
_2.disconnect(this._rendered[i]);
this._rendered[i]=false;
}
_1a=_3.map(fn.split(" ").slice(1),function(_1b){
return new dd._Filter(_1b).resolve(_17);
});
fn=fn.split(" ",2)[0];
}
if(!this._rendered[i]){
if(!this._object){
this._rendered[i]=_18.addEvent(_17,_19,fn,_1a);
}else{
this._rendered[i]=_2.connect(this._object,_19,_17.getThis(),fn);
}
}
}
this._clear=true;
return _18;
},unrender:function(_1c,_1d){
while(this._rendered.length){
_2.disconnect(this._rendered.pop());
}
return _1d;
},clone:function(){
return new this.constructor(this._command,this._object);
}});
function _1e(n1){
var n2=n1.cloneNode(true);
if(_7("ie")){
_4("script",n2).forEach("item.text = this[index].text;",_4("script",n1));
}
return n2;
};
_8.DojoTypeNode=_1.extend(function(_1f,_20){
this._node=_1f;
this._parsed=_20;
var _21=_1f.getAttribute("dojoAttachEvent")||_1f.getAttribute("data-dojo-attach-event");
if(_21){
this._events=new _8.EventNode(_1.trim(_21));
}
var _22=_1f.getAttribute("dojoAttachPoint")||_1f.getAttribute("data-dojo-attach-point");
if(_22){
this._attach=new _8.AttachNode(_1.trim(_22).split(/\s*,\s*/));
}
if(!_20){
this._dijit=_6.instantiate([_1e(_1f)])[0];
}else{
_1f=_1e(_1f);
var old=_8.widgetsInTemplate;
_8.widgetsInTemplate=false;
this._template=new dd.DomTemplate(_1f);
_8.widgetsInTemplate=old;
}
},{render:function(_23,_24){
if(this._parsed){
var _25=new dd.DomBuffer();
this._template.render(_23,_25);
var _26=_1e(_25.getRootNode());
var div=document.createElement("div");
div.appendChild(_26);
var _27=div.innerHTML;
div.removeChild(_26);
if(_27!=this._rendered){
this._rendered=_27;
if(this._dijit){
this._dijit.destroyRecursive();
}
this._dijit=_6.instantiate([_26])[0];
}
}
var _28=this._dijit.domNode;
if(this._events){
this._events._object=this._dijit;
this._events.render(_23,_24);
}
if(this._attach){
this._attach._object=this._dijit;
this._attach.render(_23,_24);
}
return _24.concat(_28);
},unrender:function(_29,_2a){
return _2a.remove(this._dijit.domNode);
},clone:function(){
return new this.constructor(this._node,this._parsed);
}});
_1.mixin(_8,{widgetsInTemplate:true,dojoAttachPoint:function(_2b,_2c){
return new _8.AttachNode(_2c.contents.slice(_2c.contents.indexOf("data-")!==-1?23:16).split(/\s*,\s*/));
},dojoAttachEvent:function(_2d,_2e){
return new _8.EventNode(_2e.contents.slice(_2e.contents.indexOf("data-")!==-1?23:16));
},dojoType:function(_2f,_30){
var _31=false;
if(_30.contents.slice(-7)==" parsed"){
_31=true;
}
var _32=_30.contents.indexOf("data-")!==-1?_30.contents.slice(15):_30.contents.slice(9);
var _33=_31?_32.slice(0,-7):_32.toString();
if(_8.widgetsInTemplate){
var _34=_2f.swallowNode();
_34.setAttribute("data-dojo-type",_33);
return new _8.DojoTypeNode(_34,_31);
}
return new dd.AttributeNode("data-dojo-type",_33);
},on:function(_35,_36){
var _37=_36.contents.split();
return new _8.EventNode(_37[0]+":"+_37.slice(1).join(" "));
}});
_8["data-dojo-type"]=_8.dojoType;
_8["data-dojo-attach-point"]=_8.dojoAttachPoint;
_8["data-dojo-attach-event"]=_8.dojoAttachEvent;
dd.register.tags("dojox.dtl.contrib",{"dijit":["attr:dojoType","attr:data-dojo-type","attr:dojoAttachPoint","attr:data-dojo-attach-point",["attr:attach","dojoAttachPoint"],["attr:attach","data-dojo-attach-point"],"attr:dojoAttachEvent","attr:data-dojo-attach-event",[/(attr:)?on(click|key(up))/i,"on"]]});
return dojox.dtl.contrib.dijit;
});
