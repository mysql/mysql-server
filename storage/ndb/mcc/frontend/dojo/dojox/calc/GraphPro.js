//>>built
require({cache:{"url:dojox/calc/templates/GraphPro.html":"<div class=\"dijitReset dijitInline dojoxCalc\"\n><table class=\"dijitReset dijitInline dojoxCalcLayout\" data-dojo-attach-point=\"calcTable\" rules=\"none\" cellspacing=0 cellpadding=0 border=0>\n\t<tr\n\t\t><td colspan=\"4\" class=\"dojoxCalcTextAreaContainer\"\n\t\t\t><div class=\"dijitTextBox dijitTextArea\" style=\"height:10em;width:auto;max-width:15.3em;border-width:0px;\" data-dojo-attach-point='displayBox'></div\n\t\t></td\n\t></tr>\n\t<tr\n\t\t><td colspan=\"4\" class=\"dojoxCalcInputContainer\"\n\t\t\t><input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-event=\"onBlur:onBlur,onKeyPress:onKeyPress\" data-dojo-attach-point='textboxWidget'\n\t\t/></td\n\t></tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"grapherMakerButton\" label=\"Graph\" data-dojo-attach-event='onClick:makeGrapherWindow' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"functionMakerButton\" label=\"Func\" data-dojo-attach-event='onClick:makeFunctionWindow' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"toFracButton\" label=\"toFrac\" value=\"toFrac(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td colspan=\"1\" class=\"dojoxCalcButtonContainer\">\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"seven\" label=\"7\" value='7' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"eight\" label=\"8\" value='8' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"nine\" label=\"9\" value='9' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"divide\" label=\"/\" value='/' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"four\" label=\"4\" value='4' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"five\" label=\"5\" value='5' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"six\" label=\"6\" value='6' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"multiply\" label=\"*\" value='*' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"one\" label=\"1\" value='1' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"two\" label=\"2\" value='2' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"three\" label=\"3\" value='3' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"add\" label=\"+\" value='+' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"decimal\" label=\".\" value='.' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"zero\" label=\"0\" value='0' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"equals\" label=\"x=y\" value='=' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcMinusButtonContainer\">\n\t\t\t<span data-dojo-type=\"dijit.form.ComboButton\" data-dojo-attach-point=\"subtract\" label='-' value='-' data-dojo-attach-event='onClick:insertOperator'>\n\n\t\t\t\t<div data-dojo-type=\"dijit.Menu\" style=\"display:none;\">\n\t\t\t\t\t<div data-dojo-type=\"dijit.MenuItem\" data-dojo-attach-event=\"onClick:insertMinus\">\n\t\t\t\t\t\t(-)\n\t\t\t\t\t</div>\n\t\t\t\t</div>\n\t\t\t</span>\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"clear\" label=\"Clear\" data-dojo-attach-event='onClick:clearText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"sqrt\" label=\"&#x221A;\" value=\"&#x221A;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"power\" label=\"^\" value=\"^\" data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"factorialButton\" label=\"!\" value=\"!\" data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"pi\" label=\"&#x03C0;\" value=\"&#x03C0;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"sin\" label=\"sin\" value=\"sin(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"cos\" label=\"cos\" value=\"cos(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"tan\" label=\"tan\" value=\"tan(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"e\" label=\"&#x03F5;\" value=\"&#x03F5;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"log10\" label=\"log\" value=\"log(\" value=\"log(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"lnE\" label=\"ln\" value=\"ln(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"round\" label=\"Round\" value=\"Round(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"intButton\" label=\"Int\" value=\"Int(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"PermutationButton\" label=\"P\" value=\"P(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"CombinationButton\" label=\"C\" value=\"C(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"comma\" label=\",\" value=',' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"AnsButton\" label=\"Ans\" value=\"Ans\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"LeftParenButton\" label=\"(\" value=\"(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"RightParenButton\" label=\")\" value=\")\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"enter\" label=\"Enter\" data-dojo-attach-event='onClick:parseTextbox' />\n\t\t</td>\n\t</tr>\n</table>\n<span data-dojo-attach-point=\"executor\" data-dojo-type=\"dojox.calc._Executor\" data-dojo-attach-event=\"onLoad:executorLoaded\"></span>\n</div>\n"}});
define("dojox/calc/GraphPro",["dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-style","dojo/dom-construct","dojo/dom-geometry","dojo/ready","dojox/calc/Standard","dojox/calc/Grapher","dojox/layout/FloatingPane","dojo/text!./templates/GraphPro.html","dojox/calc/_Executor","dijit/Menu","dijit/MenuItem","dijit/form/ComboButton","dijit/form/Button","dijit/form/TextBox"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1("dojox.calc.GraphPro",_8,{templateString:_b,grapher:null,funcMaker:null,aFloatingPane:null,executorLoaded:function(){
this.inherited(arguments);
_7(_2.hitch(this,function(){
if(this.writeStore==null&&"functionMakerButton" in this){
_4.set(this.functionMakerButton.domNode,{visibility:"hidden"});
}
}));
},makeFunctionWindow:function(){
var _c=_3.body();
var _d=_5.create("div");
_c.appendChild(_d);
this.aFloatingPane=new dojox.layout.FloatingPane({resizable:false,dockable:true,maxable:false,closable:true,duration:300,title:"Function Window",style:"position:absolute;left:10em;top:10em;width:50em;"},_d);
var _e=this;
var d=_5.create("div");
this.funcMaker=new _9.FuncGen({writeStore:_e.writeStore,readStore:_e.readStore,functions:_e.functions,deleteFunction:_e.executor.deleteFunction,onSaved:function(){
var _f,_c;
if((_f=this.combo.get("value"))==""){
this.status.set("value","The function needs a name");
}else{
if((_c=this.textarea.get("value"))==""){
this.status.set("value","The function needs a body");
}else{
var _10=this.args.get("value");
if(!(_f in this.functions)){
this.combo.item=this.writeStore.put({name:_f,args:_10,body:_c});
}
this.saveFunction(_f,_10,_c);
this.status.set("value","Function "+_f+" was saved");
}
}
},saveFunction:_2.hitch(_e,_e.saveFunction)},d);
this.aFloatingPane.set("content",this.funcMaker);
this.aFloatingPane.startup();
this.aFloatingPane.bringToTop();
},makeGrapherWindow:function(){
var _11=_3.body();
var _12=_5.create("div");
_11.appendChild(_12);
this.aFloatingPane=new dojox.layout.FloatingPane({resizable:false,dockable:true,maxable:false,closable:true,duration:300,title:"Graph Window",style:"position:absolute;left:10em;top:5em;width:50em;"},_12);
var _13=this;
var d=_5.create("div");
this.grapher=new _9.Grapher({myPane:this.aFloatingPane,drawOne:function(i){
this.array[i][this.chartIndex].resize(this.graphWidth.get("value"),this.graphHeight.get("value"));
this.array[i][this.chartIndex].axes["x"].max=this.graphMaxX.get("value");
if(this.array[i][this.expressionIndex].get("value")==""){
this.setStatus(i,"Error");
return;
}
var _14;
var _15=(this.array[i][this.functionMode]=="y=");
if(this.array[i][this.expressionIndex].get("value")!=this.array[i][this.evaluatedExpression]){
var _16="x";
if(!_15){
_16="y";
}
_14=_13.executor.Function("",_16,"return "+this.array[i][this.expressionIndex].get("value"));
this.array[i][this.evaluatedExpression]=this.array[i][this.expressionIndex].value;
this.array[i][this.functionRef]=_14;
}else{
_14=this.array[i][this.functionRef];
}
var _17=this.array[i][this.colorIndex].get("value");
if(!_17){
_17="black";
}
_9.draw(this.array[i][this.chartIndex],_14,{graphNumber:this.array[i][this.funcNumberIndex],fOfX:_15,color:{stroke:{color:_17}}});
this.setStatus(i,"Drawn");
},onDraw:function(){
for(var i=0;i<this.rowCount;i++){
if((!this.dirty&&this.array[i][this.checkboxIndex].get("checked"))||(this.dirty&&this.array[i][this.statusIndex].innerHTML=="Drawn")){
this.drawOne(i);
}else{
this.array[i][this.chartIndex].resize(this.graphWidth.get("value"),this.graphHeight.get("value"));
this.array[i][this.chartIndex].axes["x"].max=this.graphMaxX.get("value");
}
}
var _18=_6.position(this.outerDiv).y-_6.position(this.myPane.domNode).y;
_18*=2;
_18=Math.abs(_18);
var _19=""+Math.max(parseInt(this.graphHeight.get("value"))+50,this.outerDiv.scrollHeight+_18);
var _1a=""+(parseInt(this.graphWidth.get("value"))+this.outerDiv.scrollWidth);
this.myPane.resize({w:_1a,h:_19});
}},d);
this.aFloatingPane.set("content",this.grapher);
this.aFloatingPane.startup();
this.aFloatingPane.bringToTop();
}});
});
