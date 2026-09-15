const fs = require("node:fs");
const path = require("node:path");
const { parseArgs } = require("node:util");
const { PNG } = require("pngjs");

const usage = `Usage:
  node Scripts/gen_zbuffer.js unpack ZBUFFER.BIN heights.png offsets.png
  node Scripts/gen_zbuffer.js pack heights.png offsets.png ZBUFFER.BIN

Packs/unpacks a 10,000-byte ZBUFFER asset using greyscale PNGs without alpha.
  heights.png: 50x50, 8-bit; height = pixel * 32 - 512 (sea level: pixel 16).
  offsets.png: 50x50, 8-bit; sample = high byte of the detail/hash word.
Unpack discards the low hash byte; pack always writes 0x04 for that byte.
Unpack rounds heights to 32-unit steps and clamps them to -512..7648.
Preserves row order. Overwrites existing output files.
  --help  Show this help`;

function readGreyscale(filename) {
  const png = PNG.sync.read(fs.readFileSync(filename));
  if (png.width !== 50 || png.height !== 50 || png.depth !== 8 ||
      png.colorType !== 0 || png.alpha) {
    throw new Error(`${filename}: expected 50x50, 8-bit greyscale PNG without transparency`);
  }
  return png.data; // pngjs expands samples to RGBA without changing their depth.
}

try {
  const { values, positionals } = parseArgs({
    allowPositionals: true,
    options: { help: { type: "boolean" } },
  });
  if (values.help) {
    console.log(usage);
  } else {
    const [mode, ...args] = positionals;
    if (args.length !== 3 || !["pack", "unpack"].includes(mode)) throw new Error(usage);
    const filenames = args.map(filename => fs.existsSync(filename)
      ? fs.realpathSync(filename)
      : path.join(fs.realpathSync(path.dirname(filename)), path.basename(filename)));
    if (new Set(filenames).size !== 3) {
      throw new Error("Binary, height image and offset image must be different files");
    }
    if (mode === "pack") {
      const heights = readGreyscale(filenames[0]);
      const offsets = readGreyscale(filenames[1]);
      const bytes = Buffer.alloc(10000);
      for (let i = 0; i < 2500; ++i) {
        bytes.writeInt16BE(heights[i * 4] * 32 - 512, i * 4);
        bytes[i * 4 + 2] = offsets[i * 4];
        bytes[i * 4 + 3] = 4;
      }
      fs.writeFileSync(filenames[2], bytes);
      console.log(`Wrote ${args[2]} (${bytes.length} bytes)`);
    } else {
      const bytes = fs.readFileSync(filenames[0]);
      if (bytes.length !== 10000) {
        throw new Error(`Expected exactly 10000 bytes, got ${bytes.length}`);
      }
      const heights = Buffer.alloc(50 * 50);
      const offsets = Buffer.alloc(50 * 50);
      for (let i = 0; i < heights.length; ++i) {
        // ponytail: 8-bit heights quantise/clamp; use 16-bit images if exact source heights are needed.
        heights[i] = Math.max(0, Math.min(255, Math.round((bytes.readInt16BE(i * 4) + 512) / 32)));
        // ponytail: low hash byte is discarded; store both bytes if exact arbitrary round trips are needed.
        offsets[i] = bytes[i * 4 + 2];
      }
      const heightPNG = PNG.sync.write({ width: 50, height: 50, data: heights }, {
        colorType: 0, inputColorType: 0, bitDepth: 8,
      });
      const offsetPNG = PNG.sync.write({ width: 50, height: 50, data: offsets }, {
        colorType: 0, inputColorType: 0, bitDepth: 8,
      });
      fs.writeFileSync(filenames[1], heightPNG);
      fs.writeFileSync(filenames[2], offsetPNG);
      console.log(`Wrote ${args[1]} and ${args[2]} (both 50x50, 8-bit)`);
    }
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
