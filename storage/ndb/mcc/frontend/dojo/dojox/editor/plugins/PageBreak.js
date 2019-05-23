//>>built
define("dojox/editor/plugins/PageBreak",["dojo","dijit","dojox","dijit/_editor/_Plugin","dijit/form/Button","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/PageBreak"],function(_1,_2,_3,_4){
_1.declare("dojox.editor.plugins.PageBreak",_4,{useDefaultCommand:false,iconClassPrefix:"dijitAdditionalEditorIcon",_unbreakableNodes:["li","ul","ol"],_pbContent:"<hr style='page-break-after: always;' class='dijitEditorPageBreak'>",_initButton:function(){
var ed=this.editor;
var _5=_1.i18n.getLocalization("dojox.editor.plugins","PageBreak");
this.button=new _2.form.Button({label:_5["pageBreak"],showLabel:false,iconClass:this.iconClassPrefix+" "+this.iconClassPrefix+"PageBreak",tabIndex:"-1",onClick:_1.hitch(this,"_insertPageBreak")});
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
},setEditor:function(_6){
this.editor=_6;
this._initButton();
},_style:function(){
if(!this._styled){
this._styled=true;
var _7=this.editor.document;
var _8=".dijitEditorPageBreak {\n"+"\tborder-top-style: solid;\n"+"\tborder-top-width: 3px;\n"+"\tborder-top-color: #585858;\n"+"\tborder-bottom-style: solid;\n"+"\tborder-bottom-width: 1px;\n"+"\tborder-bottom-color: #585858;\n"+"\tborder-left-style: solid;\n"+"\tborder-left-width: 1px;\n"+"\tborder-left-color: #585858;\n"+"\tborder-right-style: solid;\n"+"\tborder-right-width: 1px;\n"+"\tborder-right-color: #585858;\n"+"\tcolor: #A4A4A4;\n"+"\tbackground-color: #A4A4A4;\n"+"\theight: 10px;\n"+"\tpage-break-after: always;\n"+"\tpadding: 0px 0px 0px 0px;\n"+"}\n\n"+"@media print {\n"+"\t.dijitEditorPageBreak { page-break-after: always; "+"background-color: rgba(0,0,0,0); color: rgba(0,0,0,0); "+"border: 0px none rgba(0,0,0,0); display: hidden; "+"width: 0px; height: 0px;}\n"+"}";
if(!_1.isIE){
var _9=_7.createElement("style");
_9.appendChild(_7.createTextNode(_8));
_7.getElementsByTagName("head")[0].appendChild(_9);
}else{
var ss=_7.createStyleSheet("");
ss.cssText=_8;
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
var _a=ed.document;
var _b=ed._sCall("getSelectedElement",[])||ed._sCall("getParentElement",[]);
while(_b&&_b!==_a.body&&_b!==_a.html){
if(ed._sCall("isTag",[_b,this._unbreakableNodes])){
return false;
}
_b=_b.parentNode;
}
return true;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
var _c=o.args.name.toLowerCase();
if(_c==="pagebreak"){
o.plugin=new _3.editor.plugins.PageBreak({});
}
});
return _3.editor.plugins.PageBreak;
});
