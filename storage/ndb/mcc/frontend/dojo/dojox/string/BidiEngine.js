define(["dojo/_base/lang", "dojo/_base/declare", "dojo/Stateful"],
  function(lang,declare,Stateful) {
	lang.getObject("string", true, dojox);

	var BidiEngine = declare("dojox.string.BidiEngine", Stateful, {
		// summary:
		//		This class provides a bidi transformation engine, i.e.
		//		functions for reordering and shaping bidi text.
		// description:
		//		Bidi stands for support for languages with a bidirectional script.
		//
		//		Usually Unicode Bidi Algorithm used by OS platform (and web browsers) is capable of properly
		//		transforming Bidi text and as a result it is adequately displayed on the screen.
		//		However, in some situations, Unicode Bidi Algorithm is not invoked or is not properly applied.
		//		This may occur in situation in which software responsible for rendering the text is not leveraging
		//		Unicode Bidi Algorithm implemented by OS (e.g. GFX renderers).
		//
		//		Bidi engine provided in this class implements Unicode Bidi Algorithm as specified at
		//		http://www.unicode.org/reports/tr9/.
		//
		//		For more information on basic Bidi concepts please read
		//		"Bidirectional script support - A primer" available from
		//		http://www.ibm.com/developerworks/websphere/library/techarticles/bidi/bidigen.html.
		//
		//		As of February 2011, Bidi engine has following limitations:
		//
		//		1. No support for following numeric shaping options:
		//			- H - Hindi,
		//			- C - Contextual,
		//			- N - Nominal.
		//		2. No support for following shaping options:
		//			- I - Initial shaping,
		//			- M - Middle shaping,
		//			- F - Final shaping,
		//			- B - Isolated shaping.
		//		3. No support for LRE/RLE/LRO/RLO/PDF (they are handled like neutrals).
		//		4. No support for Windows compatibility.
		//		5. No support for  insert/remove marks.
		//		6. No support for code pages.
		//

		// Input Bidi layout in which inputText is passed to the function.
		inputFormat: "ILYNN",

		// Output Bidi layout to which inputText should be transformed.
		outputFormat: "VLNNN",

		// Array, containing positions of each character from the source text in the resulting text.
		sourceToTarget: [],

		// Array, containing positions of each character from the resulting text in the source text.
		targetToSource: [],

		// Array, containing bidi level of each character from the source text
		levels: [],

		bidiTransform: function (/*String*/text, /*String*/formatIn, /*String*/formatOut) {
			// summary:
			//		Central public API for Bidi engine. Transforms the text according to formatIn, formatOut
			//		parameters. If formatIn or formatOut parametrs are not valid throws an exception.
			// inputText:
			//		Input text subject to application of Bidi transformation.
			// formatIn:
			//		Input Bidi layout in which inputText is passed to the function.
			// formatOut:
			//		Output Bidi layout to which inputText should be transformed.
			// description:
			//		Both formatIn and formatOut parameters are 5 letters long strings.
			//		For example - "ILYNN". Each letter is associated with specific attribute of Bidi layout.
			//		Possible and default values for each one of the letters are provided below:
			//
			//		First letter:
			//
			//		- Letter position/index:
			//			1
			//		- Letter meaning:
			//			Ordering Schema.
			//		- Possible values:
			//			- I - Implicit (Logical).
			//			- V - Visual.
			//		- Default value:
			//			I
			//
			//		Second letter:
			//
			//		- Letter position/index:
			//			2
			//		- Letter meaning:
			//			Orientation.
			//		- Possible values:
			//			- L - Left To Right.
			//			- R - Right To Left.
			//			- C - Contextual Left to Right.
			//			- D - Contextual Right to Left.
			//		- Default value:
			//			L
			//
			//		Third letter:
			//
			//		- Letter position/index:
			//			3
			//		- Letter meaning:
			//			Symmetric Swapping.
			//		- Possible values:
			//			- Y - Symmetric swapping is on.
			//			- N - Symmetric swapping is off.
			//		- Default value:
			//			Y
			//
			//		Fourth letter:
			//
			//		- Letter position/index:
			//			4
			//		- Letter meaning:
			//			Shaping.
			//		- Possible values:
			//			- S - Text is shaped.
			//			- N - Text is not shaped.
			//		- Default value:
			//			N
			//
			//		Fifth letter:
			//
			//		- Letter position/index:
			//			5
			//		- Letter meaning:
			//			Numeric Shaping.
			//		- Possible values:
			//			- N - Nominal.
			//		- Default value:
			//			N
			//
			//		The output of this function is original text (passed via first argument) transformed from
			//		input Bidi layout (second argument) to output Bidi layout (last argument).
			//
			//		Sample call:
			//	|	mytext = bidiTransform("HELLO WORLD", "ILYNN", "VLYNN");
			//		In this case, "HELLO WORLD" text is transformed from Logical - LTR to Visual - LTR Bidi layout
			//		with default values for symmetric swapping (Yes), shaping (Not shaped) and numeric shaping
			//		(Nominal).
			// returns: String
			//		Original text transformed from input Bidi layout (second argument)
			//		to output Bidi layout (last argument).
			//		Throws an exception if the bidi layout strings are not valid.
			// tags:
			//		public

			this.sourceToTarget = [];
			this.targetToSource = [];
			if (!text) {
				return "";
			}
			initMaps(this.sourceToTarget, this.targetToSource, text.length);
			if (!this.checkParameters(formatIn, formatOut)) {
				return text;
			}

			formatIn = this.inputFormat;
			formatOut = this.outputFormat;
			var result = text;
			var bdx = BDX;
			var orientIn = getOrientation(formatIn.charAt(1)),
				orientOut = getOrientation(formatOut.charAt(1)),
				osIn = (formatIn.charAt(0) === "I") ? "L" : formatIn.charAt(0),
				osOut = (formatOut.charAt(0) === "I") ? "L" : formatOut.charAt(0),
				inFormat = osIn + orientIn,
				outFormat = osOut + orientOut,
				swap = formatIn.charAt(2) + formatOut.charAt(2);

			bdx.defInFormat = inFormat;
			bdx.defOutFormat = outFormat;
			bdx.defSwap = swap;

			var stage1Text = doBidiReorder(text, inFormat, outFormat, swap, bdx),
				isRtl = false;

			if (formatOut.charAt(1) === "R") {
				isRtl = true;
			} else if (formatOut.charAt(1) === "C" || formatOut.charAt(1) === "D") {
				isRtl = this.checkContextual(stage1Text);
			}

			this.sourceToTarget = stMap;
			this.targetToSource = reverseMap(this.sourceToTarget);
			tsMap = this.targetToSource;

			if (formatIn.charAt(3) === formatOut.charAt(3)) {
				result = stage1Text;
			} else if (formatOut.charAt(3) === "S") {
				result = shape(isRtl, stage1Text, true);
			} else {  //formatOut.charAt(3) === "N"
				result = deshape(stage1Text, isRtl, true);
			}
			this.sourceToTarget = stMap;
			this.targetToSource = tsMap;
			this.levels = lvMap;
			return result;
		},

		_inputFormatSetter: function (format) {
			if (!validFormat.test(format)) {
				throw new Error("dojox/string/BidiEngine: the bidi layout string is wrong!");
			}
			this.inputFormat = format;
		},

		_outputFormatSetter: function (format) {
			if (!validFormat.test(format)) {
				throw new Error("dojox/string/BidiEngine: the bidi layout string is wrong!");
			}
			this.outputFormat = format;
		},

		checkParameters: function (/*String*/formatIn, /*String*/formatOut) {
			// summary:
			//		Checks layout parameters.
			// formatIn:
			//		Input Bidi layout in which inputText is passed to the function.
			// formatOut:
			//		Output Bidi layout to which inputText should be transformed.
			// description:
			//		Checks, that layout parameters are different and contain allowed values.
			//		Allowed values for format string are:
			//			- 1st letter: I, V
			//			- 2nd letter: L, R, C, D
			//			- 3rd letter: Y, N
			//			- 4th letter: S, N
			//			- 5th letter: N
			// returns: /*Boolean*/
			//		true - if layout parameters are valid.
			//		false - otherwise.
			// tags:
			//		private

			if (!formatIn) {
				formatIn = this.inputFormat;
			} else {
				this.set("inputFormat", formatIn);
			}
			if (!formatOut) {
				formatOut = this.outputFormat;
			} else {
				this.set("outputFormat", formatOut);
			}
			if (formatIn === formatOut) {
				return false;
			}
			return true;
		},

		checkContextual: function (/*String*/text) {
			// summary:
			//		Determine the base direction of a bidi text according
			//		to its first strong directional character.
			// text:
			//		The text to check.
			// returns: /*String*/
			//		"ltr" or "rtl" according to the first strong character.
			//		If there is no strong character, returns the value of the
			//		document dir property.
			// tags:
			//		public
			var dir = firstStrongDir(text);
			if (dir !== "ltr" && dir !== "rtl") {
				try {
					dir = document.dir.toLowerCase();
				} catch (e) {
				}
				if (dir !== "ltr" && dir !== "rtl") {
					dir = "ltr";
				}
			}
			return dir;
		},

		hasBidiChar: function (/*String*/text) {
			// summary:
			//		Return true if text contains RTL directed character.
			// text:
			//		The source string.
			// description:
			//		Searches for RTL directed character.
			//		Returns true if found, else returns false.
			// returns: /*Boolean*/
			//		true - if text has a RTL directed character.
			//		false - otherwise.
			// tags:
			//		public

		    return bidiChars.test(text);
		}
	});

	function doBidiReorder(/*String*/text, /*String*/inFormat,
				/*String*/outFormat, /*String*/swap, /*Object*/bdx) {
		// summary:
		//		Reorder the source text according to the bidi attributes
		//		of source and result.
		// text:
		//		The text to reorder.
		// inFormat:
		//		Ordering scheme and base direction of the source text.
		//		Can be "LLTR", "LRTL", "LCLR", "LCRL", "VLTR", "VRTL",
		//		"VCLR", "VCRL".
		//		The first letter is "L" for logical ordering scheme,
		//		"V" for visual ordering scheme.
		//		The other letters specify the base direction.
		//		"CLR" means contextual direction defaulting to LTR if
		//		there is no strong letter.
		//		"CRL" means contextual direction defaulting to RTL if
		//		there is no strong letter.
		//		The initial value is "LLTR", if none, the initial value is used.
		// outFormat:
		//		Required ordering scheme and base direction of the
		//		result. Has the same format as inFormat.
		//		If none, the initial value "VLTR" is used.
		// swap:
		//		Symmetric swapping attributes of source and result.
		//		The allowed values can be "YN", "NY", "YY" and "NN".
		//		The first letter reflects the symmetric swapping attribute
		//		of the source, the second letter that of the result.
		// bdx: Object
		//		Used for intermediate data storage
		// returns:
		//		Text reordered according to source and result attributes.

		var params = prepareReorderingParameters(text, {inFormat: inFormat, outFormat: outFormat, swap: swap}, bdx);
		if (params.inFormat === params.outFormat) {
			return text;
		}
		inFormat = params.inFormat;
		outFormat = params.outFormat;
		swap = params.swap;
		var inOrdering = inFormat.substring(0, 1),
		inOrientation = inFormat.substring(1, 4),
		outOrdering = outFormat.substring(0, 1),
		outOrientation = outFormat.substring(1, 4);
		bdx.inFormat = inFormat;
		bdx.outFormat = outFormat;
		bdx.swap = swap;
		if ((inOrdering === "L") && (outFormat === "VLTR")) { //core cases
			//cases: LLTR->VLTR, LRTL->VLTR
			if (inOrientation === "LTR") {
				bdx.dir = LTR;
				return doReorder(text, bdx);
			}
			if (inOrientation === "RTL") {
				bdx.dir = RTL;
				return doReorder(text, bdx);
			}
		}
		if ((inOrdering === "V") && (outOrdering === "V")) {
			//inOrientation != outOrientation
			//cases: VRTL->VLTR, VLTR->VRTL
			bdx.dir = inOrientation === "RTL"? RTL : LTR;
			return invertStr(text, bdx);
		}
		if ((inOrdering === "L") && (outFormat === "VRTL")) {
			//cases: LLTR->VRTL, LRTL->VRTL
			if (inOrientation === "LTR") {
				bdx.dir = LTR;
				text = doReorder(text, bdx);
			} else {
				//inOrientation == RTL
				bdx.dir = RTL;
				text = doReorder(text, bdx);
			}
			return invertStr(text);
		}
		if ((inFormat === "VLTR") && (outFormat === "LLTR")) {
			//case: VLTR->LLTR
			bdx.dir = LTR;
			return doReorder(text, bdx);
		}
		if ((inOrdering === "V") && (outOrdering === "L") && (inOrientation !== outOrientation)) {
			//cases: VLTR->LRTL, VRTL->LLTR
			text = invertStr(text);
			return (inOrientation === "RTL") ? doBidiReorder(text, "LLTR", "VLTR", swap, bdx) :
												doBidiReorder(text, "LRTL", "VRTL", swap, bdx);
		}
		if ((inFormat === "VRTL") && (outFormat === "LRTL")) {
			//case VRTL->LRTL
			return doBidiReorder(text, "LRTL", "VRTL", swap, bdx);
		}
		if ((inOrdering === "L") && (outOrdering === "L")) {
			//inOrientation != outOrientation
			//cases: LRTL->LLTR, LLTR->LRTL
			var saveSwap = bdx.swap;
			bdx.swap = saveSwap.substr(0, 1) + "N";
			if (inOrientation === "RTL") {
				//LRTL->LLTR
				bdx.dir = RTL;
				text = doReorder(text, bdx);
				bdx.swap = "N" + saveSwap.substr(1, 2);
				bdx.dir = LTR;
				text = doReorder(text, bdx);
			} else { //LLTR->LRTL
				bdx.dir = LTR;
				text = doReorder(text, bdx);
				bdx.swap = "N" + saveSwap.substr(1, 2);
				text = doBidiReorder(text, "VLTR", "LRTL", bdx.swap, bdx);
			}
			return text;
		}
	}

	function prepareReorderingParameters(/*String*/text, /*Object*/params, /*Object*/bdx) {
		// summary:
		//		Prepare reordering parameters
		// text:
		//		The text to reorder.
		// params:
		//      Object, containing reordering parameters:
		//         - inFormat: Ordering scheme and base direction of the source text.
		//         - outFormat: Required ordering scheme and base direction of the result.
		//         - swap: Symmetric swapping attributes of source and result.
		// bdx: Object
		//		Used for intermediate data storage
		// tags:
		//		private

		if (params.inFormat === undefined) {
			params.inFormat = bdx.defInFormat;
		}
		if (params.outFormat === undefined) {
			params.outFormat = bdx.defOutFormat;
		}
		if (params.swap === undefined) {
			params.swap = bdx.defSwap;
		}
		if (params.inFormat === params.outFormat) {
			return params;
		}
		var dir, inOrdering = params.inFormat.substring(0, 1),
		inOrientation = params.inFormat.substring(1, 4),
		outOrdering = params.outFormat.substring(0, 1),
		outOrientation = params.outFormat.substring(1, 4);
		if (inOrientation.charAt(0) === "C") {
			dir = firstStrongDir(text);
			if (dir === "ltr" || dir === "rtl") {
				inOrientation = dir.toUpperCase();
			} else {
				inOrientation = params.inFormat.charAt(2) === "L" ? "LTR" : "RTL";
			}
			params.inFormat = inOrdering + inOrientation;
		}
		if (outOrientation.charAt(0) === "C") {
			dir = firstStrongDir(text);
			if (dir === "rtl") {
				outOrientation = "RTL";
			} else if (dir === "ltr") {
				dir = lastStrongDir(text);
				outOrientation = dir.toUpperCase();
			} else {
				outOrientation = params.outFormat.charAt(2) === "L" ? "LTR" : "RTL";
			}
			params.outFormat = outOrdering + outOrientation;
		}
		return params;
	}

	function shape(/*boolean*/rtl, /*String*/text, /*boolean*/compress) {
		// summary:
		//		Shape the source text.
		// rtl:
		//		Flag indicating if the text is in RTL direction (logical
		//		direction for Arabic words).
		// text:
		//		The text to shape.
		// compress:
		//		A flag indicates to insert extra space after the lam alef compression
		//		to preserve the buffer size or not insert an extra space which will lead
		//		to decrease the buffer size. This option can be:
		//
		//		- true (default) to not insert extra space after compressing Lam+Alef into one character Lamalef
		//		- false to insert an extra space after compressed Lamalef to preserve the buffer size
		// returns:
		//		text shaped.
		// tags:
		//		private.

		if (text.length === 0) {
			return;
		}
		if (rtl === undefined) {
			rtl = true;
		}
		if (compress === undefined) {
			compress = true;
		}
		text = String(text);

		var str06 = text.split(""),
			Ix = 0,
			step = +1,
			nIEnd = str06.length;
		if (!rtl) {
			Ix = str06.length - 1;
			step = -1;
			nIEnd = 1;
		}
		var compressArray = doShape(str06, Ix, step, nIEnd, compress);
		var outBuf = "";
		for (var idx = 0; idx < str06.length; idx++) {
			if (!(compress && indexOf(compressArray, compressArray.length, idx) > -1)) {
				outBuf += str06[idx];
			} else {
				updateMap(tsMap, idx, !rtl, -1);
				stMap.splice(idx, 1);
			}
		}
		return outBuf;
	}

	function doShape(str06, Ix, step, nIEnd, compress) {
		// summary:
		//		Shape the source text.
		// str06:
		//		Array containing source text
		// Ix:
		//		Index of the first handled element
		// step:
		//		direction of the process
		// nIEnd:
		//		Index of the last handled element
		// compress:
		//		A flag indicates to insert extra space after the lam alef compression
		//		to preserve the buffer size or not insert an extra space which will lead
		//		to decrease the buffer size.
		// returns:
		//		Array, contained shaped text.
		// tags:
		//		private.

		var previousCursive = 0, compressArray = [], compressArrayIndx = 0;
		for (var index = Ix; index * step < nIEnd; index = index + step) {
			if (isArabicAlefbet(str06[index]) || isArabicDiacritics(str06[index])) {
				// Arabic letter Lam
				if (str06[index] === "\u0644" && isNextAlef(str06, (index + step), step, nIEnd)) {
					str06[index] = (previousCursive === 0) ?
							getLamAlefFE(str06[index + step], LamAlefInialTableFE) :
							getLamAlefFE(str06[index + step], LamAlefMedialTableFE);
					index += step;
					setAlefToSpace(str06, index, step, nIEnd);
					if (compress) {
						compressArray[compressArrayIndx] = index;
						compressArrayIndx++;
					}
					previousCursive = 0;
					continue;
				}
				var currentChr = str06[index];
				if (previousCursive === 1) {
					// if next is Arabic
					// Character is in medial form
					// else character is in final form
					str06[index] = (isNextArabic(str06, (index + step), step, nIEnd)) ?
						getMedialFormCharacterFE(str06[index]) : getFormCharacterFE(str06[index], FinalForm);
				} else {
					if (isNextArabic(str06, (index + step), step, nIEnd) === true) {
						//character is in Initial form
						str06[index] = getFormCharacterFE(str06[index], InitialForm);
					} else {
						str06[index] = getFormCharacterFE(str06[index], IsolatedForm);
					}
				}
				//exam if the current character is cursive
				if (!isArabicDiacritics(currentChr)) {
					previousCursive = 1;
				}
				if (isStandAlonCharacter(currentChr) === true) {
					previousCursive = 0;
				}
			} else {
				previousCursive = 0;
			}
		}
		return compressArray;
	}

	function firstStrongDir(/*String*/text) {
		// summary:
		//		Return the first strong character direction
		// text:
		//		The source string.
		// description:
		//		Searches for first "strong" character.
		//		Returns if strong character was found with the direction defined by this
		//		character, if no strong character was found returns an empty string.
		// returns: String
		//		"ltr" - if the first strong character is Latin.
		//		"rtl" - if the first strong character is RTL directed character.
		//		"" - if the strong character wasn't found.
		// tags:
		//		private

		var fdc = /[A-Za-z\u05d0-\u065f\u066a-\u06ef\u06fa-\u07ff\ufb1d-\ufdff\ufe70-\ufefc]/.exec(text);
		// if found return the direction that defined by the character
		return fdc ? (fdc[0] <= "z" ? "ltr" : "rtl") : "";
	}

	function lastStrongDir(text) {
		// summary:
		//		Return the last strong character direction
		// text:
		//		The source string.
		// description:
		//		Searches for first (from the end) "strong" character.
		//		Returns if strong character was found with the direction defined by this
		//		character, if no strong character was found returns an empty string.
		// tags:
		//		private
		var chars = text.split("");
		chars.reverse();
		return firstStrongDir(chars.join(""));
	}

	function deshape(/*String*/text, /*boolean*/rtl, /*boolean*/consumeNextSpace) {
		// summary:
		//		deshape the source text.
		// text:
		//		the text to be deshape.
		// rtl:
		//		flag indicating if the text is in RTL direction (logical
		//		direction for Arabic words).
		// consumeNextSpace:
		//		flag indicating whether to consume the space next to the
		//		the lam alef if there is a space followed the Lamalef character to preserve the buffer size.
		//		In case there is no space next to the lam alef the buffer size will be increased due to the
		//		expansion of the lam alef one character into lam+alef two characters
		// returns:
		//		text deshaped.
		if (text.length === 0) {
			return;
		}
		if (consumeNextSpace === undefined) {
			consumeNextSpace = true;
		}
		if (rtl === undefined) {
			rtl = true;
		}
		text = String(text);

		var outBuf = "", strFE = [];
		strFE = text.split("");
		for (var i = 0; i < text.length; i++) {
			var increase = false;
			if (strFE[i] >= "\uFE70" && strFE[i] < "\uFEFF") {
				var chNum = text.charCodeAt(i);
				if (strFE[i] >= "\uFEF5" && strFE[i] <= "\uFEFC") {
					//expand the LamAlef
					if (rtl) {
						//Lam + Alef
						if (i > 0 && consumeNextSpace && strFE[i - 1] === " ") {
							outBuf = outBuf.substring(0, outBuf.length - 1) + "\u0644";
						} else {
							outBuf += "\u0644";
							increase = true;
						}
						outBuf += AlefTable[(chNum - 65269) / 2];
					} else {
						outBuf += AlefTable[(chNum - 65269) / 2];
						outBuf += "\u0644";
						if (i + 1 < text.length && consumeNextSpace && strFE[i + 1] === " ") {
							i++;
						} else {
							increase = true;
						}
					}
					if (increase) {
						updateMap(tsMap, i, true, 1);
						stMap.splice(i, 0, stMap[i]);
					}
				} else {
					outBuf += FETo06Table[chNum - 65136];
				}
			} else {
				outBuf += strFE[i];
			}
		}
		return outBuf;
	}

	function doReorder(str, bdx) {
		// summary:
		//		Helper to the doBidiReorder. Manages the UBA.
		// str:
		//		the string to reorder.
		// bdx: Object
		//		Used for intermediate data storage
		// returns:
		//		text reordered according to source and result attributes.
		// tags:
		//		private
		var chars = str.split(""), levels = [];

		computeLevels(chars, levels, bdx);
		swapChars(chars, levels, bdx);
		invertLevel(2, chars, levels, bdx);
		invertLevel(1, chars, levels, bdx);
		lvMap = levels;
		return chars.join("");
	}

	function computeLevels(chars, levels, bdx) {
		var len = chars.length,
			impTab = bdx.dir ? impTabRtl : impTabLtr,
			prevState = null, newClass = null, newLevel = null, newState = 0,
			action = null, cond = null, condPos = -1, i = null, ix = null,
			types = [],
			classes = [];
		bdx.hiLevel = bdx.dir;
		bdx.lastArabic = false;
		bdx.hasUbatAl = false;
		bdx.hasUbatB = false;
		bdx.hasUbatS = false;
		for (i = 0; i < len; i++) {
			types[i] = getCharacterType(chars[i]);
		}
		for (ix = 0; ix < len; ix++) {
			prevState = newState;
			classes[ix] = newClass = getCharClass(chars, types, classes, ix, bdx);
			newState = impTab[prevState][newClass];
			action = newState & 0xF0;
			newState &= 0x0F;
			levels[ix] = newLevel = impTab[newState][ITIL];
			if (action > 0) {
				if (action === 0x10) {	// set conditional run to level 1
					for (i = condPos; i < ix; i++) {
						levels[i] = 1;
					}
					condPos = -1;
				} else {	// 0x20 confirm the conditional run
					condPos = -1;
				}
			}
			cond = impTab[newState][ITCOND];
			if (cond) {
				if (condPos === -1) {
					condPos = ix;
				}
			} else {	// unconditional level
				if (condPos > -1) {
					for (i = condPos; i < ix; i++) {
						levels[i] = newLevel;
					}
					condPos = -1;
				}
			}
			if (types[ix] === UBAT_B) {
				levels[ix] = 0;
			}
			bdx.hiLevel |= newLevel;
		}
		if (bdx.hasUbatS) {
			handleUbatS(types, levels, len, bdx);
		}
	}

	function handleUbatS(types, levels, len, bdx) {
		for (var i = 0; i < len; i++) {
			if (types[i] === UBAT_S) {
				levels[i] = bdx.dir;
				for (var j = i - 1; j >= 0; j--) {
					if (types[j] === UBAT_WS) {
						levels[j] = bdx.dir;
					} else {
						break;
					}
				}
			}
		}
	}

	function swapChars(chars, levels, bdx) {
		// summary:
		//		Swap characters with symmetrical mirroring as all kinds of parenthesis.
		//		(When needed).
		// chars:
		//		The source string as Array of characters.
		// levels:
		//		An array (like hash) of flags for each character in the source string,
		//		that defines if swapping should be applied on the following character.
		// bdx: Object
		//		Used for intermediate data storage
		// tags:
		//		private

		if (bdx.hiLevel === 0 || bdx.swap.substr(0, 1) === bdx.swap.substr(1, 2)) {
			return;
		}
		for (var i = 0; i < chars.length; i++) {
			if (levels[i] === 1) {
				chars[i] = getMirror(chars[i]);
			}
		}
	}

	function getCharacterType(ch) {
		// summary:
		//		Return the type of the character.
		// ch:
		//		The character to be checked.

		// description:
		//		Check the type of the character according to MasterTable,
		//		type = LTR, RTL, neutral,Arabic-Indic digit etc.
		// tags:
		//		private
		var uc = ch.charCodeAt(0),
			hi = MasterTable[uc >> 8];
		return (hi < TBBASE) ? hi : UnicodeTable[hi - TBBASE][uc & 0xFF];
	}

	function invertStr(str, bdx) {
		// summary:
		//		Return the reversed string.
		// str:
		//		The string to be reversed.
		// description:
		//		Reverse the string str.
		// tags:
		//		private
		var chars = str.split("");
		if (bdx) {
			var levels = [];
			computeLevels(chars, levels, bdx);
			lvMap = levels;
		}
		chars.reverse();
		stMap.reverse();
		return chars.join("");
	}

	function indexOf(cArray, cLength, idx) {
		for (var i = 0; i < cLength; i++) {
			if (cArray[i] === idx) {
				return i;
			}
		}
		return -1;
	}

	function isArabicAlefbet(c) {
		for (var i = 0; i < ArabicAlefBetIntervalsBegine.length; i++) {
			if (c >= ArabicAlefBetIntervalsBegine[i] && c <= ArabicAlefBetIntervalsEnd[i]) {
				return true;
			}
		}
		return false;
	}

	function isNextArabic(str06, index, step, nIEnd) {
		while (((index) * step) < nIEnd && isArabicDiacritics(str06[index])) {
			index += step;
		}
		if (((index) * step) < nIEnd && isArabicAlefbet(str06[index])) {
			return true;
		}
		return false;
	}

	function isNextAlef(str06, index, step, nIEnd) {
		while (((index) * step) < nIEnd && isArabicDiacritics(str06[index])) {
			index += step;
		}
		var c = " ";
		if (((index) * step) < nIEnd) {
			c = str06[index];
		} else {
			return false;
		}
		for (var i = 0; i < AlefTable.length; i++) {
			if (AlefTable[i] === c) {
				return true;
			}
		}
		return false;
	}

	function invertLevel(lev, chars, levels, bdx) {
		if (bdx.hiLevel < lev) {
			return;
		}
		if (lev === 1 && bdx.dir === RTL && !bdx.hasUbatB) {
			chars.reverse();
			stMap.reverse();
			return;
		}
		var len = chars.length, start = 0, end, lo, hi, tmp;
		while (start < len) {
			if (levels[start] >= lev) {
				end = start + 1;
				while (end < len && levels[end] >= lev) {
					end++;
				}
				for (lo = start, hi = end - 1 ; lo < hi; lo++, hi--) {
					tmp = chars[lo];
					chars[lo] = chars[hi];
					chars[hi] = tmp;
					tmp = stMap[lo];
					stMap[lo] = stMap[hi];
					stMap[hi] = tmp;
				}
				start = end;
			}
			start++;
		}
	}

	function getCharClass(chars, types, classes, ix, bdx) {
		// summary:
		//		Return the class if ix character in chars.
		// chars:
		//		The source string as Array of characters.
		// types:
		//		Array of types, for each character in chars.
		// classes:
		//		Array of classes that already been solved.
		// ix:
		//		the index of checked character.
		// bdx: Object
		//		Used for intermediate data storage
		// tags:
		//		private
		var cType = types[ix],
			results = {
				UBAT_L : function () { bdx.lastArabic = false; return UBAT_L; },
				UBAT_R : function () { bdx.lastArabic = false; return UBAT_R; },
				UBAT_ON : function () { return UBAT_ON; },
				UBAT_AN : function () { return UBAT_AN; },
				UBAT_EN : function () { return bdx.lastArabic ? UBAT_AN : UBAT_EN; },
				UBAT_AL : function () { bdx.lastArabic = true; bdx.hasUbatAl = true; return UBAT_R; },
				UBAT_WS : function () { return UBAT_ON; },
				UBAT_CS : function () {
										var wType, nType;
										if (ix < 1 || (ix + 1) >= types.length ||
											((wType = classes[ix - 1]) !== UBAT_EN && wType !== UBAT_AN) ||
											((nType = types[ix + 1]) !== UBAT_EN && nType !== UBAT_AN)) {
											return UBAT_ON;
										}
										if (bdx.lastArabic) {
											nType = UBAT_AN;
										}
										return nType === wType ? nType : UBAT_ON;
									},
				UBAT_ES : function () {
										var wType = ix > 0 ? classes[ix - 1] : UBAT_B;
										if (wType === UBAT_EN && (ix + 1) < types.length && types[ix + 1] === UBAT_EN) {
											return UBAT_EN;
										}
										return UBAT_ON;
									},
				UBAT_ET : function () {
										if (ix > 0 && classes[ix - 1] === UBAT_EN) {
											return UBAT_EN;
										}
										if (bdx.lastArabic) {
											return UBAT_ON;
										}
										var i = ix + 1,
											len = types.length;
										while (i < len && types[i] === UBAT_ET) {
											i++;
										}
										if (i < len && types[i] === UBAT_EN) {
											return UBAT_EN;
										}
										return UBAT_ON;
									},
				UBAT_NSM : function () {
										if (bdx.inFormat === "VLTR") {	// visual to implicit transformation
											var len = types.length,
												i = ix + 1;
											while (i < len && types[i] === UBAT_NSM) {
												i++;
											}
											if (i < len) {
												var c = chars[ix],
													rtlCandidate = (c >= 0x0591 && c <= 0x08FF) || c === 0xFB1E,
													wType = types[i];
												if (rtlCandidate && (wType === UBAT_R || wType === UBAT_AL)) {
													return UBAT_R;
												}
											}
										}
										if (ix < 1 || types[ix - 1] === UBAT_B) {
											return UBAT_ON;
										}
										return classes[ix - 1];
									},
				UBAT_B : function () { bdx.lastArabic = true; bdx.hasUbatB = true; return bdx.dir; },
				UBAT_S : function () { bdx.hasUbatS = true; return UBAT_ON; },
				UBAT_LRE : function () { bdx.lastArabic = false; return UBAT_ON; },
				UBAT_RLE : function () { bdx.lastArabic = false; return UBAT_ON; },
				UBAT_LRO : function () { bdx.lastArabic = false; return UBAT_ON; },
				UBAT_RLO : function () { bdx.lastArabic = false; return UBAT_ON; },
				UBAT_PDF : function () { bdx.lastArabic = false; return UBAT_ON; },
				UBAT_BN : function () { return UBAT_ON; }
			};
		return results[TYPES_NAMES[cType]]();
	}

	function getMirror(c) {
		// summary:
		//		Calculates the mirrored character of c
		// c:
		//		The character to be mirrored.
		// tags:
		//		private
		var mid, low = 0, high = SwapTable.length - 1;

		while (low <= high) {
			mid = Math.floor((low + high) / 2);
			if (c < SwapTable[mid][0]) {
				high = mid - 1;
			} else if (c > SwapTable[mid][0]) {
				low = mid + 1;
			} else {
				return SwapTable[mid][1];
			}
		}
		return c;
	}

	function isStandAlonCharacter(c) {
		for (var i = 0; i < StandAlonForm.length; i++) {
			if (StandAlonForm[i] === c) {
				return true;
			}
		}
		return false;
	}

	function getMedialFormCharacterFE(c) {
		for (var i = 0; i < BaseForm.length; i++) {
			if (c === BaseForm[i]) {
				return MedialForm[i];
			}
		}
		return c;
	}

	function getFormCharacterFE(/*char*/ c, /*char[]*/formArr) {
		for (var i = 0; i < BaseForm.length; i++) {
			if (c === BaseForm[i]) {
				return formArr[i];
			}
		}
		return c;
	}

	function isArabicDiacritics(c) {
		return	(c >= "\u064b" && c <= "\u0655") ? true : false;
	}

	function getOrientation(/*Char*/ oc) {
		if (oc === "L") {
			return "LTR";
		}
		if (oc === "R") {
			return "RTL";
		}
		if (oc === "C") {
			return "CLR";
		}
		if (oc === "D") {
			return "CRL";
		}
	}

	function setAlefToSpace(str06, index, step, nIEnd) {
		while (((index) * step) < nIEnd && isArabicDiacritics(str06[index])) {
			index += step;
		}
		if (((index) * step) < nIEnd) {
			str06[index] = " ";
			return true;
		}
		return false;
	}

	function getLamAlefFE(alef06, LamAlefForm) {
		for (var i = 0; i < AlefTable.length; i++) {
			if (alef06 === AlefTable[i]) {
				return LamAlefForm[i];
			}
		}
		return alef06;
	}

	function initMaps(map1, map2, length) {
		stMap = [];
		lvMap = [];
		for (var i = 0; i < length; i++) {
			map1[i] = i;
			map2[i] = i;
			stMap[i] = i;
		}
	}

	function reverseMap(sourceMap) {
		var map = new Array(sourceMap.length);
		for (var i = 0; i < sourceMap.length; i++) {
			map[sourceMap[i]] = i;
		}
		return map;
	}

	function updateMap(map, value, isGreater, update) {
		for (var i = 0; i < map.length; i++) {
			if (map[i] > value || (!isGreater && map[i] === value)) {
				map[i] += update;
			}
		}
	}

	var stMap = [];
	var tsMap = [];
	var lvMap = [];

	var	BDX = {
			dir: 0,
			defInFormat: "LLTR",
			defoutFormat: "VLTR",
			defSwap: "YN",
			inFormat: "LLTR",
			outFormat: "VLTR",
			swap: "YN",
			hiLevel: 0,
			lastArabic: false,
			hasUbatAl: false,
			hasBlockSep: false,
			hasSegSep: false
		};

	var ITIL = 5;

	var ITCOND = 6;

	var LTR = 0;

	var RTL = 1;

	var validFormat = /^[(I|V)][(L|R|C|D)][(Y|N)][(S|N)][N]$/;

	var bidiChars = /[\u0591-\u06ff\ufb1d-\ufefc]/;

	/****************************************************************************/
	/* Array in which directional characters are replaced by their symmetric.	*/
	/****************************************************************************/
	var SwapTable = [
		[ "\u0028", "\u0029" ],	/* Round brackets					*/
		[ "\u0029", "\u0028" ],
		[ "\u003C", "\u003E" ],	/* Less than/greater than			*/
		[ "\u003E", "\u003C" ],
		[ "\u005B", "\u005D" ],	/* Square brackets					*/
		[ "\u005D", "\u005B" ],
		[ "\u007B", "\u007D" ],	/* Curly brackets					*/
		[ "\u007D", "\u007B" ],
		[ "\u00AB", "\u00BB" ],	/* Double angle quotation marks		*/
		[ "\u00BB", "\u00AB" ],
		[ "\u2039", "\u203A" ],	/* single angle quotation mark		*/
		[ "\u203A", "\u2039" ],
		[ "\u207D", "\u207E" ],	/* Superscript parentheses			*/
		[ "\u207E", "\u207D" ],
		[ "\u208D", "\u208E" ],	/* Subscript parentheses			*/
		[ "\u208E", "\u208D" ],
		[ "\u2264", "\u2265" ],	/* Less/greater than or equal		*/
		[ "\u2265", "\u2264" ],
		[ "\u2329", "\u232A" ],	/* Angle brackets					*/
		[ "\u232A", "\u2329" ],
		[ "\uFE59", "\uFE5A" ],	/* Small round brackets				*/
		[ "\uFE5A", "\uFE59" ],
		[ "\uFE5B", "\uFE5C" ],	/* Small curly brackets				*/
		[ "\uFE5C", "\uFE5B" ],
		[ "\uFE5D", "\uFE5E" ],	/* Small tortoise shell brackets	*/
		[ "\uFE5E", "\uFE5D" ],
		[ "\uFE64", "\uFE65" ],	/* Small less than/greater than		*/
		[ "\uFE65", "\uFE64" ]
	];
	var AlefTable = ["\u0622", "\u0623", "\u0625", "\u0627"];

	var LamAlefInialTableFE = ["\ufef5", "\ufef7", "\ufef9", "\ufefb"];

	var LamAlefMedialTableFE = ["\ufef6", "\ufef8", "\ufefa", "\ufefc"];
	/**
	 * Arabic Characters in the base form
	 */
	var BaseForm = ["\u0627", "\u0628", "\u062A", "\u062B", "\u062C", "\u062D", "\u062E", "\u062F", "\u0630", "\u0631",
                    "\u0632", "\u0633", "\u0634", "\u0635", "\u0636", "\u0637", "\u0638", "\u0639", "\u063A", "\u0641",
                    "\u0642", "\u0643", "\u0644", "\u0645", "\u0646", "\u0647", "\u0648", "\u064A", "\u0625", "\u0623",
                    "\u0622", "\u0629", "\u0649", "\u0644", "\u0645", "\u0646", "\u0647", "\u0648", "\u064A", "\u0625",
                    "\u0623", "\u0622", "\u0629", "\u0649", "\u06CC", "\u0626", "\u0624"];

	/**
	 * Arabic shaped characters in Isolated form
	 */
	var IsolatedForm = ["\uFE8D", "\uFE8F", "\uFE95", "\uFE99", "\uFE9D", "\uFEA1", "\uFEA5", "\uFEA9", "\uFEAB",
                        "\uFEAD", "\uFEAF", "\uFEB1", "\uFEB5", "\uFEB9", "\uFEBD", "\uFEC1", "\uFEC5", "\uFEC9",
                        "\uFECD", "\uFED1", "\uFED5", "\uFED9", "\uFEDD", "\uFEE1", "\uFEE5", "\uFEE9", "\uFEED",
                        "\uFEF1", "\uFE87", "\uFE83", "\uFE81", "\uFE93", "\uFEEF", "\uFBFC", "\uFE89", "\uFE85",
                        "\uFE70", "\uFE72", "\uFE74", "\uFE76", "\uFE78", "\uFE7A", "\uFE7C", "\uFE7E", "\uFE80",
                        "\uFE89", "\uFE85"];

	/**
	 * Arabic shaped characters in Final form
	 */
	var FinalForm = ["\uFE8E", "\uFE90", "\uFE96", "\uFE9A", "\uFE9E", "\uFEA2", "\uFEA6", "\uFEAA", "\uFEAC", "\uFEAE",
                     "\uFEB0", "\uFEB2", "\uFEB6", "\uFEBA", "\uFEBE", "\uFEC2", "\uFEC6", "\uFECA", "\uFECE", "\uFED2",
                     "\uFED6", "\uFEDA", "\uFEDE", "\uFEE2", "\uFEE6", "\uFEEA", "\uFEEE", "\uFEF2", "\uFE88", "\uFE84",
                     "\uFE82", "\uFE94", "\uFEF0", "\uFBFD", "\uFE8A", "\uFE86", "\uFE70", "\uFE72", "\uFE74", "\uFE76",
                     "\uFE78", "\uFE7A", "\uFE7C", "\uFE7E", "\uFE80", "\uFE8A", "\uFE86"];

	/**
	 * Arabic shaped characters in Media form
	 */
	var MedialForm = ["\uFE8E", "\uFE92", "\uFE98", "\uFE9C", "\uFEA0", "\uFEA4", "\uFEA8", "\uFEAA", "\uFEAC",
                      "\uFEAE", "\uFEB0", "\uFEB4", "\uFEB8", "\uFEBC", "\uFEC0", "\uFEC4", "\uFEC8", "\uFECC",
                      "\uFED0", "\uFED4", "\uFED8", "\uFEDC", "\uFEE0", "\uFEE4", "\uFEE8", "\uFEEC", "\uFEEE",
                      "\uFEF4", "\uFE88", "\uFE84", "\uFE82", "\uFE94", "\uFEF0", "\uFBFF", "\uFE8C", "\uFE86",
                      "\uFE71", "\uFE72", "\uFE74", "\uFE77", "\uFE79", "\uFE7B", "\uFE7D", "\uFE7F", "\uFE80",
                      "\uFE8C", "\uFE86"];

	/**
	 * Arabic shaped characters in Initial form
	 */
	var InitialForm = ["\uFE8D", "\uFE91", "\uFE97", "\uFE9B", "\uFE9F", "\uFEA3", "\uFEA7", "\uFEA9", "\uFEAB",
                       "\uFEAD", "\uFEAF", "\uFEB3", "\uFEB7", "\uFEBB", "\uFEBF", "\uFEC3", "\uFEC7", "\uFECB",
                       "\uFECF", "\uFED3", "\uFED7", "\uFEDB", "\uFEDF", "\uFEE3", "\uFEE7", "\uFEEB", "\uFEED",
                       "\uFEF3", "\uFE87", "\uFE83", "\uFE81", "\uFE93", "\uFEEF", "\uFBFE", "\uFE8B", "\uFE85",
                       "\uFE70", "\uFE72", "\uFE74", "\uFE76", "\uFE78", "\uFE7A", "\uFE7C", "\uFE7E", "\uFE80",
                       "\uFE8B", "\uFE85"];

	/**
	 * Arabic characters that couldn't join to the next character
	 */
	var StandAlonForm = ["\u0621", "\u0622", "\u0623", "\u0624", "\u0625", "\u0627", "\u0629", "\u062F", "\u0630",
                         "\u0631", "\u0632", "\u0648", "\u0649"];

	var FETo06Table = ["\u064B", "\u064B", "\u064C", "\u061F", "\u064D", "\u061F", "\u064E", "\u064E", "\u064F",
                       "\u064F", "\u0650", "\u0650", "\u0651", "\u0651", "\u0652", "\u0652", "\u0621", "\u0622",
                       "\u0622", "\u0623", "\u0623", "\u0624", "\u0624", "\u0625", "\u0625", "\u0626", "\u0626",
                       "\u0626", "\u0626", "\u0627", "\u0627", "\u0628", "\u0628", "\u0628", "\u0628", "\u0629",
                       "\u0629", "\u062A", "\u062A", "\u062A", "\u062A", "\u062B", "\u062B", "\u062B", "\u062B",
                       "\u062C", "\u062C", "\u062C", "\u062c", "\u062D", "\u062D", "\u062D", "\u062D", "\u062E",
                       "\u062E", "\u062E", "\u062E", "\u062F", "\u062F", "\u0630", "\u0630", "\u0631", "\u0631",
                       "\u0632", "\u0632", "\u0633", "\u0633", "\u0633", "\u0633", "\u0634", "\u0634", "\u0634",
                       "\u0634", "\u0635", "\u0635", "\u0635", "\u0635", "\u0636", "\u0636", "\u0636", "\u0636",
                       "\u0637", "\u0637", "\u0637", "\u0637", "\u0638", "\u0638", "\u0638", "\u0638", "\u0639",
                       "\u0639", "\u0639", "\u0639", "\u063A", "\u063A", "\u063A", "\u063A", "\u0641", "\u0641",
                       "\u0641", "\u0641", "\u0642", "\u0642", "\u0642", "\u0642", "\u0643", "\u0643", "\u0643",
                       "\u0643", "\u0644", "\u0644", "\u0644", "\u0644", "\u0645", "\u0645", "\u0645", "\u0645",
                       "\u0646", "\u0646", "\u0646", "\u0646", "\u0647", "\u0647", "\u0647", "\u0647", "\u0648",
                       "\u0648", "\u0649", "\u0649", "\u064A", "\u064A", "\u064A", "\u064A", "\uFEF5", "\uFEF6",
                       "\uFEF7", "\uFEF8", "\uFEF9", "\uFEFA", "\uFEFB", "\uFEFC", "\u061F", "\u061F", "\u061F"];

	var ArabicAlefBetIntervalsBegine = ["\u0621", "\u0641"];

	var ArabicAlefBetIntervalsEnd = ["\u063A", "\u064a"];

	var	impTabLtr = [
	/*		L,		R,		EN,		AN,		N,		IL,		Cond */
		[	0,		3,		0,		1,		0,		0,		0	], /* 0 LTR text	*/
		[	0,		3,		0,		1,		2,		2,		0	], /* 1 LTR+AN		*/
		[	0,		3,		0,		0x11,	2,		0,		1	], /* 2 LTR+AN+N	*/
		[	0,		3,		5,		5,		4,		1,		0	], /* 3 RTL text	*/
		[	0,		3,		0x15,	0x15,	4,		0,		1	], /* 4 RTL cont	*/
		[	0,		3,		5,		5,		4,		2,		0	]  /* 5 RTL+EN/AN	*/
	];
	var impTabRtl = [
	/*		L,		R,		EN,		AN,		N,		IL,		Cond */
		[	2,		0,		1,		1,		0,		1,		0	], /* 0 RTL text	*/
		[	2,		0,		1,		1,		0,		2,		0	], /* 1 RTL+EN/AN	*/
		[	2,		0,		2,		1,		3,		2,		0	], /* 2 LTR text	*/
		[	2,		0,		2,		0x21,	3,		1,		1	]  /* 3 LTR+cont	*/
	];

	var UBAT_L	= 0; /* left to right				*/
	var UBAT_R	= 1; /* right to left				*/
	var UBAT_EN = 2; /* European digit				*/
	var UBAT_AN = 3; /* Arabic-Indic digit			*/
	var UBAT_ON = 4; /* neutral						*/
	var UBAT_B	= 5; /* block separator				*/
	var UBAT_S	= 6; /* segment separator			*/
	var UBAT_AL = 7; /* Arabic Letter				*/
	var UBAT_WS = 8; /* white space					*/
	var UBAT_CS = 9; /* common digit separator		*/
	var UBAT_ES = 10; /* European digit separator	*/
	var UBAT_ET = 11; /* European digit terminator	*/
	var UBAT_NSM = 12; /* Non Spacing Mark			*/
	var UBAT_LRE = 13; /* LRE						*/
	var UBAT_RLE = 14; /* RLE						*/
	var UBAT_PDF = 15; /* PDF						*/
	var UBAT_LRO = 16; /* LRO						*/
	var UBAT_RLO = 17; /* RLO						*/
	var UBAT_BN	= 18; /* Boundary Neutral			*/

	var TYPES_NAMES = [ "UBAT_L", "UBAT_R", "UBAT_EN", "UBAT_AN", "UBAT_ON", "UBAT_B", "UBAT_S", "UBAT_AL", "UBAT_WS",
						"UBAT_CS", "UBAT_ES", "UBAT_ET", "UBAT_NSM", "UBAT_LRE", "UBAT_RLE", "UBAT_PDF", "UBAT_LRO",
						"UBAT_RLO", "UBAT_BN" ];
	var TBBASE = 100;

	var TB00 = TBBASE + 0;
	var TB05 = TBBASE + 1;
	var TB06 = TBBASE + 2;
	var TB07 = TBBASE + 3;
	var TB20 = TBBASE + 4;
	var TBFB = TBBASE + 5;
	var TBFE = TBBASE + 6;
	var TBFF = TBBASE + 7;

	var L	= UBAT_L;
	var R	= UBAT_R;
	var EN	= UBAT_EN;
	var AN	= UBAT_AN;
	var ON	= UBAT_ON;
	var B	= UBAT_B;
	var S	= UBAT_S;
	var AL	= UBAT_AL;
	var WS	= UBAT_WS;
	var CS	= UBAT_CS;
	var ES	= UBAT_ES;
	var ET	= UBAT_ET;
	var NSM	= UBAT_NSM;
	var LRE	= UBAT_LRE;
	var RLE	= UBAT_RLE;
	var PDF	= UBAT_PDF;
	var LRO	= UBAT_LRO;
	var RLO	= UBAT_RLO;
	var BN	= UBAT_BN;

	var MasterTable = [
 /*******************************************************************************************************/
 /*     0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F     */
 /*******************************************************************************************************/
 /*0-*/ TB00, L,    L,    L,    L,    TB05, TB06, TB07, R,    L,    L,    L,    L,    L,    L,    L,
 /*1-*/ L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,
 /*2-*/ TB20, ON,   ON,   ON,   L,    ON,   L,    ON,   L,    ON,   ON,   ON,   L,    L,    ON,   ON,
 /*3-*/ L,    L,    L,    L,    L,    ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*4-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   L,    L,    ON,
 /*5-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*6-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*7-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*8-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*9-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   L,
 /*A-*/ L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    L,    ON,   ON,   ON,
 /*B-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*C-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*D-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   L,    L,    ON,   ON,   L,    L,    ON,   ON,   L,
 /*E-*/ L,    ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,
 /*F-*/ ON,   ON,   ON,   ON,   ON,   ON,   ON,   ON,   L,    L,    L,    TBFB, AL,   AL,   TBFE, TBFF
	];

	var UnicodeTable = [
        [ /*	Table 00: Unicode 00xx */
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  S,   B,   S,   WS,  B,   BN,  BN,
    /*1-*/  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  B,   B,   B,   S,
    /*2-*/  WS,  ON,  ON,  ET,  ET,  ET,  ON,  ON,  ON,  ON,  ON,  ES,  CS,  ES,  CS,  CS,
    /*3-*/  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  CS,  ON,  ON,  ON,  ON,  ON,
    /*4-*/  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*5-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  ON,
    /*6-*/  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*7-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  BN,
    /*8-*/  BN,  BN,  BN,  BN,  BN,  B,   BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,
    /*9-*/  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,  BN,
    /*A-*/  CS,  ON,  ET,  ET,  ET,  ET,  ON,  ON,  ON,  ON,  L,   ON,  ON,  BN,  ON,  ON,
    /*B-*/  ET,  ET,  EN,  EN,  ON,  L,   ON,  ON,  ON,  EN,  L,   ON,  ON,  ON,  ON,  ON,
    /*C-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*D-*/  L,   L,   L,   L,   L,   L,   L,   ON,  L,   L,   L,   L,   L,   L,   L,   L,
    /*E-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*F-*/  L,   L,   L,   L,   L,   L,   L,   ON,  L,   L,   L,   L,   L,   L,   L,   L
		],
		[ /*	Table 01: Unicode 05xx */
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*1-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*2-*/  L,   L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*3-*/  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*4-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*5-*/  L,   L,   L,   L,   L,   L,   L,   ON,  ON,  L,   L,   L,   L,   L,   L,   L,
    /*6-*/  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*7-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*8-*/  L,   L,   L,   L,   L,   L,   L,   L,   ON,  L,   ON,  ON,  ON,  ON,  ON,  ON,
    /*9-*/  ON,  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*A-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*B-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, R,   NSM,
    /*C-*/  R,   NSM, NSM, R,   NSM, NSM, R,   NSM, ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*D-*/  R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,
    /*E-*/  R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   ON,  ON,  ON,  ON,  ON,
    /*F-*/  R,   R,   R,   R,   R,   ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON
		],
		[ /*	Table 02: Unicode 06xx */
    /****************************************************************************************/
	/*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
	/****************************************************************************************/
    /*0-*/  AN,  AN,  AN,  AN,  ON,  ON,  ON,  ON,  AL,  ET,  ET,  AL,  CS,  AL,  ON,  ON,
    /*1-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, AL,  ON,  ON,  AL,  AL,
    /*2-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*3-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*4-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  NSM, NSM, NSM, NSM, NSM,
    /*5-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*6-*/  AN,  AN,  AN,  AN,  AN,  AN,  AN,  AN,  AN,  AN,  ET,  AN,  AN,  AL,  AL,  AL,
    /*7-*/  NSM, AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*8-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*9-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*A-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*B-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*C-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*D-*/  AL,  AL,  AL,  AL,  AL,  AL,  NSM, NSM, NSM, NSM, NSM, NSM, NSM, AN,  ON,  NSM,
    /*E-*/  NSM, NSM, NSM, NSM, NSM, AL,  AL,  NSM, NSM, ON,  NSM, NSM, NSM, NSM, AL,  AL,
    /*F-*/  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  AL,  AL,  AL,  AL,  AL,  AL
		],
		[	/*	Table	03:	Unicode	07xx	*/
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  ON,  AL,
    /*1-*/  AL,  NSM, AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*2-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*3-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*4-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, ON,  ON,  AL,  AL,  AL,
    /*5-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*6-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*7-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*8-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*9-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*A-*/  AL,  AL,  AL,  AL,  AL,  AL,  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*B-*/  NSM, AL,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*C-*/  R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,
    /*D-*/  R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,
    /*E-*/  R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   R,   NSM, NSM, NSM, NSM, NSM,
    /*F-*/  NSM, NSM, NSM, NSM, R,   R,   ON,  ON,  ON,  ON,  R,   ON,  ON,  ON,  ON,  ON
		],
		[	/*	Table	04:	Unicode	20xx	*/
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  WS,  WS,  WS,  WS,  WS,  WS,  WS,  WS,  WS,  WS,  WS,  BN,  BN,  BN,  L,   R,
    /*1-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*2-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  WS,  B,   LRE, RLE, PDF, LRO, RLO, CS,
    /*3-*/  ET,  ET,  ET,  ET,  ET,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*4-*/  ON,  ON,  ON,  ON,  CS,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*5-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  WS,
    /*6-*/  BN,  BN,  BN,  BN,  BN,  ON,  ON,  ON,  ON,  ON,  BN,  BN,  BN,  BN,  BN,  BN,
    /*7-*/  EN,  L,   ON,  ON,  EN,  EN,  EN,  EN,  EN,  EN,  ES,  ES,  ON,  ON,  ON,  L,
    /*8-*/  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  ES,  ES,  ON,  ON,  ON,  ON,
    /*9-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,
    /*A-*/  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,
    /*B-*/  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ET,  ON,  ON,  ON,  ON,  ON,  ON,
    /*C-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*D-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*E-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*F-*/  NSM, ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON
		],
		[	/*	Table	05:	Unicode	FBxx	*/
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*1-*/  ON,  ON,  ON,  L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  ON,  R,   NSM, R,
    /*2-*/  R,   R,   R,   R,   R,   R,   R,   R,   R,   ES,  R,   R,   R,   R,   R,   R,
    /*3-*/  R,   R,   R,   R,   R,   R,   R,   ON,  R,   R,   R,   R,   R,   ON,  R,   ON,
    /*4-*/  R,   R,   ON,  R,   R,   ON,  R,   R,   R,   R,   R,   R,   R,   R,   R,   R,
    /*5-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*6-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*7-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*8-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*9-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*A-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*B-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*C-*/  AL,  AL,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*D-*/  ON,  ON,  ON,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*E-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*F-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL
		],
		[	/*	Table	06:	Unicode	FExx	*/
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM, NSM,
    /*1-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*2-*/  NSM, NSM, NSM, NSM, NSM, NSM, NSM, ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*3-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*4-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*5-*/  CS,  ON,  CS,  ON,  ON,  CS,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ET,
    /*6-*/  ON,  ON,  ES,  ES,  ON,  ON,  ON,  ON,  ON,  ET,  ET,  ON,  ON,  ON,  ON,  ON,
    /*7-*/  AL,  AL,  AL,  AL,  AL,  ON,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*8-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*9-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*A-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*B-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*C-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*D-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*E-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,
    /*F-*/  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  ON,  ON,  BN
		],
		[	/*	Table	07:	Unicode	FFxx	*/
    /****************************************************************************************/
    /*      0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F	*/
    /****************************************************************************************/
    /*0-*/  ON,  ON,  ON,  ET,  ET,  ET,  ON,  ON,  ON,  ON,  ON,  ES,  CS,  ES,  CS,  CS,
    /*1-*/  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  EN,  CS,  ON,  ON,  ON,  ON,  ON,
    /*2-*/  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*3-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  ON,
    /*4-*/  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*5-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   ON,  ON,  ON,  ON,  ON,
    /*6-*/  ON,  ON,  ON,  ON,  ON,  ON,  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*7-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*8-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*9-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*A-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,
    /*B-*/  L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   L,   ON,
    /*C-*/  ON,  ON,  L,   L,   L,   L,   L,   L,   ON,  ON,  L,   L,   L,   L,   L,   L,
    /*D-*/  ON,  ON,  L,   L,   L,   L,   L,   L,   ON,  ON,  L,   L,   L,   ON,  ON,  ON,
    /*E-*/  ET,  ET,  ON,  ON,  ON,  ET,  ET,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,
    /*F-*/  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON,  ON
		]
	];

	return BidiEngine;
});
