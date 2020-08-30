//>>built
define("dojox/dtl/filter/htmlstrings",["dojo/_base/lang","../_base"],function(_1,dd){
var _2=_1.getObject("filter.htmlstrings",true,dd);
_1.mixin(_2,{_linebreaksrn:/(\r\n|\n\r)/g,_linebreaksn:/\n{2,}/g,_linebreakss:/(^\s+|\s+$)/g,_linebreaksbr:/\n/g,_removetagsfind:/[a-z0-9]+/g,_striptags:/<[^>]*?>/g,linebreaks:function(_3){
var _4=[];
var dh=_2;
_3=_3.replace(dh._linebreaksrn,"\n");
var _5=_3.split(dh._linebreaksn);
for(var i=0;i<_5.length;i++){
var _6=_5[i].replace(dh._linebreakss,"").replace(dh._linebreaksbr,"<br />");
_4.push("<p>"+_6+"</p>");
}
return _4.join("\n\n");
},linebreaksbr:function(_7){
var dh=_2;
return _7.replace(dh._linebreaksrn,"\n").replace(dh._linebreaksbr,"<br />");
},removetags:function(_8,_9){
var dh=_2;
var _a=[];
var _b;
while(_b=dh._removetagsfind.exec(_9)){
_a.push(_b[0]);
}
_a="("+_a.join("|")+")";
return _8.replace(new RegExp("</?s*"+_a+"s*[^>]*>","gi"),"");
},striptags:function(_c){
return _c.replace(dojox.dtl.filter.htmlstrings._striptags,"");
}});
return _2;
});
