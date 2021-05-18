define("dojox/encoding/digests/_sha-64", ["./_sha-32"], function(sha32){
	//	basic functions for 64-bit word based SHA-2 processing.  Includes
	//	a constructor for int64.  Relies on the sha32 base for encoding functions,
	//	but provides its own, making it easier for the user to not worry about 64-bit
	//	word handling.

	var int64 = function(h, l){ return { h: h, l: l }; };

	//	64-bit math functions
	function copy(dst, src){
		dst.h = src.h;
		dst.l = src.l;
	}

	//Right-rotates a 64-bit number by shift
	function rrot(dst, x, shift){
		dst.l = (x.l >>> shift) | (x.h << (32-shift));
		dst.h = (x.h >>> shift) | (x.l << (32-shift));
	}

	//Reverses the dwords of the source and then rotates right by shift.
	function revrrot(dst, x, shift){
		dst.l = (x.h >>> shift) | (x.l << (32-shift));
		dst.h = (x.l >>> shift) | (x.h << (32-shift));
	}

	//Bitwise-shifts right a 64-bit number by shift
	function shr(dst, x, shift){
		dst.l = (x.l >>> shift) | (x.h << (32-shift));
		dst.h = (x.h >>> shift);
	}

	//Adds two 64-bit numbers
	function add(dst, x, y){
		var w0 = (x.l & 0xffff) + (y.l & 0xffff);
		var w1 = (x.l >>> 16) + (y.l >>> 16) + (w0 >>> 16);
		var w2 = (x.h & 0xffff) + (y.h & 0xffff) + (w1 >>> 16);
		var w3 = (x.h >>> 16) + (y.h >>> 16) + (w2 >>> 16);
		dst.l = (w0 & 0xffff) | (w1 << 16);
		dst.h = (w2 & 0xffff) | (w3 << 16);
	}

	//Same, except with 4 addends. Works faster than adding them one by one.
	function add4(dst, a, b, c, d){
		var w0 = (a.l & 0xffff) + (b.l & 0xffff) + (c.l & 0xffff) + (d.l & 0xffff);
		var w1 = (a.l >>> 16) + (b.l >>> 16) + (c.l >>> 16) + (d.l >>> 16) + (w0 >>> 16);
		var w2 = (a.h & 0xffff) + (b.h & 0xffff) + (c.h & 0xffff) + (d.h & 0xffff) + (w1 >>> 16);
		var w3 = (a.h >>> 16) + (b.h >>> 16) + (c.h >>> 16) + (d.h >>> 16) + (w2 >>> 16);
		dst.l = (w0 & 0xffff) | (w1 << 16);
		dst.h = (w2 & 0xffff) | (w3 << 16);
	}

	//Same, except with 5 addends
	function add5(dst, a, b, c, d, e){
		var w0 = (a.l & 0xffff) + (b.l & 0xffff) + (c.l & 0xffff) + (d.l & 0xffff) + (e.l & 0xffff);
		var w1 = (a.l >>> 16) + (b.l >>> 16) + (c.l >>> 16) + (d.l >>> 16) + (e.l >>> 16) + (w0 >>> 16);
		var w2 = (a.h & 0xffff) + (b.h & 0xffff) + (c.h & 0xffff) + (d.h & 0xffff) + (e.h & 0xffff) + (w1 >>> 16);
		var w3 = (a.h >>> 16) + (b.h >>> 16) + (c.h >>> 16) + (d.h >>> 16) + (e.h >>> 16) + (w2 >>> 16);
		dst.l = (w0 & 0xffff) | (w1 << 16);
		dst.h = (w2 & 0xffff) | (w3 << 16);
	}

	//	constants
	var K = [
		int64(0x428a2f98, 0xd728ae22), int64(0x71374491, 0x23ef65cd), int64(0xb5c0fbcf, 0xec4d3b2f), int64(0xe9b5dba5, 0x8189dbbc), 
		int64(0x3956c25b, 0xf348b538), int64(0x59f111f1, 0xb605d019), int64(0x923f82a4, 0xaf194f9b), int64(0xab1c5ed5, 0xda6d8118), 
		int64(0xd807aa98, 0xa3030242), int64(0x12835b01, 0x45706fbe), int64(0x243185be, 0x4ee4b28c), int64(0x550c7dc3, 0xd5ffb4e2), 
		int64(0x72be5d74, 0xf27b896f), int64(0x80deb1fe, 0x3b1696b1), int64(0x9bdc06a7, 0x25c71235), int64(0xc19bf174, 0xcf692694), 
		int64(0xe49b69c1, 0x9ef14ad2), int64(0xefbe4786, 0x384f25e3), int64(0x0fc19dc6, 0x8b8cd5b5), int64(0x240ca1cc, 0x77ac9c65), 
		int64(0x2de92c6f, 0x592b0275), int64(0x4a7484aa, 0x6ea6e483), int64(0x5cb0a9dc, 0xbd41fbd4), int64(0x76f988da, 0x831153b5), 
		int64(0x983e5152, 0xee66dfab), int64(0xa831c66d, 0x2db43210), int64(0xb00327c8, 0x98fb213f), int64(0xbf597fc7, 0xbeef0ee4), 
		int64(0xc6e00bf3, 0x3da88fc2), int64(0xd5a79147, 0x930aa725), int64(0x06ca6351, 0xe003826f), int64(0x14292967, 0x0a0e6e70), 
		int64(0x27b70a85, 0x46d22ffc), int64(0x2e1b2138, 0x5c26c926), int64(0x4d2c6dfc, 0x5ac42aed), int64(0x53380d13, 0x9d95b3df), 
		int64(0x650a7354, 0x8baf63de), int64(0x766a0abb, 0x3c77b2a8), int64(0x81c2c92e, 0x47edaee6), int64(0x92722c85, 0x1482353b), 
		int64(0xa2bfe8a1, 0x4cf10364), int64(0xa81a664b, 0xbc423001), int64(0xc24b8b70, 0xd0f89791), int64(0xc76c51a3, 0x0654be30), 
		int64(0xd192e819, 0xd6ef5218), int64(0xd6990624, 0x5565a910), int64(0xf40e3585, 0x5771202a), int64(0x106aa070, 0x32bbd1b8), 
		int64(0x19a4c116, 0xb8d2d0c8), int64(0x1e376c08, 0x5141ab53), int64(0x2748774c, 0xdf8eeb99), int64(0x34b0bcb5, 0xe19b48a8), 
		int64(0x391c0cb3, 0xc5c95a63), int64(0x4ed8aa4a, 0xe3418acb), int64(0x5b9cca4f, 0x7763e373), int64(0x682e6ff3, 0xd6b2b8a3), 
		int64(0x748f82ee, 0x5defb2fc), int64(0x78a5636f, 0x43172f60), int64(0x84c87814, 0xa1f0ab72), int64(0x8cc70208, 0x1a6439ec), 
		int64(0x90befffa, 0x23631e28), int64(0xa4506ceb, 0xde82bde9), int64(0xbef9a3f7, 0xb2c67915), int64(0xc67178f2, 0xe372532b), 
		int64(0xca273ece, 0xea26619c), int64(0xd186b8c7, 0x21c0c207), int64(0xeada7dd6, 0xcde0eb1e), int64(0xf57d4f7f, 0xee6ed178), 
		int64(0x06f067aa, 0x72176fba), int64(0x0a637dc5, 0xa2c898a6), int64(0x113f9804, 0xbef90dae), int64(0x1b710b35, 0x131c471b), 
		int64(0x28db77f5, 0x23047d84), int64(0x32caab7b, 0x40c72493), int64(0x3c9ebe0a, 0x15c9bebc), int64(0x431d67c4, 0x9c100d4c), 
		int64(0x4cc5d4be, 0xcb3e42b6), int64(0x597f299c, 0xfc657e2a), int64(0x5fcb6fab, 0x3ad6faec), int64(0x6c44198c, 0x4a475817)
	];

	//	our return object
	var o = {
		outputTypes: sha32.outputTypes,
		stringToUtf8: function(s){ return sha32.stringToUtf8(s); },
		toWord: function(s){ return sha32.toWord(s); },
		toHex: function(wa){ return sha32.toHex(wa); },
		toBase64: function(wa){ return sha32.toBase64(wa); },
		_toString: function(wa){ return sha32._toString(wa); }
	};

	//	the main function
	o.digest = function(msg, length, hash, depth){
		//	prep the hash
		var HASH = [];
		for(var i=0, l=hash.length; i<l; i+=2){
			HASH.push(int64(hash[i], hash[i+1]));
		}

		//	initialize our variables
		var T1 = int64(0,0),
			T2 = int64(0,0),
			a = int64(0,0),
			b = int64(0,0),
			c = int64(0,0),
			d = int64(0,0),
			e = int64(0,0),
			f = int64(0,0),
			g = int64(0,0),
			h = int64(0,0),
			s0 = int64(0,0),
			s1 = int64(0,0),
			Ch = int64(0,0),
			Maj = int64(0,0),
			r1 = int64(0,0),
			r2 = int64(0,0),
			r3 = int64(0,0);
		var j, i;
		var w = new Array(80);
		for(i=0; i<80; i++) w[i] = int64(0, 0);

		// append padding to the source string.
		msg[length >> 5] |= 0x80 << (24 - (length & 0x1f));
		msg[((length + 128 >> 10)<< 5) + 31] = length;

		for(i=0; i<msg.length; i+=32){
			copy(a, HASH[0]);
			copy(b, HASH[1]);
			copy(c, HASH[2]);
			copy(d, HASH[3]);
			copy(e, HASH[4]);
			copy(f, HASH[5]);
			copy(g, HASH[6]);
			copy(h, HASH[7]);

			for(j=0; j<16; j++){
				w[j].h = msg[i + 2*j];
				w[j].l = msg[i + 2*j + 1];
			}

			for(j=16; j<80; j++){
				//sigma1
				rrot(r1, w[j-2], 19);
				revrrot(r2, w[j-2], 29);
				shr(r3, w[j-2], 6);
				s1.l = r1.l ^ r2.l ^ r3.l;
				s1.h = r1.h ^ r2.h ^ r3.h;

				//sigma0
				rrot(r1, w[j-15], 1);
				rrot(r2, w[j-15], 8);
				shr(r3, w[j-15], 7);
				s0.l = r1.l ^ r2.l ^ r3.l;
				s0.h = r1.h ^ r2.h ^ r3.h;

				add4(w[j], s1, w[j-7], s0, w[j-16]);
			}

			for(j = 0; j < 80; j++){
				//Ch
				Ch.l = (e.l & f.l) ^ (~e.l & g.l);
				Ch.h = (e.h & f.h) ^ (~e.h & g.h);

				//Sigma1
				rrot(r1, e, 14);
				rrot(r2, e, 18);
				revrrot(r3, e, 9);
				s1.l = r1.l ^ r2.l ^ r3.l;
				s1.h = r1.h ^ r2.h ^ r3.h;

				//Sigma0
				rrot(r1, a, 28);
				revrrot(r2, a, 2);
				revrrot(r3, a, 7);
				s0.l = r1.l ^ r2.l ^ r3.l;
				s0.h = r1.h ^ r2.h ^ r3.h;

				//Maj
				Maj.l = (a.l & b.l) ^ (a.l & c.l) ^ (b.l & c.l);
				Maj.h = (a.h & b.h) ^ (a.h & c.h) ^ (b.h & c.h);

				add5(T1, h, s1, Ch, K[j], w[j]);
				add(T2, s0, Maj);

				copy(h, g);
				copy(g, f);
				copy(f, e);
				add(e, d, T1);
				copy(d, c);
				copy(c, b);
				copy(b, a);
				add(a, T1, T2);
			}

			add(HASH[0], HASH[0], a);
			add(HASH[1], HASH[1], b);
			add(HASH[2], HASH[2], c);
			add(HASH[3], HASH[3], d);
			add(HASH[4], HASH[4], e);
			add(HASH[5], HASH[5], f);
			add(HASH[6], HASH[6], g);
			add(HASH[7], HASH[7], h);
		}
		
		//	convert the final hash back to 32-bit words
		var ret = [];
		if(depth == 384){ HASH.length = 6; }
		for(var i=0, l=HASH.length; i<l; i++){
			ret[i*2] = HASH[i].h;
			ret[i*2+1] = HASH[i].l;
		}
		return ret;
	};

	return o;
});
