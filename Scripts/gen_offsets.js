// gen_offsets.js
const fs = require('fs');
const { PNG } = require('pngjs');

const W = 50, H = 50;

// Different diagonal structure from the original.
const AX = 7, AY = 9;

// Usage: node gen_offsets.js 12345
const seedArg = process.argv[2] ?? '12345';

function seed32(s) {
    if (/^\d+$/.test(s))
        return Number(s) >>> 0;

    // FNV-1a, so string seeds work too.
    let h = 0x811c9dc5;
    for (let i = 0; i < s.length; ++i) {
        h ^= s.charCodeAt(i);
        h = Math.imul(h, 0x01000193);
    }
    return h >>> 0;
}

function makeRand(seed) {
    let state = seed >>> 0;

    return () => {
        state =
            (Math.imul(state, 1664525) + 1013904223) >>> 0;

        return state / 0x100000000;
    };
}

const rand = makeRand(seed32(seedArg));


// Generate permutation 0..255
const perm = Uint8Array.from(
    { length: 256 },
    (_, i) => i
);

for (let i = 255; i > 0; --i) {
    const j = (rand() * (i + 1)) | 0;

    const t = perm[i];
    perm[i] = perm[j];
    perm[j] = t;
}


// Generate pattern
const pixels = Buffer.allocUnsafe(W * H);

for (let y = 0, p = 0; y < H; ++y) {
    for (let x = 0; x < W; ++x) {
        pixels[p++] =
            perm[(AX * x + AY * y) & 255];
    }
}


fs.writeFileSync('offsets.png', PNG.sync.write({ width: W, height: H, data: pixels }, {
    colorType: 0, inputColorType: 0, bitDepth: 8,
}));

console.log(`wrote offsets.png, seed=${seedArg}`);
