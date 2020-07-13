//>>built
define("dojox/dtl/tag/loop",["dojo/_base/lang","dojo/_base/array","dojo/_base/json","../_base","dojox/string/tokenize"],function(_1,_2,_3,dd,_4){
var _5=_1.getObject("tag.loop",true,dd);
_5.CycleNode=_1.extend(function(_6,_7,_8,_9){
this.cyclevars=_6;
this.name=_7;
this.contents=_8;
this.shared=_9||{counter:-1,map:{}};
},{render:function(_a,_b){
if(_a.forloop&&!_a.forloop.counter0){
this.shared.counter=-1;
}
++this.shared.counter;
var _c=this.cyclevars[this.shared.counter%this.cyclevars.length];
var _d=this.shared.map;
if(!_d[_c]){
_d[_c]=new dd._Filter(_c);
}
_c=_d[_c].resolve(_a,_b);
if(this.name){
_a[this.name]=_c;
}
this.contents.set(_c);
return this.contents.render(_a,_b);
},unrender:function(_e,_f){
return this.contents.unrender(_e,_f);
},clone:function(_10){
return new this.constructor(this.cyclevars,this.name,this.contents.clone(_10),this.shared);
}});
_5.IfChangedNode=_1.extend(function(_11,_12,_13){
this.nodes=_11;
this._vars=_12;
this.shared=_13||{last:null,counter:0};
this.vars=_2.map(_12,function(_14){
return new dojox.dtl._Filter(_14);
});
},{render:function(_15,_16){
if(_15.forloop){
if(_15.forloop.counter<=this.shared.counter){
this.shared.last=null;
}
this.shared.counter=_15.forloop.counter;
}
var _17;
if(this.vars.length){
_17=_3.toJson(_2.map(this.vars,function(_18){
return _18.resolve(_15);
}));
}else{
_17=this.nodes.dummyRender(_15,_16);
}
if(_17!=this.shared.last){
var _19=(this.shared.last===null);
this.shared.last=_17;
_15=_15.push();
_15.ifchanged={firstloop:_19};
_16=this.nodes.render(_15,_16);
_15=_15.pop();
}else{
_16=this.nodes.unrender(_15,_16);
}
return _16;
},unrender:function(_1a,_1b){
return this.nodes.unrender(_1a,_1b);
},clone:function(_1c){
return new this.constructor(this.nodes.clone(_1c),this._vars,this.shared);
}});
_5.RegroupNode=_1.extend(function(_1d,key,_1e){
this._expression=_1d;
this.expression=new dd._Filter(_1d);
this.key=key;
this.alias=_1e;
},{_push:function(_1f,_20,_21){
if(_21.length){
_1f.push({grouper:_20,list:_21});
}
},render:function(_22,_23){
_22[this.alias]=[];
var _24=this.expression.resolve(_22);
if(_24){
var _25=null;
var _26=[];
for(var i=0;i<_24.length;i++){
var id=_24[i][this.key];
if(_25!==id){
this._push(_22[this.alias],_25,_26);
_25=id;
_26=[_24[i]];
}else{
_26.push(_24[i]);
}
}
this._push(_22[this.alias],_25,_26);
}
return _23;
},unrender:function(_27,_28){
return _28;
},clone:function(_29,_2a){
return this;
}});
_1.mixin(_5,{cycle:function(_2b,_2c){
var _2d=_2c.split_contents();
if(_2d.length<2){
throw new Error("'cycle' tag requires at least two arguments");
}
if(_2d[1].indexOf(",")!=-1){
var _2e=_2d[1].split(",");
_2d=[_2d[0]];
for(var i=0;i<_2e.length;i++){
_2d.push("\""+_2e[i]+"\"");
}
}
if(_2d.length==2){
var _2f=_2d[_2d.length-1];
if(!_2b._namedCycleNodes){
throw new Error("No named cycles in template: '"+_2f+"' is not defined");
}
if(!_2b._namedCycleNodes[_2f]){
throw new Error("Named cycle '"+_2f+"' does not exist");
}
return _2b._namedCycleNodes[_2f];
}
if(_2d.length>4&&_2d[_2d.length-2]=="as"){
var _2f=_2d[_2d.length-1];
var _30=new _5.CycleNode(_2d.slice(1,_2d.length-2),_2f,_2b.create_text_node());
if(!_2b._namedCycleNodes){
_2b._namedCycleNodes={};
}
_2b._namedCycleNodes[_2f]=_30;
}else{
_30=new _5.CycleNode(_2d.slice(1),null,_2b.create_text_node());
}
return _30;
},ifchanged:function(_31,_32){
var _33=_32.contents.split();
var _34=_31.parse(["endifchanged"]);
_31.delete_first_token();
return new _5.IfChangedNode(_34,_33.slice(1));
},regroup:function(_35,_36){
var _37=_4(_36.contents,/(\s+)/g,function(_38){
return _38;
});
if(_37.length<11||_37[_37.length-3]!="as"||_37[_37.length-7]!="by"){
throw new Error("Expected the format: regroup list by key as newList");
}
var _39=_37.slice(2,-8).join("");
var key=_37[_37.length-5];
var _3a=_37[_37.length-1];
return new _5.RegroupNode(_39,key,_3a);
}});
return _5;
});
