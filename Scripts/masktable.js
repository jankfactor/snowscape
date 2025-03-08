function generateMask(xLeft, xRight) {
    // Start with all bits set (but handle as unsigned)
    let maskL = 0xFFFFFFFF >>> (xLeft * 4);
    let maskR = 0xFFFFFFFF << (xRight * 4);
    let mask = ~(maskL & maskR);
    
    // Convert to hex string correctly even for negative values
    return mask >>> 0; // Convert to unsigned 32-bit integer
}

function generateLookupTable() {
    console.log('; Raster mask lookup table for all xLeft/xRight combinations');
    console.log('RasterLookup');

    for (let xRight = 0; xRight < 8; xRight++) {
        for (let xLeft = 7; xLeft >= 0; xLeft--) {
            const mask = generateMask(xLeft, xRight);
            // Convert to hex string with proper formatting
            const hexStr = mask.toString(16).toUpperCase().padStart(8, '0');
            console.log(`        DCD &${hexStr}`);
        }
        console.log(''); // Empty line between groups
    }
}

generateLookupTable();