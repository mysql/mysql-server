//>>built
define("dojox/form/manager/_NodeMixin",["dojo/_base/lang","dojo/_base/array","dojo/on","dojo/dom","dojo/dom-attr","dojo/query","./_Mixin","dijit/form/_FormWidget","dijit/_base/manager","dojo/_base/declare"],function(_1,_2,on,_3,_4,_5,_6,_7,_8,_9){
var fm=_1.getObject("dojox.form.manager",true),aa=fm.actionAdapter,_a=fm._keys,ce=fm.changeEvent=function(_b){
var _c="click";
switch(_b.tagName.toLowerCase()){
case "textarea":
_c="keyup";
break;
case "select":
_c="change";
break;
case "input":
switch(_b.type.toLowerCase()){
case "text":
case "password":
_c="keyup";
break;
}
break;
}
return _c;
},_d=function(_e,_f){
var _10=_4.get(_e,"name");
_f=_f||this.domNode;
if(_10&&!(_10 in this.formWidgets)){
for(var n=_e;n&&n!==_f;n=n.parentNode){
if(_4.get(n,"widgetId")&&_8.byNode(n).isInstanceOf(_7)){
return null;
}
}
if(_e.tagName.toLowerCase()=="input"&&_e.type.toLowerCase()=="radio"){
var a=this.formNodes[_10];
a=a&&a.node;
if(a&&_1.isArray(a)){
a.push(_e);
}else{
this.formNodes[_10]={node:[_e],connections:[]};
}
}else{
this.formNodes[_10]={node:_e,connections:[]};
}
}else{
_10=null;
}
return _10;
},_11=function(_12){
var _13={};
aa(function(_14,n){
var o=_4.get(n,"data-dojo-observer")||_4.get(n,"observer");
if(o&&typeof o=="string"){
_2.forEach(o.split(","),function(o){
o=_1.trim(o);
if(o&&_1.isFunction(this[o])){
_13[o]=1;
}
},this);
}
}).call(this,null,this.formNodes[_12].node);
return _a(_13);
},_15=function(_16,_17){
var t=this.formNodes[_16],c=t.connections;
if(c.length){
_2.forEach(c,function(_18){
_18.remove();
});
c=t.connections=[];
}
aa(function(_19,n){
var _1a=ce(n);
_2.forEach(_17,function(o){
c.push(on(n,_1a,_1.hitch(this,function(evt){
if(this.watching){
this[o](this.formNodeValue(_16),_16,n,evt);
}
})));
},this);
}).call(this,null,t.node);
};
return _9("dojox.form.manager._NodeMixin",null,{destroy:function(){
for(var _1b in this.formNodes){
_2.forEach(this.formNodes[_1b].connections,function(_1c){
_1c.remove();
});
}
this.formNodes={};
this.inherited(arguments);
},registerNode:function(_1d){
if(typeof _1d=="string"){
_1d=_3.byId(_1d);
}
var _1e=_d.call(this,_1d);
if(_1e){
_15.call(this,_1e,_11.call(this,_1e));
}
return this;
},unregisterNode:function(_1f){
if(_1f in this.formNodes){
_2.forEach(this.formNodes[_1f].connections,function(_20){
_20.remove();
});
delete this.formNodes[_1f];
}
return this;
},registerNodeDescendants:function(_21){
if(typeof _21=="string"){
_21=_3.byId(_21);
}
_5("input, select, textarea, button",_21).map(function(n){
return _d.call(this,n,_21);
},this).forEach(function(_22){
if(_22){
_15.call(this,_22,_11.call(this,_22));
}
},this);
return this;
},unregisterNodeDescendants:function(_23){
if(typeof _23=="string"){
_23=_3.byId(_23);
}
_5("input, select, textarea, button",_23).map(function(n){
return _4.get(_23,"name")||null;
}).forEach(function(_24){
if(_24){
this.unregisterNode(_24);
}
},this);
return this;
},formNodeValue:function(_25,_26){
var _27=arguments.length==2&&_26!==undefined,_28;
if(typeof _25=="string"){
_25=this.formNodes[_25];
if(_25){
_25=_25.node;
}
}
if(!_25){
return null;
}
if(_1.isArray(_25)){
if(_27){
_2.forEach(_25,function(_29){
_29.checked="";
});
_2.forEach(_25,function(_2a){
_2a.checked=_2a.value===_26?"checked":"";
});
return this;
}
_2.some(_25,function(_2b){
if(_2b.checked){
_28=_2b;
return true;
}
return false;
});
return _28?_28.value:"";
}
switch(_25.tagName.toLowerCase()){
case "select":
if(_25.multiple){
if(_27){
if(_1.isArray(_26)){
var _2c={};
_2.forEach(_26,function(v){
_2c[v]=1;
});
_5("> option",_25).forEach(function(opt){
opt.selected=opt.value in _2c;
});
return this;
}
_5("> option",_25).forEach(function(opt){
opt.selected=opt.value===_26;
});
return this;
}
_28=_5("> option",_25).filter(function(opt){
return opt.selected;
}).map(function(opt){
return opt.value;
});
return _28.length==1?_28[0]:_28;
}
if(_27){
_5("> option",_25).forEach(function(opt){
opt.selected=opt.value===_26;
});
return this;
}
return _25.value||"";
case "button":
if(_27){
_25.innerHTML=""+_26;
return this;
}
return _25.innerHTML;
case "input":
if(_25.type.toLowerCase()=="checkbox"){
if(_27){
_25.checked=_26?"checked":"";
return this;
}
return Boolean(_25.checked);
}
}
if(_27){
_25.value=""+_26;
return this;
}
return _25.value;
},inspectFormNodes:function(_2d,_2e,_2f){
var _30,_31={};
if(_2e){
if(_1.isArray(_2e)){
_2.forEach(_2e,function(_32){
if(_32 in this.formNodes){
_31[_32]=_2d.call(this,_32,this.formNodes[_32].node,_2f);
}
},this);
}else{
for(_30 in _2e){
if(_30 in this.formNodes){
_31[_30]=_2d.call(this,_30,this.formNodes[_30].node,_2e[_30]);
}
}
}
}else{
for(_30 in this.formNodes){
_31[_30]=_2d.call(this,_30,this.formNodes[_30].node,_2f);
}
}
return _31;
}});
});
