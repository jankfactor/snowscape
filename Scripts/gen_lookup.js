// prettier-ignore
const inputPalette = [
  0x000000,
  0x004060,
  0x206080,
  0x4080a0,
  0x80c0e0,
  0xe0e0e0,
  0x402000, 
  0x602000,
  0x804020,
  0xa06000,
  0xc08060,
  0x004000,
  0x006000,
  0xa02000,
  0xc0c000,
  0x20a0e0,
];

const rowLength = 64;
const bayerSize = 2; // Bayer pattern size (2x2)

const findNearestColor = (color, palette) => {
  const chroma = require("chroma-js");

  // Convert hex number to hex string for chroma-js
  const hexString = "#" + color.toString(16).padStart(6, "0");
  const inputColor = chroma(hexString);

  let closestColorIndex = 0;
  let minDistance = Infinity;

  for (let i = 0; i < palette.length; i++) {
    const paletteColor = palette[i];
    // Convert palette hex number to hex string
    const paletteHex = "#" + paletteColor.toString(16).padStart(6, "0");

    // Calculate color difference in the LAB color space (perceptually uniform)
    //const distance = chroma.distance(inputColor, chroma(paletteHex), "lab");
    const distance = chroma.deltaE(inputColor, chroma(paletteHex));

    if (distance < minDistance) {
      minDistance = distance;
      closestColorIndex = i;
    }
  }

  return closestColorIndex;
};

const testColor = 0x20a0e0;
const nearestColorIndex = findNearestColor(testColor, inputPalette);
console.log(
  `Nearest color index for ${testColor.toString(16)}: ${nearestColorIndex}`
);
console.log(`Nearest color: ${inputPalette[nearestColorIndex].toString(16)}`);

// Add PNG generation functionality
const fs = require("fs");
const PNG = require("pngjs").PNG;

// Generate a perceptually uniform gradient between two colors
const generateGradient = (startColor, endColor, steps) => {
  const chroma = require("chroma-js");

  // Convert hex numbers to hex strings for chroma-js
  const startHex = "#" + startColor.toString(16).padStart(6, "0");
  const endHex = "#" + endColor.toString(16).padStart(6, "0");

  // Create a perceptually uniform scale in LAB color space
  const scale = chroma
    .scale([startHex, endHex])
    .mode("lab") // Use LAB color space for perceptual uniformity
    .colors(steps, "hex");

  // Convert the hex strings back to numbers
  const gradient = scale.map((hex) => parseInt(hex.substring(1), 16));

  return gradient;
};

// Generate a row of the lookup table
const generateLookupRow = (row, palette, testColor) => {
  const paletteColor = palette[row % palette.length];
  // Generate a gradient from palette color to test color
  const gradientColors = generateGradient(paletteColor, testColor, rowLength);
  const rowIndices = [];

  // Fill the row with the gradient, using nearest palette colors
  for (let x = 0; x < rowLength; x++) {
    const gradientColor = gradientColors[x];
    const nearestIndex = findNearestColor(gradientColor, palette);
    rowIndices.push(nearestIndex);
  }

  return rowIndices;
};

const dither2x2 = [
  [0.0, 0.75],
  [0.5, 0.25],
];

const dither4x4 = [
  [0.0 / 16.0, 8.0 / 16.0, 2.0 / 16.0, 10.0 / 16.0],
  [12.0 / 16.0, 4.0 / 16.0, 14.0 / 16.0, 6.0 / 16.0],
  [3.0 / 16.0, 11.0 / 16.0, 1.0 / 16.0, 9.0 / 16.0],
  [15.0 / 16.0, 7.0 / 16.0, 13.0 / 16.0, 5.0 / 16.0],
];

const dither = (x, y, size) => {
  switch (size) {
    case 2:
      return dither2x2[y][x];
    case 4:
      return dither4x4[y][x];
    default:
      throw new Error("Unsupported dither size");
  }
};

const drawBayerPattern = (x, y, intensity, color1, color2, png) => {
  // Enough pixels to use a 2x2 bayer dither pattern

  for (let dy = 0; dy < bayerSize; dy++) {
    for (let dx = 0; dx < bayerSize; dx++) {
      const ditherValue = dither(dx, dy, bayerSize);
      const pixelX = x * bayerSize + dx;
      const pixelY = y * bayerSize + dy;

      // Choose color based on intensity
      const color = intensity <= ditherValue ? color1 : color2;

      // Set pixel color in the PNG
      png.data[(pixelY * png.width + pixelX) * 4] = (color >> 16) & 0xff; // Red
      png.data[(pixelY * png.width + pixelX) * 4 + 1] = (color >> 8) & 0xff; // Green
      png.data[(pixelY * png.width + pixelX) * 4 + 2] = color & 0xff; // Blue
      png.data[(pixelY * png.width + pixelX) * 4 + 3] = 0xff; // Alpha (fully opaque)
    }
  }
};

const generateLookupTablePNG = (palette, testColor) => {
  const height = inputPalette.length * bayerSize; // Number of rows in the lookup table
  const width = rowLength * bayerSize; // Number of columns in the lookup table
  const png = new PNG({ width, height });

  // Fill the PNG with the generated lookup table
  for (let y = 0; y < height; y++) {
    const rowIndices = generateLookupRow(y, palette, testColor);

    let prevColor = rowIndices.length - 1;
    let nextColor = rowIndices.length - 1;
    let col = rowIndices.length - 1;
    let firstBlock = true;

    while (col >= 0) {
      col--;

      // The color changed or we reached the start of the row
      if (col <= 0 || rowIndices[col] !== rowIndices[nextColor]) {
        prevColor = col;
        if (prevColor < 0) {
          prevColor = 0;
        }

        let spread = nextColor - prevColor;

        const color1 = rowIndices[prevColor];
        const color2 = rowIndices[nextColor];

        for (let x = prevColor; x <= nextColor; x++) {
          let intensity = (x - prevColor) / spread;

          if (firstBlock) {
            intensity = Math.min(intensity, 1.0 - (1.0 / (bayerSize * bayerSize)));
          }

          drawBayerPattern(
            x,
            y,
            intensity,
            palette[color1],
            palette[color2],
            png
          );
        }

        if (firstBlock) {
          firstBlock = false;
        }

        nextColor = prevColor;
      }
    }
  }

  return png;
};

// Generate and save the lookup table PNG
console.log("Generating color lookup table...");
const lookupTable = generateLookupTablePNG(inputPalette, testColor);
const pngBuffer = PNG.sync.write(lookupTable);
fs.writeFileSync("color_lookup.png", pngBuffer);
console.log("Lookup table saved as color_lookup.png");
