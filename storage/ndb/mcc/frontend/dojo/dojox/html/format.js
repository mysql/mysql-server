//>>built
define("dojox/html/format",["dojo/_base/kernel","./entities","dojo/_base/array","dojo/_base/window","dojo/_base/sniff"],function(_1,_2,_3,_4,_5){
var _6=_1.getObject("dojox.html.format",true);
_6.prettyPrint=function(_7,_8,_9,_a,_b){
var _c=[];
var _d=0;
var _e=[];
var _f="\t";
var _10="";
var _11=[];
var i;
var _12=/[=]([^"']+?)(\s|>)/g;
var _13=/style=("[^"]*"|'[^']*'|\S*)/gi;
var _14=/[\w-]+=("[^"]*"|'[^']*'|\S*)/gi;
if(_8&&_8>0&&_8<10){
_f="";
for(i=0;i<_8;i++){
_f+=" ";
}
}
var _15=_4.doc.createElement("div");
_15.innerHTML=_7;
var _16=_2.encode;
var _17=_2.decode;
var _18=function(tag){
switch(tag){
case "a":
case "b":
case "strong":
case "s":
case "strike":
case "i":
case "u":
case "em":
case "sup":
case "sub":
case "span":
case "font":
case "big":
case "cite":
case "q":
case "small":
return true;
default:
return false;
}
};
var div=_15.ownerDocument.createElement("div");
var _19=function(_1a){
var _1b=_1a.cloneNode(false);
div.appendChild(_1b);
var _1c=div.innerHTML;
div.innerHTML="";
return _1c;
};
var _1d=function(){
var i,txt="";
for(i=0;i<_d;i++){
txt+=_f;
}
return txt.length;
};
var _1e=function(){
var i;
for(i=0;i<_d;i++){
_c.push(_f);
}
};
var _1f=function(){
_c.push("\n");
};
var _20=function(n){
_10+=_16(n.nodeValue,_a);
};
var _21=function(txt){
var i;
var _22;
var _23=txt.split("\n");
for(i=0;i<_23.length;i++){
_23[i]=_1.trim(_23[i]);
}
txt=_23.join(" ");
txt=_1.trim(txt);
if(txt!==""){
var _24=[];
if(_9&&_9>0){
var _25=_1d();
var _26=_9;
if(_9>_25){
_26-=_25;
}
while(txt){
if(txt.length>_9){
for(i=_26;(i>0&&txt.charAt(i)!==" ");i--){
}
if(!i){
for(i=_26;(i<txt.length&&txt.charAt(i)!==" ");i++){
}
}
var _27=txt.substring(0,i);
_27=_1.trim(_27);
txt=_1.trim(txt.substring((i==txt.length)?txt.length:i+1,txt.length));
if(_27){
_22="";
for(i=0;i<_d;i++){
_22+=_f;
}
_27=_22+_27+"\n";
}
_24.push(_27);
}else{
_22="";
for(i=0;i<_d;i++){
_22+=_f;
}
txt=_22+txt+"\n";
_24.push(txt);
txt=null;
}
}
return _24.join("");
}else{
_22="";
for(i=0;i<_d;i++){
_22+=_f;
}
txt=_22+txt+"\n";
return txt;
}
}else{
return "";
}
};
var _28=function(txt){
if(txt){
txt=txt.replace(/&quot;/gi,"\"");
txt=txt.replace(/&gt;/gi,">");
txt=txt.replace(/&lt;/gi,"<");
txt=txt.replace(/&amp;/gi,"&");
}
return txt;
};
var _29=function(txt){
if(txt){
txt=_28(txt);
var i,t,c,_2a;
var _2b=0;
var _2c=txt.split("\n");
var _2d=[];
for(i=0;i<_2c.length;i++){
var _2e=_2c[i];
var _2f=(_2e.indexOf("\n")>-1);
_2e=_1.trim(_2e);
if(_2e){
var _30=_2b;
for(c=0;c<_2e.length;c++){
var ch=_2e.charAt(c);
if(ch==="{"){
_2b++;
}else{
if(ch==="}"){
_2b--;
_30=_2b;
}
}
}
_2a="";
for(t=0;t<_d+_30;t++){
_2a+=_f;
}
_2d.push(_2a+_2e+"\n");
}else{
if(_2f&&i===0){
_2d.push("\n");
}
}
}
txt=_2d.join("");
}
return txt;
};
var _31=function(_32){
var _33=_32.nodeName.toLowerCase();
var _34=_1.trim(_19(_32));
var tag=_34.substring(0,_34.indexOf(">")+1);
tag=tag.replace(_12,"=\"$1\"$2");
tag=tag.replace(_13,function(_35){
var sL=_35.substring(0,6);
var _36=_35.substring(6,_35.length);
var _37=_36.charAt(0);
_36=_1.trim(_36.substring(1,_36.length-1));
_36=_36.split(";");
var _38=[];
_3.forEach(_36,function(s){
s=_1.trim(s);
if(s){
s=s.substring(0,s.indexOf(":")).toLowerCase()+s.substring(s.indexOf(":"),s.length);
_38.push(s);
}
});
_38=_38.sort();
_36=_38.join("; ");
var ts=_1.trim(_36);
if(!ts||ts===";"){
return "";
}else{
_36+=";";
return sL+_37+_36+_37;
}
});
var _39=[];
tag=tag.replace(_14,function(_3a){
_39.push(_1.trim(_3a));
return "";
});
_39=_39.sort();
tag="<"+_33;
if(_39.length){
tag+=" "+_39.join(" ");
}
if(_34.indexOf("</")!=-1){
_e.push(_33);
tag+=">";
}else{
if(_b){
tag+=" />";
}else{
tag+=">";
}
_e.push(false);
}
var _3b=_18(_33);
_11.push(_3b);
if(_10&&!_3b){
_c.push(_21(_10));
_10="";
}
if(!_3b){
_1e();
_c.push(tag);
_1f();
_d++;
}else{
_10+=tag;
}
};
var _3c=function(){
var _3d=_11.pop();
if(_10&&!_3d){
_c.push(_21(_10));
_10="";
}
var ct=_e.pop();
if(ct){
ct="</"+ct+">";
if(!_3d){
_d--;
_1e();
_c.push(ct);
_1f();
}else{
_10+=ct;
}
}else{
_d--;
}
};
var _3e=function(n){
var _3f=_17(n.nodeValue,_a);
_1e();
_c.push("<!--");
_1f();
_d++;
_c.push(_21(_3f));
_d--;
_1e();
_c.push("-->");
_1f();
};
var _40=function(_41){
var _42=_41.childNodes;
if(_42){
var i;
for(i=0;i<_42.length;i++){
var n=_42[i];
if(n.nodeType===1){
var tg=_1.trim(n.tagName.toLowerCase());
if(_5("ie")&&n.parentNode!=_41){
continue;
}
if(tg&&tg.charAt(0)==="/"){
continue;
}else{
_31(n);
if(tg==="script"){
_c.push(_29(n.innerHTML));
}else{
if(tg==="pre"){
var _43=n.innerHTML;
if(_5("mozilla")){
_43=_43.replace("<br>","\n");
_43=_43.replace("<pre>","");
_43=_43.replace("</pre>","");
}
if(_43.charAt(_43.length-1)!=="\n"){
_43+="\n";
}
_c.push(_43);
}else{
_40(n);
}
}
_3c();
}
}else{
if(n.nodeType===3||n.nodeType===4){
_20(n);
}else{
if(n.nodeType===8){
_3e(n);
}
}
}
}
}
};
_40(_15);
if(_10){
_c.push(_21(_10));
_10="";
}
return _c.join("");
};
return _6;
});
