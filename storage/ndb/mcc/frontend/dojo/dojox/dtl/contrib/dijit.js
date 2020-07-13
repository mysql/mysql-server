//>>built
define("dojox/dtl/contrib/dijit",["dojo/_base/lang","dojo/_base/connect","dojo/_base/array","dojo/query","../_base","../dom","dojo/parser","dojo/_base/sniff"],function(_1,_2,_3,_4,dd,_5,_6,_7){
var _8=_1.getObject("contrib.dijit",true,dd);
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
var _1c=fn;
if(_1.isArray(_1a)){
_1c=function(e){
this[fn].apply(this,[e].concat(_1a));
};
}
this._rendered[i]=_2.connect(this._object,_19,_17.getThis(),fn);
}
}
}
this._clear=true;
return _18;
},unrender:function(_1d,_1e){
while(this._rendered.length){
_2.disconnect(this._rendered.pop());
}
return _1e;
},clone:function(){
return new this.constructor(this._command,this._object);
}});
function _1f(n1){
var n2=n1.cloneNode(true);
if(_7("ie")){
_4("script",n2).forEach("item.text = this[index].text;",_4("script",n1));
}
return n2;
};
_8.DojoTypeNode=_1.extend(function(_20,_21){
this._node=_20;
this._parsed=_21;
var _22=_20.getAttribute("dojoAttachEvent")||_20.getAttribute("data-dojo-attach-event");
if(_22){
this._events=new _8.EventNode(_1.trim(_22));
}
var _23=_20.getAttribute("dojoAttachPoint")||_20.getAttribute("data-dojo-attach-point");
if(_23){
this._attach=new _8.AttachNode(_1.trim(_23).split(/\s*,\s*/));
}
if(!_21){
this._dijit=_6.instantiate([_1f(_20)])[0];
}else{
_20=_1f(_20);
var old=_8.widgetsInTemplate;
_8.widgetsInTemplate=false;
this._template=new dd.DomTemplate(_20);
_8.widgetsInTemplate=old;
}
},{render:function(_24,_25){
if(this._parsed){
var _26=new dd.DomBuffer();
this._template.render(_24,_26);
var _27=_1f(_26.getRootNode());
var div=document.createElement("div");
div.appendChild(_27);
var _28=div.innerHTML;
div.removeChild(_27);
if(_28!=this._rendered){
this._rendered=_28;
if(this._dijit){
this._dijit.destroyRecursive();
}
this._dijit=_6.instantiate([_27])[0];
}
}
var _29=this._dijit.domNode;
if(this._events){
this._events._object=this._dijit;
this._events.render(_24,_25);
}
if(this._attach){
this._attach._object=this._dijit;
this._attach.render(_24,_25);
}
return _25.concat(_29);
},unrender:function(_2a,_2b){
return _2b.remove(this._dijit.domNode);
},clone:function(){
return new this.constructor(this._node,this._parsed);
}});
_1.mixin(_8,{widgetsInTemplate:true,dojoAttachPoint:function(_2c,_2d){
return new _8.AttachNode(_2d.contents.slice(_2d.contents.indexOf("data-")!==-1?23:16).split(/\s*,\s*/));
},dojoAttachEvent:function(_2e,_2f){
return new _8.EventNode(_2f.contents.slice(_2f.contents.indexOf("data-")!==-1?23:16));
},dojoType:function(_30,_31){
var _32=false;
if(_31.contents.slice(-7)==" parsed"){
_32=true;
}
var _33=_31.contents.indexOf("data-")!==-1?_31.contents.slice(15):_31.contents.slice(9);
var _34=_32?_33.slice(0,-7):_33.toString();
if(_8.widgetsInTemplate){
var _35=_30.swallowNode();
_35.setAttribute("data-dojo-type",_34);
return new _8.DojoTypeNode(_35,_32);
}
return new dd.AttributeNode("data-dojo-type",_34);
},on:function(_36,_37){
var _38=_37.contents.split();
return new _8.EventNode(_38[0]+":"+_38.slice(1).join(" "));
}});
_8["data-dojo-type"]=_8.dojoType;
_8["data-dojo-attach-point"]=_8.dojoAttachPoint;
_8["data-dojo-attach-event"]=_8.dojoAttachEvent;
dd.register.tags("dojox.dtl.contrib",{"dijit":["attr:dojoType","attr:data-dojo-type","attr:dojoAttachPoint","attr:data-dojo-attach-point",["attr:attach","dojoAttachPoint"],["attr:attach","data-dojo-attach-point"],"attr:dojoAttachEvent","attr:data-dojo-attach-event",[/(attr:)?on(click|key(up))/i,"on"]]});
return _8;
});
