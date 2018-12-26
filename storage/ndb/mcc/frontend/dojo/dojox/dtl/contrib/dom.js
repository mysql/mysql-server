//>>built
define("dojox/dtl/contrib/dom",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/connect","dojo/dom-style","dojo/dom-construct","../_base","../dom"],function(_1,_2,_3,_4,_5,dd,_6){
var _7=_2.getObject("dojox.dtl.contrib.dom",true);
var _8={render:function(){
return this.contents;
}};
_7.StyleNode=_2.extend(function(_9){
this.contents={};
this._current={};
this._styles=_9;
for(var _a in _9){
if(_9[_a].indexOf("{{")!=-1){
var _b=new dd.Template(_9[_a]);
}else{
var _b=_2.delegate(_8);
_b.contents=_9[_a];
}
this.contents[_a]=_b;
}
},{render:function(_c,_d){
for(var _e in this.contents){
var _f=this.contents[_e].render(_c);
if(this._current[_e]!=_f){
_4.set(_d.getParent(),_e,this._current[_e]=_f);
}
}
return _d;
},unrender:function(_10,_11){
this._current={};
return _11;
},clone:function(_12){
return new this.constructor(this._styles);
}});
_7.BufferNode=_2.extend(function(_13,_14){
this.nodelist=_13;
this.options=_14;
},{_swap:function(_15,_16){
if(!this.swapped&&this.parent.parentNode){
if(_15=="node"){
if((_16.nodeType==3&&!this.options.text)||(_16.nodeType==1&&!this.options.node)){
return;
}
}else{
if(_15=="class"){
if(_15!="class"){
return;
}
}
}
this.onAddNode&&_3.disconnect(this.onAddNode);
this.onRemoveNode&&_3.disconnect(this.onRemoveNode);
this.onChangeAttribute&&_3.disconnect(this.onChangeAttribute);
this.onChangeData&&_3.disconnect(this.onChangeData);
this.swapped=this.parent.cloneNode(true);
this.parent.parentNode.replaceChild(this.swapped,this.parent);
}
},render:function(_17,_18){
this.parent=_18.getParent();
if(this.options.node){
this.onAddNode=_3.connect(_18,"onAddNode",_2.hitch(this,"_swap","node"));
this.onRemoveNode=_3.connect(_18,"onRemoveNode",_2.hitch(this,"_swap","node"));
}
if(this.options.text){
this.onChangeData=_3.connect(_18,"onChangeData",_2.hitch(this,"_swap","node"));
}
if(this.options["class"]){
this.onChangeAttribute=_3.connect(_18,"onChangeAttribute",_2.hitch(this,"_swap","class"));
}
_18=this.nodelist.render(_17,_18);
if(this.swapped){
this.swapped.parentNode.replaceChild(this.parent,this.swapped);
_5.destroy(this.swapped);
}else{
this.onAddNode&&_3.disconnect(this.onAddNode);
this.onRemoveNode&&_3.disconnect(this.onRemoveNode);
this.onChangeAttribute&&_3.disconnect(this.onChangeAttribute);
this.onChangeData&&_3.disconnect(this.onChangeData);
}
delete this.parent;
delete this.swapped;
return _18;
},unrender:function(_19,_1a){
return this.nodelist.unrender(_19,_1a);
},clone:function(_1b){
return new this.constructor(this.nodelist.clone(_1b),this.options);
}});
_2.mixin(_7,{buffer:function(_1c,_1d){
var _1e=_1d.contents.split().slice(1);
var _1f={};
var _20=false;
for(var i=_1e.length;i--;){
_20=true;
_1f[_1e[i]]=true;
}
if(!_20){
_1f.node=true;
}
var _21=_1c.parse(["endbuffer"]);
_1c.next_token();
return new _7.BufferNode(_21,_1f);
},html:function(_22,_23){
_1.deprecated("{% html someVariable %}","Use {{ someVariable|safe }} instead");
return _22.create_variable_node(_23.contents.slice(5)+"|safe");
},style_:function(_24,_25){
var _26={};
_25=_25.contents.replace(/^style\s+/,"");
var _27=_25.split(/\s*;\s*/g);
for(var i=0,_28;_28=_27[i];i++){
var _29=_28.split(/\s*:\s*/g);
var key=_29[0];
var _2a=_2.trim(_29[1]);
if(_2a){
_26[key]=_2a;
}
}
return new _7.StyleNode(_26);
}});
dd.register.tags("dojox.dtl.contrib",{"dom":["html","attr:style","buffer"]});
return dojox.dtl.contrib.dom;
});
