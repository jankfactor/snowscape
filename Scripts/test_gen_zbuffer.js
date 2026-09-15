const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const { spawnSync } = require("node:child_process");
const { PNG } = require("pngjs");

const dir = fs.mkdtempSync(path.join(os.tmpdir(), "zbuffer-test-"));
const input = path.join(dir, "ZBUFFER.BIN");
const output = path.join(dir, "ZBUFFER");
const heights = path.join(dir, "heights.png");
const offsets = path.join(dir, "offsets.png");
function run(...args) {
  const result = spawnSync(process.execPath, [path.join(__dirname, "gen_zbuffer.js"), ...args], {
    encoding: "utf8",
  });
  if (result.error) throw result.error;
  return result;
}

try {
  const bytes = Buffer.alloc(10000);
  for (let i = 0; i < 2500; ++i) {
    bytes.writeInt16BE((i % 256) * 32 - 512, i * 4);
    bytes.writeUInt16BE(((i % 256) << 8) | 4, i * 4 + 2);
  }
  fs.writeFileSync(input, bytes);
  fs.writeFileSync(heights, Buffer.alloc(30000, 255));
  fs.writeFileSync(offsets, Buffer.alloc(30000, 255));
  const result = run("unpack", input, heights, offsets);
  assert.equal(result.status, 0, result.stderr);
  const heightPNG = fs.readFileSync(heights);
  const offsetPNG = fs.readFileSync(offsets);
  const h = PNG.sync.read(heightPNG);
  const d = PNG.sync.read(offsetPNG);
  assert.deepEqual([h.width, h.height, h.depth, h.colorType, h.alpha], [50, 50, 8, 0, false]);
  assert.deepEqual([d.width, d.height, d.depth, d.colorType, d.alpha], [50, 50, 8, 0, false]);
  for (let i = 0; i < 2500; ++i) {
    assert.equal(h.data[i * 4], i % 256);
    assert.equal(d.data[i * 4], i % 256);
  }
  fs.writeFileSync(output, Buffer.alloc(30000, 255));
  let packed = run("pack", heights, offsets, output);
  assert.equal(packed.status, 0, packed.stderr);
  assert.deepEqual(fs.readFileSync(output), bytes); // Exact for representable heights and low bytes of 4.
  assert.match(run("pack", heights, offsets, heights).stderr, /different files/);
  // Other low bytes are discarded on unpack and replaced with 4 on pack.
  const altered = Buffer.from(bytes);
  altered[3] = 0;
  altered[7] = 255;
  fs.writeFileSync(input, altered);
  assert.equal(run("unpack", input, heights, offsets).status, 0);
  packed = run("pack", heights, offsets, output);
  assert.equal(packed.status, 0, packed.stderr);
  assert.deepEqual(fs.readFileSync(output), bytes);
  const samples = [-32768, -513, -512, -497, -496, -1, 0, 1, 7648, 7649, 32767];
  samples.forEach((height, i) => altered.writeInt16BE(height, i * 4));
  fs.writeFileSync(input, altered);
  assert.equal(run("unpack", input, heights, offsets).status, 0);
  const quantised = PNG.sync.read(fs.readFileSync(heights));
  [0, 0, 0, 0, 1, 16, 16, 16, 255, 255, 255].forEach((pixel, i) => {
    assert.equal(quantised.data[i * 4], pixel);
  });
  fs.writeFileSync(heights, heightPNG);
  fs.writeFileSync(input, bytes);
  assert.deepEqual(fs.readFileSync(input), bytes);
  assert.ok(heightPNG.length < 30000 && offsetPNG.length < 30000);
  assert.match(run("unpack", input, input, offsets).stderr, /different files/);
  assert.match(run("unpack", input, heights, heights).stderr, /different files/);
  for (const length of [9999, 10001]) {
    fs.writeFileSync(input, Buffer.alloc(length));
    assert.match(run("unpack", input, heights, offsets).stderr, /Expected exactly 10000 bytes/);
    assert.deepEqual(fs.readFileSync(heights), heightPNG);
    assert.deepEqual(fs.readFileSync(offsets), offsetPNG);
  }
  for (const [filename, width, depth, colorType] of [
    [offsets, 100, 8, 0], [offsets, 50, 16, 0],
    [offsets, 50, 8, 2], [offsets, 50, 8, 4], [heights, 50, 16, 0],
  ]) {
    fs.writeFileSync(filename, PNG.sync.write({
      width, height: 50,
      data: depth === 16 ? new Uint16Array(width * 50 * 4) : Buffer.alloc(width * 50 * 4),
    }, { colorType, bitDepth: depth }));
    assert.match(run("pack", heights, offsets, output).stderr, /expected 50x50/);
    assert.deepEqual(fs.readFileSync(output), bytes);
    fs.writeFileSync(heights, heightPNG);
    fs.writeFileSync(offsets, offsetPNG);
  }
  fs.writeFileSync(offsets, "not a PNG");
  assert.notEqual(run("pack", heights, offsets, output).status, 0);
  assert.deepEqual(fs.readFileSync(output), bytes);
  assert.equal(run("--help").status, 0);
  assert.notEqual(run().status, 0);
  assert.notEqual(run("unpack", path.join(dir, "missing"), heights, offsets).status, 0);
  console.log("ZBUFFER pack/unpack checks passed");
} finally {
  fs.rmSync(dir, { recursive: true, force: true });
}
