// prettier-ignore

// Original Amiga Palette, COLOR0-COLOR15 in same order.
const inputPalette = [
  0x000000,
  0xEEEEEE,
  0x22AAEE,
  0x004466,
  0x226688,
  0x4488AA,
  0x88CCEE,
  0x442200,
  0x662200,
  0x884422,
  0xCC8866,
  0x004400,
  0x006600,
  0xAA6600,
  0xAA2200,
  0xCCCC00,
];

const rowBlendIndices = [
  0, 3, 4, 5, 6, 1
];

const rowLength = 16;
const bayerSize = 2; // Bayer pattern size (2x2)
const rowBlendIntensities = [0.25, 0.5, 0.75];
const straightBlend = true;

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
const generateLookupRow = (paletteIndex, palette, testColor) => {
  const paletteColor = palette[paletteIndex];
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

const renderLookupRow = (paletteIndex, palette, testColor, useStraightBlend) => {
  const width = rowLength * bayerSize; // Number of columns in the lookup table
  const png = new PNG({ width, height: bayerSize });

  if (useStraightBlend) {
    const blendSteps = bayerSize * bayerSize;
    const duplicateCount = rowLength / blendSteps;

    for (let x = 0; x < rowLength; x++) {
      const blendStep = Math.floor(x / duplicateCount);
      drawBayerPattern(
        x,
        0,
        blendStep / blendSteps,
        palette[paletteIndex],
        testColor,
        png
      );
    }

    return png;
  }

  const rowIndices = generateLookupRow(paletteIndex, palette, testColor);

  let prevColor = rowIndices.length - 1;
  let nextColor = rowIndices.length - 1;
  let col = rowIndices.length - 1;
  let firstBlock = true;

  while (col >= 0) {
    col--;

    // The color changed or we reached the start of the row
    if (col <= 0 || rowIndices[col] !== rowIndices[nextColor]) {
      prevColor = Math.max(col, 0);

      const spread = nextColor - prevColor;
      const color1 = rowIndices[prevColor];
      const color2 = rowIndices[nextColor];

      for (let x = prevColor; x <= nextColor; x++) {
        let intensity = spread === 0 ? 0 : (x - prevColor) / spread;

        if (firstBlock) {
          intensity = Math.min(
            intensity,
            1.0 - 1.0 / (bayerSize * bayerSize)
          );
        }

        drawBayerPattern(
          x,
          0,
          intensity,
          palette[color1],
          palette[color2],
          png
        );
      }

      firstBlock = false;
      nextColor = prevColor;
    }
  }

  return png;
};

const copyLookupRow = (source, row, png) => {
  const rowOffset = row * bayerSize * png.width * 4;
  source.data.copy(png.data, rowOffset);
};

const drawBayerRowBlend = (row1, row2, row, intensity, png) => {
  const destinationY = row * bayerSize;

  for (let y = 0; y < bayerSize; y++) {
    for (let x = 0; x < png.width; x++) {
      const source = intensity <= dither(x % bayerSize, y, bayerSize)
        ? row1
        : row2;
      const sourceOffset = (y * png.width + x) * 4;
      const destinationOffset = ((destinationY + y) * png.width + x) * 4;

      for (let channel = 0; channel < 4; channel++) {
        png.data[destinationOffset + channel] = source.data[sourceOffset + channel];
      }
    }
  }
};

const generateLookupTablePNG = (
  palette,
  testColor,
  blendIndices,
  useStraightBlend
) => {
  const rowCount = blendIndices.length +
    (blendIndices.length - 1) * rowBlendIntensities.length;
  const height = rowCount * bayerSize;
  const width = rowLength * bayerSize;
  const png = new PNG({ width, height });
  const anchorRows = blendIndices.map((paletteIndex) =>
    renderLookupRow(paletteIndex, palette, testColor, useStraightBlend)
  );
  let outputRow = 0;

  copyLookupRow(anchorRows[0], outputRow++, png);
  for (let row = 0; row < anchorRows.length - 1; row++) {
    for (const intensity of rowBlendIntensities) {
      drawBayerRowBlend(
        anchorRows[row],
        anchorRows[row + 1],
        outputRow++,
        intensity,
        png
      );
    }
    copyLookupRow(anchorRows[row + 1], outputRow++, png);
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
const lookupTable = generateLookupTablePNG(
  inputPalette,
  testColor,
  rowBlendIndices,
  straightBlend
);
const lookupTableBuffer = exportPNGtoBinaryLookup(lookupTable);
console.log("Exporting lookup table to binary format...");
fs.writeFileSync("lookup9", lookupTableBuffer);
console.log("Lookup table saved as color_lookup.bin");
const pngBuffer = PNG.sync.write(lookupTable);
fs.writeFileSync("color_lookup.png", pngBuffer);
console.log("Lookup table saved as color_lookup.png");
