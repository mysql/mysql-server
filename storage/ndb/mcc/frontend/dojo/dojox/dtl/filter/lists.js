//>>built
define("dojox/dtl/filter/lists",["dojo/_base/lang","../_base"],function(_1,dd){
_1.getObject("dojox.dtl.filter.lists",true);
_1.mixin(dd.filter.lists,{_dictsort:function(a,b){
if(a[0]==b[0]){
return 0;
}
return (a[0]<b[0])?-1:1;
},dictsort:function(_2,_3){
if(!_3){
return _2;
}
var i,_4,_5=[];
if(!_1.isArray(_2)){
var _6=_2,_2=[];
for(var _7 in _6){
_2.push(_6[_7]);
}
}
for(i=0;i<_2.length;i++){
_5.push([new dojox.dtl._Filter("var."+_3).resolve(new dojox.dtl._Context({"var":_2[i]})),_2[i]]);
}
_5.sort(dojox.dtl.filter.lists._dictsort);
var _8=[];
for(i=0;_4=_5[i];i++){
_8.push(_4[1]);
}
return _8;
},dictsortreversed:function(_9,_a){
if(!_a){
return _9;
}
var _b=dojox.dtl.filter.lists.dictsort(_9,_a);
return _b.reverse();
},first:function(_c){
return (_c.length)?_c[0]:"";
},join:function(_d,_e){
return _d.join(_e||",");
},length:function(_f){
return (isNaN(_f.length))?(_f+"").length:_f.length;
},length_is:function(_10,arg){
return _10.length==parseInt(arg);
},random:function(_11){
return _11[Math.floor(Math.random()*_11.length)];
},slice:function(_12,arg){
arg=arg||"";
var _13=arg.split(":");
var _14=[];
for(var i=0;i<_13.length;i++){
if(!_13[i].length){
_14.push(null);
}else{
_14.push(parseInt(_13[i]));
}
}
if(_14[0]===null){
_14[0]=0;
}
if(_14[0]<0){
_14[0]=_12.length+_14[0];
}
if(_14.length<2||_14[1]===null){
_14[1]=_12.length;
}
if(_14[1]<0){
_14[1]=_12.length+_14[1];
}
return _12.slice(_14[0],_14[1]);
},_unordered_list:function(_15,_16){
var ddl=dojox.dtl.filter.lists;
var i,_17="";
for(i=0;i<_16;i++){
_17+="\t";
}
if(_15[1]&&_15[1].length){
var _18=[];
for(i=0;i<_15[1].length;i++){
_18.push(ddl._unordered_list(_15[1][i],_16+1));
}
return _17+"<li>"+_15[0]+"\n"+_17+"<ul>\n"+_18.join("\n")+"\n"+_17+"</ul>\n"+_17+"</li>";
}else{
return _17+"<li>"+_15[0]+"</li>";
}
},unordered_list:function(_19){
return dojox.dtl.filter.lists._unordered_list(_19,1);
}});
return dojox.dtl.filter.lists;
});
