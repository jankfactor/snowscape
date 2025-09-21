#include "math3d.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "cvector.h"

fix *g_oneOver;   // Reciprocal table (for max screen height of 256 in Mode 13)
extern unsigned int OneOver; // Address of the above table for ASM access
fix *g_SineTable; // SIN table. Offset used for COS.

void SetupMathsGlobals(int isAllocating)
{
    if (isAllocating)
    {
        printf("Allocating tables required for Math3D...\n");
        cvector_reserve(g_SineTable, SINETABLE_SIZE);
        cvector_reserve(g_oneOver, ONEOVERTABLE_SIZE);
        OneOver = (unsigned int)(g_oneOver);
        printf("Done.\n");
    }
    else
    {
        printf("Freeing tables required for Math3D...\n");
        cvector_free(g_SineTable);
        cvector_free(g_oneOver);
        printf("Done.\n");
    }
}

void SetIdentity(MAT43 *mat)
{
    mat->m11 = 65536;
    mat->m12 = 0;
    mat->m13 = 0;
    mat->m21 = 0;
    mat->m22 = 65536;
    mat->m23 = 0;
    mat->m31 = 0;
    mat->m32 = 0;
    mat->m33 = 65536;
    mat->tx = 0;
    mat->ty = 0;
    mat->tz = 0;
}

void SetScale(MAT43 *mat, fix sx, fix sy, fix sz)
{
    mat->m11 = fixmult(mat->m11, sx);
    mat->m22 = fixmult(mat->m22, sy);
    mat->m33 = fixmult(mat->m33, sz);
}

void SetScaleUniversal(MAT43 *mat, fix s)
{
    SetScale(mat, s, s, s);
}

void EulerToMat(MAT43 *mat, int heading, int pitch, int bank)
{
    fix sh, ch, sp, cp, sb, cb;
    sh = fixsin(heading);
    ch = fixcos(heading);
    sp = fixsin(pitch);
    cp = fixcos(pitch);
    sb = fixsin(bank);
    cb = fixcos(bank);

    mat->m11 = fixmult(ch, cb) + fixmult(fixmult(sh, sp), sb);
    mat->m12 = fixmult(-ch, sb) + fixmult(fixmult(sh, sp), cb);
    mat->m13 = fixmult(sh, cp);

    mat->m21 = fixmult(sb, cp);
    mat->m22 = fixmult(cb, cp);
    mat->m23 = -sp;

    mat->m31 = fixmult(-sh, cb) + fixmult(fixmult(ch, sp), sb);
    mat->m32 = fixmult(sb, sh) + fixmult(fixmult(ch, sp), cb);
    mat->m33 = fixmult(ch, cp);

    mat->tx = mat->ty = mat->tz = 0;
}

void RotateAxis(MAT43 *mat, V3D *axis, int angle)
{
    fix s, c, a, ax, ay, az;
    s = fixsin(angle);
    c = fixcos(angle);
    a = 65536 - c;
    ax = fixmult(a, axis->x);
    ay = fixmult(a, axis->y);
    az = fixmult(a, axis->z);

    mat->m11 = fixmult(ax, axis->x) + c;
    mat->m12 = fixmult(ax, axis->y) + fixmult(s, axis->z);
    mat->m13 = fixmult(ax, axis->z) - fixmult(s, axis->y);
    mat->m21 = fixmult(ay, axis->x) - fixmult(s, axis->z);
    mat->m22 = fixmult(ay, axis->y) + c;
    mat->m23 = fixmult(ay, axis->z) + fixmult(s, axis->x);
    mat->m31 = fixmult(az, axis->x) + fixmult(s, axis->y);
    mat->m32 = fixmult(az, axis->y) - fixmult(s, axis->x);
    mat->m33 = fixmult(az, axis->z) + c;

    mat->tx = mat->ty = mat->tz = 0;
}

void RotateX(MAT43 *mat, int angle)
{
    fix s, c;
    s = fixsin(angle);
    c = fixcos(angle);

    mat->m11 = 65536;
    mat->m12 = 0;
    mat->m13 = 0;
    mat->m21 = 0;
    mat->m22 = c;
    mat->m23 = s;
    mat->m31 = 0;
    mat->m32 = -s;
    mat->m33 = c;
    mat->tx = mat->ty = mat->tz = 0;
}

void RotateY(MAT43 *mat, int angle)
{
    fix s, c;
    s = fixsin(angle);
    c = fixcos(angle);

    mat->m11 = c;
    mat->m12 = 0;
    mat->m13 = -s;
    mat->m21 = 0;
    mat->m22 = 65536;
    mat->m23 = 0;
    mat->m31 = s;
    mat->m32 = 0;
    mat->m33 = c;
    mat->tx = mat->ty = mat->tz = 0;
}

void Normal(V3D *a, V3D *b, V3D *c, V3D *n)
{
    V3D v1, v2;
    v1.x = a->x - b->x;
    v1.y = a->y - b->y;
    v1.z = a->z - b->z;
    v2.x = a->x - c->x;
    v2.y = a->y - c->y;
    v2.z = a->z - c->z;

    n->x = fixmult(v1.y, v2.z) - fixmult(v1.z, v2.y);
    n->y = fixmult(v1.z, v2.x) - fixmult(v1.x, v2.z);
    n->z = fixmult(v1.x, v2.y) - fixmult(v1.y, v2.x);
}

void Normalize(V3D *v)
{
    double len, x, y, z;
    x = fix2float(v->x);
    y = fix2float(v->y);
    z = fix2float(v->z);
    len = sqrt(x * x + y * y + z * z);

    v->x = float2fix((float)(x / len));
    v->y = float2fix((float)(y / len));
    v->z = float2fix((float)(z / len));
}

fix DotProduct(const V3D *v1, const V3D *v2)
{
    return fixmult(v1->x, v2->x) + fixmult(v1->y, v2->y) + fixmult(v1->z, v2->z);
}

void MultMatMat(MAT43 *dest, MAT43 *a, MAT43 *b)
{
    MAT43 tmp;
    tmp.m11 = fixmult(a->m11, b->m11) + fixmult(a->m12, b->m21) + fixmult(a->m13, b->m31);
    tmp.m12 = fixmult(a->m11, b->m12) + fixmult(a->m12, b->m22) + fixmult(a->m13, b->m32);
    tmp.m13 = fixmult(a->m11, b->m13) + fixmult(a->m12, b->m23) + fixmult(a->m13, b->m33);

    tmp.m21 = fixmult(a->m21, b->m11) + fixmult(a->m22, b->m21) + fixmult(a->m23, b->m31);
    tmp.m22 = fixmult(a->m21, b->m12) + fixmult(a->m22, b->m22) + fixmult(a->m23, b->m32);
    tmp.m23 = fixmult(a->m21, b->m13) + fixmult(a->m22, b->m23) + fixmult(a->m23, b->m33);

    tmp.m31 = fixmult(a->m31, b->m11) + fixmult(a->m32, b->m21) + fixmult(a->m33, b->m31);
    tmp.m32 = fixmult(a->m31, b->m12) + fixmult(a->m32, b->m22) + fixmult(a->m33, b->m32);
    tmp.m33 = fixmult(a->m31, b->m13) + fixmult(a->m32, b->m23) + fixmult(a->m33, b->m33);

    // Translation
    tmp.tx = fixmult(a->tx, b->m11) + fixmult(a->ty, b->m21) + fixmult(a->tz, b->m31) + b->tx;
    tmp.ty = fixmult(a->tx, b->m12) + fixmult(a->ty, b->m22) + fixmult(a->tz, b->m32) + b->ty;
    tmp.tz = fixmult(a->tx, b->m13) + fixmult(a->ty, b->m23) + fixmult(a->tz, b->m33) + b->tz;

    *dest = tmp;
}

void MultV3DMat(V3D *v, V3D *dest, MAT43 *mat)
{
    dest->x = fixmult(v->x, mat->m11) + fixmult(v->y, mat->m12) + fixmult(v->z, mat->m13) + mat->tx;
    dest->y = fixmult(v->x, mat->m21) + fixmult(v->y, mat->m22) + fixmult(v->z, mat->m23) + mat->ty;
    dest->z = fixmult(v->x, mat->m31) + fixmult(v->y, mat->m32) + fixmult(v->z, mat->m33) + mat->tz;
}

void MultV4DMat(V4D *v, V4D *dest, MAT44 *mat)
{
    dest->x = fixmult(v->x, mat->m11) + fixmult(v->y, mat->m12) + fixmult(v->z, mat->m13) + fixmult(v->w, mat->m14);
    dest->y = fixmult(v->x, mat->m21) + fixmult(v->y, mat->m22) + fixmult(v->z, mat->m23) + fixmult(v->w, mat->m24);
    dest->z = fixmult(v->x, mat->m31) + fixmult(v->y, mat->m32) + fixmult(v->z, mat->m33) + fixmult(v->w, mat->m34);
    dest->w = fixmult(v->x, mat->m41) + fixmult(v->y, mat->m42) + fixmult(v->z, mat->m43) + fixmult(v->w, mat->m44);
}

V3D SubV3D(const V3D *a, const V3D *b)
{
    V3D result;
    result.x = a->x - b->x;
    result.y = a->y - b->y;
    result.z = a->z - b->z;
    return result;
}

V3D CrossProductV3D(const V3D *a, const V3D *b)
{
    V3D result;
    result.x = fixmult(a->y, b->z) - fixmult(a->z, b->y);
    result.y = fixmult(a->z, b->x) - fixmult(a->x, b->z);
    result.z = fixmult(a->x, b->y) - fixmult(a->y, b->x);
    return result;
}

void LookAt(const V3D *eyePos, const V3D *forward, MAT43 *mat)
{
    // Calculate the forward vector (direction from eye to target)
    V3D up, right;

    // Define the up vector (world's up)
    up.x = 0;
    up.y = 65536;
    up.z = 0;

    // Calculate the right vector (perpendicular to forward and up)
    right = CrossProductV3D(&up, forward);
    up = CrossProductV3D(forward, &right);

    mat->tx = -DotProduct(&right, eyePos);
    mat->ty = -DotProduct(&up, eyePos);
    mat->tz = -DotProduct(forward, eyePos);

    // Fill in the matrix values
    mat->m11 = right.x;
    mat->m12 = right.y;
    mat->m13 = right.z;

    mat->m21 = up.x;
    mat->m22 = up.y;
    mat->m23 = up.z;

    mat->m31 = forward->x;
    mat->m32 = forward->y;
    mat->m33 = forward->z;
}

void PerspectiveProjection(MAT44 *mat, float fov, float aspect, float znear, float zfar)
{
    float yScale = 1.f / tanf(fov / 2.f);
    float xScale = yScale * aspect;

    mat->m11 = float2fix(xScale);
    mat->m12 = 0;
    mat->m13 = 0;
    mat->m14 = 0;

    mat->m21 = 0;
    mat->m22 = float2fix(yScale);
    mat->m23 = 0;
    mat->m24 = 0;

    mat->m31 = 0;
    mat->m32 = 0;
    mat->m33 = float2fix(zfar / (zfar - znear));
    mat->m34 = 65535;

    mat->m41 = 0;
    mat->m42 = 0;
    mat->m43 = float2fix(znear * zfar / (zfar - znear));
    mat->m44 = 0;
}
