#ifndef MATH3D_H
#define MATH3D_H

#ifndef PI
#define PI 3.14159265
#endif // PI

#ifndef M_PI
#define M_PI 3.1415926535
#endif // M_PI

#define MAX_CLIPPING_VERTS 10
#define MAX_CLIPPING_TRIS 10
#define CLIP_LEFT 0x01
#define CLIP_RIGHT 0x02
#define CLIP_TOP 0x04
#define CLIP_BOTTOM 0x08

// We are working with a signed 32-bit fixed-point value which is divided halfway
// to offer 15 bits of integer (1 bit for sign) and 16 bits of mantissa
typedef signed int fix;

// Fixed point utilities
#define int2fix(x) ((x) << 16)
#define fix2int(x) ((x) >> 16)
#define float2fix(a) (fix)((a) * 65536.f)
#define fix2float(a) (float)((a) / 65536.f)
#define fixmult(a, b) (fix)(((a) >> 8) * ((b) >> 8))
#define fixmultINTL(a, b) (fix)(((a) >> 16) * (b))
#define fixdiv(a, b) (fix)(((a) << 8) / ((b) >> 8)) // NOTE - slow

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif // min

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif // max

#ifndef clamp
#define clamp(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#endif // clamp

// SIN/COS lookups
#define SINETABLE_SIZE 1024
#define _SINETABLE_SIZE 1023
#define fixsin(a) g_SineTable[(a) & _SINETABLE_SIZE]
#define fixcos(a) g_SineTable[((a) + 256) & _SINETABLE_SIZE]
extern fix *g_SineTable;

// Reciprocal table e.g., 1/a
#define ONEOVERTABLE_SIZE 1024
#define oneover(a) g_oneOver[(a)]
#define multOneOver(a, b) (fix)((a >> 16) * (oneover(b))) // Essentially INT * FIX, useful for reciprocal
extern fix *g_oneOver;

// Edge list buffers
#define EDGELIST_SIZE 256 // 256 * 4 = 1024 bytes, max screen height of 256
// extern fix *g_edgeList;
// extern fix *g_edgeListRight;

// General utilities
#define largest(x, y, z) ((x) * (x > y & x > z) + (y) * (y > x & y > z) + (z) * (z > x & z > y))
#define smin(a, b) (b + ((a - b) & ((a - b) >> (sizeof(int) * 8 - 1))))
#define smallest(x, y, z) smin(x, smin(y, z))
#define orient2d(a, b, c) fixmult((b).x - (a).x, (c).y - (a).y) - fixmult((b).y - (a).y, (c).x - (a).x)
#define orient2dint(a, b, c) (((b).x - (a).x) * ((c).y - (a).y)) - (((b).y - (a).y) * ((c).x - (a).x))

#define subtractV3D(a, b, c) \
    (c).x = (a).x - (b).x;   \
    (c).y = (a).y - (b).y;   \
    (c).z = (a).z - (b).z;

typedef struct V2D
{
    fix x;
    fix y;
} V2D;

typedef struct V3D
{
    fix x;
    fix y;
    fix z;
} V3D;

typedef struct V4D
{
    fix x;
    fix y;
    fix z;
    fix w;
} V4D;

#define TRI_INTENSITY_MASK 0xFF
#define TRI_CLIPPED_BIT 0x200

#define SET_BIT(a, b) ((a) |= (b))
#define CLEAR_BIT(a, b) ((a) &= ~(b))
#define TEST_BIT(a, b) ((a) & (b))

typedef struct TRI
{
    unsigned short a, b, c, flags;
    V2D centerpoint;
    fix depth;
    void *next;
} TRI;

typedef struct POLYGON
{
    V3D verts[MAX_CLIPPING_VERTS];
    int numVerts;
} POLYGON;

typedef struct MAT43
{
    fix m11, m12, m13;
    fix m21, m22, m23;
    fix m31, m32, m33;
    fix tx, ty, tz;
} MAT43;

typedef struct MAT44
{
    fix m11, m12, m13, m14;
    fix m21, m22, m23, m24;
    fix m31, m32, m33, m34;
    fix m41, m42, m43, m44;
} MAT44;

void SetupMathsGlobals(int isAllocating);

void SetIdentity(MAT43 *mat);
void SetScale(MAT43 *mat, fix sx, fix sy, fix sz);
void SetScaleUniversal(MAT43 *mat, fix s);
void MultMatMat(MAT43 *dest, MAT43 *a, MAT43 *b);
void MultV3DMat(V3D *v, V3D *dest, MAT43 *mat);
void MultV4DMat(V4D *v, V4D *dest, MAT44 *mat);
void RotateX(MAT43 *mat, int angle);
void RotateY(MAT43 *mat, int angle);
void RotateAxis(MAT43 *mat, V3D *axis, int angle);
void EulerToMat(MAT43 *mat, int heading, int pitch, int bank);
void Normal(V3D *a, V3D *b, V3D *c, V3D *n);
void Normalize(V3D *v);
fix DotProduct(const V3D *v1, const V3D *v2);
V3D SubV3D(const V3D *a, const V3D *b);
V3D CrossProductV3D(const V3D *a, const V3D *b);
void LookAt(const V3D *eyePos, const V3D *forward, MAT43 *mat);
void PerspectiveProjection(MAT44 *mat, float fov, float aspect, float znear, float zfar);

#endif // MATH3D_H

