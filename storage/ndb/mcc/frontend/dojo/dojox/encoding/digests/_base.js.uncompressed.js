define("dojox/encoding/digests/_base", ["dojo/_base/lang"], function(lang){
	//	These functions are 32-bit word-based.  See _sha-64 for 64-bit word ops.
	var base = lang.getObject("dojox.encoding.digests", true);

	base.outputTypes={
		// summary:
		//		Enumeration for input and output encodings.
		Base64:0, Hex:1, String:2, Raw:3
	};

	//	word-based addition
	base.addWords=function(/* word */a, /* word */b){
		// summary:
		//		add a pair of words together with rollover
		var l=(a&0xFFFF)+(b&0xFFFF);
		var m=(a>>16)+(b>>16)+(l>>16);
		return (m<<16)|(l&0xFFFF);	//	word
	};

	//	word-based conversion method, for efficiency sake;
	//	most digests operate on words, and this should be faster
	//	than the encoding version (which works on bytes).
	var chrsz=8;	//	16 for Unicode
	var mask=(1<<chrsz)-1;

	base.stringToWord=function(/* string */s){
		// summary:
		//		convert a string to a word array
		var wa=[];
		for(var i=0, l=s.length*chrsz; i<l; i+=chrsz){
			wa[i>>5]|=(s.charCodeAt(i/chrsz)&mask)<<(i%32);
		}
		return wa;	//	word[]
	};

	base.wordToString=function(/* word[] */wa){
		// summary:
		//		convert an array of words to a string
		var s=[];
		for(var i=0, l=wa.length*32; i<l; i+=chrsz){
			s.push(String.fromCharCode((wa[i>>5]>>>(i%32))&mask));
		}
		return s.join("");	//	string
	};

	base.wordToHex=function(/* word[] */wa){
		// summary:
		//		convert an array of words to a hex tab
		var h="0123456789abcdef", s=[];
		for(var i=0, l=wa.length*4; i<l; i++){
			s.push(h.charAt((wa[i>>2]>>((i%4)*8+4))&0xF)+h.charAt((wa[i>>2]>>((i%4)*8))&0xF));
		}
		return s.join("");	//	string
	};

	base.wordToBase64=function(/* word[] */wa){
		// summary:
		//		convert an array of words to base64 encoding, should be more efficient
		//		than using dojox.encoding.base64
		var p="=", tab="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", s=[];
		for(var i=0, l=wa.length*4; i<l; i+=3){
			var t=(((wa[i>>2]>>8*(i%4))&0xFF)<<16)|(((wa[i+1>>2]>>8*((i+1)%4))&0xFF)<<8)|((wa[i+2>>2]>>8*((i+2)%4))&0xFF);
			for(var j=0; j<4; j++){
				if(i*8+j*6>wa.length*32){
					s.push(p);
				} else {
					s.push(tab.charAt((t>>6*(3-j))&0x3F));
				}
			}
		}
		return s.join("");	//	string
	};

	//	convert to UTF-8
	base.stringToUtf8 = function(input){
		var output = "";
		var i = -1;
		var x, y;

		while(++i < input.length){
			x = input.charCodeAt(i);
			y = i + 1 < input.length ? input.charCodeAt(i + 1) : 0;
			if(0xD800 <= x && x <= 0xDBFF && 0xDC00 <= y && y <= 0xDFFF){
				x = 0x10000 + ((x & 0x03FF) << 10) + (y & 0x03FF);
				i++;
			}

			if(x <= 0x7F)
				output += String.fromCharCode(x);
			else if(x <= 0x7FF)
				output += String.fromCharCode(0xC0 | ((x >>> 6) & 0x1F), 0x80 | (x & 0x3F));
			else if(x <= 0xFFFF)
				output += String.fromCharCode(0xE0 | ((x >>> 12) & 0x0F), 0x80 | ((x >>> 6) & 0x3F), 0x80 | (x & 0x3F));
			else if(x <= 0x1FFFFF)
				output += String.fromCharCode(0xF0 | ((x >>> 18) & 0x07), 0x80 | ((x >>> 12) & 0x3F), 0x80 | ((x >>> 6) & 0x3F), 0x80 | (x & 0x3F));
		}
		return output;
	};

	return base;
});
