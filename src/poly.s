.set OS_WriteC, 0
.set OS_Byte, 6
.set OS_ChangeDynamicArea, 42
.set OS_ReadModeVariable, 53
.set OS_RemoveCursors, 54
.set OS_ReadDynamicArea, 92

        .section .text, "ax"

.set ScreenHeightLimit, 255

        .global EdgeList
        .global FogTable  
        .global OneOver
        .global ScreenBank
        .global ScreenStart
        .global ScreenMax
        .global ScreenPartial

// ====== RESERVE SCREEN BANKS =====
// Set the display to Mode 13 and disable the screen cursor.

        .global  VDUSetup
VDUSetup:
        // Only enable OS_WriteC VDU output through OS_Byte_3
        MOV r0,#3
        MOV r1,#84
        SWI OS_Byte

        // Set Mode 13
        MOV r0,#22 // VDU 22
        SWI OS_WriteC
        .ifdef PAL_256
        MOV r0,#13 // 256 color mode
        .else
        MOV r0,#9 // 16 color mode
        .endif
        SWI OS_WriteC
        SWI OS_RemoveCursors
        MOVS pc,lr

        .global  UpdateMemAddress // (R1: screenStart, R2: screenMax)
UpdateMemAddress:
        STR a1,ScreenStart
        //STR a2,ScreenMax
        MOVS pc,lr

// ====== RESERVE SCREEN BANKS =====
// Reserve 2 banks of screen memory

        .global  ReserveScreenBanks
ReserveScreenBanks:
        MVN r0,#0 // -1 to get current screen mode
        MOV r1,#7 // Number of bytes for entire screen
        SWI OS_ReadModeVariable

        MOV r3,#2   // Double buffered (2 banks)
        MUL r1,r2,r3 // Double the number of bytes for 1 screen
        MOV r2,r1

        MOV r0,#2 // Read area 2 (aka screen area)
        SWI OS_ReadDynamicArea

        SUB r1,r2,r1 // Subtract 2 screens from total available memory
        MOV r0,#2
        SWI OS_ChangeDynamicArea
//        MOV r0,r1 ; Return the amount the area has changed (in bytes)
        MOVS pc,lr
// ====== SWITCH SCREEN BANK =====
// Toggles the current screen bank for drawing.

        .global  SwitchScreenBank
SwitchScreenBank:
        MOV r0,#19 // Wait for refresh
        SWI OS_Byte

        LDR r1,ScreenBank // Load the current drawing bank and make it the visible bank
        MOV r0,#113 // Select the visible bank
        SWI OS_Byte

        LDR r1,ScreenBank // Reload, as 113 may corrupt r1
        ADD r1,r1,#1 // Increment current bank
        CMP r1,#2    // If greater than 2
        MOVGT r1,#1  // Reset back to 1
        STR r1,ScreenBank

        MOV r0,#112 // Set the buffer bank for drawing
        SWI OS_Byte

        MOV pc,lr

// ====== CLEAR SCREEN =====
// Clears the current screen buffer 40 bytes at a time.

        .global ClearScreen // ClearScreen(int color);
ClearScreen:
        STMFD sp!,{r4-r11}
        MOV r3, r0
        MOV r4, r0
        MOV r5, r0
        MOV r6, r0
        MOV r7, r0
        MOV r8, r0
        MOV r9, r0
        MOV r10, r0
        MOV r11, r0

        CMP r1,#0
        LDRNE r2,ScreenMax
        LDREQ r2,ScreenPartial
        LDR r1,ScreenStart
        MOVEQ r2,r2,LSR#1
        ADD r12, r1, r2
        MOV r2,r0
        // a1 has start, a2 is the max mem location
CSloop:
        // Write 10 words at a time till max
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        STMEA r1!,{r2-r11}
        CMP r1,r12
        BLT CSloop

        LDMFD sp!,{r4-r11}
        MOV pc,lr

r0_LongGradient    .req r0   // Gradient of the long edge
r1_ShortGradient   .req r1   // Gradient of a short edge
r2_EdgeList        .req r2   // Edge list
v1_X               .req r7   // V1 X
v1_Y               .req r8   // V1 Y
v2_X               .req r9   // V2 X
v2_Y               .req r10  // V2 Y
v3_X               .req r11  // V3 X
v3_Y               .req r12  // V3 Y

        .global FillEdgeLists // FillEdgeList(int triList, int color);
FillEdgeLists:
        STMFD sp!,{r1,r4-r12,r14} // Store the current registers

        LDMFD r0!,{v1_X,v1_Y}       // Load 3 2D un-sorted coords
        ADD   r0,r0,#4            // Skip z
        LDMFD r0!,{v2_X,v2_Y}
        ADD   r0,r0,#4
        LDMFD r0,{v3_X,v3_Y}

        CMP   v1_X,#320
        CMPLO v2_X,#320
        CMPLO v3_X,#320
        CMPLO v1_Y,#255
        CMPLO v2_Y,#255
        CMPLO v3_Y,#255
        BLO TrivialTriangleRoutine

// ==========================================
// ========= CLIPPED TRIANGLE ===============
// ==========================================

ClippedTriangleRoutine:

        CMP   v1_Y, #255
        CMPGE v2_Y, #255
        CMPGE v3_Y, #255
        BGE   EdgeListEnd // All Y coords are off screen bottom

        CMP   v1_X, #320
        CMPGE v2_X, #320
        CMPGE v3_X, #320
        BGE   EdgeListEnd // All X coords are off screen

        CMP   v1_Y, #0
        CMPLE v2_Y, #0
        CMPLE v3_Y, #0
        BLE   EdgeListEnd // All Y coords are off screen top

        CMP   v1_X, #0
        CMPLE v2_X, #0
        CMPLE v3_X, #0
        BLE   EdgeListEnd // All X coords are off screen

        // Sort V0-V2 by Y, swap where necessary.
        // V0 and V1
        CMP v1_Y,v2_Y
        MOVGT r2,v1_X
        MOVGT v1_X,v2_X
        MOVGT v2_X,r2
        MOVGT r2,v1_Y
        MOVGT v1_Y,v2_Y
        MOVGT v2_Y,r2

        // V0 and V2
        CMP v1_Y,v3_Y
        MOVGT r2,v1_X
        MOVGT v1_X,v3_X
        MOVGT v3_X,r2
        MOVGT r2,v1_Y
        MOVGT v1_Y,v3_Y
        MOVGT v3_Y,r2

        // V1 and V2
        CMP v2_Y,v3_Y
        MOVGT r2,v2_X
        MOVGT v2_X,v3_X
        MOVGT v3_X,r2
        MOVGT r2,v2_Y
        MOVGT v2_Y,v3_Y
        MOVGT v3_Y,r2

        // LONG DELTA CALCULATION
        // We always calculate the long edge first as even if we jump to the bottom half, we still need
        // to step along the long edge to find the correct x starting position.
CalcLongSide:
        // Calculate m between V0 and V2
        SUB r14,v3_Y,v1_Y          // y3 - y1
        SUB r1,v3_X,v1_X           // x3 - x1

        CMP r14,#0x0400           // Due to reciprocal limit, we need to limit the gradient inputs
        BLT LongSideTableLookup

        STMFD sp!,{r1,r14} // Store the current registers
        MOV r0,r1
        MOV r1,r14
        BL GenericDivide
        LDMFD sp!,{r1,r14} // Restore the current registers
        B LongSideGradientSafe

LongSideTableLookup:
        LDR r6,OneOver          // start of oneOver block
        LDR r3,[r6,r14,LSL#2]    // >> 16 << 2 (4 byte jump)
        MUL r0_LongGradient,r3,r1  // Store m in r0, r3 is available
LongSideGradientSafe:

        // CLIPPING
        // If our middle vertex is above the screen we need the longside to catch up
        // and go straight to drawing the bottom half. By proxy, v1_Y is also offscreen.
        CMP   v2_Y,#0
        MOVLT r4,v1_X,ASL#16                 // x1 to fixed point
        SUBLT r14,v2_Y,v1_Y                  // y2 - y1, if less than 0, we have a flat top triangle
        MLALT r4,r0_LongGradient,r14,r4      // r4 += m * (+v1_Y)
        MOVLT v1_Y,#0
        LDRLT r2_EdgeList,EdgeList           // Edge list needs to be set up for the jump to the bottom half
        BLT CalcBottomShortSide               // Flat top triangle, skip the top part

CalcTopShortSide:
        SUB r2,v2_X,v1_X                    // x2 - x1
        SUBS r14,v2_Y,v1_Y                  // y2 - y1, if less than 0, we have a flat top triangle
        MOVLE r4,v1_X,ASL#16                    // x0 to fixed point
        LDRLE r2_EdgeList,EdgeList              // Bottom half assumes edge list is already assigned to r2
        ADDLE r2_EdgeList,r2_EdgeList,v1_Y,LSL#2        // Add the Y coord to the list address
        BLE CalcBottomShortSide                         // Flat top triangle, skip the top part

        CMP r14,#0x0400 // Due to reciprocal limit, we need to limit the gradient inputs
        BLT TopTableLookup

        STMFD sp!,{r0,r2,r4-r6,r14} // Store the current registers
        MOV r0,r2
        MOV r1,r14
        BL GenericDivide
        MOV r1,r0
        LDMFD sp!,{r0,r2,r4-r6, r14} // Restore the current registers
        B TopGradientSafe

TopTableLookup:
        LDR r6,OneOver          // start of oneOver block
        LDR r4,[r6,r14,LSL#2]
        MUL r1_ShortGradient,r4,r2

TopGradientSafe:
        LDR r2_EdgeList,EdgeList

        SUB r14,v2_Y,v1_Y       // Need to reset the y delta

        MOV r4,v1_X,ASL#16 // x1 to fixed point
        MOV r5,r4
        // r7 is free at this point, use it as a temp

        // CLIPPING
        // If y1 < 0, we need to adjust the starting position
        CMP v1_Y,#0
        ADDLT r14,r14,v1_Y              // r14 = positive y delta
        RSBLT r3,v1_Y,#0                // r2 = positive v1_Y
        MOVLT v1_Y,#0                   // v1_Y = 0
        MLALT r4,r0_LongGradient,r3,r4  // r4 += m * (+v1_Y)
        MLALT r5,r1_ShortGradient,r3,r5 // r5 += m * (+v1_Y)
        ADDGE r2_EdgeList,r2_EdgeList,v1_Y,LSL#2 // Add the Y coord to the list address
        // If y2 > ScreenHeightLimit, we need to trim the top part and reduce the y delta
        CMP v2_Y,#255
        SUBGE r7,v2_Y,#255        // r7 = positive y delta over ScreenHeightLimit
        SUBGE r14,r14,r7                        // Reduce the y delta accordingly

LFillEdgeLists__local_12_2:
                           // Fill the edge list for the top part of the triangle
        MOV r7,r4
        CMP r7,#0
        MOVLT r7,#0
        CMP r7,#0x01400000
        MOVGE r7,#0x01400000
        MOV r3,r7,LSR#16

        MOV r7,r5
        CMP r7,#0
        MOVLT r7,#0
        CMP r7,#0x01400000
        MOVGE r7,#0x01400000

        MOV r3,r3,LSL#16
        ORR r3,r3,r7,LSR#16

        STR r3,[r2_EdgeList],#4     // Store the current x value
        ADD r4,r4,r0_LongGradient   // Add the gradient to the current x value
        ADD r5,r5,r1_ShortGradient  // Add the gradient to the current x value
        SUBS r14,r14,#1          // Decrement the y counter
        BGT LFillEdgeLists__local_12_2                // Loop until we reach y2

CalcBottomShortSide:
        SUB r14,v3_Y,v2_Y         // y3 - y2
        SUB r1_ShortGradient,v3_X,v2_X // x3 - x2

        CMP r14,#0x0400            // Due to reciprocal limit, we need to limit the gradient inputs
        BLT BottomTableLookup

        STMFD sp!,{r0,r2,r4-r6,r14} // Store the current registers
        MOV r0,r1
        MOV r1,r14
        BL GenericDivide
        MOV r1,r0
        LDMFD sp!,{r0,r2,r4-r6, r14} // Restore the current registers
        B BottomGradientSafe

BottomTableLookup:
        LDR r6,OneOver          // start of oneOver block
        LDR r5,[r6,r14,LSL#2]    // >> 16 << 2 (4 byte jump)
        MUL r1_ShortGradient,r5,r1_ShortGradient

BottomGradientSafe:
        MOV r5,v2_X,ASL#16         // x2 to fixed point

        SUB r14,v3_Y,v2_Y      // Need to reset the y delta

        // CLIPPING
        // If y1 < 0, we need to adjust the starting position
        CMP   v2_Y,#0
        ADDLT r14,r14,v2_Y              // r14 = positive y delta
        RSBLT r3,v2_Y,#0                // r2 = positive v1_Y
        MLALT r4,r0_LongGradient,r3,r4  // r4 += m * (+v1_Y)
        MLALT r5,r1_ShortGradient,r3,r5 // r5 += m * (+v1_Y)
        // If y3 > ScreenHeightLimit, we need to adjust the ending position
        CMP   v3_Y,#255
        SUBGE r7,v3_Y,#255        // r7 = positive y delta over 255
        SUBGE r14,r14,r7                        // Reduce the y delta accordingly

LFillEdgeLists__local_14_2:
                           // Fill the edge list for the bottom part of the triangle
        MOV r7,r4
        CMP r7,#0
        MOVLT r7,#0
        CMP r7,#0x01400000
        MOVGE r7,#0x01400000
        MOV r3,r7,LSR#16

        MOV r7,r5
        CMP r7,#0
        MOVLT r7,#0
        CMP r7,#0x01400000
        MOVGE r7,#0x01400000

        MOV r3,r3,LSL#16
        ORR r3,r3,r7,LSR#16

        STR r3,[r2_EdgeList],#4     // Store the current x value
        ADD r4,r4,r0_LongGradient   // Add the gradient to the current x value
        ADD r5,r5,r1_ShortGradient  // Add the gradient to the current x value
        SUBS r14,r14,#1          // Decrement the y counter
        BGT LFillEdgeLists__local_14_2                // Loop until we reach y2

        // CLIPPING
        // If y2 > ScreenHeightLimit, we need to adjust the ending position
        CMP v3_Y,#255
        MOVGE v3_Y,#255
        CMPGE v1_Y,v3_Y
        BGE EdgeListEnd

        B Triv_DrawEdges

// ==========================================
// ========= TRIVIAL TRIANGLE ===============
// ==========================================

TrivialTriangleRoutine:

        // Sort V0-V2 by Y, swap where necessary.
        // V0 and V1
        CMP v1_Y,v2_Y
        MOVGT r2,v1_X
        MOVGT v1_X,v2_X
        MOVGT v2_X,r2
        MOVGT r2,v1_Y
        MOVGT v1_Y,v2_Y
        MOVGT v2_Y,r2

        // V0 and V2
        CMP v1_Y,v3_Y
        MOVGT r2,v1_X
        MOVGT v1_X,v3_X
        MOVGT v3_X,r2
        MOVGT r2,v1_Y
        MOVGT v1_Y,v3_Y
        MOVGT v3_Y,r2

        // V1 and V2
        CMP v2_Y,v3_Y
        MOVGT r2,v2_X
        MOVGT v2_X,v3_X
        MOVGT v3_X,r2
        MOVGT r2,v2_Y
        MOVGT v2_Y,v3_Y
        MOVGT v3_Y,r2

        // LONG DELTA CALCULATION
        // We always calculate the long edge first as even if we jump to the bottom half, we still need
        // to step along the long edge to find the correct x starting position.
Triv_CalcLongSide:
        // Calculate m between V0 and V2
        SUB r14,v3_Y,v1_Y          // y3 - y1
        SUB r1,v3_X,v1_X           // x3 - x1

        LDR r6,OneOver          // start of oneOver block
        LDR r3,[r6,r14,LSL#2]    // >> 16 << 2 (4 byte jump)
        MUL r0_LongGradient,r3,r1  // Store m in r0, r3 is available

Triv_CalcTopShortSide:
        SUB r2,v2_X,v1_X                    // x2 - x1
        SUBS r14,v2_Y,v1_Y                  // y2 - y1, if less than 0, we have a flat top triangle
        MOVLE r4,v1_X,ASL#16                    // x0 to fixed point
        LDRLE r2_EdgeList,EdgeList              // Bottom half assumes edge list is already assigned to r2
        ADDLE r2_EdgeList,r2_EdgeList,v1_Y,LSL#2        // Add the Y coord to the list address
        BLE Triv_CalcBottomShortSide                         // Flat top triangle, skip the top part

        LDR r4,[r6,r14,LSL#2]
        MUL r1_ShortGradient,r4,r2
        LDR r2_EdgeList,EdgeList

        MOV r4,v1_X,ASL#16 // x1 to fixed point
        MOV r5,r4
        // r7 is free at this point, use it as a temp

        ADD r2_EdgeList,r2_EdgeList,v1_Y,LSL#2 // Add the Y coord to the list address

Triv_TopEdgeList:
                       // Fill the edge list for the top part of the triangle
        MOV r3,r4,LSR#16
        MOV r3,r3,LSL#16
        ORR r3,r3,r5,LSR#16

        STR r3,[r2_EdgeList],#4     // Store the current x value
        ADD r4,r4,r0_LongGradient   // Add the gradient to the current x value
        ADD r5,r5,r1_ShortGradient  // Add the gradient to the current x value
        SUBS r14,r14,#1          // Decrement the y counter
        BGT Triv_TopEdgeList                // Loop until we reach y2

Triv_CalcBottomShortSide:
        SUB r14,v3_Y,v2_Y         // y3 - y2
        SUB r1_ShortGradient,v3_X,v2_X // x3 - x2

        LDR r5,[r6,r14,LSL#2]    // >> 16 << 2 (4 byte jump)
        MUL r1_ShortGradient,r5,r1_ShortGradient
        MOV r5,v2_X,ASL#16         // x2 to fixed point

Triv_BottomEdgeList:
                          // Fill the edge list for the bottom part of the triangle
        MOV r3,r4,LSR#16
        MOV r3,r3,LSL#16
        ORR r3,r3,r5,LSR#16

        STR r3,[r2_EdgeList],#4     // Store the current x value
        ADD r4,r4,r0_LongGradient   // Add the gradient to the current x value
        ADD r5,r5,r1_ShortGradient  // Add the gradient to the current x value
        SUBS r14,r14,#1          // Decrement the y counter
        BGT Triv_BottomEdgeList                // Loop until we reach y2

Triv_DrawEdges:
        SUB r14,v3_Y,v1_Y          // y2 - y1 (i.e., the number of lines to draw)
        LDR r12,EdgeList
        ADD r12,r12,v1_Y,LSL#2      // Add the top Y coord to the list address
        LDR r11,ScreenStart     // Load the screen mem start location
        MOV r3,v1_Y               // Initial Y position
        .ifdef PAL_256
        MOV r2,r3,LSL#8         // Multiply by 320 in 2 stages (<< 8) + (<< 6)
        ADD r2,r2,r3,LSL#6      // Total Y offset * 320
        .else
        MOV r2,r3,LSL#7         // Multiply by 160 in 2 stages (<< 7) + (<< 5)
        ADD r2,r2,r3,LSL#5      // Total Y offset * 160
        .endif
        ADD r11,r11,r2          // Add Y offset to screen offset start location

        LDR r0,FogTable
        LDR r7,[sp]             // Load the color
        ADD r0,r0,r7,LSL#2      // Load the fog value
        TST v1_Y,#1             // Does the triangle start on an odd line?
        .ifdef PAL_256
        LDRNE r7,[r0]             // Load the fog value
        LDRNE r8,[r0,#256]       // Load the fog value
        LDREQ r8,[r0]             // Load the fog value
        LDREQ r7,[r0,#256]       // Load the fog value
        .else
        LDRNE r7,[r0]             // Load the fog value
        LDRNE r8,[r0,#64]       // Load the fog value offset by 16 bytes
        LDREQ r8,[r0]             // Load the fog value
        LDREQ r7,[r0,#64]       // Load the fog value offset by 16 bytes
        .endif

// ==========================================
// ========= RASTERIZE THE EDGE LIST ========
// ==========================================

RasterScanlineLoop:
        LDR r2,[r12],#4         // Load the left edge x coord
        MOV r3,r2,LSR#16        // Move the left edge x coord to integer
        MOV r2,r2,LSL#16        // Clear out the left edge x coord leaving the right edge
        MOV r2,r2,LSR#16        // Move back to integer

        CMP r3,r2       // if x2 < x1
        BEQ Continue      // Skip the swap
        EORMI r3,r3,r2    // swap x1 and x2
        EORMI r2,r3,r2    // swap x1 and x2
        EORMI r3,r3,r2    // swap x1 and x2

        .ifdef PAL_256
        ADD r9,r11,r2           // Add the left edge x coord to the screen offset
        ADD r10,r11,r3          // Add the right edge x coord to the screen offset

        MOV r0,r7       // Load the fog value
        MOV r1,r0
        ANDS r2,r9,#1   // Used to rotate the color
        MOVNE r1,r1,ROR#8  // Rotate the color

        SUB r4,r10,r9           // Get the number of pixels left
        CMP r4,#4
        BLT SpinLastBytes            

        // The following are awkward starting points, so we'll just use STRB        
        TST r9,#3
        STRNEB r1,[r9],#1 // Store the color
        MOVNE r1,r1,ROR#8  // Rotate the color
        SUBNE r4,r4,#1
        TSTNE r9,#3
        STRNEB r1,[r9],#1 // Store the color
        MOVNE r1,r1,ROR#8  // Rotate the color
        SUBNE r4,r4,#1
        TSTNE r9,#3
        STRNEB r1,[r9],#1 // Store the color
        MOVNE r1,r1,ROR#8  // Rotate the color
        SUBNE r4,r4,#1
        .else

        // Our first check is to see if we have an xL and xR within the same quad boundary
        EOR r9, r2, r3 
        TST r9, #504 // Are xL and xR on the same boundary? (ie, anything outside of 0b111)

        // If not, we have an easy job as we just mask 0xFFFFFFFF and shift it by our required pixels
        AND r0, r2, #7 // How far in we are
        MOV r0, r0, LSL #2 // Shift in nibbles
        MVN r1, #0 // 0xFFFFFFFF
        MOV r0, r1, LSL r0

        // If xL and xR are on the same boundary, shift r1 right and clear from the other side
        ANDEQ r10, r3, #7
        MOVEQ r10, r10, LSL #2 // Shift in nibbles
        BICEQ r0, r0, r1, LSL r10

        // Load existing screen color, mask and write back
        ADD r9, r11, r2, LSR #1 // Add the left edge x coord to the screen offset
        BIC r9, r9, #3 // Move screen buffer back to boundary
        LDR r1, [r9] // Load existing color
        BIC r1, r1, r0 // Mask out existing color
        AND r0, r7, r0 // Mask out new color with inverted mask (which is no longer required)
        ORR r0, r0, r1 // Combine masked data
        STR r0, [r9], #4 // Write it back in again
        BEQ Continue // If this was a short raster, we can just jump ahead. 

        // Otherwise, let's get the end sorted
        ANDS r0, r3, #7 // How far in we are on the xR side
        MOV r0, r0, LSL #2 // Shift in nibbles
        MVN r1, #0 // 0xFFFFFFFF
        MOV r0, r1, LSL r0

        ADD r10, r11, r3, LSR #1 // Add the right edge x coord to the screen offset
        BICNE r10, r10, #3 // Move screen buffer back to boundary
        LDR r1, [r10] // Load existing color
        AND r1, r1, r0 // Mask out existing color
        BIC r0, r7, r0 // Mask out new color with inverted mask (which is no longer required)
        ORR r0, r0, r1 // Combine masked data
        STR r0, [r10], #4 // Write it back in again
        ADD r10, r11, r3, LSR #1 // Add the right edge x coord to the screen offset
        .endif

QuadBlit:
        .ifdef PAL_256
        MOV r4,r4,LSR#4
        CMP r4,#32
        BGE RotateColor
        RSB r4,r4,#32 // reverse order
        MOV r1,r0
        MOV r2,r0
        MOV r3,r0
        ADD pc,pc,r4,LSL#2 // If remaining width > 16 pixels, we can use this jump table.
        MOV r0,r0
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}
        STMIA r9!,{r0-r3}

        // ; In theory, there should be less than 16 bytes left, so attempt STR with quads now
        SUB r4,r10,r9           // Get the number of pixels left
        MOV r4,r4,LSR#2
        RSB r4,r4,#4 // reverse order
        ADD pc,pc,r4,LSL#2 // If remaining width > 4 pixels, we can use this jump table.
        MOV r0,r0
        STR r0,[r9],#4
        STR r0,[r9],#4
        STR r0,[r9],#4
        STR r0,[r9],#4

RotateColor:
        MOV r1,r0
        ANDS r2,r9,#1   // Used to rotate the color
        MOVNE r1,r1,ROR#8  // Rotate the color

SpinLastBytes:
        CMP r9,r10
        STRLTB r1,[r9],#1
        MOVLT r1,r1,ROR#8
        CMPLT r9,r10
        STRLTB r1,[r9],#1
        MOVLT r1,r1,ROR#8
        CMPLT r9,r10
        STRLTB r1,[r9],#1
        MOVLT r1,r1,ROR#8
        CMPLT r9,r10
        STRLTB r1,[r9],#1

        .else

        MOV r0, r7
        SUB r4, r10, r9 // Get the number of pixels left
        MOV r4, r4, LSR #4
        CMP r4, #16 // Reduced from 32 to 16 since bytes are halved
        BGE Continue
        RSB r4, r4, #16 // Reverse order, reduced from 32 to 16
        // MOV r0, #&DD
        // EOR r0, r0, r0, LSL #8
        // EOR r0, r0, r0, LSL #16
        MOV r1, r0
        MOV r2, r0
        MOV r3, r0
        ADD pc, pc, r4, LSL #2
        MOV r0, r0
        STMIA r9!, {r0-r3} // Reduced number of STMIA instructions by half
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}
        STMIA r9!, {r0-r3}

        // ; In theory, there should be less than 16 bytes left, so attempt STR with quads now
        SUB r4, r10, r9 // Get the number of pixels left
        MOV r4, r4, LSR #2
        RSB r4, r4, #4 // reverse order
        ADD pc, pc, r4, LSL #2 // If remaining width > 4 pixels, we can use this jump table.
        MOV r0, r0
        STR r0, [r9], #4
        STR r0, [r9], #4
        STR r0, [r9], #4
        STR r0, [r9], #4
        .endif

Continue:
        EOR r7, r7, r8 // Swap dither pattern
        EOR r8, r7, r8 //
        EOR r7, r7, r8 //

        .ifdef PAL_256
        ADD r11,r11,#320        // Add 320 to the screen offset
        SUBS r14,r14,#1         // Decrement the y counter
        .else
        ADD r11, r11, #160 // Changed from 320 to 160 bytes per scanline
        SUBS r14, r14, #1 // Decrement the y counter
        .endif
        BGT RasterScanlineLoop // Loop until we reach y2

EdgeListEnd:
        LDMFD sp!,{r1,r4-r12,r14}  // Restore registers before returning
        MOV pc,lr

EdgeList:
                .word 0          // Our table of edge lists
FogTable:
                .word 0          // Our table of fog values
OneOver:
                .word 0          // Our table of reciprocal 1/X values
ScreenBank:
                .word 1          // Initial screen bank index
ScreenStart:
                .word 0

        .ifdef PAL_256
ScreenMax:
                .word 0x014000
ScreenPartial:
                .word 0xfa00   // 0 to 200 in Mode 13
        .else
ScreenMax:
                .word 0xa000   // Changed from 14000 to A000 (halved for 4-bit mode)
ScreenPartial:
                .word 0xfa00   // 0 to 200 in Mode 9
        .endif

ALIGN:

        .global KeyPress // KeyPress(int keycode);
KeyPress:
        EOR r1,r0,#255
        MOV r0,#129
        MOV r2,#255
        SWI OS_Byte
        MOV r0,r1 // r0 contains either 0xFF or 0x00
        MOV pc,lr

        // EXPORT GenericDivide
GenericDivide:
        // Enter with dividend in R0, divisor in R1.
        // Trashes R4 - R6.
        // Returns with quotient in R0.
        // The divisor must not be zero. The dividend can be negative.
        CMP     R0, #0
        MOVEQ   pc, lr

        MOVS    R4, R0          // Store, as we need to check sign
        RSBMI   R0, R0, #0      // Make positive

        MOV     R1, R1, LSL#16  // Int to Fix
        MOV     R6, #0          // Result in R6
        MOV     R5, #-0xffff80000000  // Used as a counter until bit is pushed off end
LGenericDivide__local_10_3:
                           MOVS    R0, R0, LSL#1   // Double R0 and store status
        CMPCC   R0, R1
        SUBCS   R0, R0, R1
        ORRCS   R6, R6, R5
        MOVS    R5, R5, LSR #1
        BCC     LGenericDivide__local_10_3

        CMP     R4, #0
        RSBMI   R0, R6, #0
        MOVPL   R0, R6

        MOV pc,lr

        .global ProjectVertex // ProjectVertex(int vertexPtr);
ProjectVertex:
        LDMFD r0!,{r1-r3}  // Load X, Y, Z from the vertex
        MOVS r3,r3,ASR#8   // Divide Z by 256
        ADDS r3,r3,#64     // Push forward on the Z plane a little
        BLE NoDivide

        STMFD sp!,{r4-r6}  // Save some registers

        // Enter with dividend (X) in R4, divisor (Z) in R3.
        // The divisor must not be zero. The dividend can be negative.
        MOVS    R4, R1          // Preserve original X for sign checking
        RSBMI   R4, R4, #0      // If negative, negate R4 to make it positive

        MOV     R5, R3          // Put the divisor in R5.
        CMP     R5, R4, LSR #1  // Then double it until
LProjectVertex__local_10_4:
                           MOVLS   R5, R5, LSL #1  // 2 * R5 > R4.
        CMP     R5, R4, LSR #1
        BLS     LProjectVertex__local_10_4           // Loop until 2 * R5 > R4
        MOV     R6, #0          // Initialise the quotient
LProjectVertex__local_20_4:
                           CMP     R4, R5          // Can we subtract R5?
        SUBCS   R4, R4, R5      // If we can, do so
        ADC     R6, R6, R6      // Double quotient and add new bit
        MOV     R5, R5, LSR #1  // Halve R5.
        CMP     R5, R3          // And loop until we've gone
        BHS     LProjectVertex__local_20_4           // past the original divisor,

        CMP     R1, #0          // Check original X's sign again
        RSBMI   R1, R6, #0      // If it was negative, negate the quotient
        MOVPL   R1, R6          // Move the quotient to R1

        // Enter with dividend (Y) in R4, divisor (Z) in R3.
        // The divisor must not be zero. The dividend can be negative.
        MOVS    R4, R2          // Preserve original Y for sign checking
        RSBMI   R4, R4, #0      // If negative, negate R4 to make it positive

        MOV     R5, R3          // Put the divisor in R5.
        CMP     R5, R4, LSR #1  // Then double it until
LProjectVertex__local_30_4:
                           MOVLS   R5, R5, LSL #1  // 2 * R5 > R4.
        CMP     R5, R4, LSR #1
        BLS     LProjectVertex__local_30_4
        MOV     R6, #0          // Initialise the quotient
LProjectVertex__local_40_4:
                           CMP     R4, R5          // Can we subtract R5?
        SUBCS   R4, R4, R5      // If we can, do so
        ADC     R6, R6, R6      // Double quotient and add new bit
        MOV     R5, R5, LSR #1  // Halve R5.
        CMP     R5, R3          // And loop until we've gone
        BHS     LProjectVertex__local_40_4            // past the original divisor,

        CMP     R2, #0          // Check original Y's sign again
        RSBPL   R2, R6, #0      // If it was positive, negate the quotient
        MOVMI   R2, R6          // Move the quotient to R1

        LDMFD sp!,{r4-r6}  // Restore some registers

NoDivide:
        ADD r1,r1,#160
        ADD r2,r2,#128

        STMFD r0!,{r1-r3}  // Store X, Y, Z back to the vertex
        MOV pc,lr


