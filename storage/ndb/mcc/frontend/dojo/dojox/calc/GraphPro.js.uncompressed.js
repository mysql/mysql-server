require({cache:{
'url:dojox/calc/templates/GraphPro.html':"<div class=\"dijitReset dijitInline dojoxCalc\"\n><table class=\"dijitReset dijitInline dojoxCalcLayout\" data-dojo-attach-point=\"calcTable\" rules=\"none\" cellspacing=0 cellpadding=0 border=0>\n\t<tr\n\t\t><td colspan=\"4\" class=\"dojoxCalcTextAreaContainer\"\n\t\t\t><div class=\"dijitTextBox dijitTextArea\" style=\"height:10em;width:auto;max-width:15.3em;border-width:0px;\" data-dojo-attach-point='displayBox'></div\n\t\t></td\n\t></tr>\n\t<tr\n\t\t><td colspan=\"4\" class=\"dojoxCalcInputContainer\"\n\t\t\t><input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-event=\"onBlur:onBlur,onKeyPress:onKeyPress\" data-dojo-attach-point='textboxWidget'\n\t\t/></td\n\t></tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"grapherMakerButton\" label=\"Graph\" data-dojo-attach-event='onClick:makeGrapherWindow' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"functionMakerButton\" label=\"Func\" data-dojo-attach-event='onClick:makeFunctionWindow' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"toFracButton\" label=\"toFrac\" value=\"toFrac(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td colspan=\"1\" class=\"dojoxCalcButtonContainer\">\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"seven\" label=\"7\" value='7' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"eight\" label=\"8\" value='8' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"nine\" label=\"9\" value='9' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"divide\" label=\"/\" value='/' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"four\" label=\"4\" value='4' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"five\" label=\"5\" value='5' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"six\" label=\"6\" value='6' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"multiply\" label=\"*\" value='*' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"one\" label=\"1\" value='1' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"two\" label=\"2\" value='2' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"three\" label=\"3\" value='3' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"add\" label=\"+\" value='+' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"decimal\" label=\".\" value='.' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"zero\" label=\"0\" value='0' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"equals\" label=\"x=y\" value='=' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcMinusButtonContainer\">\n\t\t\t<span data-dojo-type=\"dijit.form.ComboButton\" data-dojo-attach-point=\"subtract\" label='-' value='-' data-dojo-attach-event='onClick:insertOperator'>\n\n\t\t\t\t<div data-dojo-type=\"dijit.Menu\" style=\"display:none;\">\n\t\t\t\t\t<div data-dojo-type=\"dijit.MenuItem\" data-dojo-attach-event=\"onClick:insertMinus\">\n\t\t\t\t\t\t(-)\n\t\t\t\t\t</div>\n\t\t\t\t</div>\n\t\t\t</span>\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"clear\" label=\"Clear\" data-dojo-attach-event='onClick:clearText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"sqrt\" label=\"&#x221A;\" value=\"&#x221A;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"power\" label=\"^\" value=\"^\" data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"factorialButton\" label=\"!\" value=\"!\" data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"pi\" label=\"&#x03C0;\" value=\"&#x03C0;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"sin\" label=\"sin\" value=\"sin(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"cos\" label=\"cos\" value=\"cos(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"tan\" label=\"tan\" value=\"tan(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"e\" label=\"&#x03F5;\" value=\"&#x03F5;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"log10\" label=\"log\" value=\"log(\" value=\"log(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"lnE\" label=\"ln\" value=\"ln(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"round\" label=\"Round\" value=\"Round(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"intButton\" label=\"Int\" value=\"Int(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"PermutationButton\" label=\"P\" value=\"P(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"CombinationButton\" label=\"C\" value=\"C(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"comma\" label=\",\" value=',' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"AnsButton\" label=\"Ans\" value=\"Ans\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"LeftParenButton\" label=\"(\" value=\"(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"RightParenButton\" label=\")\" value=\")\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"enter\" label=\"Enter\" data-dojo-attach-event='onClick:parseTextbox' />\n\t\t</td>\n\t</tr>\n</table>\n<span data-dojo-attach-point=\"executor\" data-dojo-type=\"dojox.calc._Executor\" data-dojo-attach-event=\"onLoad:executorLoaded\"></span>\n</div>\n"}});
define("dojox/calc/GraphPro", [
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/window",
	"dojo/dom-style",
	"dojo/dom-construct",
	"dojo/dom-geometry",
	"dojo/ready",
	"dojox/calc/Standard",
	"dojox/calc/Grapher",
	"dojox/layout/FloatingPane",
	"dojo/text!./templates/GraphPro.html",
	"dojox/calc/_Executor", // template
	"dijit/Menu", // template
	"dijit/MenuItem", // template
	"dijit/form/ComboButton", // template
	"dijit/form/Button", // template
	"dijit/form/TextBox" // template
], function(declare, lang, win, domStyle, domConstruct, domGeometry, ready, Standard, calc, FloatingPane, template){


	return declare(
		"dojox.calc.GraphPro",
		Standard,
	{
		// summary:
		//		The dialog widget for a graphing, scientific calculator

		templateString: template,

		grapher:null,
		funcMaker:null,
		aFloatingPane: null,

		executorLoaded: function(){
			// summary:
			//		when executor loads check to see if the writestore is there
			this.inherited(arguments);
			ready(lang.hitch(this, function(){
				if(this.writeStore == null && "functionMakerButton" in this){
					domStyle.set(this.functionMakerButton.domNode, { visibility: "hidden" });
				}
			}));
		},
		makeFunctionWindow: function(){
			// summary:
			//		use this function to create a function window (with the button on the layout)
			var body = win.body();

			var pane = domConstruct.create('div');
			body.appendChild(pane);

			this.aFloatingPane = new dojox.layout.FloatingPane({resizable:false, dockable:true, maxable:false, closable:true, duration:300, title:"Function Window", style:"position:absolute;left:10em;top:10em;width:50em;"}, pane);
			var that = this;
			var d = domConstruct.create("div");
			this.funcMaker = new calc.FuncGen({
				writeStore:that.writeStore,
				readStore:that.readStore,
				functions:that.functions,
				deleteFunction: that.executor.deleteFunction,
				onSaved:function(){
					var	name,
						body;
					if((name = this.combo.get("value")) == ""){
						this.status.set("value", "The function needs a name");
					}else if((body = this.textarea.get("value")) == ""){
						// i don't think users need empty functions for math
						this.status.set("value", "The function needs a body");
					}else{
						var args = this.args.get("value");
						if(!(name in this.functions)){
							this.combo.item = this.writeStore.put({name: name, args: args, body: body});
						}
						this.saveFunction(name, args, body);
						this.status.set("value", "Function "+name+" was saved");
					}
				},
				saveFunction: lang.hitch(that, that.saveFunction)
			}, d);
			this.aFloatingPane.set('content', this.funcMaker);
			this.aFloatingPane.startup();
			this.aFloatingPane.bringToTop();
		},
		makeGrapherWindow: function(){
			// summary:
			//		use this to make a Grapher window appear with a button
			var body = win.body();

			var pane = domConstruct.create('div');
			body.appendChild(pane);

			this.aFloatingPane = new dojox.layout.FloatingPane({resizable:false, dockable:true, maxable:false, closable:true, duration:300, title:"Graph Window", style:"position:absolute;left:10em;top:5em;width:50em;"}, pane);
			var that = this;

			var d = domConstruct.create("div");
			this.grapher = new calc.Grapher({
				myPane: this.aFloatingPane,
				drawOne: function(i){
					this.array[i][this.chartIndex].resize(this.graphWidth.get("value"), this.graphHeight.get("value"));
					this.array[i][this.chartIndex].axes["x"].max = this.graphMaxX.get('value');
					if(this.array[i][this.expressionIndex].get("value")==""){
						this.setStatus(i, "Error");
						return;
					}
					var func;
					var yEquals = (this.array[i][this.functionMode]=="y=");
					if(this.array[i][this.expressionIndex].get("value")!=this.array[i][this.evaluatedExpression]){
						var args = 'x';
						if(!yEquals){
							args = 'y';
						}
						func = that.executor.Function('', args, "return "+this.array[i][this.expressionIndex].get('value'));
						this.array[i][this.evaluatedExpression] = this.array[i][this.expressionIndex].value;
						this.array[i][this.functionRef] = func;
					}
					else{
						func = this.array[i][this.functionRef];
					}
					var pickedColor = this.array[i][this.colorIndex].get("value");
					if(!pickedColor){
						pickedColor = 'black';
					}
					calc.draw(this.array[i][this.chartIndex], func, {graphNumber:this.array[i][this.funcNumberIndex], fOfX:yEquals, color:{stroke:{color:pickedColor}}});
					this.setStatus(i, "Drawn");
				},
				onDraw:function(){
					for(var i = 0; i < this.rowCount; i++){
						if((!this.dirty && this.array[i][this.checkboxIndex].get("checked")) || (this.dirty && this.array[i][this.statusIndex].innerHTML=="Drawn")){
							this.drawOne(i);
						}else{
							this.array[i][this.chartIndex].resize(this.graphWidth.get("value"), this.graphHeight.get("value"));
							this.array[i][this.chartIndex].axes["x"].max = this.graphMaxX.get('value');
						}
					}

					var bufferY = domGeometry.position(this.outerDiv).y-domGeometry.position(this.myPane.domNode).y;
					bufferY*=2;
					bufferY=Math.abs(bufferY);
					var height = "" + Math.max(parseInt(this.graphHeight.get('value'))+50, this.outerDiv.scrollHeight+bufferY);
					var width = "" + (parseInt(this.graphWidth.get('value')) + this.outerDiv.scrollWidth);
					this.myPane.resize({w:width, h:height});
				}
			}, d);
			this.aFloatingPane.set('content', this.grapher);
			this.aFloatingPane.startup();
			this.aFloatingPane.bringToTop();
		}
	});
});
