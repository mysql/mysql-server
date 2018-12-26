//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/NodeList-traverse,dojo/NodeList-manipulate,dojo/io/script"],function(_1,_2,_3){
_2.provide("dojox.jq");
_2.require("dojo.NodeList-traverse");
_2.require("dojo.NodeList-manipulate");
_2.require("dojo.io.script");
(function(){
_2.config.ioPublish=true;
var _4="|img|meta|hr|br|input|";
function _5(_6,_7){
_6+="";
_6=_6.replace(/<\s*(\w+)([^\/\>]*)\/\s*>/g,function(_8,_9,_a){
if(_4.indexOf("|"+_9+"|")==-1){
return "<"+_9+_a+"></"+_9+">";
}else{
return _8;
}
});
return _2._toDom(_6,_7);
};
function _b(_c){
var _d=_c.indexOf("-");
if(_d!=-1){
if(_d==0){
_c=_c.substring(1);
}
_c=_c.replace(/-(\w)/g,function(_e,_f){
return _f.toUpperCase();
});
}
return _c;
};
var _10=_2.global.$;
var _11=_2.global.jQuery;
var $=_2.global.$=_2.global.jQuery=function(){
var arg=arguments[0];
if(!arg){
return $._wrap([],null,$);
}else{
if(_2.isString(arg)){
if(arg.charAt(0)=="<"){
arg=_5(arg);
if(arg.nodeType==11){
arg=arg.childNodes;
}else{
return $._wrap([arg],null,$);
}
}else{
var _12=_2._NodeListCtor;
_2._NodeListCtor=$;
var _13=arguments[1];
if(_13&&_13._is$){
_13=_13[0];
}else{
if(_2.isString(_13)){
_13=_2.query(_13)[0];
}
}
var nl=_2.query.call(this,arg,_13);
_2._NodeListCtor=_12;
return nl;
}
}else{
if(_2.isFunction(arg)){
$.ready(arg);
return $;
}else{
if(arg==document||arg==window){
return $._wrap([arg],null,$);
}else{
if(_2.isArray(arg)){
var ary=[];
for(var i=0;i<arg.length;i++){
if(_2.indexOf(ary,arg[i])==-1){
ary.push(arg[i]);
}
}
return $._wrap(arg,null,$);
}else{
if("nodeType" in arg){
return $._wrap([arg],null,$);
}
}
}
}
}
}
return $._wrap(_2._toArray(arg),null,$);
};
var _14=_2.NodeList.prototype;
var f=$.fn=$.prototype=_2.delegate(_14);
$._wrap=_2.NodeList._wrap;
var _15=/^H\d/i;
var _16=_2.query.pseudos;
_2.mixin(_16,{has:function(_17,_18){
return function(_19){
return $(_18,_19).length;
};
},visible:function(_1a,_1b){
return function(_1c){
return _2.style(_1c,"visible")!="hidden"&&_2.style(_1c,"display")!="none";
};
},hidden:function(_1d,_1e){
return function(_1f){
return _1f.type=="hidden"||_2.style(_1f,"visible")=="hidden"||_2.style(_1f,"display")=="none";
};
},selected:function(_20,_21){
return function(_22){
return _22.selected;
};
},checked:function(_23,_24){
return function(_25){
return _25.nodeName.toUpperCase()=="INPUT"&&_25.checked;
};
},disabled:function(_26,_27){
return function(_28){
return _28.getAttribute("disabled");
};
},enabled:function(_29,_2a){
return function(_2b){
return !_2b.getAttribute("disabled");
};
},input:function(_2c,_2d){
return function(_2e){
var n=_2e.nodeName.toUpperCase();
return n=="INPUT"||n=="SELECT"||n=="TEXTAREA"||n=="BUTTON";
};
},button:function(_2f,_30){
return function(_31){
return (_31.nodeName.toUpperCase()=="INPUT"&&_31.type=="button")||_31.nodeName.toUpperCase()=="BUTTON";
};
},header:function(_32,_33){
return function(_34){
return _34.nodeName.match(_15);
};
}});
var _35={};
_2.forEach(["text","password","radio","checkbox","submit","image","reset","file"],function(_36){
_35[_36]=function(_37,_38){
return function(_39){
return _39.nodeName.toUpperCase()=="INPUT"&&_39.type==_36;
};
};
});
_2.mixin(_16,_35);
$.browser={mozilla:_2.isMoz,msie:_2.isIE,opera:_2.isOpera,safari:_2.isSafari};
$.browser.version=_2.isIE||_2.isMoz||_2.isOpera||_2.isSafari||_2.isWebKit;
$.ready=$.fn.ready=function(_3a){
_2.addOnLoad(_2.hitch(null,_3a,$));
return this;
};
f._is$=true;
f.size=function(){
return this.length;
};
$.prop=function(_3b,_3c){
if(_2.isFunction(_3c)){
return _3c.call(_3b);
}else{
return _3c;
}
};
$.className={add:_2.addClass,remove:_2.removeClass,has:_2.hasClass};
$.makeArray=function(_3d){
if(typeof _3d=="undefined"){
return [];
}else{
if(_3d.length&&!_2.isString(_3d)&&!("location" in _3d)){
return _2._toArray(_3d);
}else{
return [_3d];
}
}
};
$.merge=function(_3e,_3f){
var _40=[_3e.length,0];
_40=_40.concat(_3f);
_3e.splice.apply(_3e,_40);
return _3e;
};
$.each=function(_41,cb){
if(_2.isArrayLike(_41)){
for(var i=0;i<_41.length;i++){
if(cb.call(_41[i],i,_41[i])===false){
break;
}
}
}else{
if(_2.isObject(_41)){
for(var _42 in _41){
if(cb.call(_41[_42],_42,_41[_42])===false){
break;
}
}
}
}
return this;
};
f.each=function(cb){
return $.each.call(this,this,cb);
};
f.eq=function(){
var nl=$();
_2.forEach(arguments,function(i){
if(this[i]){
nl.push(this[i]);
}
},this);
return nl;
};
f.get=function(_43){
if(_43||_43==0){
return this[_43];
}
return this;
};
f.index=function(arg){
if(arg._is$){
arg=arg[0];
}
return this.indexOf(arg);
};
var _44=[];
var _45=0;
var _46=_2._scopeName+"DataId";
var _47=function(_48){
var id=_48.getAttribute(_46);
if(!id){
id=_45++;
_48.setAttribute(_46,id);
}
};
var _49=function(_4a){
var _4b={};
if(_4a.nodeType==1){
var id=_47(_4a);
_4b=_44[id];
if(!_4b){
_4b=_44[id]={};
}
}
return _4b;
};
$.data=function(_4c,_4d,_4e){
var _4f=null;
if(_4d=="events"){
_4f=_50[_4c.getAttribute(_51)];
var _52=true;
if(_4f){
for(var _53 in _4f){
_52=false;
break;
}
}
return _52?null:_4f;
}
var _54=_49(_4c);
if(typeof _4e!="undefined"){
_54[_4d]=_4e;
}else{
_4f=_54[_4d];
}
return _4e?this:_4f;
};
$.removeData=function(_55,_56){
var _57=_49(_55);
delete _57[_56];
if(_55.nodeType==1){
var _58=true;
for(var _59 in _57){
_58=false;
break;
}
if(_58){
_55.removeAttribute(_46);
}
}
return this;
};
f.data=function(_5a,_5b){
var _5c=null;
this.forEach(function(_5d){
_5c=$.data(_5d,_5a,_5b);
});
return _5b?this:_5c;
};
f.removeData=function(_5e){
this.forEach(function(_5f){
$.removeData(_5f,_5e);
});
return this;
};
function _60(obj,_61){
if(obj==_61){
return obj;
}
var _62={};
for(var x in _61){
if((_62[x]===undefined||_62[x]!=_61[x])&&_61[x]!==undefined&&obj!=_61[x]){
if(_2.isObject(obj[x])&&_2.isObject(_61[x])){
if(_2.isArray(_61[x])){
obj[x]=_61[x];
}else{
obj[x]=_60(obj[x],_61[x]);
}
}else{
obj[x]=_61[x];
}
}
}
if(_2.isIE&&_61){
var p=_61.toString;
if(typeof p=="function"&&p!=obj.toString&&p!=_62.toString&&p!="\nfunction toString() {\n    [native code]\n}\n"){
obj.toString=_61.toString;
}
}
return obj;
};
f.extend=function(){
var _63=[this];
_63=_63.concat(arguments);
return $.extend.apply($,_63);
};
$.extend=function(){
var _64=arguments,_65;
for(var i=0;i<_64.length;i++){
var obj=_64[i];
if(obj&&_2.isObject(obj)){
if(!_65){
_65=obj;
}else{
_60(_65,obj);
}
}
}
return _65;
};
$.noConflict=function(_66){
var me=$;
_2.global.$=_10;
if(_66){
_2.global.jQuery=_11;
}
return me;
};
f.attr=function(_67,_68){
if(arguments.length==1&&_2.isString(arguments[0])){
var _69=this[0];
if(!_69){
return null;
}
var arg=arguments[0];
var _6a=_2.attr(_69,arg);
var _6b=_69[arg];
if((arg in _69)&&!_2.isObject(_6b)&&_67!="href"){
return _6b;
}else{
return _6a||_6b;
}
}else{
if(_2.isObject(_67)){
for(var _6c in _67){
this.attr(_6c,_67[_6c]);
}
return this;
}else{
var _6d=_2.isFunction(_68);
this.forEach(function(_6e,_6f){
var _70=_6e[_67];
if((_67 in _6e)&&!_2.isObject(_70)&&_67!="href"){
_6e[_67]=(_6d?_68.call(_6e,_6f):_68);
}else{
if(_6e.nodeType==1){
_2.attr(_6e,_67,(_6d?_68.call(_6e,_6f):_68));
}
}
});
return this;
}
}
};
f.removeAttr=function(_71){
this.forEach(function(_72,_73){
var _74=_72[_71];
if((_71 in _72)&&!_2.isObject(_74)&&_71!="href"){
delete _72[_71];
}else{
if(_72.nodeType==1){
if(_71=="class"){
_72.removeAttribute(_71);
}else{
_2.removeAttr(_72,_71);
}
}
}
});
return this;
};
f.toggleClass=function(_75,_76){
var _77=arguments.length>1;
this.forEach(function(_78){
_2.toggleClass(_78,_75,_77?_76:!_2.hasClass(_78,_75));
});
return this;
};
f.toggle=function(){
var _79=arguments;
if(arguments.length>1&&_2.isFunction(arguments[0])){
var _7a=0;
var _7b=function(){
var _7c=_79[_7a].apply(this,arguments);
_7a+=1;
if(_7a>_79.length-1){
_7a=0;
}
};
return this.bind("click",_7b);
}else{
var _7d=arguments.length==1?arguments[0]:undefined;
this.forEach(function(_7e){
var _7f=typeof _7d=="undefined"?_2.style(_7e,"display")=="none":_7d;
var _80=(_7f?"show":"hide");
var nl=$(_7e);
nl[_80].apply(nl,_79);
});
return this;
}
};
f.hasClass=function(_81){
return this.some(function(_82){
return _2.hasClass(_82,_81);
});
};
f.html=f.innerHTML;
_2.forEach(["filter","slice"],function(_83){
f[_83]=function(){
var nl;
if(_2.isFunction(arguments[0])){
var _84=arguments[0];
arguments[0]=function(_85,_86){
return _84.call(_85,_85,_86);
};
}
if(_83=="filter"&&_2.isString(arguments[0])){
var nl=this._filterQueryResult(this,arguments[0]);
}else{
var _87=_2._NodeListCtor;
_2._NodeListCtor=f;
nl=$(_14[_83].apply(this,arguments));
_2._NodeListCtor=_87;
}
return nl._stash(this);
};
});
f.map=function(_88){
return this._buildArrayFromCallback(_88);
};
$.map=function(ary,_89){
return f._buildArrayFromCallback.call(ary,_89);
};
$.inArray=function(_8a,ary){
return _2.indexOf(ary,_8a);
};
f.is=function(_8b){
return (_8b?!!this.filter(_8b).length:false);
};
f.not=function(){
var _8c=$.apply($,arguments);
var nl=$(_14.filter.call(this,function(_8d){
return _8c.indexOf(_8d)==-1;
}));
return nl._stash(this);
};
f.add=function(){
return this.concat.apply(this,arguments);
};
function _8e(_8f){
var doc=_8f.contentDocument||(((_8f.name)&&(_8f.document)&&(document.getElementsByTagName("iframe")[_8f.name].contentWindow)&&(document.getElementsByTagName("iframe")[_8f.name].contentWindow.document)))||((_8f.name)&&(document.frames[_8f.name])&&(document.frames[_8f.name].document))||null;
return doc;
};
f.contents=function(){
var ary=[];
this.forEach(function(_90){
if(_90.nodeName.toUpperCase()=="IFRAME"){
var doc=_8e(_90);
if(doc){
ary.push(doc);
}
}else{
var _91=_90.childNodes;
for(var i=0;i<_91.length;i++){
ary.push(_91[i]);
}
}
});
return this._wrap(ary)._stash(this);
};
f.find=function(_92){
var ary=[];
this.forEach(function(_93){
if(_93.nodeType==1){
ary=ary.concat(_2._toArray($(_92,_93)));
}
});
return this._getUniqueAsNodeList(ary)._stash(this);
};
f.andSelf=function(){
return this.add(this._parent);
};
f.remove=function(_94){
var nl=(_94?this._filterQueryResult(this,_94):this);
nl.removeData();
nl.forEach(function(_95){
_95.parentNode.removeChild(_95);
});
return this;
};
$.css=function(_96,_97,_98){
_97=_b(_97);
var _99=(_98?_2.style(_96,_97,_98):_2.style(_96,_97));
return _99;
};
f.css=function(_9a,_9b){
if(_2.isString(_9a)){
_9a=_b(_9a);
if(arguments.length==2){
if(!_2.isString(_9b)&&_9a!="zIndex"){
_9b=_9b+"px";
}
this.forEach(function(_9c){
if(_9c.nodeType==1){
_2.style(_9c,_9a,_9b);
}
});
return this;
}else{
_9b=_2.style(this[0],_9a);
if(!_2.isString(_9b)&&_9a!="zIndex"){
_9b=_9b+"px";
}
return _9b;
}
}else{
for(var _9d in _9a){
this.css(_9d,_9a[_9d]);
}
return this;
}
};
function _9e(nl,_9f,_a0,_a1){
if(_a1){
var mod={};
mod[_a0]=_a1;
nl.forEach(function(_a2){
_2[_9f](_a2,mod);
});
return nl;
}else{
return Math.abs(Math.round(_2[_9f](nl[0])[_a0]));
}
};
f.height=function(_a3){
return _9e(this,"contentBox","h",_a3);
};
f.width=function(_a4){
return _9e(this,"contentBox","w",_a4);
};
function _a5(_a6,_a7,_a8,_a9,_aa){
var _ab=false;
if((_ab=_a6.style.display=="none")){
_a6.style.display="block";
}
var cs=_2.getComputedStyle(_a6);
var _ac=Math.abs(Math.round(_2._getContentBox(_a6,cs)[_a7]));
var pad=_a8?Math.abs(Math.round(_2._getPadExtents(_a6,cs)[_a7])):0;
var _ad=_a9?Math.abs(Math.round(_2._getBorderExtents(_a6,cs)[_a7])):0;
var _ae=_aa?Math.abs(Math.round(_2._getMarginExtents(_a6,cs)[_a7])):0;
if(_ab){
_a6.style.display="none";
}
return pad+_ac+_ad+_ae;
};
f.innerHeight=function(){
return _a5(this[0],"h",true);
};
f.innerWidth=function(){
return _a5(this[0],"w",true);
};
f.outerHeight=function(_af){
return _a5(this[0],"h",true,true,_af);
};
f.outerWidth=function(_b0){
return _a5(this[0],"w",true,true,_b0);
};
var _50=[];
var _b1=1;
var _51=_2._scopeName+"eventid";
var _b2;
function _b3(_b4){
_b4=_b4.split("$$")[0];
var _b5=_b4.indexOf(".");
if(_b5!=-1){
_b4=_b4.substring(0,_b5);
}
return _b4;
};
function _b6(_b7,_b8){
if(_b8.indexOf("ajax")==0){
return _2.subscribe(_b9[_b8],function(dfd,res){
var _ba=new $.Event(_b8);
if("ajaxComplete|ajaxSend|ajaxSuccess".indexOf(_b8)!=-1){
_bb(_b7,[_ba,dfd.ioArgs.xhr,dfd.ioArgs.args]);
}else{
if(_b8=="ajaxError"){
_bb(_b7,[_ba,dfd.ioArgs.xhr,dfd.ioArgs.args,res]);
}else{
_bb(_b7,[_ba]);
}
}
});
}else{
return _2.connect(_b7,"on"+_b8,function(e){
_bb(_b7,arguments);
});
}
};
$.Event=function(_bc){
if(this==$){
return new $.Event(_bc);
}
if(typeof _bc=="string"){
this.type=_bc.replace(/!/,"");
}else{
_2.mixin(this,_bc);
}
this.timeStamp=(new Date()).getTime();
this._isFake=true;
this._isStrict=(this.type.indexOf("!")!=-1);
};
var ep=$.Event.prototype={preventDefault:function(){
this.isDefaultPrevented=this._true;
},stopPropagation:function(){
this.isPropagationStopped=this._true;
},stopImmediatePropagation:function(){
this.isPropagationStopped=this._true;
this.isImmediatePropagationStopped=this._true;
},_true:function(){
return true;
},_false:function(){
return false;
}};
_2.mixin(ep,{isPropagationStopped:ep._false,isImmediatePropagationStopped:ep._false,isDefaultPrevented:ep._false});
function _bd(_be,_bf){
_be=_be||[];
_be=[].concat(_be);
var evt=_be[0];
if(!evt||!evt.preventDefault){
evt=_bf&&_bf.preventDefault?_bf:new $.Event(_bf);
_be.unshift(evt);
}
return _be;
};
var _c0=false;
function _bb(_c1,_c2,_c3){
_c0=true;
_c2=_c2||_b2;
_c3=_c3;
if(_c1.nodeType==9){
_c1=_c1.documentElement;
}
var _c4=_c1.getAttribute(_51);
if(!_c4){
return;
}
var evt=_c2[0];
var _c5=evt.type;
var _c6=_b3(_c5);
var cbs=_50[_c4][_c6];
var _c7;
if(_c3){
_c7=_c3.apply(_c1,_c2);
}
if(_c7!==false){
for(var _c8 in cbs){
if(_c8!="_connectId"&&(!evt._isStrict&&(_c8.indexOf(_c5)==0)||(evt._isStrict&&_c8==_c5))){
evt[_2._scopeName+"callbackId"]=_c8;
var cb=cbs[_c8];
if(typeof cb.data!="undefined"){
evt.data=cb.data;
}else{
evt.data=null;
}
if((_c7=cb.fn.apply(evt.target,_c2))===false&&!evt._isFake){
_2.stopEvent(evt);
}
evt.result=_c7;
}
}
}
return _c7;
};
f.triggerHandler=function(_c9,_ca,_cb){
var _cc=this[0];
if(_cc&&_cc.nodeType!=3&&_cc.nodeType!=8){
_ca=_bd(_ca,_c9);
return _bb(_cc,_ca,_cb);
}else{
return undefined;
}
};
f.trigger=function(_cd,_ce,_cf){
_ce=_bd(_ce,_cd);
var evt=_ce[0];
var _cd=_b3(evt.type);
_b2=_ce;
currentExtraFunc=_cf;
var _d0=null;
var _d1=!evt.target;
this.forEach(function(_d2){
if(_d2.nodeType!=3&&_d2.nodeType!=8){
if(_d2.nodeType==9){
_d2=_d2.documentElement;
}
if(evt._isFake){
evt.currentTarget=_d2;
if(_d1){
evt.target=_d2;
}
}
if(_cf){
var _d3=_ce.slice(1);
_d0=_cf.apply(_d2,(_d0=null?_d3:_d3.concat(_d0)));
}
if(_d0!==false){
_c0=false;
if(_d2[_cd]){
try{
_d0=_d2[_cd]();
}
catch(e){
}
}else{
if(_d2["on"+_cd]){
try{
_d0=_d2["on"+_cd]();
}
catch(e){
}
}
}
if(!_c0){
_d0=_bb(_d2,_ce);
}
var _d4=_d2.parentNode;
if(_d0!==false&&!evt.isImmediatePropagationStopped()&&!evt.isPropagationStopped()&&_d4&&_d4.nodeType==1){
$(_d4).trigger(_cd,_ce,_cf);
}
}
}
});
_b2=null;
currentExtraFunc=null;
return this;
};
var _d5=0;
f.bind=function(_d6,_d7,fn){
_d6=_d6.split(" ");
if(!fn){
fn=_d7;
_d7=null;
}
this.forEach(function(_d8){
if(_d8.nodeType!=3&&_d8.nodeType!=8){
if(_d8.nodeType==9){
_d8=_d8.documentElement;
}
var _d9=_d8.getAttribute(_51);
if(!_d9){
_d9=_b1++;
_d8.setAttribute(_51,_d9);
_50[_d9]={};
}
for(var i=0;i<_d6.length;i++){
var _da=_d6[i];
var _db=_b3(_da);
if(_db==_da){
_da=_db+"$$"+(_d5++);
}
var lls=_50[_d9];
if(!lls[_db]){
lls[_db]={_connectId:_b6(_d8,_db)};
}
lls[_db][_da]={fn:fn,data:_d7};
}
}
});
return this;
};
function _dc(src,_dd){
var _de=_dd.getAttribute(_51);
var sls=_50[_de];
if(!sls){
return;
}
var _df=_df=_b1++;
_dd.setAttribute(_51,_df);
var tls=_50[_df]={};
var _e0={};
for(var _e1 in sls){
var _e2=tls[_e1]={_connectId:_b6(_dd,_e1)};
var _e3=sls[_e1];
for(var _e4 in _e3){
_e2[_e4]={fn:_e3[_e4].fn,data:_e3[_e4].data};
}
}
};
function _e5(lls,_e6,_e7,_e8,fn){
var _e9=lls[_e6];
if(_e9){
var _ea=_e7.indexOf(".")!=-1;
var _eb=false;
if(_e8){
delete _e9[_e8];
}else{
if(!_ea&&!fn){
_eb=true;
}else{
if(_ea){
if(_e7.charAt(0)=="."){
for(var _ec in _e9){
if(_ec.indexOf(_e7)==_ec.length-_e7.length){
delete _e9[_ec];
}
}
}else{
delete _e9[_e7];
}
}else{
for(var _ec in _e9){
if(_ec.indexOf("$$")!=-1&&_e9[_ec].fn==fn){
delete _e9[_ec];
break;
}
}
}
}
}
var _ed=true;
for(var _ec in _e9){
if(_ec!="_connectId"){
_ed=false;
break;
}
}
if(_eb||_ed){
if(_e6.indexOf("ajax")!=-1){
_2.unsubscribe(_e9._connectId);
}else{
_2.disconnect(_e9._connectId);
}
delete lls[_e6];
}
}
};
f.unbind=function(_ee,fn){
var _ef=_ee?_ee[_2._scopeName+"callbackId"]:null;
_ee=_ee&&_ee.type?_ee.type:_ee;
_ee=_ee?_ee.split(" "):_ee;
this.forEach(function(_f0){
if(_f0.nodeType!=3&&_f0.nodeType!=8){
if(_f0.nodeType==9){
_f0=_f0.documentElement;
}
var _f1=_f0.getAttribute(_51);
if(_f1){
var lls=_50[_f1];
if(lls){
var _f2=_ee;
if(!_f2){
_f2=[];
for(var _f3 in lls){
_f2.push(_f3);
}
}
for(var i=0;i<_f2.length;i++){
var _f4=_f2[i];
var _f5=_b3(_f4);
if(_f4.charAt(0)=="."){
for(var _f3 in lls){
_e5(lls,_f3,_f4,_ef,fn);
}
}else{
_e5(lls,_f5,_f4,_ef,fn);
}
}
}
}
}
});
return this;
};
f.one=function(_f6,_f7){
var _f8=function(){
$(this).unbind(_f6,arguments.callee);
return _f7.apply(this,arguments);
};
return this.bind(_f6,_f8);
};
f._cloneNode=function(src){
var _f9=src.cloneNode(true);
if(src.nodeType==1){
var _fa=_2.query("["+_51+"]",_f9);
for(var i=0,_fb;_fb=_fa[i];i++){
var _fc=_2.query("["+_51+"=\""+_fb.getAttribute(_51)+"\"]",src)[0];
if(_fc){
_dc(_fc,_fb);
}
}
}
return _f9;
};
_2.getObject("$.event.global",true);
_2.forEach(["blur","focus","dblclick","click","error","keydown","keypress","keyup","load","mousedown","mouseenter","mouseleave","mousemove","mouseout","mouseover","mouseup","submit","ajaxStart","ajaxSend","ajaxSuccess","ajaxError","ajaxComplete","ajaxStop"],function(evt){
f[evt]=function(_fd){
if(_fd){
this.bind(evt,_fd);
}else{
this.trigger(evt);
}
return this;
};
});
function _fe(_ff){
if(_2.isString(_ff)){
if(_ff=="slow"){
_ff=700;
}else{
if(_ff="fast"){
_ff=300;
}else{
_ff=500;
}
}
}
return _ff;
};
f.hide=function(_100,_101){
_100=_fe(_100);
this.forEach(function(node){
var _102=node.style;
var cs=_2.getComputedStyle(node);
if(cs.display=="none"){
return;
}
_102.overflow="hidden";
_102.display="block";
if(_100){
_2.anim(node,{width:0,height:0,opacity:0},_100,null,function(){
_102.width="";
_102.height="";
_102.display="none";
return _101&&_101.call(node);
});
}else{
_2.style(node,"display","none");
if(_101){
_101.call(node);
}
}
});
return this;
};
f.show=function(_103,_104){
_103=_fe(_103);
this.forEach(function(node){
var _105=node.style;
var cs=_2.getComputedStyle(node);
if(cs.display!="none"){
return;
}
if(_103){
var _106=parseFloat(_105.width);
var _107=parseFloat(_105.height);
if(!_106||!_107){
_105.display="block";
var box=_2.marginBox(node);
_106=box.w;
_107=box.h;
}
_105.width=0;
_105.height=0;
_105.overflow="hidden";
_2.attr(node,"opacity",0);
_105.display="block";
_2.anim(node,{width:_106,height:_107,opacity:1},_103,null,_104?_2.hitch(node,_104):undefined);
}else{
_2.style(node,"display","block");
if(_104){
_104.call(node);
}
}
});
return this;
};
$.ajaxSettings={};
$.ajaxSetup=function(args){
_2.mixin($.ajaxSettings,args);
};
var _b9={"ajaxStart":"/dojo/io/start","ajaxSend":"/dojo/io/send","ajaxSuccess":"/dojo/io/load","ajaxError":"/dojo/io/error","ajaxComplete":"/dojo/io/done","ajaxStop":"/dojo/io/stop"};
for(var _108 in _b9){
if(_108.indexOf("ajax")==0){
(function(_109){
f[_109]=function(_10a){
this.forEach(function(node){
_2.subscribe(_b9[_109],function(){
var _10b=new $.Event(_109);
var _10c=arguments[0]&&arguments[0].ioArgs;
var xhr=_10c&&_10c.xhr;
var args=_10c&&_10c.args;
var res=arguments[1];
if("ajaxComplete|ajaxSend|ajaxSuccess".indexOf(_109)!=-1){
return _10a.call(node,_10b,xhr,args);
}else{
if(_109=="ajaxError"){
return _10a.call(node,_10b,xhr,args,res);
}else{
return _10a.call(node,_10b);
}
}
});
});
return this;
};
})(_108);
}
}
var _10d=_2._xhrObj;
_2._xhrObj=function(args){
var xhr=_10d.apply(_2,arguments);
if(args&&args.beforeSend){
if(args.beforeSend(xhr)===false){
return false;
}
}
return xhr;
};
$.ajax=function(args){
var temp=_2.delegate($.ajaxSettings);
for(var _10e in args){
if(_10e=="data"&&_2.isObject(args[_10e])&&_2.isObject(temp.data)){
for(var prop in args[_10e]){
temp.data[prop]=args[_10e][prop];
}
}else{
temp[_10e]=args[_10e];
}
}
args=temp;
var url=args.url;
if("async" in args){
args.sync=!args.async;
}
if(args.global===false){
args.ioPublish=false;
}
if(args.data){
var data=args.data;
if(_2.isString(data)){
args.content=_2.queryToObject(data);
}else{
for(var _10e in data){
if(_2.isFunction(data[_10e])){
data[_10e]=data[_10e]();
}
}
args.content=data;
}
}
var _10f=args.dataType;
if("dataType" in args){
if(_10f=="script"){
_10f="javascript";
}else{
if(_10f=="html"){
_10f="text";
}
}
args.handleAs=_10f;
}else{
_10f=args.handleAs="text";
args.guessedType=true;
}
if("cache" in args){
args.preventCache=!args.cache;
}else{
if(args.dataType=="script"||args.dataType=="jsonp"){
args.preventCache=true;
}
}
if(args.error){
args._jqueryError=args.error;
delete args.error;
}
args.handle=function(_110,_111){
var _112="success";
if(_110 instanceof Error){
_112=(_110.dojoType=="timeout"?"timeout":"error");
if(args._jqueryError){
args._jqueryError(_111.xhr,_112,_110);
}
}else{
var xml=(_111.args.guessedType&&_111.xhr&&_111.xhr.responseXML);
if(xml){
_110=xml;
}
if(args.success){
args.success(_110,_112,_111.xhr);
}
}
if(args.complete){
args.complete(_110,_112,_111.xhr);
}
return _110;
};
var _113=(_10f=="jsonp");
if(_10f=="javascript"){
var _114=url.indexOf(":");
var _115=url.indexOf("/");
if(_114>0&&_114<_115){
var _116=url.indexOf("/",_115+2);
if(_116==-1){
_116=url.length;
}
if(location.protocol!=url.substring(0,_114+1)||location.hostname!=url.substring(_115+2,_116)){
_113=true;
}
}
}
if(_113){
if(_10f=="jsonp"){
var cb=args.jsonp;
if(!cb){
var _117=args.url.split("?")[1];
if(_117&&(_117=_2.queryToObject(_117))){
cb=_118(_117);
if(cb){
var _119=new RegExp("([&\\?])?"+cb+"=?");
args.url=args.url.replace(_119+"=?");
}
}
if(!cb){
cb=_118(args.content);
if(cb){
delete args.content[cb];
}
}
}
args.jsonp=cb||"callback";
}
var dfd=_2.io.script.get(args);
return dfd;
}else{
var dfd=_2.xhr(args.type||"GET",args);
return dfd.ioArgs.xhr===false?false:dfd.ioArgs.xhr;
}
};
function _118(obj){
for(var prop in obj){
if(prop.indexOf("callback")==prop.length-8){
return prop;
}
}
return null;
};
$.getpost=function(_11a,url,data,_11b,_11c){
var args={url:url,type:_11a};
if(data){
if(_2.isFunction(data)&&!_11b){
args.complete=data;
}else{
args.data=data;
}
}
if(_11b){
if(_2.isString(_11b)&&!_11c){
_11c=_11b;
}else{
args.complete=_11b;
}
}
if(_11c){
args.dataType=_11c;
}
return $.ajax(args);
};
$.get=_2.hitch($,"getpost","GET");
$.post=_2.hitch($,"getpost","POST");
$.getJSON=function(url,data,_11d){
return $.getpost("GET",url,data,_11d,"json");
};
$.getScript=function(url,_11e){
return $.ajax({url:url,success:_11e,dataType:"script"});
};
f.load=function(url,data,_11f){
var node=this[0];
if(!node||!node.nodeType||node.nodeType==9){
_2.addOnLoad(url);
return this;
}
var _120=url.split(/\s+/);
url=_120[0];
var _121=_120[1];
var _122=_11f||data;
var cb=_2.hitch(this,function(_123,_124,xhr){
var _125=_123.match(/\<\s*body[^>]+>.*<\/body\s*>/i);
if(_125){
_123=_125;
}
var _126=_2._toDom(_123);
if(_121){
var temp=$(_2.create("div"));
temp.append(_126);
_126=temp.find(_121);
}else{
_126=$(_126.nodeType==11?_126.childNodes:_126);
}
this.html(_126);
if(_122){
setTimeout(_2.hitch(this,function(){
this.forEach(function(node){
_122.call(node,_123,_124,xhr);
});
}),10);
}
});
if(!_11f){
data=cb;
}else{
_11f=cb;
}
var _127="GET";
if(data&&_2.isObject(data)){
_127="POST";
}
$.getpost(_127,url,data,_11f,"html");
return this;
};
var _128="file|submit|image|reset|button|";
f.serialize=function(){
var ret="";
var strs=this.map(function(node){
if(node.nodeName.toUpperCase()=="FORM"){
return _2.formToQuery(node);
}else{
var type=(node.type||"").toLowerCase();
if(_128.indexOf(type)==-1){
var val=_2.fieldToObject(node);
if(node.name&&val!=null){
var q={};
q[node.name]=val;
return _2.objectToQuery(q);
}
}
}
});
return ret+strs.join("&");
};
$.param=function(obj){
if(obj._is$&&obj.serialize){
return obj.serialize();
}else{
if(_2.isArray(obj)){
return _2.map(obj,function(item){
return $.param(item);
}).join("&");
}else{
return _2.objectToQuery(obj);
}
}
};
$.isFunction=function(){
var _129=_2.isFunction.apply(_2,arguments);
if(_129){
_129=(typeof (arguments[0])!="object");
}
return _129;
};
})();
});
