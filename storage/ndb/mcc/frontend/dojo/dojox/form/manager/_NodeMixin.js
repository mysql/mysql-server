//>>built
define("dojox/form/manager/_NodeMixin",["dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/dom","dojo/dom-attr","dojo/query","./_Mixin","dijit/form/_FormWidget","dijit/_base/manager","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var fm=_1.getObject("dojox.form.manager",true),aa=fm.actionAdapter,_b=fm._keys,ce=fm.changeEvent=function(_c){
var _d="onclick";
switch(_c.tagName.toLowerCase()){
case "textarea":
_d="onkeyup";
break;
case "select":
_d="onchange";
break;
case "input":
switch(_c.type.toLowerCase()){
case "text":
case "password":
_d="onkeyup";
break;
}
break;
}
return _d;
},_e=function(_f,_10){
var _11=_5.get(_f,"name");
_10=_10||this.domNode;
if(_11&&!(_11 in this.formWidgets)){
for(var n=_f;n&&n!==_10;n=n.parentNode){
if(_5.get(n,"widgetId")&&_9.byNode(n).isInstanceOf(_8)){
return null;
}
}
if(_f.tagName.toLowerCase()=="input"&&_f.type.toLowerCase()=="radio"){
var a=this.formNodes[_11];
a=a&&a.node;
if(a&&_1.isArray(a)){
a.push(_f);
}else{
this.formNodes[_11]={node:[_f],connections:[]};
}
}else{
this.formNodes[_11]={node:_f,connections:[]};
}
}else{
_11=null;
}
return _11;
},_12=function(_13){
var _14={};
aa(function(_15,n){
var o=_5.get(n,"observer");
if(o&&typeof o=="string"){
_2.forEach(o.split(","),function(o){
o=_1.trim(o);
if(o&&_1.isFunction(this[o])){
_14[o]=1;
}
},this);
}
}).call(this,null,this.formNodes[_13].node);
return _b(_14);
},_16=function(_17,_18){
var t=this.formNodes[_17],c=t.connections;
if(c.length){
_2.forEach(c,_3.disconnect);
c=t.connections=[];
}
aa(function(_19,n){
var _1a=ce(n);
_2.forEach(_18,function(o){
c.push(_3.connect(n,_1a,this,function(evt){
if(this.watching){
this[o](this.formNodeValue(_17),_17,n,evt);
}
}));
},this);
}).call(this,null,t.node);
};
return _a("dojox.form.manager._NodeMixin",null,{destroy:function(){
for(var _1b in this.formNodes){
_2.forEach(this.formNodes[_1b].connections,_3.disconnect);
}
this.formNodes={};
this.inherited(arguments);
},registerNode:function(_1c){
if(typeof _1c=="string"){
_1c=_4.byId(_1c);
}
var _1d=_e.call(this,_1c);
if(_1d){
_16.call(this,_1d,_12.call(this,_1d));
}
return this;
},unregisterNode:function(_1e){
if(_1e in this.formNodes){
_2.forEach(this.formNodes[_1e].connections,this.disconnect,this);
delete this.formNodes[_1e];
}
return this;
},registerNodeDescendants:function(_1f){
if(typeof _1f=="string"){
_1f=_4.byId(_1f);
}
_6("input, select, textarea, button",_1f).map(function(n){
return _e.call(this,n,_1f);
},this).forEach(function(_20){
if(_20){
_16.call(this,_20,_12.call(this,_20));
}
},this);
return this;
},unregisterNodeDescendants:function(_21){
if(typeof _21=="string"){
_21=_4.byId(_21);
}
_6("input, select, textarea, button",_21).map(function(n){
return _5.get(_21,"name")||null;
}).forEach(function(_22){
if(_22){
this.unregisterNode(_22);
}
},this);
return this;
},formNodeValue:function(_23,_24){
var _25=arguments.length==2&&_24!==undefined,_26;
if(typeof _23=="string"){
_23=this.formNodes[_23];
if(_23){
_23=_23.node;
}
}
if(!_23){
return null;
}
if(_1.isArray(_23)){
if(_25){
_2.forEach(_23,function(_27){
_27.checked="";
});
_2.forEach(_23,function(_28){
_28.checked=_28.value===_24?"checked":"";
});
return this;
}
_2.some(_23,function(_29){
if(_29.checked){
_26=_29;
return true;
}
return false;
});
return _26?_26.value:"";
}
switch(_23.tagName.toLowerCase()){
case "select":
if(_23.multiple){
if(_25){
if(_1.isArray(_24)){
var _2a={};
_2.forEach(_24,function(v){
_2a[v]=1;
});
_6("> option",_23).forEach(function(opt){
opt.selected=opt.value in _2a;
});
return this;
}
_6("> option",_23).forEach(function(opt){
opt.selected=opt.value===_24;
});
return this;
}
var _26=_6("> option",_23).filter(function(opt){
return opt.selected;
}).map(function(opt){
return opt.value;
});
return _26.length==1?_26[0]:_26;
}
if(_25){
_6("> option",_23).forEach(function(opt){
opt.selected=opt.value===_24;
});
return this;
}
return _23.value||"";
case "button":
if(_25){
_23.innerHTML=""+_24;
return this;
}
return _23.innerHTML;
case "input":
if(_23.type.toLowerCase()=="checkbox"){
if(_25){
_23.checked=_24?"checked":"";
return this;
}
return Boolean(_23.checked);
}
}
if(_25){
_23.value=""+_24;
return this;
}
return _23.value;
},inspectFormNodes:function(_2b,_2c,_2d){
var _2e,_2f={};
if(_2c){
if(_1.isArray(_2c)){
_2.forEach(_2c,function(_30){
if(_30 in this.formNodes){
_2f[_30]=_2b.call(this,_30,this.formNodes[_30].node,_2d);
}
},this);
}else{
for(_2e in _2c){
if(_2e in this.formNodes){
_2f[_2e]=_2b.call(this,_2e,this.formNodes[_2e].node,_2c[_2e]);
}
}
}
}else{
for(_2e in this.formNodes){
_2f[_2e]=_2b.call(this,_2e,this.formNodes[_2e].node,_2d);
}
}
return _2f;
}});
});
