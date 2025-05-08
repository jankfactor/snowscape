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
  const width = 32;
  const paletteColor = palette[row % palette.length];
  // Generate a gradient from palette color to test color
  const gradientColors = generateGradient(paletteColor, testColor, width);
  const rowIndices = [];

  // Fill the row with the gradient, using nearest palette colors
  for (let x = 0; x < width; x++) {
    const gradientColor = gradientColors[x];
    const nearestIndex = findNearestColor(gradientColor, palette);
    rowIndices.push(nearestIndex);
  }

  return rowIndices;
};

const drawBayerPattern = (x, y, intensity, color1, color2, png) => {
  // Enough pixels to use a 2x2 bayer dither pattern
  const dither = [
    [0.2, 0.8],
    [0.6, 0.4],
  ];

  for (let dy = 0; dy < 2; dy++) {
    for (let dx = 0; dx < 2; dx++) {
      const ditherValue = dither[dy][dx];
      const pixelX = x * 2 + dx;
      const pixelY = y * 2 + dy;

      // Choose color based on intensity
      const color = intensity > ditherValue ? color1 : color2;

      // Set pixel color in the PNG
      png.data[(pixelY * png.width + pixelX) * 4] = (color >> 16) & 0xff; // Red
      png.data[(pixelY * png.width + pixelX) * 4 + 1] = (color >> 8) & 0xff; // Green
      png.data[(pixelY * png.width + pixelX) * 4 + 2] = color & 0xff; // Blue
      png.data[(pixelY * png.width + pixelX) * 4 + 3] = 0xff; // Alpha (fully opaque)
    }
  }
};

// const draw2x2Block = (x, y, color, png) => {
//   // Draw a 2x2 block of the specified color
//   for (let dy = 0; dy < 2; dy++) {
//     for (let dx = 0; dx < 2; dx++) {
//       const pixelX = x * 2 + dx;
//       const pixelY = y * 2 + dy;

//       // Set pixel color in the PNG
//       png.data[(pixelY * png.width + pixelX) * 4] = (color >> 16) & 0xff; // Red
//       png.data[(pixelY * png.width + pixelX) * 4 + 1] = (color >> 8) & 0xff; // Green
//       png.data[(pixelY * png.width + pixelX) * 4 + 2] = color & 0xff; // Blue
//       png.data[(pixelY * png.width + pixelX) * 4 + 3] = 0xff; // Alpha (fully opaque)
//     }
//   }
// };

const generateLookupTablePNG = (palette, testColor) => {
  const height = 32; // Number of rows in the lookup table
  const width = 64; // Number of columns in the lookup table
  const png = new PNG({ width, height });

  // Fill the PNG with the generated lookup table
  for (let y = 0; y < height; y++) {
    const rowIndices = generateLookupRow(y, palette, testColor);

    let firstColor = 0;
    let nextColor = 0;
    let col = 0;

    while (col < rowIndices.length) {
      col++;

      // The color changed or we reached the end of the row
      if (
        col >= rowIndices.length ||
        rowIndices[col] !== rowIndices[firstColor]
      ) {
        nextColor = col;
        if (nextColor >= rowIndices.length) {
          nextColor = rowIndices.length - 1;
        }

        let spread = nextColor - firstColor;

        const color1 = rowIndices[firstColor];
        const color2 = rowIndices[nextColor];

        // if (spread > 4) {
          for (let x = firstColor; x < nextColor; x++) {
            const intensity = (x - firstColor) / spread;
            drawBayerPattern(
              x,
              y,
              intensity,
              palette[color2],
              palette[color1],
              png
            );
          }
          firstColor = nextColor;
        // } else {
        //   for (let x = firstColor; x < nextColor; x++) {
        //     draw2x2Block(x, y, palette[rowIndices[firstColor]], png);
        //   }
        //   firstColor = nextColor;
        // }
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
