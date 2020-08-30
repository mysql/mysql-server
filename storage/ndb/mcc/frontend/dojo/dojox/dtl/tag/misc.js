//>>built
define("dojox/dtl/tag/misc",["dojo/_base/lang","dojo/_base/array","dojo/_base/connect","../_base"],function(_1,_2,_3,dd){
var _4=_1.getObject("tag.misc",true,dd);
_4.DebugNode=_1.extend(function(_5){
this.text=_5;
},{render:function(_6,_7){
var _8=_6.getKeys();
var _9=[];
var _a={};
for(var i=0,_b;_b=_8[i];i++){
_a[_b]=_6[_b];
_9+="["+_b+": "+typeof _6[_b]+"]\n";
}
return this.text.set(_9).render(_6,_7,this);
},unrender:function(_c,_d){
return _d;
},clone:function(_e){
return new this.constructor(this.text.clone(_e));
},toString:function(){
return "ddtm.DebugNode";
}});
_4.FilterNode=_1.extend(function(_f,_10){
this._varnode=_f;
this._nodelist=_10;
},{render:function(_11,_12){
var _13=this._nodelist.render(_11,new dojox.string.Builder());
_11=_11.update({"var":_13.toString()});
var _14=this._varnode.render(_11,_12);
_11=_11.pop();
return _12;
},unrender:function(_15,_16){
return _16;
},clone:function(_17){
return new this.constructor(this._expression,this._nodelist.clone(_17));
}});
_4.FirstOfNode=_1.extend(function(_18,_19){
this._vars=_18;
this.vars=_2.map(_18,function(_1a){
return new dojox.dtl._Filter(_1a);
});
this.contents=_19;
},{render:function(_1b,_1c){
for(var i=0,_1d;_1d=this.vars[i];i++){
var _1e=_1d.resolve(_1b);
if(typeof _1e!="undefined"){
if(_1e===null){
_1e="null";
}
this.contents.set(_1e);
return this.contents.render(_1b,_1c);
}
}
return this.contents.unrender(_1b,_1c);
},unrender:function(_1f,_20){
return this.contents.unrender(_1f,_20);
},clone:function(_21){
return new this.constructor(this._vars,this.contents.clone(_21));
}});
_4.SpacelessNode=_1.extend(function(_22,_23){
this.nodelist=_22;
this.contents=_23;
},{render:function(_24,_25){
if(_25.getParent){
var _26=[_3.connect(_25,"onAddNodeComplete",this,"_watch"),_3.connect(_25,"onSetParent",this,"_watchParent")];
_25=this.nodelist.render(_24,_25);
_3.disconnect(_26[0]);
_3.disconnect(_26[1]);
}else{
var _27=this.nodelist.dummyRender(_24);
this.contents.set(_27.replace(/>\s+</g,"><"));
_25=this.contents.render(_24,_25);
}
return _25;
},unrender:function(_28,_29){
return this.nodelist.unrender(_28,_29);
},clone:function(_2a){
return new this.constructor(this.nodelist.clone(_2a),this.contents.clone(_2a));
},_isEmpty:function(_2b){
return (_2b.nodeType==3&&!_2b.data.match(/[^\s\n]/));
},_watch:function(_2c){
if(this._isEmpty(_2c)){
var _2d=false;
if(_2c.parentNode.firstChild==_2c){
_2c.parentNode.removeChild(_2c);
}
}else{
var _2e=_2c.parentNode.childNodes;
if(_2c.nodeType==1&&_2e.length>2){
for(var i=2,_2f;_2f=_2e[i];i++){
if(_2e[i-2].nodeType==1&&this._isEmpty(_2e[i-1])){
_2c.parentNode.removeChild(_2e[i-1]);
return;
}
}
}
}
},_watchParent:function(_30){
var _31=_30.childNodes;
if(_31.length){
while(_30.childNodes.length){
var _32=_30.childNodes[_30.childNodes.length-1];
if(!this._isEmpty(_32)){
return;
}
_30.removeChild(_32);
}
}
}});
_4.TemplateTagNode=_1.extend(function(tag,_33){
this.tag=tag;
this.contents=_33;
},{mapping:{openblock:"{%",closeblock:"%}",openvariable:"{{",closevariable:"}}",openbrace:"{",closebrace:"}",opencomment:"{#",closecomment:"#}"},render:function(_34,_35){
this.contents.set(this.mapping[this.tag]);
return this.contents.render(_34,_35);
},unrender:function(_36,_37){
return this.contents.unrender(_36,_37);
},clone:function(_38){
return new this.constructor(this.tag,this.contents.clone(_38));
}});
_4.WidthRatioNode=_1.extend(function(_39,max,_3a,_3b){
this.current=new dd._Filter(_39);
this.max=new dd._Filter(max);
this.width=_3a;
this.contents=_3b;
},{render:function(_3c,_3d){
var _3e=+this.current.resolve(_3c);
var max=+this.max.resolve(_3c);
if(typeof _3e!="number"||typeof max!="number"||!max){
this.contents.set("");
}else{
this.contents.set(""+Math.round((_3e/max)*this.width));
}
return this.contents.render(_3c,_3d);
},unrender:function(_3f,_40){
return this.contents.unrender(_3f,_40);
},clone:function(_41){
return new this.constructor(this.current.getExpression(),this.max.getExpression(),this.width,this.contents.clone(_41));
}});
_4.WithNode=_1.extend(function(_42,_43,_44){
this.target=new dd._Filter(_42);
this.alias=_43;
this.nodelist=_44;
},{render:function(_45,_46){
var _47=this.target.resolve(_45);
_45=_45.push();
_45[this.alias]=_47;
_46=this.nodelist.render(_45,_46);
_45=_45.pop();
return _46;
},unrender:function(_48,_49){
return _49;
},clone:function(_4a){
return new this.constructor(this.target.getExpression(),this.alias,this.nodelist.clone(_4a));
}});
_1.mixin(_4,{comment:function(_4b,_4c){
_4b.skip_past("endcomment");
return dd._noOpNode;
},debug:function(_4d,_4e){
return new _4.DebugNode(_4d.create_text_node());
},filter:function(_4f,_50){
var _51=_50.contents.split(null,1)[1];
var _52=_4f.create_variable_node("var|"+_51);
var _53=_4f.parse(["endfilter"]);
_4f.next_token();
return new _4.FilterNode(_52,_53);
},firstof:function(_54,_55){
var _56=_55.split_contents().slice(1);
if(!_56.length){
throw new Error("'firstof' statement requires at least one argument");
}
return new _4.FirstOfNode(_56,_54.create_text_node());
},spaceless:function(_57,_58){
var _59=_57.parse(["endspaceless"]);
_57.delete_first_token();
return new _4.SpacelessNode(_59,_57.create_text_node());
},templatetag:function(_5a,_5b){
var _5c=_5b.contents.split();
if(_5c.length!=2){
throw new Error("'templatetag' statement takes one argument");
}
var tag=_5c[1];
var _5d=_4.TemplateTagNode.prototype.mapping;
if(!_5d[tag]){
var _5e=[];
for(var key in _5d){
_5e.push(key);
}
throw new Error("Invalid templatetag argument: '"+tag+"'. Must be one of: "+_5e.join(", "));
}
return new _4.TemplateTagNode(tag,_5a.create_text_node());
},widthratio:function(_5f,_60){
var _61=_60.contents.split();
if(_61.length!=4){
throw new Error("widthratio takes three arguments");
}
var _62=+_61[3];
if(typeof _62!="number"){
throw new Error("widthratio final argument must be an integer");
}
return new _4.WidthRatioNode(_61[1],_61[2],_62,_5f.create_text_node());
},with_:function(_63,_64){
var _65=_64.split_contents();
if(_65.length!=4||_65[2]!="as"){
throw new Error("do_width expected format as 'with value as name'");
}
var _66=_63.parse(["endwith"]);
_63.next_token();
return new _4.WithNode(_65[1],_65[3],_66);
}});
return _4;
});
