//>>built
define("dojox/embed/Quicktime",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom","dojo/dom-construct","dojo/domReady"],function(_1,_2,_3,_4,_5,_6){
var _7,_8={major:0,minor:0,rev:0},_9,_a={width:320,height:240,redirect:null},_b="dojox-embed-quicktime-",_c=0,_d="This content requires the <a href=\"http://www.apple.com/quicktime/download/\" title=\"Download and install QuickTime.\">QuickTime plugin</a>.",_e=_1.getObject("dojox.embed",true);
function _f(_10){
_10=_1.mixin(_2.clone(_a),_10||{});
if(!("path" in _10)&&!_10.testing){
console.error("dojox.embed.Quicktime(ctor):: no path reference to a QuickTime movie was provided.");
return null;
}
if(_10.testing){
_10.path="";
}
if(!("id" in _10)){
_10.id=_b+_c++;
}
return _10;
};
if(_3("ie")){
_9=(function(){
try{
var o=new ActiveXObject("QuickTimeCheckObject.QuickTimeCheck.1");
if(o!==undefined){
var v=o.QuickTimeVersion.toString(16);
function p(i){
return (v.substring(i,i+1)-0)||0;
};
_8={major:p(0),minor:p(1),rev:p(2)};
return o.IsQuickTimeAvailable(0);
}
}
catch(e){
}
return false;
})();
_7=function(_11){
if(!_9){
return {id:null,markup:_d};
}
_11=_f(_11);
if(!_11){
return null;
}
var s="<object classid=\"clsid:02BF25D5-8C17-4B23-BC80-D3488ABDDC6B\" "+"codebase=\"http://www.apple.com/qtactivex/qtplugin.cab#version=6,0,2,0\" "+"id=\""+_11.id+"\" "+"width=\""+_11.width+"\" "+"height=\""+_11.height+"\">"+"<param name=\"src\" value=\""+_11.path+"\"/>";
for(var p in _11.params||{}){
s+="<param name=\""+p+"\" value=\""+_11.params[p]+"\"/>";
}
s+="</object>";
return {id:_11.id,markup:s};
};
}else{
_9=(function(){
for(var i=0,p=navigator.plugins,l=p.length;i<l;i++){
if(p[i].name.indexOf("QuickTime")>-1){
return true;
}
}
return false;
})();
_7=function(_12){
if(!_9){
return {id:null,markup:_d};
}
_12=_f(_12);
if(!_12){
return null;
}
var s="<embed type=\"video/quicktime\" src=\""+_12.path+"\" "+"id=\""+_12.id+"\" "+"name=\""+_12.id+"\" "+"pluginspage=\"www.apple.com/quicktime/download\" "+"enablejavascript=\"true\" "+"width=\""+_12.width+"\" "+"height=\""+_12.height+"\"";
for(var p in _12.params||{}){
s+=" "+p+"=\""+_12.params[p]+"\"";
}
s+="></embed>";
return {id:_12.id,markup:s};
};
}
_e.Quicktime=function(_13,_14){
return _e.Quicktime.place(_13,_14);
};
_1.mixin(_e.Quicktime,{minSupported:6,available:_9,supported:_9,version:_8,initialized:false,onInitialize:function(){
_e.Quicktime.initialized=true;
},place:function(_15,_16){
var o=_7(_15);
if(!(_16=_5.byId(_16))){
_16=_6.create("div",{id:o.id+"-container"},_4.body());
}
if(o){
_16.innerHTML=o.markup;
if(o.id){
return _3("ie")?dom.byId(o.id):document[o.id];
}
}
return null;
}});
if(!_3("ie")){
var id="-qt-version-test",o=_7({testing:true,width:4,height:4}),c=10,top="-1000px",_17="1px";
function _18(){
setTimeout(function(){
var qt=document[o.id],n=_5.byId(id);
if(qt){
try{
var v=qt.GetQuickTimeVersion().split(".");
_e.Quicktime.version={major:parseInt(v[0]||0),minor:parseInt(v[1]||0),rev:parseInt(v[2]||0)};
if((_e.Quicktime.supported=v[0])){
_e.Quicktime.onInitialize();
}
c=0;
}
catch(e){
if(c--){
_18();
}
}
}
if(!c&&n){
_6.destroy(n);
}
},20);
};
if(_4.doc.readyState==="loaded"||_4.doc.readyState==="complete"){
_6.create("div",{innerHTML:o.markup,id:id,style:{top:top,left:0,width:_17,height:_17,overflow:"hidden",position:"absolute"}},_4.body());
}else{
document.write("<div style=\"top:"+top+";left:0;width:"+_17+";height:"+_17+";overflow:hidden;position:absolute\" id=\""+id+"\">"+o.markup+"</div>");
}
_18();
}else{
if(_3("ie")&&_9){
setTimeout(function(){
_e.Quicktime.onInitialize();
},10);
}
}
return _e.Quicktime;
});
