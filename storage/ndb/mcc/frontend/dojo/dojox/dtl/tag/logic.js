//>>built
define("dojox/dtl/tag/logic",["dojo/_base/lang","../_base"],function(_1,dd){
_1.getObject("dojox.dtl.tag.logic",true);
var _2=dd.text;
var _3=dd.tag.logic;
_3.IfNode=_1.extend(function(_4,_5,_6,_7){
this.bools=_4;
this.trues=_5;
this.falses=_6;
this.type=_7;
},{render:function(_8,_9){
var i,_a,_b,_c,_d;
if(this.type=="or"){
for(i=0;_a=this.bools[i];i++){
_b=_a[0];
_c=_a[1];
_d=_c.resolve(_8);
if((_d&&!_b)||(_b&&!_d)){
if(this.falses){
_9=this.falses.unrender(_8,_9);
}
return (this.trues)?this.trues.render(_8,_9,this):_9;
}
}
if(this.trues){
_9=this.trues.unrender(_8,_9);
}
return (this.falses)?this.falses.render(_8,_9,this):_9;
}else{
for(i=0;_a=this.bools[i];i++){
_b=_a[0];
_c=_a[1];
_d=_c.resolve(_8);
if(_d==_b){
if(this.trues){
_9=this.trues.unrender(_8,_9);
}
return (this.falses)?this.falses.render(_8,_9,this):_9;
}
}
if(this.falses){
_9=this.falses.unrender(_8,_9);
}
return (this.trues)?this.trues.render(_8,_9,this):_9;
}
return _9;
},unrender:function(_e,_f){
_f=(this.trues)?this.trues.unrender(_e,_f):_f;
_f=(this.falses)?this.falses.unrender(_e,_f):_f;
return _f;
},clone:function(_10){
var _11=(this.trues)?this.trues.clone(_10):null;
var _12=(this.falses)?this.falses.clone(_10):null;
return new this.constructor(this.bools,_11,_12,this.type);
}});
_3.IfEqualNode=_1.extend(function(_13,_14,_15,_16,_17){
this.var1=new dd._Filter(_13);
this.var2=new dd._Filter(_14);
this.trues=_15;
this.falses=_16;
this.negate=_17;
},{render:function(_18,_19){
var _1a=this.var1.resolve(_18);
var _1b=this.var2.resolve(_18);
_1a=(typeof _1a!="undefined")?_1a:"";
_1b=(typeof _1a!="undefined")?_1b:"";
if((this.negate&&_1a!=_1b)||(!this.negate&&_1a==_1b)){
if(this.falses){
_19=this.falses.unrender(_18,_19,this);
}
return (this.trues)?this.trues.render(_18,_19,this):_19;
}
if(this.trues){
_19=this.trues.unrender(_18,_19,this);
}
return (this.falses)?this.falses.render(_18,_19,this):_19;
},unrender:function(_1c,_1d){
return _3.IfNode.prototype.unrender.call(this,_1c,_1d);
},clone:function(_1e){
var _1f=this.trues?this.trues.clone(_1e):null;
var _20=this.falses?this.falses.clone(_1e):null;
return new this.constructor(this.var1.getExpression(),this.var2.getExpression(),_1f,_20,this.negate);
}});
_3.ForNode=_1.extend(function(_21,_22,_23,_24){
this.assign=_21;
this.loop=new dd._Filter(_22);
this.reversed=_23;
this.nodelist=_24;
this.pool=[];
},{render:function(_25,_26){
var i,j,k;
var _27=false;
var _28=this.assign;
for(k=0;k<_28.length;k++){
if(typeof _25[_28[k]]!="undefined"){
_27=true;
_25=_25.push();
break;
}
}
if(!_27&&_25.forloop){
_27=true;
_25=_25.push();
}
var _29=this.loop.resolve(_25)||[];
for(i=_29.length;i<this.pool.length;i++){
this.pool[i].unrender(_25,_26,this);
}
if(this.reversed){
_29=_29.slice(0).reverse();
}
var _2a=_1.isObject(_29)&&!_1.isArrayLike(_29);
var _2b=[];
if(_2a){
for(var key in _29){
_2b.push(_29[key]);
}
}else{
_2b=_29;
}
var _2c=_25.forloop={parentloop:_25.get("forloop",{})};
var j=0;
for(i=0;i<_2b.length;i++){
var _2d=_2b[i];
_2c.counter0=j;
_2c.counter=j+1;
_2c.revcounter0=_2b.length-j-1;
_2c.revcounter=_2b.length-j;
_2c.first=!j;
_2c.last=(j==_2b.length-1);
if(_28.length>1&&_1.isArrayLike(_2d)){
if(!_27){
_27=true;
_25=_25.push();
}
var _2e={};
for(k=0;k<_2d.length&&k<_28.length;k++){
_2e[_28[k]]=_2d[k];
}
_1.mixin(_25,_2e);
}else{
_25[_28[0]]=_2d;
}
if(j+1>this.pool.length){
this.pool.push(this.nodelist.clone(_26));
}
_26=this.pool[j++].render(_25,_26,this);
}
delete _25.forloop;
if(_27){
_25=_25.pop();
}else{
for(k=0;k<_28.length;k++){
delete _25[_28[k]];
}
}
return _26;
},unrender:function(_2f,_30){
for(var i=0,_31;_31=this.pool[i];i++){
_30=_31.unrender(_2f,_30);
}
return _30;
},clone:function(_32){
return new this.constructor(this.assign,this.loop.getExpression(),this.reversed,this.nodelist.clone(_32));
}});
_1.mixin(_3,{if_:function(_33,_34){
var i,_35,_36,_37=[],_38=_34.contents.split();
_38.shift();
_34=_38.join(" ");
_38=_34.split(" and ");
if(_38.length==1){
_36="or";
_38=_34.split(" or ");
}else{
_36="and";
for(i=0;i<_38.length;i++){
if(_38[i].indexOf(" or ")!=-1){
throw new Error("'if' tags can't mix 'and' and 'or'");
}
}
}
for(i=0;_35=_38[i];i++){
var not=false;
if(_35.indexOf("not ")==0){
_35=_35.slice(4);
not=true;
}
_37.push([not,new dd._Filter(_35)]);
}
var _39=_33.parse(["else","endif"]);
var _3a=false;
var _34=_33.next_token();
if(_34.contents=="else"){
_3a=_33.parse(["endif"]);
_33.next_token();
}
return new _3.IfNode(_37,_39,_3a,_36);
},_ifequal:function(_3b,_3c,_3d){
var _3e=_3c.split_contents();
if(_3e.length!=3){
throw new Error(_3e[0]+" takes two arguments");
}
var end="end"+_3e[0];
var _3f=_3b.parse(["else",end]);
var _40=false;
var _3c=_3b.next_token();
if(_3c.contents=="else"){
_40=_3b.parse([end]);
_3b.next_token();
}
return new _3.IfEqualNode(_3e[1],_3e[2],_3f,_40,_3d);
},ifequal:function(_41,_42){
return _3._ifequal(_41,_42);
},ifnotequal:function(_43,_44){
return _3._ifequal(_43,_44,true);
},for_:function(_45,_46){
var _47=_46.contents.split();
if(_47.length<4){
throw new Error("'for' statements should have at least four words: "+_46.contents);
}
var _48=_47[_47.length-1]=="reversed";
var _49=(_48)?-3:-2;
if(_47[_47.length+_49]!="in"){
throw new Error("'for' tag received an invalid argument: "+_46.contents);
}
var _4a=_47.slice(1,_49).join(" ").split(/ *, */);
for(var i=0;i<_4a.length;i++){
if(!_4a[i]||_4a[i].indexOf(" ")!=-1){
throw new Error("'for' tag received an invalid argument: "+_46.contents);
}
}
var _4b=_45.parse(["endfor"]);
_45.next_token();
return new _3.ForNode(_4a,_47[_47.length+_49+1],_48,_4b);
}});
return dojox.dtl.tag.logic;
});
