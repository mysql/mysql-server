//>>built
define("dojox/dtl/tag/loader",["dojo/_base/lang","../_base","dojo/_base/array","dojo/_base/connect"],function(_1,dd,_2,_3){
_1.getObject("dojox.dtl.tag.loader",true);
var _4=dd.tag.loader;
_4.BlockNode=_1.extend(function(_5,_6){
this.name=_5;
this.nodelist=_6;
},{"super":function(){
if(this.parent){
var _7=this.parent.nodelist.dummyRender(this.context,null,true);
if(typeof _7=="string"){
_7=new String(_7);
}
_7.safe=true;
return _7;
}
return "";
},render:function(_8,_9){
var _a=this.name;
var _b=this.nodelist;
var _c;
if(_9.blocks){
var _d=_9.blocks[_a];
if(_d){
_c=_d.parent;
_b=_d.nodelist;
_d.used=true;
}
}
this.rendered=_b;
_8=_8.push();
this.context=_8;
this.parent=null;
if(_b!=this.nodelist){
this.parent=this;
}
_8.block=this;
if(_9.getParent){
var _e=_9.getParent();
var _f=_3.connect(_9,"onSetParent",function(_10,up,_11){
if(up&&_11){
_9.setParent(_e);
}
});
}
_9=_b.render(_8,_9,this);
_f&&_3.disconnect(_f);
_8=_8.pop();
return _9;
},unrender:function(_12,_13){
return this.rendered.unrender(_12,_13);
},clone:function(_14){
return new this.constructor(this.name,this.nodelist.clone(_14));
},toString:function(){
return "dojox.dtl.tag.loader.BlockNode";
}});
_4.ExtendsNode=_1.extend(function(_15,_16,_17,_18,key){
this.getTemplate=_15;
this.nodelist=_16;
this.shared=_17;
this.parent=_18;
this.key=key;
},{parents:{},getParent:function(_19){
var _1a=this.parent;
if(!_1a){
var _1b;
_1a=this.parent=_19.get(this.key,false);
if(!_1a){
throw new Error("extends tag used a variable that did not resolve");
}
if(typeof _1a=="object"){
var url=_1a.url||_1a.templatePath;
if(_1a.shared){
this.shared=true;
}
if(url){
_1a=this.parent=url.toString();
}else{
if(_1a.templateString){
_1b=_1a.templateString;
_1a=this.parent=" ";
}else{
_1a=this.parent=this.parent.toString();
}
}
}
if(_1a&&_1a.indexOf("shared:")===0){
this.shared=true;
_1a=this.parent=_1a.substring(7,_1a.length);
}
}
if(!_1a){
throw new Error("Invalid template name in 'extends' tag.");
}
if(_1a.render){
return _1a;
}
if(this.parents[_1a]){
return this.parents[_1a];
}
this.parent=this.getTemplate(_1b||dojox.dtl.text.getTemplateString(_1a));
if(this.shared){
this.parents[_1a]=this.parent;
}
return this.parent;
},render:function(_1c,_1d){
var _1e=this.getParent(_1c);
_1e.blocks=_1e.blocks||{};
_1d.blocks=_1d.blocks||{};
for(var i=0,_1f;_1f=this.nodelist.contents[i];i++){
if(_1f instanceof dojox.dtl.tag.loader.BlockNode){
var old=_1e.blocks[_1f.name];
if(old&&old.nodelist!=_1f.nodelist){
_1d=old.nodelist.unrender(_1c,_1d);
}
_1e.blocks[_1f.name]=_1d.blocks[_1f.name]={shared:this.shared,nodelist:_1f.nodelist,used:false};
}
}
this.rendered=_1e;
return _1e.nodelist.render(_1c,_1d,this);
},unrender:function(_20,_21){
return this.rendered.unrender(_20,_21,this);
},toString:function(){
return "dojox.dtl.block.ExtendsNode";
}});
_4.IncludeNode=_1.extend(function(_22,_23,_24,_25,_26){
this._path=_22;
this.constant=_23;
this.path=(_23)?_22:new dd._Filter(_22);
this.getTemplate=_24;
this.text=_25;
this.parsed=(arguments.length==5)?_26:true;
},{_cache:[{},{}],render:function(_27,_28){
var _29=((this.constant)?this.path:this.path.resolve(_27)).toString();
var _2a=Number(this.parsed);
var _2b=false;
if(_29!=this.last){
_2b=true;
if(this.last){
_28=this.unrender(_27,_28);
}
this.last=_29;
}
var _2c=this._cache[_2a];
if(_2a){
if(!_2c[_29]){
_2c[_29]=dd.text._resolveTemplateArg(_29,true);
}
if(_2b){
var _2d=this.getTemplate(_2c[_29]);
this.rendered=_2d.nodelist;
}
return this.rendered.render(_27,_28,this);
}else{
if(this.text instanceof dd._TextNode){
if(_2b){
this.rendered=this.text;
this.rendered.set(dd.text._resolveTemplateArg(_29,true));
}
return this.rendered.render(_27,_28);
}else{
if(!_2c[_29]){
var _2e=[];
var div=document.createElement("div");
div.innerHTML=dd.text._resolveTemplateArg(_29,true);
var _2f=div.childNodes;
while(_2f.length){
var _30=div.removeChild(_2f[0]);
_2e.push(_30);
}
_2c[_29]=_2e;
}
if(_2b){
this.nodelist=[];
var _31=true;
for(var i=0,_32;_32=_2c[_29][i];i++){
this.nodelist.push(_32.cloneNode(true));
}
}
for(var i=0,_33;_33=this.nodelist[i];i++){
_28=_28.concat(_33);
}
}
}
return _28;
},unrender:function(_34,_35){
if(this.rendered){
_35=this.rendered.unrender(_34,_35);
}
if(this.nodelist){
for(var i=0,_36;_36=this.nodelist[i];i++){
_35=_35.remove(_36);
}
}
return _35;
},clone:function(_37){
return new this.constructor(this._path,this.constant,this.getTemplate,this.text.clone(_37),this.parsed);
}});
_1.mixin(_4,{block:function(_38,_39){
var _3a=_39.contents.split();
var _3b=_3a[1];
_38._blocks=_38._blocks||{};
_38._blocks[_3b]=_38._blocks[_3b]||[];
_38._blocks[_3b].push(_3b);
var _3c=_38.parse(["endblock","endblock "+_3b]).rtrim();
_38.next_token();
return new dojox.dtl.tag.loader.BlockNode(_3b,_3c);
},extends_:function(_3d,_3e){
var _3f=_3e.contents.split();
var _40=false;
var _41=null;
var key=null;
if(_3f[1].charAt(0)=="\""||_3f[1].charAt(0)=="'"){
_41=_3f[1].substring(1,_3f[1].length-1);
}else{
key=_3f[1];
}
if(_41&&_41.indexOf("shared:")==0){
_40=true;
_41=_41.substring(7,_41.length);
}
var _42=_3d.parse();
return new dojox.dtl.tag.loader.ExtendsNode(_3d.getTemplate,_42,_40,_41,key);
},include:function(_43,_44){
var _45=_44.contents.split();
if(_45.length!=2){
throw new Error(_45[0]+" tag takes one argument: the name of the template to be included");
}
var _46=_45[1];
var _47=false;
if((_46.charAt(0)=="\""||_46.slice(-1)=="'")&&_46.charAt(0)==_46.slice(-1)){
_46=_46.slice(1,-1);
_47=true;
}
return new _4.IncludeNode(_46,_47,_43.getTemplate,_43.create_text_node());
},ssi:function(_48,_49){
var _4a=_49.contents.split();
var _4b=false;
if(_4a.length==3){
_4b=(_4a.pop()=="parsed");
if(!_4b){
throw new Error("Second (optional) argument to ssi tag must be 'parsed'");
}
}
var _4c=_4.include(_48,new dd.Token(_49.token_type,_4a.join(" ")));
_4c.parsed=_4b;
return _4c;
}});
return dojox.dtl.tag.loader;
});
