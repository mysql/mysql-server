//>>built
define("dojox/editor/plugins/PageBreak",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/PageBreak"],function(_1,_2,_3,_4){
var _5=_1.declare("dojox.editor.plugins.PageBreak",_4,{useDefaultCommand:false,iconClassPrefix:"dijitAdditionalEditorIcon",_unbreakableNodes:["li","ul","ol"],_pbContent:"<hr style='page-break-after: always;' class='dijitEditorPageBreak'>",_initButton:function(){
var ed=this.editor;
var _6=_1.i18n.getLocalization("dojox.editor.plugins","PageBreak");
this.button=new _2.form.Button({label:_6["pageBreak"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"PageBreak",tabIndex:"-1",onClick:_1.hitch(this,"_insertPageBreak")});
ed.onLoadDeferred.addCallback(_1.hitch(this,function(){
ed.addKeyHandler(_1.keys.ENTER,true,true,_1.hitch(this,this._insertPageBreak));
if(_1.isWebKit||_1.isOpera){
this.connect(this.editor,"onKeyDown",_1.hitch(this,function(e){
if((e.keyCode===_1.keys.ENTER)&&e.ctrlKey&&e.shiftKey){
this._insertPageBreak();
}
}));
}
}));
},updateState:function(){
this.button.set("disabled",this.get("disabled"));
},setEditor:function(_7){
this.editor=_7;
this._initButton();
},_style:function(){
if(!this._styled){
this._styled=true;
var _8=this.editor.document;
var _9=".dijitEditorPageBreak {\n"+"\tborder-top-style: solid;\n"+"\tborder-top-width: 3px;\n"+"\tborder-top-color: #585858;\n"+"\tborder-bottom-style: solid;\n"+"\tborder-bottom-width: 1px;\n"+"\tborder-bottom-color: #585858;\n"+"\tborder-left-style: solid;\n"+"\tborder-left-width: 1px;\n"+"\tborder-left-color: #585858;\n"+"\tborder-right-style: solid;\n"+"\tborder-right-width: 1px;\n"+"\tborder-right-color: #585858;\n"+"\tcolor: #A4A4A4;\n"+"\tbackground-color: #A4A4A4;\n"+"\theight: 10px;\n"+"\tpage-break-after: always;\n"+"\tpadding: 0px 0px 0px 0px;\n"+"}\n\n"+"@media print {\n"+"\t.dijitEditorPageBreak { page-break-after: always; "+"background-color: rgba(0,0,0,0); color: rgba(0,0,0,0); "+"border: 0px none rgba(0,0,0,0); display: hidden; "+"width: 0px; height: 0px;}\n"+"}";
if(!_1.isIE){
var _a=_8.createElement("style");
_a.appendChild(_8.createTextNode(_9));
_8.getElementsByTagName("head")[0].appendChild(_a);
}else{
var ss=_8.createStyleSheet("");
ss.cssText=_9;
}
}
},_insertPageBreak:function(){
try{
if(!this._styled){
this._style();
}
if(this._allowBreak()){
this.editor.execCommand("inserthtml",this._pbContent);
}
}
catch(e){
console.warn(e);
}
},_allowBreak:function(){
var ed=this.editor;
var _b=ed.document;
var _c=ed._sCall("getSelectedElement",[])||ed._sCall("getParentElement",[]);
while(_c&&_c!==_b.body&&_c!==_b.html){
if(ed._sCall("isTag",[_c,this._unbreakableNodes])){
return false;
}
_c=_c.parentNode;
}
return true;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _d=o.args.name.toLowerCase();
if(_d==="pagebreak"){
o.plugin=new _5({});
}
});
return _5;
});
