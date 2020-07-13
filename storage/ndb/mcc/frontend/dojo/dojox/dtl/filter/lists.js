//>>built
define("dojox/dtl/filter/lists",["dojo/_base/lang","../_base"],function(_1,dd){
var _2=_1.getObject("filter.lists",true,dd);
_1.mixin(_2,{_dictsort:function(a,b){
if(a[0]==b[0]){
return 0;
}
return (a[0]<b[0])?-1:1;
},dictsort:function(_3,_4){
if(!_4){
return _3;
}
var i,_5,_6=[];
if(!_1.isArray(_3)){
var _7=_3,_3=[];
for(var _8 in _7){
_3.push(_7[_8]);
}
}
for(i=0;i<_3.length;i++){
_6.push([new dojox.dtl._Filter("var."+_4).resolve(new dojox.dtl._Context({"var":_3[i]})),_3[i]]);
}
_6.sort(dojox.dtl.filter.lists._dictsort);
var _9=[];
for(i=0;_5=_6[i];i++){
_9.push(_5[1]);
}
return _9;
},dictsortreversed:function(_a,_b){
if(!_b){
return _a;
}
var _c=dojox.dtl.filter.lists.dictsort(_a,_b);
return _c.reverse();
},first:function(_d){
return (_d.length)?_d[0]:"";
},join:function(_e,_f){
return _e.join(_f||",");
},length:function(_10){
return (isNaN(_10.length))?(_10+"").length:_10.length;
},length_is:function(_11,arg){
return _11.length==parseInt(arg);
},random:function(_12){
return _12[Math.floor(Math.random()*_12.length)];
},slice:function(_13,arg){
arg=arg||"";
var _14=arg.split(":");
var _15=[];
for(var i=0;i<_14.length;i++){
if(!_14[i].length){
_15.push(null);
}else{
_15.push(parseInt(_14[i]));
}
}
if(_15[0]===null){
_15[0]=0;
}
if(_15[0]<0){
_15[0]=_13.length+_15[0];
}
if(_15.length<2||_15[1]===null){
_15[1]=_13.length;
}
if(_15[1]<0){
_15[1]=_13.length+_15[1];
}
return _13.slice(_15[0],_15[1]);
},_unordered_list:function(_16,_17){
var ddl=dojox.dtl.filter.lists;
var i,_18="";
for(i=0;i<_17;i++){
_18+="\t";
}
if(_16[1]&&_16[1].length){
var _19=[];
for(i=0;i<_16[1].length;i++){
_19.push(ddl._unordered_list(_16[1][i],_17+1));
}
return _18+"<li>"+_16[0]+"\n"+_18+"<ul>\n"+_19.join("\n")+"\n"+_18+"</ul>\n"+_18+"</li>";
}else{
return _18+"<li>"+_16[0]+"</li>";
}
},unordered_list:function(_1a){
return _2._unordered_list(_1a,1);
}});
return _2;
});
