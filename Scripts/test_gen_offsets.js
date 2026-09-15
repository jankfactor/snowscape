const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const { createHash } = require('node:crypto');
const { PNG } = require('pngjs');

const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'offsets-test-'));
try {
    // Decoded pixel hashes from the original encoder; compression may change.
    for (const [args, hash] of [
        [[], '85e54eeac5efa570c8091c60decbb0d86cf6f297a72aefe5a0148795ab814008'],
        [['0'], 'd5a52d951e2be36636910a0f7920349a7f6480c81c78514f5c30852b7f89e6ac'],
        [['snow'], '4014bb70c7af20cc75fa75cdfd1aee6006be759d7ec365ad34b0018b9769a080'],
        [['4294967296'], 'd5a52d951e2be36636910a0f7920349a7f6480c81c78514f5c30852b7f89e6ac'],
        [[''], '65b8f9f918f128d3653533b8bb3edb1a902b6c4739d9d0b23315ee47c9b4df26'],
    ]) {
        execFileSync(process.execPath, [path.join(__dirname, 'gen_offsets.js'), ...args], { cwd: dir });
        const png = PNG.sync.read(fs.readFileSync(path.join(dir, 'offsets.png')));
        assert.deepEqual([png.width, png.height, png.depth, png.colorType, png.alpha], [50, 50, 8, 0, false]);
        assert.equal(createHash('sha256').update(png.data).digest('hex'), hash);
    }
    console.log('Offset PNG checks passed');
} finally {
    fs.rmSync(dir, { recursive: true, force: true });
}
