require({cache:{
'url:dojox/calc/templates/Standard.html':"<div class=\"dijitReset dijitInline dojoxCalc\"\n><table class=\"dijitReset dijitInline dojoxCalcLayout\" data-dojo-attach-point=\"calcTable\" rules=\"none\" cellspacing=0 cellpadding=0 border=0>\n\t<tr\n\t\t><td colspan=\"4\" class=\"dojoxCalcInputContainer\"\n\t\t\t><input data-dojo-type=\"dijit.form.TextBox\" data-dojo-attach-event=\"onBlur:onBlur,onKeyPress:onKeyPress\" data-dojo-attach-point='textboxWidget'\n\t\t/></td\n\t></tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"seven\" label=\"7\" value='7' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"eight\" label=\"8\" value='8' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"nine\" label=\"9\" value='9' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"divide\" label=\"/\" value='/' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"four\" label=\"4\" value='4' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"five\" label=\"5\" value='5' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"six\" label=\"6\" value='6' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"multiply\" label=\"*\" value='*' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"one\" label=\"1\" value='1' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"two\" label=\"2\" value='2' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"three\" label=\"3\" value='3' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"add\" label=\"+\" value='+' data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"decimal\" label=\".\" value='.' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"zero\" label=\"0\" value='0' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"equals\" label=\"x=y\" value='=' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcMinusButtonContainer\">\n\t\t\t<span data-dojo-type=\"dijit.form.ComboButton\" data-dojo-attach-point=\"subtract\" label='-' value='-' data-dojo-attach-event='onClick:insertOperator'>\n\n\t\t\t\t<div data-dojo-type=\"dijit.Menu\" style=\"display:none;\">\n\t\t\t\t\t<div data-dojo-type=\"dijit.MenuItem\" data-dojo-attach-event=\"onClick:insertMinus\">\n\t\t\t\t\t\t(-)\n\t\t\t\t\t</div>\n\t\t\t\t</div>\n\t\t\t</span>\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"clear\" label=\"Clear\" data-dojo-attach-event='onClick:clearText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"sqrt\" label=\"&#x221A;\" value=\"&#x221A;\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"power\" label=\"^\" value=\"^\" data-dojo-attach-event='onClick:insertOperator' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"comma\" label=\",\" value=',' data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t</tr>\n\t<tr>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"AnsButton\" label=\"Ans\" value=\"Ans\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"LeftParenButton\" label=\"(\" value=\"(\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"RightParenButton\" label=\")\" value=\")\" data-dojo-attach-event='onClick:insertText' />\n\t\t</td>\n\t\t<td class=\"dojoxCalcButtonContainer\">\n\t\t\t<button data-dojo-type=\"dijit.form.Button\" data-dojo-attach-point=\"enter\" label=\"Enter\" data-dojo-attach-event='onClick:parseTextbox' />\n\t\t</td>\n\t</tr>\n</table>\n<span data-dojo-attach-point=\"executor\" data-dojo-type=\"dojox.calc._Executor\" data-dojo-attach-event=\"onLoad:executorLoaded\"></span>\n</div>\n"}});
define("dojox/calc/Standard", [
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/sniff",
	"dojo/_base/window",
	"dojo/_base/event",
	"dojo/dom-style",
	"dojo/ready",
	"dojo/keys",
	"dijit/registry",
	"dijit/typematic",
	"dijit/_WidgetBase",
	"dijit/_WidgetsInTemplateMixin",
	"dijit/_TemplatedMixin",
	"dijit/form/_TextBoxMixin",
	"dojox/math/_base",
	"dijit/TooltipDialog",
	"dojo/text!./templates/Standard.html",
	"dojox/calc/_Executor", // template
	"dijit/Menu", // template
	"dijit/MenuItem", // template
	"dijit/form/ComboButton", // template
	"dijit/form/Button", // template
	"dijit/form/TextBox" // template
], function(declare, lang, has, win, event, domStyle, ready, keys, registry, typematic, WidgetBase, WidgetsInTemplateMixin, TemplatedMixin, _TextBoxMixin, math, TooltipDialog, template, calc){

	return declare(
		"dojox.calc.Standard",
		[WidgetBase, TemplatedMixin, WidgetsInTemplateMixin],
	{
		// summary:
		//		The dialog layout for a standard 4 function/algebraic calculator

		templateString: template,

		readStore:null,
		writeStore:null,
		functions: [],

		executorLoaded: function(){
			// summary:
			//		load in the stores after executor is loaded (the stores need executor to be loaded because it parses them)
			ready(lang.hitch(this, function(){
				this.loadStore(this.readStore, true);
				this.loadStore(this.writeStore);
			}));
		},

		saveFunction: function(name, args, body){
			// summary:
			//		make the function with executor
			this.functions[name] = this.executor.normalizedFunction(name, args, body);
			this.functions[name].args = args;
			this.functions[name].body = body;
		},

		loadStore: function(store, isReadOnly){
			// summary:
			//		load an entire store, and make it publicly editable/viewable based on isReadOnly
			if(!store){
				return;
			}
			store.query({}).forEach(lang.hitch(this, function(item){
				lang.hitch(this, isReadOnly ? this.executor.normalizedFunction : this.saveFunction)(item.name, item.args, item.body);
			}));
		},

		parseTextbox: function(){
			// summary:
			//		parse the contents of the textboxWidget and display the answer somewhere (depending on the layout)
			var text = this.textboxWidget.textbox.value;
			if(text == "" && this.commandList.length > 0){
				this.setTextboxValue(this.textboxWidget, this.commandList[this.commandList.length-1]);
				text = this.textboxWidget.textbox.value;
			}
			if(text!=""){
				var ans = this.executor.eval(text);

				if((typeof ans == "number" && isNaN(ans))){
					if(this.commandList.length == 0 || this.commandList[this.commandList.length - 1] != text){
						this.commandList.push(text);
					}
					this.print(text, false);
					this.print("Not a Number", true);
				}else if(((typeof ans == "object" && "length" in ans) || typeof ans != "object") && typeof ans != "function" && ans != null){
					this.executor.eval("Ans="+ans);
					// add it to the command list as well
					if(this.commandList.length == 0 || this.commandList[this.commandList.length - 1] != text){
						this.commandList.push(text);
					}
					this.print(text, false);
					this.print(ans, true);
				}
				this.commandIndex = this.commandList.length-1;
				//this.displayBox.textbox.scrollTop=this.displayBox.textbox.scrollHeight;
				if(this.hasDisplay){
					this.displayBox.scrollTop=this.displayBox.scrollHeight;
				}
				//this.clearText();
				//this.textboxWidget.focus();
				_TextBoxMixin.selectInputText(this.textboxWidget.textbox);

			}else{
				this.textboxWidget.focus();
			}
		},
		cycleCommands: function(count, node, event){
			// summary:
			//		Cycle through the commands that the user has entered.
			//		It does not wrap around.
			if(count == -1 || this.commandList.length==0){
				return;
			}
			var keyNum = event.charOrCode;
			//up arrow
			if(keyNum == keys.UP_ARROW){
				this.cycleCommandUp();
			}else if(keyNum == keys.DOWN_ARROW){
				this.cycleCommandDown();
			}
		},
		cycleCommandUp: function(){
			// summary:
			//		cycle up through the list of commands the user has entered already
			if(this.commandIndex-1<0){
				this.commandIndex=0;
			}else{
				this.commandIndex--;
			}
			this.setTextboxValue(this.textboxWidget, this.commandList[this.commandIndex]);
		},
		cycleCommandDown: function(){
			// summary:
			//		cycle down through the list of commands the user has entered already
			if(this.commandIndex+1>=this.commandList.length){
				this.commandIndex=this.commandList.length;
				this.setTextboxValue(this.textboxWidget, "");
			}else{
				this.commandIndex++;
				this.setTextboxValue(this.textboxWidget, this.commandList[this.commandIndex]);
			}

		},
		onBlur: function(){
			// summary:
			//		IE is lacking in function when it comes to the text boxes, so here, make it work like other browsers do by forcing a node.selectionStart and End onto it
			if(has('ie')){
				var tr = win.doc.selection.createRange().duplicate();
				var selectedText = tr.text || '';
				var ntr = this.textboxWidget.textbox.createTextRange();
				tr.move("character",0);
				ntr.move("character",0);
				try{
					ntr.setEndPoint("EndToEnd", tr);
					this.textboxWidget.textbox.selectionEnd = (this.textboxWidget.textbox.selectionStart = String(ntr.text).replace(/\r/g,"").length) + selectedText.length;

				}catch(e){}
			}
		},
		onKeyPress: function(e){
			// summary:
			//		handle key input for Enter and operators
			if(e.charOrCode == keys.ENTER){
				this.parseTextbox();
				// stop form submissions
				event.stop(e);
			}else if(e.charOrCode == '!' || e.charOrCode == '^' || e.charOrCode == '*' || e.charOrCode == '/' || e.charOrCode == '-' || e.charOrCode == '+'){
				if(has('ie')){
					var tr = win.doc.selection.createRange().duplicate();
					var selectedText = tr.text || '';
					var ntr = this.textboxWidget.textbox.createTextRange();
					tr.move("character",0);
					ntr.move("character",0);
					try{
						ntr.setEndPoint("EndToEnd", tr);
						this.textboxWidget.textbox.selectionEnd = (this.textboxWidget.textbox.selectionStart = String(ntr.text).replace(/\r/g,"").length) + selectedText.length;

					}catch(e){}
				}

				if(this.textboxWidget.get("value")==""){
					this.setTextboxValue(this.textboxWidget, "Ans");
				}else if(this.putInAnsIfTextboxIsHighlighted(this.textboxWidget.textbox, event.charOrCode)){
					this.setTextboxValue(this.textboxWidget, "Ans");//this.insertText("Ans");
					// move the cursor to the end of "Ans"
					_TextBoxMixin.selectInputText(this.textboxWidget.textbox, this.textboxWidget.textbox.value.length, this.textboxWidget.textbox.value.length);
				}
			}
		},
		insertMinus: function(){
			// summary:
			//		insert a minus sign when they press (-) in the combo button
			this.insertText('-');
		},
		print: function(text, isRight){
			// summary:
			//		print the answer (typically) to the display or the input box
			var t = "<span style='display:block;";
			if(isRight){
				t += "text-align:right;'>";
			}else{
				t += "text-align:left;'>";
			}
			t += text+"<br></span>";
			if(this.hasDisplay){
				this.displayBox.innerHTML += t;
			}else{// if there is not a display box, put the answer in the input box
				this.setTextboxValue(this.textboxWidget, text);
			}
			//this.setTextboxValue(this.displayBox, this.displayBox.get('value')+'\n'+text);
		},
		setTextboxValue: function(widget, val){
			// summary:
			//		set a widget's value
			widget.set('value', val);
		},
		putInAnsIfTextboxIsHighlighted: function(node){
			// summary:
			//		try seeing if the textbox is highlighted completely so you know if Ans should be put in for an operator like +

			//console.log("Entered "+node.selectionStart + " "+ node.selectionEnd);
			if(typeof node.selectionStart == "number"){ // not-IE
				if(node.selectionStart==0 && node.selectionEnd == node.value.length){
					//node.value = "Ans";
					//dijit.selectInputText(node, node.value.length, node.value.length);
					return true;
				}
			}else if(document.selection){ // IE
				//console.log("Entered 2");
				var range = document.selection.createRange();
				//console.log("Range: "+range.text +" Node: "+node.value);
				if(node.value == range.text){
					//this.insertText("Ans");
					return true;
				}
			}
			return false;
		},
		clearText: function(){
			// summary:
			//		this clears the input box if it has content, but if it does not it clears the display
			if(this.hasDisplay && this.textboxWidget.get('value')==""){
				this.displayBox.innerHTML = "";//this.setTextboxValue(this.displayBox, "");
			}else{
				this.setTextboxValue(this.textboxWidget, "");
			}
			this.textboxWidget.focus();
		},
		/*insertMinusSign: function(){
			//
			var v = this.subtract.get('label');
			if(v != '(-)' && this.putInAnsIfTextboxIsHighlighted(this.textboxWidget.textbox)){
				this.insertText("Ans-");
				return;
			}
			this.insertText('-');
		},*/
		insertOperator: function(newText){
			// summary:
			//		insert an operator with a button
			if(typeof newText == "object"){
				newText = newText = registry.getEnclosingWidget(newText["target"]).value;
			}
			if(this.textboxWidget.get("value") == "" || this.putInAnsIfTextboxIsHighlighted(this.textboxWidget.textbox)){
				newText = "Ans"+newText;
			}
			this.insertText(newText);
		},
		insertText: function(newText){//(node, newText){
			// summary:
			//		insert text to the textboxWidget node
			setTimeout(lang.hitch(this, function(){

			var node = this.textboxWidget.textbox;
			if(node.value==""){
				node.selectionStart = 0;
				node.selectionEnd = 0;
			}
			if(typeof newText == "object"){
				newText = newText = registry.getEnclosingWidget(newText["target"]).value;
			}

			var value = node.value.replace(/\r/g,'');
			if(typeof node.selectionStart == "number"){ // not-IE
			        var pos = node.selectionStart;
			        var cr = 0;
		        	if(has('opera')){
		                	cr = (node.value.substring(0,pos).match(/\r/g) || []).length;
			        }
			        node.value = value.substring(0, node.selectionStart-cr) + newText + value.substring(node.selectionEnd-cr);
				node.focus();
				pos += newText.length;
			        //node.setSelectionRange(pos, pos);
				_TextBoxMixin.selectInputText(this.textboxWidget.textbox, pos, pos);
			}else if(document.selection){ // IE
				if(this.handle){
					clearTimeout(this.handle);
					this.handle = null;
				}
				node.focus();
				this.handle = setTimeout(function(){
			        	var range = document.selection.createRange();
				        range.text = newText;
				        // show cursor
		        		range.select();
					this.handle = null;
				}, 0);

			}
			}), 0);
		},
		hasDisplay: false,
		postCreate: function(){
			// summary:
			//		run startup, see if there is an upper display box, etc
			this.handle = null;
			this.commandList = [];
			this.commandIndex = 0;

			if(this.displayBox){
				this.hasDisplay = true;
			}
			if(this.toFracButton && !calc.toFrac){
				domStyle.set(this.toFracButton.domNode, { visibility: "hidden" });
			}
			if(this.functionMakerButton && !calc.FuncGen){
				domStyle.set(this.functionMakerButton.domNode, { visibility: "hidden" });
			}
			if(this.grapherMakerButton && !calc.Grapher){
				domStyle.set(this.grapherMakerButton.domNode, { visibility: "hidden" });
			}
			this._connects.push(typematic.addKeyListener(this.textboxWidget.textbox,
					{
						charOrCode:keys.UP_ARROW,
						shiftKey:false,
						metaKey:false,
						ctrlKey:false // ALT is optional since its unspecified
					},
					this, this.cycleCommands, 200, 200));
			this._connects.push(typematic.addKeyListener(this.textboxWidget.textbox,
					{
						charOrCode:keys.DOWN_ARROW,
						shiftKey:false,
						metaKey:false,
						ctrlKey:false // ALT is optional since its unspecified
					},
					this, this.cycleCommands, 200, 200));


			//onClick="this.insertText(document.getElementById('textbox'), '\u221A')"
			//this.sqrt.set("onClick", lang.hitch(this, "insertText", this.textboxWidget, '\u221A'));
			//this.pi.set("onClick", lang.hitch(this, "insertText", this.textboxWidget, '\u03C0'));
			this.startup()
		}
	});

});
