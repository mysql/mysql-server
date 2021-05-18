define("dojox/encoding/digests/_sha-32", ["./_base"], function(base){
	//	basic functions for 32-bit word based SHA-2 processing.

	//	create a new object that uses a delegated base.
	var o = (function(b){
		var tmp = function(){};
		tmp.prototype = b;
		var ret = new tmp();
		return ret;
	})(base);

	//	expose the output type conversion functions
	o.toWord = function(s){
		var wa = Array(s.length >> 2);
		for(var i=0; i<wa.length; i++) wa[i] = 0;
		for(var i=0; i<s.length*8; i+=8)
			wa[i>>5] |= (s.charCodeAt(i/8)&0xFF)<<(24-i%32);
		return wa;
	};

	o.toHex = function(wa){
		var h="0123456789abcdef", s=[];
		for(var i=0, l=wa.length*4; i<l; i++){
			s.push(h.charAt((wa[i>>2]>>((3-i%4)*8+4))&0xF), h.charAt((wa[i>>2]>>((3-i%4)*8))&0xF));
		}
		return s.join("");	//	string
	};

	o.toBase64 = function(wa){
		var p="=", tab="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", s=[];
		for(var i=0, l=wa.length*4; i<l; i+=3){
			var t=(((wa[i>>2]>>8*(3-i%4))&0xFF)<<16)|(((wa[i+1>>2]>>8*(3-(i+1)%4))&0xFF)<<8)|((wa[i+2>>2]>>8*(3-(i+2)%4))&0xFF);
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

	o._toString = function(wa){
		var s = "";
		for(var i=0; i<wa.length*32; i+=8)
			s += String.fromCharCode((wa[i>>5]>>>(24-i%32))&0xFF);
		return s;
	};

	//	the encoding functions
	function S (X, n) {return ( X >>> n ) | (X << (32 - n));}
	function R (X, n) {return ( X >>> n );}
	function Ch(x, y, z) {return ((x & y) ^ ((~x) & z));}
	function Maj(x, y, z) {return ((x & y) ^ (x & z) ^ (y & z));}
	function Sigma0256(x) {return (S(x, 2) ^ S(x, 13) ^ S(x, 22));}
	function Sigma1256(x) {return (S(x, 6) ^ S(x, 11) ^ S(x, 25));}
	function Gamma0256(x) {return (S(x, 7) ^ S(x, 18) ^ R(x, 3));}
	function Gamma1256(x) {return (S(x, 17) ^ S(x, 19) ^ R(x, 10));}
	function Sigma0512(x) {return (S(x, 28) ^ S(x, 34) ^ S(x, 39));}
	function Sigma1512(x) {return (S(x, 14) ^ S(x, 18) ^ S(x, 41));}
	function Gamma0512(x) {return (S(x, 1)  ^ S(x, 8) ^ R(x, 7));}
	function Gamma1512(x) {return (S(x, 19) ^ S(x, 61) ^ R(x, 6));}

	//	math alias
	var add = base.addWords;

	//	constant K array
	var K = [
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	];

	//	the exposed function, used internally by SHA-224 and SHA-256
	o.digest = function(msg, length, hash, depth){
		//	clone the hash
		hash = hash.slice(0);

		var w = new Array(64);
		var a, b, c, d, e, f, g, h;
		var i, j, T1, T2;

		//	append padding
		msg[length >> 5] |= 0x80 << (24 - length % 32);
		msg[((length + 64 >> 9) << 4) + 15] = length;

		//	do the digest
		for(i = 0; i < msg.length; i += 16){
			a = hash[0];
			b = hash[1];
			c = hash[2];
			d = hash[3];
			e = hash[4];
			f = hash[5];
			g = hash[6];
			h = hash[7];

			for(j = 0; j < 64; j++) {
				if (j < 16){
					w[j] = msg[j + i];
				} else { 
					w[j] = add(add(add(Gamma1256(w[j - 2]), w[j - 7]), Gamma0256(w[j - 15])), w[j - 16]);
				}

				T1 = add(add(add(add(h, Sigma1256(e)), Ch(e, f, g)), K[j]), w[j]);
				T2 = add(Sigma0256(a), Maj(a, b, c));
				h = g;
				g = f;
				f = e;
				e = add(d, T1);
				d = c;
				c = b;
				b = a;
				a = add(T1, T2);
			}

			hash[0] = add(a, hash[0]);
			hash[1] = add(b, hash[1]);
			hash[2] = add(c, hash[2]);
			hash[3] = add(d, hash[3]);
			hash[4] = add(e, hash[4]);
			hash[5] = add(f, hash[5]);
			hash[6] = add(g, hash[6]);
			hash[7] = add(h, hash[7]);
		}
		if(depth == 224){
			hash.pop();	//	take off the last word
		}
		return hash;
	}
	return o;
});
