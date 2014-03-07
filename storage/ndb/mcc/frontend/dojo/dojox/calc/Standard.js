//>>built
require({cache:{"url:dojox/calc/templates/Standard.html":"<div class=\"dijitReset dijitInline dojoxCalc\"\n><table class=\"dijitReset dijitInline dojoxCalcLayout\" data-dojo-attach-point=\"calcTable\" rules=\"none\" cellspacing=0 cellpadding=0 border=0>\n\t<tr\n\t\t><td colspan=\"4\" class=\"dojoxCalcInputContainer\"\n\t\t\t><input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-event=\"onBlur:onBlur,onKeyPress:onKeyPress\" data-dojo-attach-point='textboxWidget'\n\t\t/></td\n\t></tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"seven\" label=\"7\" value='7' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"eight\" label=\"8\" value='8' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"nine\" label=\"9\" value='9' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"divide\" label=\"/\" value='/' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"four\" label=\"4\" value='4' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"five\" label=\"5\" value='5' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"six\" label=\"6\" value='6' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"multiply\" label=\"*\" value='*' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"one\" label=\"1\" value='1' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"two\" label=\"2\" value='2' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"three\" label=\"3\" value='3' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"add\" label=\"+\" value='+' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"decimal\" label=\".\" value='.' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"zero\" label=\"0\" value='0' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"equals\" label=\"x=y\" value='=' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcMinusButtonContainer\">\n\t\t\t<span data-dojo-type=\"dijit.form.ComboButton\" data-dojo-attach-point=\"subtract\" label='-' value='-' data-dojo-attach-event='onClick:insertOperator'>\n\n\t\t\t\t<div data-dojo-type=\"dijit.Menu\" style=\"display:none;\">\n\t\t\t\t\t<div data-dojo-type=\"dijit.MenuItem\" data-dojo-attach-event=\"onClick:insertMinus\">\n\t\t\t\t\t\t(-)\n\t\t\t\t\t</div>\n\t\t\t\t</div>\n\t\t\t</span>\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"clear\" label=\"Clear\" data-dojo-attach-event='onClick:clearText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"sqrt\" label=\"&#x221A;\" value=\"&#x221A;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"power\" label=\"^\" value=\"^\" data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"comma\" label=\",\" value=',' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"AnsButton\" label=\"Ans\" value=\"Ans\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"LeftParenButton\" label=\"(\" value=\"(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"RightParenButton\" label=\")\" value=\")\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"enter\" label=\"Enter\" data-dojo-attach-event='onClick:parseTextbox' />\n\t\t</td>\n\t</tr>\n</table>\n<span data-dojo-attach-point=\"executor\" data-dojo-type=\"dojox.calc._Executor\" data-dojo-attach-event=\"onLoad:executorLoaded\"></span>\n</div>\n"}});
define("dojox/calc/Standard",["dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/_base/event","dojo/dom-style","dojo/ready","dojo/keys","dijit/registry","dijit/typematic","dijit/_WidgetBase","dijit/_WidgetsInTemplateMixin","dijit/_TemplatedMixin","dijit/form/_TextBoxMixin","dojox/math/_base","dijit/TooltipDialog","dojo/text!./templates/Standard.html","dojox/calc/_Executor","dijit/Menu","dijit/MenuItem","dijit/form/ComboButton","dijit/form/Button","dijit/form/TextBox"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
return _1("dojox.calc.Standard",[_b,_d,_c],{templateString:_11,readStore:null,writeStore:null,functions:[],executorLoaded:function(){
_7(_2.hitch(this,function(){
this.loadStore(this.readStore,true);
this.loadStore(this.writeStore);
}));
},saveFunction:function(_13,_14,_15){
this.functions[_13]=this.executor.normalizedFunction(_13,_14,_15);
this.functions[_13].args=_14;
this.functions[_13].body=_15;
},loadStore:function(_16,_17){
if(!_16){
return;
}
_16.query({}).forEach(_2.hitch(this,function(_18){
_2.hitch(this,_17?this.executor.normalizedFunction:this.saveFunction)(_18.name,_18.args,_18.body);
}));
},parseTextbox:function(){
var _19=this.textboxWidget.textbox.value;
if(_19==""&&this.commandList.length>0){
this.setTextboxValue(this.textboxWidget,this.commandList[this.commandList.length-1]);
_19=this.textboxWidget.textbox.value;
}
if(_19!=""){
var ans=this.executor.eval(_19);
if((typeof ans=="number"&&isNaN(ans))){
if(this.commandList.length==0||this.commandList[this.commandList.length-1]!=_19){
this.commandList.push(_19);
}
this.print(_19,false);
this.print("Not a Number",true);
}else{
if(((typeof ans=="object"&&"length" in ans)||typeof ans!="object")&&typeof ans!="function"&&ans!=null){
this.executor.eval("Ans="+ans);
if(this.commandList.length==0||this.commandList[this.commandList.length-1]!=_19){
this.commandList.push(_19);
}
this.print(_19,false);
this.print(ans,true);
}
}
this.commandIndex=this.commandList.length-1;
if(this.hasDisplay){
this.displayBox.scrollTop=this.displayBox.scrollHeight;
}
_e.selectInputText(this.textboxWidget.textbox);
}else{
this.textboxWidget.focus();
}
},cycleCommands:function(_1a,_1b,_1c){
if(_1a==-1||this.commandList.length==0){
return;
}
var _1d=_1c.charOrCode;
if(_1d==_8.UP_ARROW){
this.cycleCommandUp();
}else{
if(_1d==_8.DOWN_ARROW){
this.cycleCommandDown();
}
}
},cycleCommandUp:function(){
if(this.commandIndex-1<0){
this.commandIndex=0;
}else{
this.commandIndex--;
}
this.setTextboxValue(this.textboxWidget,this.commandList[this.commandIndex]);
},cycleCommandDown:function(){
if(this.commandIndex+1>=this.commandList.length){
this.commandIndex=this.commandList.length;
this.setTextboxValue(this.textboxWidget,"");
}else{
this.commandIndex++;
this.setTextboxValue(this.textboxWidget,this.commandList[this.commandIndex]);
}
},onBlur:function(){
if(_3("ie")){
var tr=_4.doc.selection.createRange().duplicate();
var _1e=tr.text||"";
var ntr=this.textboxWidget.textbox.createTextRange();
tr.move("character",0);
ntr.move("character",0);
try{
ntr.setEndPoint("EndToEnd",tr);
this.textboxWidget.textbox.selectionEnd=(this.textboxWidget.textbox.selectionStart=String(ntr.text).replace(/\r/g,"").length)+_1e.length;
}
catch(e){
}
}
},onKeyPress:function(e){
if(e.charOrCode==_8.ENTER){
this.parseTextbox();
_5.stop(e);
}else{
if(e.charOrCode=="!"||e.charOrCode=="^"||e.charOrCode=="*"||e.charOrCode=="/"||e.charOrCode=="-"||e.charOrCode=="+"){
if(_3("ie")){
var tr=_4.doc.selection.createRange().duplicate();
var _1f=tr.text||"";
var ntr=this.textboxWidget.textbox.createTextRange();
tr.move("character",0);
ntr.move("character",0);
try{
ntr.setEndPoint("EndToEnd",tr);
this.textboxWidget.textbox.selectionEnd=(this.textboxWidget.textbox.selectionStart=String(ntr.text).replace(/\r/g,"").length)+_1f.length;
}
catch(e){
}
}
if(this.textboxWidget.get("value")==""){
this.setTextboxValue(this.textboxWidget,"Ans");
}else{
if(this.putInAnsIfTextboxIsHighlighted(this.textboxWidget.textbox,_5.charOrCode)){
this.setTextboxValue(this.textboxWidget,"Ans");
_e.selectInputText(this.textboxWidget.textbox,this.textboxWidget.textbox.value.length,this.textboxWidget.textbox.value.length);
}
}
}
}
},insertMinus:function(){
this.insertText("-");
},print:function(_20,_21){
var t="<span style='display:block;";
if(_21){
t+="text-align:right;'>";
}else{
t+="text-align:left;'>";
}
t+=_20+"<br></span>";
if(this.hasDisplay){
this.displayBox.innerHTML+=t;
}else{
this.setTextboxValue(this.textboxWidget,_20);
}
},setTextboxValue:function(_22,val){
_22.set("value",val);
},putInAnsIfTextboxIsHighlighted:function(_23){
if(typeof _23.selectionStart=="number"){
if(_23.selectionStart==0&&_23.selectionEnd==_23.value.length){
return true;
}
}else{
if(document.selection){
var _24=document.selection.createRange();
if(_23.value==_24.text){
return true;
}
}
}
return false;
},clearText:function(){
if(this.hasDisplay&&this.textboxWidget.get("value")==""){
this.displayBox.innerHTML="";
}else{
this.setTextboxValue(this.textboxWidget,"");
}
this.textboxWidget.focus();
},insertOperator:function(_25){
if(typeof _25=="object"){
_25=_25=_9.getEnclosingWidget(_25["target"]).value;
}
if(this.textboxWidget.get("value")==""||this.putInAnsIfTextboxIsHighlighted(this.textboxWidget.textbox)){
_25="Ans"+_25;
}
this.insertText(_25);
},insertText:function(_26){
setTimeout(_2.hitch(this,function(){
var _27=this.textboxWidget.textbox;
if(_27.value==""){
_27.selectionStart=0;
_27.selectionEnd=0;
}
if(typeof _26=="object"){
_26=_26=_9.getEnclosingWidget(_26["target"]).value;
}
var _28=_27.value.replace(/\r/g,"");
if(typeof _27.selectionStart=="number"){
var pos=_27.selectionStart;
var cr=0;
if(_3("opera")){
cr=(_27.value.substring(0,pos).match(/\r/g)||[]).length;
}
_27.value=_28.substring(0,_27.selectionStart-cr)+_26+_28.substring(_27.selectionEnd-cr);
_27.focus();
pos+=_26.length;
_e.selectInputText(this.textboxWidget.textbox,pos,pos);
}else{
if(document.selection){
if(this.handle){
clearTimeout(this.handle);
this.handle=null;
}
_27.focus();
this.handle=setTimeout(function(){
var _29=document.selection.createRange();
_29.text=_26;
_29.select();
this.handle=null;
},0);
}
}
}),0);
},hasDisplay:false,postCreate:function(){
this.handle=null;
this.commandList=[];
this.commandIndex=0;
if(this.displayBox){
this.hasDisplay=true;
}
if(this.toFracButton&&!_12.toFrac){
_6.set(this.toFracButton.domNode,{visibility:"hidden"});
}
if(this.functionMakerButton&&!_12.FuncGen){
_6.set(this.functionMakerButton.domNode,{visibility:"hidden"});
}
if(this.grapherMakerButton&&!_12.Grapher){
_6.set(this.grapherMakerButton.domNode,{visibility:"hidden"});
}
this._connects.push(_a.addKeyListener(this.textboxWidget.textbox,{charOrCode:_8.UP_ARROW,shiftKey:false,metaKey:false,ctrlKey:false},this,this.cycleCommands,200,200));
this._connects.push(_a.addKeyListener(this.textboxWidget.textbox,{charOrCode:_8.DOWN_ARROW,shiftKey:false,metaKey:false,ctrlKey:false},this,this.cycleCommands,200,200));
this.startup();
}});
});
