// adapted from whitequark's murmurhash3, the only
// implementation that properly handled UTF-16 strings:
// https://github.com/whitequark/murmurhash3-js/blob/ae8d1695101efd3b1f6f423b050148f8ac4b3741/murmurhash3.js#L77-L115

function mul32(m, n) {
	var nlo = n & 0xffff;
	var nhi = n - nlo;
	return ((nhi * m | 0) + (nlo * m | 0)) | 0;
}

function murmurhash3_32(data, len, seed) {
	var c1 = 0xcc9e2d51, c2 = 0x1b873593;

	var h1 = seed;
	var roundedEnd = len & ~0x1;

	for (var i = 0; i < roundedEnd; i += 2) {
		var k1 = data.charCodeAt(i) | (data.charCodeAt(i + 1) << 16);

		k1 = this.mul32(k1, c1);
		k1 = ((k1 & 0x1ffff) << 15) | (k1 >>> 17);  // ROTL32(k1,15);
		k1 = this.mul32(k1, c2);

		h1 ^= k1;
		h1 = ((h1 & 0x7ffff) << 13) | (h1 >>> 19);  // ROTL32(h1,13);
		h1 = (h1 * 5 + 0xe6546b64) | 0;
	}

	if((len % 2) == 1) {
		k1 = data.charCodeAt(roundedEnd);
		k1 = this.mul32(k1, c1);
		k1 = ((k1 & 0x1ffff) << 15) | (k1 >>> 17);  // ROTL32(k1,15);
		k1 = this.mul32(k1, c2);
		h1 ^= k1;
	}

	// finalization
	h1 ^= (len << 1);

	// fmix(h1);
	h1 ^= h1 >>> 16;
	h1  = this.mul32(h1, 0x85ebca6b);
	h1 ^= h1 >>> 13;
	h1  = this.mul32(h1, 0xc2b2ae35);
	h1 ^= h1 >>> 16;

	return h1;
}
