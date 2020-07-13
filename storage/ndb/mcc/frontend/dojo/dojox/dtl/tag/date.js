//>>built
define("dojox/dtl/tag/date",["dojo/_base/lang","../_base","../utils/date"],function(_1,dd,_2){
var _3=_1.getObject("tag.date",true,dd);
_3.NowNode=function(_4,_5){
this._format=_4;
this.format=new _2.DateFormat(_4);
this.contents=_5;
};
_1.extend(_3.NowNode,{render:function(_6,_7){
this.contents.set(this.format.format(new Date()));
return this.contents.render(_6,_7);
},unrender:function(_8,_9){
return this.contents.unrender(_8,_9);
},clone:function(_a){
return new this.constructor(this._format,this.contents.clone(_a));
}});
_3.now=function(_b,_c){
var _d=_c.split_contents();
if(_d.length!=2){
throw new Error("'now' statement takes one argument");
}
return new _3.NowNode(_d[1].slice(1,-1),_b.create_text_node());
};
return _3;
});
