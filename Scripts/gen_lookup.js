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
    const chroma = require('chroma-js');
    
    // Convert hex number to hex string for chroma-js
    const hexString = '#' + color.toString(16).padStart(6, '0');
    const inputColor = chroma(hexString);
    
    let closestColorIndex = 0;
    let minDistance = Infinity;
    
    for (let i = 0; i < palette.length; i++) {
        const paletteColor = palette[i];
        // Convert palette hex number to hex string
        const paletteHex = '#' + paletteColor.toString(16).padStart(6, '0');
        
        // Calculate color difference in the LAB color space (perceptually uniform)
        const distance = chroma.distance(inputColor, chroma(paletteHex), 'lab');
        
        if (distance < minDistance) {
            minDistance = distance;
            closestColorIndex = i;
        }
    }
    
    return closestColorIndex;
};

const testColor = 0x20a0e0;
const nearestColorIndex = findNearestColor(testColor, inputPalette);
console.log(`Nearest color index for ${testColor.toString(16)}: ${nearestColorIndex}`);
console.log(`Nearest color: ${inputPalette[nearestColorIndex].toString(16)}`);

// Add PNG generation functionality
const fs = require('fs');
const PNG = require('pngjs').PNG;

// Generate a perceptually uniform gradient between two colors
const generateGradient = (startColor, endColor, steps) => {
    const chroma = require('chroma-js');
    
    // Convert hex numbers to hex strings for chroma-js
    const startHex = '#' + startColor.toString(16).padStart(6, '0');
    const endHex = '#' + endColor.toString(16).padStart(6, '0');
    
    // Create a perceptually uniform scale in LAB color space
    const scale = chroma.scale([startHex, endHex])
                        .mode('lab')  // Use LAB color space for perceptual uniformity
                        .colors(steps, 'hex');
    
    // Convert the hex strings back to numbers
    const gradient = scale.map(hex => parseInt(hex.substring(1), 16));
    
    return gradient;
};

// Generate the lookup table PNG
const generateLookupTablePNG = (palette, testColor) => {
    // Create a new 16x16 PNG
    const width = 16;
    const height = 16;
    const png = new PNG({ width, height, colorType: 6 });
    
    // For each row (representing a palette color)
    for (let y = 0; y < height; y++) {
        const paletteColor = palette[y % palette.length];
        
        // Generate a gradient from palette color to test color
        const gradientColors = generateGradient(paletteColor, testColor, width);
        
        // Fill the row with the gradient, using nearest palette colors
        for (let x = 0; x < width; x++) {
            const gradientColor = gradientColors[x];
            const nearestIndex = findNearestColor(gradientColor, palette);
            const nearestColor = palette[nearestIndex];
            
            // Convert to RGB components for PNG
            const r = (nearestColor >> 16) & 0xFF;
            const g = (nearestColor >> 8) & 0xFF;
            const b = nearestColor & 0xFF;
            
            // Set pixel in PNG (each pixel has 4 bytes: R,G,B,A)
            const idx = (width * y + x) << 2;
            png.data[idx] = r;
            png.data[idx + 1] = g;
            png.data[idx + 2] = b;
            png.data[idx + 3] = 255; // Alpha (fully opaque)
        }
    }
    
    return png;
};

// Generate and save the lookup table PNG
console.log('Generating color lookup table...');
const lookupTable = generateLookupTablePNG(inputPalette, testColor);
const pngBuffer = PNG.sync.write(lookupTable);
fs.writeFileSync('color_lookup.png', pngBuffer);
console.log('Lookup table saved as color_lookup.png');
