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

const rowLength = 16;
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

const fill2x2BlockDiagonal = (x, y, png) => {
  // Enough pixels to use a 2x2 bayer dither pattern

  for (let i = 0; i < bayerSize; i++) {
    const pixelX = x * bayerSize + i;
    const pixelY = y * bayerSize + i;
    const offsetY = (y - 1) * bayerSize + i;

    // Set pixel color in the PNG
    png.data[(offsetY * png.width + pixelX) * 4] = png.data[(pixelY * png.width + pixelX) * 4]
    png.data[(offsetY * png.width + pixelX) * 4 + 1] = png.data[(pixelY * png.width + pixelX) * 4 + 1]
    png.data[(offsetY * png.width + pixelX) * 4 + 2] = png.data[(pixelY * png.width + pixelX) * 4 + 2]
    png.data[(offsetY * png.width + pixelX) * 4 + 3] = png.data[(pixelY * png.width + pixelX) * 4 + 3]
  }
};

const generateLookupTablePNG = (palette, testColor) => {
  const height = inputPalette.length * bayerSize * 2; // Number of rows in the lookup table
  const width = rowLength * bayerSize; // Number of columns in the lookup table
  const png = new PNG({ width, height });

  // Fill the PNG with the generated lookup table
  for (let y = 0; y < height / 2; y++) {
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
            intensity = Math.min(
              intensity,
              1.0 - 1.0 / (bayerSize * bayerSize)
            );
          }

          drawBayerPattern(
            x,
            y * 2,
            intensity,
            palette[color1],
            palette[color2],
            png
          );

          if (y > 0) {
            fill2x2BlockDiagonal(
              x,
              y * 2,
              png
            );
          }

          // Fill out the second row of the Bayer pattern
          drawBayerPattern(
            x,
            y * 2 + 1,
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

/**
 * Exports PNG data to a binary lookup table format.
 * Each pixel is stored as a 4-bit index to the palette.
 * Two indices are packed into each byte (high nibble, low nibble).
 * Each byte is repeated 4 times for 32-bit register loading.
 */
const exportPNGtoBinaryLookup = (png) => {
  const width = png.width;
  const height = png.height;

  // Calculate original size (2 pixels per byte)
  const originalSize = Math.ceil((width * height) / 2);
  // Create buffer to hold binary data (each byte repeated 4 times)
  const bufferSize = originalSize * 4;
  const buffer = Buffer.alloc(bufferSize);

  let currentByte = 0;
  let bufferIndex = 0;
  let isHighNibble = true;

  // Process each pixel
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      // Get pixel color from PNG
      const idx = (y * width + x) << 2;
      const r = png.data[idx];
      const g = png.data[idx + 1];
      const b = png.data[idx + 2];

      // Convert RGB to hex format
      const pixelColor = (r << 16) | (g << 8) | b;

      // Try to find exact match first (faster)
      let paletteIndex = -1;
      for (let i = 0; i < inputPalette.length; i++) {
        if (inputPalette[i] === pixelColor) {
          paletteIndex = i;
          break;
        }
      }

      // If no exact match, use findNearestColor
      if (paletteIndex === -1) {
        paletteIndex = findNearestColor(pixelColor, inputPalette);
      }

      // Pack two indices per byte
      if (isHighNibble) {
        // First index goes in high nibble (most significant 4 bits)
        currentByte = paletteIndex << 4;
        isHighNibble = false;
      } else {
        // Second index goes in low nibble (least significant 4 bits)
        currentByte |= paletteIndex;

        isHighNibble = true;

        // Write the byte 4 times consecutively for 32-bit register loading
        for (let i = 0; i < 4; i++) {
          buffer[bufferIndex++] = currentByte;
        }
      }
    }
  }

  return buffer;
};

// Generate and save the lookup table PNG
console.log("Generating color lookup table...");
const lookupTable = generateLookupTablePNG(inputPalette, testColor);
const lookupTableBuffer = exportPNGtoBinaryLookup(lookupTable);
console.log("Exporting lookup table to binary format...");
fs.writeFileSync("lookup9", lookupTableBuffer);
console.log("Lookup table saved as color_lookup.bin");
const pngBuffer = PNG.sync.write(lookupTable);
fs.writeFileSync("color_lookup.png", pngBuffer);
console.log("Lookup table saved as color_lookup.png");
