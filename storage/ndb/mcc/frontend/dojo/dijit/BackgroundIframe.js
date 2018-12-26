//>>built
define("dijit/BackgroundIframe",["require",".","dojo/_base/config","dojo/dom-construct","dojo/dom-style","dojo/_base/lang","dojo/on","dojo/_base/sniff","dojo/_base/window"],function(_1,_2,_3,_4,_5,_6,on,_7,_8){
var _9=new function(){
var _a=[];
this.pop=function(){
var _b;
if(_a.length){
_b=_a.pop();
_b.style.display="";
}else{
if(_7("ie")<9){
var _c=_3["dojoBlankHtmlUrl"]||_1.toUrl("dojo/resources/blank.html")||"javascript:\"\"";
var _d="<iframe src='"+_c+"' role='presentation'"+" style='position: absolute; left: 0px; top: 0px;"+"z-index: -1; filter:Alpha(Opacity=\"0\");'>";
_b=_8.doc.createElement(_d);
}else{
_b=_4.create("iframe");
_b.src="javascript:\"\"";
_b.className="dijitBackgroundIframe";
_b.setAttribute("role","presentation");
_5.set(_b,"opacity",0.1);
}
_b.tabIndex=-1;
}
return _b;
};
this.push=function(_e){
_e.style.display="none";
_a.push(_e);
};
}();
_2.BackgroundIframe=function(_f){
if(!_f.id){
throw new Error("no id");
}
if(_7("ie")||_7("mozilla")){
var _10=(this.iframe=_9.pop());
_f.appendChild(_10);
if(_7("ie")<7||_7("quirks")){
this.resize(_f);
this._conn=on(_f,"resize",_6.hitch(this,function(){
this.resize(_f);
}));
}else{
_5.set(_10,{width:"100%",height:"100%"});
}
}
};
_6.extend(_2.BackgroundIframe,{resize:function(_11){
if(this.iframe){
_5.set(this.iframe,{width:_11.offsetWidth+"px",height:_11.offsetHeight+"px"});
}
},destroy:function(){
if(this._conn){
this._conn.remove();
this._conn=null;
}
if(this.iframe){
_9.push(this.iframe);
delete this.iframe;
}
}});
return _2.BackgroundIframe;
});
