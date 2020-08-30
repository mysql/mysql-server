//>>built
define("dojox/grid/enhanced/plugins/Printer",["dojo/_base/declare","dojo/_base/html","dojo/_base/Deferred","dojo/_base/lang","dojo/_base/sniff","dojo/_base/xhr","dojo/_base/array","dojo/query","dojo/DeferredList","../_Plugin","../../EnhancedGrid","./exporter/TableWriter"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_1("dojox.grid.enhanced.plugins.Printer",_a,{name:"printer",constructor:function(_e){
this.grid=_e;
this._mixinGrid(_e);
_e.setExportFormatter(function(_f,_10,_11,_12){
return _10.format(_11,_12);
});
},_mixinGrid:function(){
var g=this.grid;
g.printGrid=_4.hitch(this,this.printGrid);
g.printSelected=_4.hitch(this,this.printSelected);
g.exportToHTML=_4.hitch(this,this.exportToHTML);
g.exportSelectedToHTML=_4.hitch(this,this.exportSelectedToHTML);
g.normalizePrintedGrid=_4.hitch(this,this.normalizeRowHeight);
},printGrid:function(_13){
this.exportToHTML(_13,_4.hitch(this,this._print));
},printSelected:function(_14){
this.exportSelectedToHTML(_14,_4.hitch(this,this._print));
},exportToHTML:function(_15,_16){
_15=this._formalizeArgs(_15);
var _17=this;
this.grid.exportGrid("table",_15,function(str){
_17._wrapHTML(_15.title,_15.cssFiles,_15.titleInBody+str).then(_16);
});
},exportSelectedToHTML:function(_18,_19){
_18=this._formalizeArgs(_18);
var _1a=this;
this.grid.exportSelected("table",_18.writerArgs,function(str){
_1a._wrapHTML(_18.title,_18.cssFiles,_18.titleInBody+str).then(_19);
});
},_loadCSSFiles:function(_1b){
var dl=_7.map(_1b,function(_1c){
_1c=_4.trim(_1c);
if(_1c.substring(_1c.length-4).toLowerCase()===".css"){
return _6.get({url:_1c});
}else{
var d=new _3();
d.callback(_1c);
return d;
}
});
return _9.prototype.gatherResults(dl);
},_print:function(_1d){
var win,_1e=this,_1f=function(w){
var doc=w.document;
doc.open();
doc.write(_1d);
doc.close();
_1e.normalizeRowHeight(doc);
};
if(!window.print){
return;
}else{
if(_5("chrome")||_5("opera")){
win=window.open("javascript: ''","","status=0,menubar=0,location=0,toolbar=0,width=1,height=1,resizable=0,scrollbars=0");
_1f(win);
win.print();
win.close();
}else{
var fn=this._printFrame,dn=this.grid.domNode;
if(!fn){
var _20=dn.id+"_print_frame";
if(!(fn=_2.byId(_20))){
fn=_2.create("iframe");
fn.id=_20;
fn.frameBorder=0;
_2.style(fn,{width:"1px",height:"1px",position:"absolute",right:0,bottom:0,border:"none",overflow:"hidden"});
if(!_5("ie")){
_2.style(fn,"visibility","hidden");
}
dn.appendChild(fn);
}
this._printFrame=fn;
}
win=fn.contentWindow;
_1f(win);
win.focus();
win.print();
}
}
},_wrapHTML:function(_21,_22,_23){
return this._loadCSSFiles(_22).then(function(_24){
var i,sb=["<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">","<html ",_2._isBodyLtr()?"":"dir=\"rtl\"","><head><title>",_21,"</title><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"></meta>"];
for(i=0;i<_24.length;++i){
sb.push("<style type=\"text/css\">",_24[i],"</style>");
}
sb.push("</head>");
if(_23.search(/^\s*<body/i)<0){
_23="<body>"+_23+"</body>";
}
sb.push(_23,"</html>");
return sb.join("");
});
},normalizeRowHeight:function(doc){
var _25=_8(".grid_view",doc.body);
var _26=_7.map(_25,function(_27){
return _8(".grid_header",_27)[0];
});
var _28=_7.map(_25,function(_29){
return _8(".grid_row",_29);
});
var _2a=_28[0].length;
var i,v,h,_2b=0;
for(v=_25.length-1;v>=0;--v){
h=_2.contentBox(_26[v]).h;
if(h>_2b){
_2b=h;
}
}
for(v=_25.length-1;v>=0;--v){
_2.style(_26[v],"height",_2b+"px");
}
for(i=0;i<_2a;++i){
_2b=0;
for(v=_25.length-1;v>=0;--v){
h=_2.contentBox(_28[v][i]).h;
if(h>_2b){
_2b=h;
}
}
for(v=_25.length-1;v>=0;--v){
_2.style(_28[v][i],"height",_2b+"px");
}
}
var _2c=0,ltr=_2._isBodyLtr();
for(v=0;v<_25.length;++v){
_2.style(_25[v],ltr?"left":"right",_2c+"px");
_2c+=_2.marginBox(_25[v]).w;
}
},_formalizeArgs:function(_2d){
_2d=(_2d&&_4.isObject(_2d))?_2d:{};
_2d.title=String(_2d.title)||"";
if(!_4.isArray(_2d.cssFiles)){
_2d.cssFiles=[_2d.cssFiles];
}
_2d.titleInBody=_2d.title?["<h1>",_2d.title,"</h1>"].join(""):"";
return _2d;
}});
_b.registerPlugin(_d,{"dependency":["exporter"]});
return _d;
});
