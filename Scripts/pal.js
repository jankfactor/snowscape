// This script reads a palette file and generates a dithered image and a binary file with the dithered palette
// USAGE:
// node pal.js

const fs = require('fs');
const path = require('path');
const PNG = require('pngjs').PNG;
const colorConvert = require('color-convert');

const pngFileName = 'out_dithered.png';
const lookupFileName = 'lookup';

const bayerSize = 2;

const png = new PNG({
  width: 128 * bayerSize,
  height: 256 * bayerSize,
  filterType: -1,
});

const filePath = path.join(__dirname, 'palette.hex');

let writeLookupTable = process.argv.includes("lookup");
let loadExistingPng = !process.argv.includes("png");

// Always load the palette.
fs.readFile(filePath, 'utf8', (err, data) => {
  if (err) {
    console.error('Error reading the file:', err);
    return;
  }

  const lines = data.split('\n');
  const palette = lines.filter((line) => line.match(/^[0-9A-Fa-f]{6}\r?$/)).map(hexToRgb);

  if (palette.length !== 256) {
    console.error('The file does not contain 256 valid hex colors');
    return;
  }

  if (loadExistingPng) {
    console.log('Loading existing PNG file');
    // Load the existing PNG file
    const existingPng = PNG.sync.read(fs.readFileSync(pngFileName));
    for (let y = 0; y < existingPng.height; y++) {
      for (let x = 0; x < existingPng.width; x++) {
        const idx = (existingPng.width * y + x) << 2;
        png.data[idx] = existingPng.data[idx];
        png.data[idx + 1] = existingPng.data[idx + 1];
        png.data[idx + 2] = existingPng.data[idx + 2];
        png.data[idx + 3] = existingPng.data[idx + 3];
      }
    }
  }
  else { // Proceed to generate the image
    console.log('Generating new PNG file');

    const numSteps = 64;
    const closestColors = [];
    const targetColor = hexToRgb("#66aaee");

    // Create the color steps towards the target for each color in the palette
    palette.forEach((color) => {
      const colorSteps = [];

      for (let i = 0; i < numSteps; i++) {
        const lerpColor = {
          r: parseInt(Math.round((1 - i / (numSteps - 1)) * (color.r - targetColor.r) + targetColor.r)),
          g: parseInt(Math.round((1 - i / (numSteps - 1)) * (color.g - targetColor.g) + targetColor.g)),
          b: parseInt(Math.round((1 - i / (numSteps - 1)) * (color.b - targetColor.b) + targetColor.b)),
        };

        const closestColor = findNearestColor(lerpColor, palette);
        const withDetails = { r: closestColor.r, g: closestColor.g, b: closestColor.b, bayerThreshold: 0, nextColor: targetColor };
        colorSteps.push(withDetails);
      }

      closestColors.push(colorSteps);
    });

    for (let y = 0; y < closestColors.length; y++) {
      let colorSet = closestColors[y];
      let startIndex = 0;
      let currentValue = colorSet[0];

      for (let i = 0; i < colorSet.length; i++) {
        if (isColorDifferent(currentValue, colorSet[i])) {
          // find the halway point of the first section
          let halwayPoint1stSection = Math.floor((i - startIndex) / 2) + startIndex;

          // find the halway point of the second section
          let oldValue = currentValue;
          currentValue = colorSet[i];

          // Traverse the next section until we either find a color that is different or we reach the end
          let j = i;
          while (j < colorSet.length && !isColorDifferent(currentValue, colorSet[j])) {
            j++;
          }
          let halwayPoint2ndSection = Math.floor((j - i) / 2) + i;

          process.stdout.write(`i: ${i}, between ${startIndex} and ${i}, halwayPoint1stSection: ${halwayPoint1stSection}, halwayPoint2ndSection: ${halwayPoint2ndSection}\n`);

          // Go between the two halway points and set the bayer threshold across that range
          for (let k = halwayPoint1stSection; k <= halwayPoint2ndSection; k++) {
            const threshold = Math.round(((k - halwayPoint1stSection + 1) / (halwayPoint2ndSection - halwayPoint1stSection)) * 3);
            colorSet[k] = {
              r: oldValue.r,
              g: oldValue.g,
              b: oldValue.b,
              bayerThreshold: threshold,
              nextColor: currentValue,
            };
            process.stdout.write(`${colorSet[k].bayerThreshold}, `);
          }
          process.stdout.write(`\n--\n`);
          i = halwayPoint2ndSection;
          startIndex = halwayPoint2ndSection;
        }
      }
    }

    const bayerMatrix = [
      [0, 2],
      [3, 1],
      // [ 0,  8,  2, 10],
      // [12,  4, 14,  6],
      // [ 3, 11,  1,  9],
      // [15,  7, 13,  5]    
    ];

    console.assert(closestColors.length === 256, 'The closestColors array does not contain 256 colors');

    // Iterate over each row
    for (let y = 0; y < closestColors.length * bayerSize; y++) {
      const colorIndex = Math.floor(y / bayerSize) % palette.length;
      const spread = closestColors[colorIndex];

      // Iterate over each pixel in the row
      //for (let x = 0; x < spread.length * bayerSize; x++) {
      for (let x = 0; x < spread.length * bayerSize * 2; x++) {
        const newColor = closestColors[colorIndex][x >> 2];
        const nextColor = newColor.nextColor;
        const fraction = newColor.bayerThreshold;///(x * 5) / png.width;
        const idx = (png.width * y + x) << 2;

        // Map the x position to the range [0, 3] for the Bayer matrix
        const xBayer = x % 2;
        const yBayer = y % 2;
        const bayerValue = bayerMatrix[yBayer][xBayer];

        // Use the Bayer value to lerp between the baseColor and targetColor
        const lerpColor = fraction > bayerValue ? nextColor : newColor;

        png.data[idx] = lerpColor.r;
        png.data[idx + 1] = lerpColor.g;
        png.data[idx + 2] = lerpColor.b;
        png.data[idx + 3] = 255;
      }
    }

    png.pack().pipe(fs.createWriteStream(pngFileName));
  } 

  if (writeLookupTable) {
    console.log('Writing lookup table');
    pngToLookupTable(png, palette, lookupFileName);
  }
});

function pngToLookupTable(png, palette, outputFilePath) {
  // Open/Create a file
  let stream = fs.createWriteStream(outputFilePath);

  for (let y = 0; y < png.height; y++) {
    for (let x = 0; x < png.width; x++) {
      const idx = (png.width * y + x) << 2;
      const r = png.data[idx];
      const g = png.data[idx + 1];
      const b = png.data[idx + 2];
      const index = palette.findIndex((color) => color.r === r && color.g === g && color.b === b);
      if (index === -1) {
        console.error(`Could not find the color at (${x}, ${y})`);
        return;
      }
      const buffer = Buffer.alloc(1);
      buffer.writeUInt8(index, 0);
      stream.write(buffer);
    }
  }

  stream.end();
}

function pngToLookupTablePackedOrder(png, palette, outputFilePath) {
  // Open/Create a file
  let stream = fs.createWriteStream(outputFilePath);

  for (let y = 0; y < png.height; y += 2) {
    for (let x = 0; x < png.width; x += 4) {
      // Process 2 lines at a time
      for (let dy = 0; dy < 2; dy++) {
        if (y + dy >= png.height) {
          continue;
        }

        // Process 4 bytes at a time
        for (let dx = 0; dx < 4; dx++) {
          if (x + dx >= png.width) {
            continue;
          }

          const idx = (png.width * (y + dy) + (x + dx)) << 2;
          const r = png.data[idx];
          const g = png.data[idx + 1];
          const b = png.data[idx + 2];
          const index = palette.findIndex((color) => color.r === r && color.g === g && color.b === b);
          if (index === -1) {
            console.error(`Could not find the color at (${x + dx}, ${y + dy})`);
            return;
          }
          const buffer = Buffer.alloc(1);
          buffer.writeUInt8(index, 0);
          stream.write(buffer);
        }
      }
    }
  }

  stream.end();
}

function hexToRgb(hex) {
  const [r, g, b] = hex.replace('#', '').match(/.{1,2}/g).map(v => parseInt(v, 16));
  return { r, g, b };
}

function isColorDifferent(color1, color2) {
  return color1.r !== color2.r || color1.g !== color2.g || color1.b !== color2.b;
}
function colorDifference(color1, color2) {
  const lab1 = colorConvert.rgb.lab.raw(color1.r, color1.g, color1.b);
  const lab2 = colorConvert.rgb.lab.raw(color2.r, color2.g, color2.b);

  const lDifference = Math.abs(lab1[0] - lab2[0]) * 3;
  const aDifference = Math.abs(lab1[1] - lab2[1]);
  const bDifference = Math.abs(lab1[2] - lab2[2]);

  return lDifference + aDifference + bDifference;
}

function findNearestColor(color, palette) {
  let nearestColor = null;
  let smallestDifference = Infinity;

  for (const paletteColor of palette) {
    const difference = colorDifference(color, paletteColor);
    if (difference < smallestDifference) {
      smallestDifference = difference;
      nearestColor = paletteColor;
    }
  }

  return nearestColor;
}
