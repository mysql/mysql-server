define(["./_sha-64", "./_sha2"], function(sha64, sha2){
	//	The 384-bit implementation of SHA-2
	
	//	Note that for 64-bit hashes, we're actually doing high-order, low-order, high-order, low-order.
	//	The 64-bit functions will assemble them into actual 64-bit "words".
	var hash = [
		0xcbbb9d5d, 0xc1059ed8, 0x629a292a, 0x367cd507, 0x9159015a, 0x3070dd17, 0x152fecd8, 0xf70e5939,
		0x67332667, 0xffc00b31, 0x8eb44a87, 0x68581511, 0xdb0c2e0d, 0x64f98fa7, 0x47b5481d, 0xbefa4fa4
	];

	return sha2(sha64, 384, 1024, hash);
});
