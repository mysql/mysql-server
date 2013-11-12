//>>built
define("dojox/grid/enhanced/plugins/Cookie",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/sniff","dojo/_base/html","dojo/_base/json","dojo/_base/window","dojo/_base/unload","dojo/cookie","../_Plugin","../../_RowSelector","../../EnhancedGrid","../../cells/_base"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_3.getObject("dojox.grid.cells");
var _e=function(_f){
return window.location+"/"+_f.id;
};
var _10=function(_11){
var _12=[];
if(!_3.isArray(_11)){
_11=[_11];
}
_2.forEach(_11,function(_13){
if(_3.isArray(_13)){
_13={"cells":_13};
}
var _14=_13.rows||_13.cells;
if(_3.isArray(_14)){
if(!_3.isArray(_14[0])){
_14=[_14];
}
_2.forEach(_14,function(row){
if(_3.isArray(row)){
_2.forEach(row,function(_15){
_12.push(_15);
});
}
});
}
});
return _12;
};
var _16=function(_17,_18){
if(_3.isArray(_17)){
var _19=_18._setStructureAttr;
_18._setStructureAttr=function(_1a){
if(!_18._colWidthLoaded){
_18._colWidthLoaded=true;
var _1b=_10(_1a);
for(var i=_1b.length-1;i>=0;--i){
if(typeof _17[i]=="number"){
_1b[i].width=_17[i]+"px";
}else{
if(_17[i]=="hidden"){
_1b[i].hidden=true;
}
}
}
}
_19.call(_18,_1a);
_18._setStructureAttr=_19;
};
}
};
var _1c=function(_1d){
return _2.map(_2.filter(_1d.layout.cells,function(_1e){
return !(_1e.isRowSelector||_1e instanceof _d.RowIndex);
}),function(_1f){
return _1f.hidden?"hidden":_5[_4("webkit")?"marginBox":"contentBox"](_1f.getHeaderNode()).w;
});
};
var _20=function(_21,_22){
if(_21&&_2.every(_21,function(_23){
return _3.isArray(_23)&&_2.every(_23,function(_24){
return _3.isArray(_24)&&_24.length>0;
});
})){
var _25=_22._setStructureAttr;
var _26=function(def){
return ("name" in def||"field" in def||"get" in def);
};
var _27=function(def){
return (def!==null&&_3.isObject(def)&&("cells" in def||"rows" in def||("type" in def&&!_26(def))));
};
_22._setStructureAttr=function(_28){
if(!_22._colOrderLoaded){
_22._colOrderLoaded=true;
_22._setStructureAttr=_25;
_28=_3.clone(_28);
if(_3.isArray(_28)&&!_2.some(_28,_27)){
_28=[{cells:_28}];
}else{
if(_27(_28)){
_28=[_28];
}
}
var _29=_10(_28);
_2.forEach(_3.isArray(_28)?_28:[_28],function(_2a,_2b){
var _2c=_2a;
if(_3.isArray(_2a)){
_2a.splice(0,_2a.length);
}else{
delete _2a.rows;
_2c=_2a.cells=[];
}
_2.forEach(_21[_2b],function(_2d){
_2.forEach(_2d,function(_2e){
var i,_2f;
for(i=0;i<_29.length;++i){
_2f=_29[i];
if(_6.toJson({"name":_2f.name,"field":_2f.field})==_6.toJson(_2e)){
break;
}
}
if(i<_29.length){
_2c.push(_2f);
}
});
});
});
}
_25.call(_22,_28);
};
}
};
var _30=function(_31){
var _32=_2.map(_2.filter(_31.views.views,function(_33){
return !(_33 instanceof _b);
}),function(_34){
return _2.map(_34.structure.cells,function(_35){
return _2.map(_2.filter(_35,function(_36){
return !(_36.isRowSelector||_36 instanceof _d.RowIndex);
}),function(_37){
return {"name":_37.name,"field":_37.field};
});
});
});
return _32;
};
var _38=function(_39,_3a){
try{
if(_3.isObject(_39)){
_3a.setSortIndex(_39.idx,_39.asc);
}
}
catch(e){
}
};
var _3b=function(_3c){
return {idx:_3c.getSortIndex(),asc:_3c.getSortAsc()};
};
if(!_4("ie")){
_8.addOnWindowUnload(function(){
_2.forEach(dijit.findWidgets(_7.body()),function(_3d){
if(_3d instanceof _c&&!_3d._destroyed){
_3d.destroyRecursive();
}
});
});
}
var _3e=_1("dojox.grid.enhanced.plugins.Cookie",_a,{name:"cookie",_cookieEnabled:true,constructor:function(_3f,_40){
this.grid=_3f;
_40=(_40&&_3.isObject(_40))?_40:{};
this.cookieProps=_40.cookieProps;
this._cookieHandlers=[];
this._mixinGrid();
this.addCookieHandler({name:"columnWidth",onLoad:_16,onSave:_1c});
this.addCookieHandler({name:"columnOrder",onLoad:_20,onSave:_30});
this.addCookieHandler({name:"sortOrder",onLoad:_38,onSave:_3b});
_2.forEach(this._cookieHandlers,function(_41){
if(_40[_41.name]===false){
_41.enable=false;
}
},this);
},destroy:function(){
this._saveCookie();
this._cookieHandlers=null;
this.inherited(arguments);
},_mixinGrid:function(){
var g=this.grid;
g.addCookieHandler=_3.hitch(this,"addCookieHandler");
g.removeCookie=_3.hitch(this,"removeCookie");
g.setCookieEnabled=_3.hitch(this,"setCookieEnabled");
g.getCookieEnabled=_3.hitch(this,"getCookieEnabled");
},_saveCookie:function(){
if(this.getCookieEnabled()){
var ck={},chs=this._cookieHandlers,_42=this.cookieProps,_43=_e(this.grid);
for(var i=chs.length-1;i>=0;--i){
if(chs[i].enabled){
ck[chs[i].name]=chs[i].onSave(this.grid);
}
}
_42=_3.isObject(this.cookieProps)?this.cookieProps:{};
_9(_43,_6.toJson(ck),_42);
}else{
this.removeCookie();
}
},onPreInit:function(){
var _44=this.grid,chs=this._cookieHandlers,_45=_e(_44),ck=_9(_45);
if(ck){
ck=_6.fromJson(ck);
for(var i=0;i<chs.length;++i){
if(chs[i].name in ck&&chs[i].enabled){
chs[i].onLoad(ck[chs[i].name],_44);
}
}
}
this._cookie=ck||{};
this._cookieStartedup=true;
},addCookieHandler:function(_46){
if(_46.name){
var _47=function(){
};
_46.onLoad=_46.onLoad||_47;
_46.onSave=_46.onSave||_47;
if(!("enabled" in _46)){
_46.enabled=true;
}
for(var i=this._cookieHandlers.length-1;i>=0;--i){
if(this._cookieHandlers[i].name==_46.name){
this._cookieHandlers.splice(i,1);
}
}
this._cookieHandlers.push(_46);
if(this._cookieStartedup&&_46.name in this._cookie){
_46.onLoad(this._cookie[_46.name],this.grid);
}
}
},removeCookie:function(){
var key=_e(this.grid);
_9(key,null,{expires:-1});
},setCookieEnabled:function(_48,_49){
if(typeof _48=="string"){
var chs=this._cookieHandlers;
for(var i=chs.length-1;i>=0;--i){
if(chs[i].name===_48){
chs[i].enabled=!!_49;
}
}
}else{
this._cookieEnabled=!!_48;
if(!this._cookieEnabled){
this.removeCookie();
}
}
},getCookieEnabled:function(_4a){
if(_3.isString(_4a)){
var chs=this._cookieHandlers;
for(var i=chs.length-1;i>=0;--i){
if(chs[i].name==_4a){
return chs[i].enabled;
}
}
return false;
}
return this._cookieEnabled;
}});
_c.registerPlugin(_3e,{"preInit":true});
return _3e;
});
