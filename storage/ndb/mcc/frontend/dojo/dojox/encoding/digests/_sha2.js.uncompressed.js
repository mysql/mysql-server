define("dojox/encoding/digests/_sha2", [], function () {
	// Return a hashing function for a _sha helper (_sha-32 or _sha-64), a key length, a block size, and a set
	// of constants.
	return function (/* sha32/64 */_sha, /* Number */_keyLength, /* Blocksize */_blockSize, /* Array */_hash) {
		function hasher(/* String */data, /* sha32/64.outputTypes */outputType) {
			outputType = outputType || _sha.outputTypes.Base64;
			var wa = _sha.digest(_sha.toWord(data), data.length * 8, _hash, _keyLength);

			switch(outputType){
				case _sha.outputTypes.Raw: {
					return wa;
				}
				case _sha.outputTypes.Hex: {
					return _sha.toHex(wa);
				}
				case _sha.outputTypes.String: {
					return _sha._toString(wa);
				}
				default: {
					return _sha.toBase64(wa);
				}
			}
		}

		hasher.hmac = function (/* String */data, /* String */key, /* sha32/64.outputTypes? */outputType) {
			outputType = outputType || _sha.outputTypes.Base64;

			//	prepare the key
			var wa = _sha.toWord(key);
			if(wa.length > 16){
				wa = _sha.digest(wa, key.length * 8, _hash, _keyLength);
			}

			//	set up the pads
			var numWords = _blockSize / 32,
				ipad = new Array(numWords),
				opad = new Array(numWords);
			for(var i=0; i<numWords; i++){
				ipad[i] = wa[i] ^ 0x36363636;
				opad[i] = wa[i] ^ 0x5c5c5c5c;
			}

			//	make the final digest
			var r1 = _sha.digest(ipad.concat(_sha.toWord(data)), _blockSize + data.length * 8, _hash, _keyLength);
			var r2 = _sha.digest(opad.concat(r1), _blockSize + _keyLength, _hash, _keyLength);

			//	return the output
			switch(outputType){
				case _sha.outputTypes.Raw: {
					return r2;
				}
				case _sha.outputTypes.Hex: {
					return _sha.toHex(r2);
				}
				case _sha.outputTypes.String: {
					return _sha._toString(r2);
				}
				default: {
					return _sha.toBase64(r2);
				}
			}
		};

		// for backwards compatibility
		hasher._hmac = hasher.hmac;

		return hasher;
	};
});
