define("dojox/grid/bidi/_BidiMixin", ["../../main", "dojo/_base/lang", "../_Builder", "dijit/_BidiSupport",
"../_Grid", "../cells/_base", "../cells/dijit"],
	function(dojox, lang, _Builder, _BidiSupport, _Grid, BaseCell, cellsDijit){
		// module:
		//		dojox/grid/bidi/BidiMixin
		
		/*=====
		return {
			// summary:
			//		Module that deals with BIDI for Grid
			// description:
			//		There's a special need for displaying BIDI text in rtl direction in ltr GUI, or sometimes needed auto support.
			//		"textDir" attribute should be set on Grid so that this class can be activated and responsible for text direction
			//		Allowed values:
			//		1. "ltr"
			//		2. "rtl"
			//		3. "auto" - contextual the direction of a text defined by first strong letter.
			//		By default the same as the page direction.
	
			//		Arabic, Hebrew, Urdu, and Farsi (Persian) are written from right to left,
			//		while numbers and segments of Latin (or Cyrillic or Greek) text are embedded in
			//		this text from left to right. The dual directionality aspects of such
			//		bidirectional (bidi) text are posing challenges to the way this text is processed and presented in computer applications.
			//		The Unicode Bidirectional Algorithm, which is commonly used for preparation of Bidi text for display,
			//		is not capable to reliably identify the natural base text direction for a given text.
			//		Only a human reader can unfailingly recognize the natural base text direction for a given text.
			//		Support for enforcing the base text direction is provided out of the box by most platforms / technologies,
			//		but very often the default settings do not guarantee proper display of English and Arabic/Hebrew text in the
			//		same application. See for reference:http://w3-03.ibm.com/globalization/page/publish/4353
		};
		=====*/

		lang.extend(_Grid, {
			/*this.textDir = textDir*/
			setCellNodeTextDirection: function(inColIndex, inRowIndex, textDir){
				this.getCell(inColIndex).getNode(inRowIndex).style.direction = textDir || "";
			},
			getCellNodeTextDirection: function(inColIndex, inRowIndex){
				return this.getCell(inColIndex).getNode(inRowIndex).style.direction;
			},
			_setTextDirAttr: function(textDir){
				this.textDir = textDir;
				this.render();
			}
		});
		
		lang.extend(_Builder._ContentBuilder, {
			_getTextDirStyle: function(textDir, inCell, inRowIndex){
				// summary:
				//		 Returns input text direction related attributes.
				// textDir:
				//		Cell text direction
				// inCell:
				//		Cell under processing
				// inRowIndex:
				//		Row index
				var item = this.grid.getItem(inRowIndex), ret = "";
				if(textDir === "auto"){
					var name = inCell.get ? inCell.get(inRowIndex, item) : (inCell.value || inCell.defaultValue);
					if(name){
						textDir = _BidiSupport.prototype._checkContextual(name);
					}
				}
				ret = " direction:" + textDir + ";";
				return ret;
			}
		});

		lang.extend(_Builder._HeaderBuilder, {
			_getTextDirStyle: function(textDir, inCell, inValue){
				// summary:
				//		 Returns input text direction related attributes.
				// textDir:
				//		Cell text direction
				// inCell:
				//		Cell under processing
				// inValue:
				//		Header cell content
				if(textDir === "auto"){
					var name = inValue || inCell.name || inCell.grid.getCellName(inCell);
					if(name){
						textDir = _BidiSupport.prototype._checkContextual(name);
					}
				}
				return (' direction:' + textDir + '; ');
			}
		});

		lang.extend(BaseCell.Cell, {
			// text direction UCC constants
			LRE: '\u202A',
			RLE: '\u202B',
			PDF: '\u202C',
			KEY_HANDLER: "onkeyup=' javascript:(function(){" +
							"var target; if (event.target) target = event.target; else if (event.srcElement) target = event.srcElement; if(!target) return;"  +			
							"var regExMatch = /[A-Za-z\u05d0-\u065f\u066a-\u06ef\u06fa-\u07ff\ufb1d-\ufdff\ufe70-\ufefc]/.exec(target.value);" +
							"target.dir = regExMatch ? ( regExMatch[0] <= \"z\" ? \"ltr\" : \"rtl\" ) : target.dir ? target.dir : \"ltr\"; })();'",
			_getTextDirMarkup: function(inDatum){
				// summary:
				//		Returns input text direction related attributes.
				// textDir:
				//		The control text direction
				// inDatum:
				//		Control text value.
				var textDirMarkup = "", textDir = this.textDir || this.grid.textDir;
				if(textDir){
					if(textDir === "auto"){
						textDirMarkup = this.KEY_HANDLER;
						textDir = _BidiSupport.prototype._checkContextual(inDatum);
					}
					textDirMarkup += " dir='" + textDir + "'; ";
				}
				return textDirMarkup;
			},
			formatEditing: function(inDatum, inRowIndex){
				this.needFormatNode(inDatum, inRowIndex);
				return '<input class="dojoxGridInput" ' + this._getTextDirMarkup(inDatum) + ' type="text" value="' + inDatum + '">';
			},
			_enforceTextDirWithUcc: function(textDir, text){
				// summary:
				//		Wraps by UCC (Unicode control characters) option's text according to textDir
				// textDir:
				//		The control text direction
				// text:
				//		The text to be wrapped.
				textDir = (textDir === "auto") ? _BidiSupport.prototype._checkContextual(text) : textDir;
				return (textDir === "rtl" ? this.RLE : this.LRE) + text + this.PDF;
			}
		});

		lang.extend(BaseCell.Select, {
			_getValueCallOrig: dojox.grid.cells.Select.prototype.getValue,
			getValue: function(inRowIndex){
				var ret = this._getValueCallOrig(inRowIndex);
				if(ret && (this.textDir || this.grid.textDir)){
					ret = ret.replace(/\u202A|\u202B|\u202C/g, "");
				}
				return ret;
			},
			formatEditing: function(inDatum, inRowIndex){
				this.needFormatNode(inDatum, inRowIndex);
				var h = [ '<select dir = \"' + (this.grid.isLeftToRight() ? 'ltr' : 'rtl') + '\" class="dojoxGridSelect">' ];
				for(var i = 0, o, v; ((o = this.options[i]) !== undefined) && ((v = this.values[i]) !== undefined); i++){
					v = v.replace ? v.replace(/&/g, '&amp;').replace(/</g, '&lt;') : v;
					o = o.replace ? o.replace(/&/g, '&amp;').replace(/</g, '&lt;') : o;
					if(this.textDir || this.grid.textDir){
						o = this._enforceTextDirWithUcc(this.textDir || this.grid.textDir, o);
					}
					h.push("<option", (inDatum == v ? ' selected' : ''), ' value = "' + v + '"', ">", o, "</option>");
				}
				h.push('</select>');
				return h.join('');
			}
		});

		lang.extend(cellsDijit.ComboBox, {
			getWidgetPropsCallOrig: dojox.grid.cells.ComboBox.prototype.getWidgetProps,
			getWidgetProps: function(inDatum){
				var ret = this.getWidgetPropsCallOrig(inDatum);
				if(this.textDir || this.grid.textDir){
					ret.textDir = this.textDir || this.grid.textDir;
				}
				return ret;
			}
		});

		lang.extend(cellsDijit._Widget, {
			getWidgetPropsCallOrig: dojox.grid.cells._Widget.prototype.getWidgetProps,
			getWidgetProps: function(inDatum){
				var ret = this.getWidgetPropsCallOrig(inDatum);
				if(this.textDir || this.grid.textDir){
					ret.textDir = this.textDir || this.grid.textDir;
				}
				return ret;
			}
		});
});
