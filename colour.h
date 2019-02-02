#define _USE_MATH_DEFINES
#include <math.h>

typedef union {
    struct { double x,y,z; };
    struct { double l;
                union {
                    struct { double a,b; };
                    struct { double u,v; };
                    struct { double c,h; };
                };
            };
} colour_t;

// COLOUR SPACE
// CIELCH -> L* = luminance: [0, 100]; C = chroma: [0, 100], h = hue [0, 360)
// CIELUV -> L* = luminance: [0, 100]; u*, v*: [?, ?]
// CIE XYZ -> tristimulus values
// sRGB -> gamma corrected sRGB [0, 1]

static colour_t D65 = {95.047, 100.0, 108.883};
static double kappa = 903.3;
static double epsilon = 0.008856;

static double clamp(double a, double b, double c) {
    return b < a ? a : c < b ? c : b;
}
static colour_t lchuv_luv(colour_t lch) {
    double u = lch.c * cos(lch.h * M_PI / 180.0);
    double v = lch.c * sin(lch.h * M_PI / 180.0);
    return (colour_t){lch.l, u, v};
}
static colour_t lchab_lab(colour_t lch) {
    double a = lch.c * cos(lch.h * M_PI / 180.0);
    double b = lch.c * sin(lch.h * M_PI / 180.0);
    return (colour_t){lch.l, a, b};
}
static colour_t luv_xyz(colour_t luv) {
    double Y = luv.l > kappa * epsilon
        ? pow((luv.l + 16.0) / 116.0, 3)
        : luv.l / kappa;
    double u0 = (4.0 * D65.x) / (D65.x + 15.0 * D65.y + 3 * D65.z);
    double v0 = (9.0 * D65.y) / (D65.x + 15.0 * D65.y + 3 * D65.z);
    double a = (1.0 / 3.0) * ((52.0 * luv.l) / (luv.u + 13.0 * luv.l * u0) - 1.0);
    double b = -5 * Y;
    double c = -1.0 / 3.0;
    double d = Y * ((39.0 * luv.l) / (luv.v + 13.0 * luv.l * v0) - 5.0);
    double X = (d - b) / (a - c);
    double Z = X * a + b;
    return (colour_t){X * 100.0, Y * 100.0, Z * 100.0};
}
static colour_t lab_xyz(colour_t lab) {
    double fy = (lab.l + 16.0) / 116.0;
    double fx = lab.a / 500.0 + fy;
    double fz = fy - lab.b / 200.0;
    double fx3 = fx * fx * fx;
    double fz3 = fz * fz * fz;
    double x = fx3 > epsilon ? fx3 : (116.0 * fx - 16.0) / kappa;
    double y = lab.l > kappa * epsilon ? pow((lab.l + 16.0) / 116.0, 3) : lab.l / kappa;
    double z = fz3 > epsilon ? fz3 : (116.0 * fz - 16.0) / kappa;
    return (colour_t){x * D65.x, y * D65.y, z * D65.z};
}
static colour_t xyz_srgb(colour_t xyz) {
    double x = xyz.x / 100.0;
    double y = xyz.y / 100.0;
    double z = xyz.z / 100.0;
    double r = x * +3.240479 + y * -1.537150 + z * -0.498535;
    double g = x * -0.969256 + y * +1.875992 + z * +0.041556;
    double b = x * +0.055648 + y * -0.204043 + z * +1.057311;
    r = r < 0.0031308 ? 12.92 * r : 1.055 * pow(r, 1 / 2.4) - 0.055;
    g = g < 0.0031308 ? 12.92 * g : 1.055 * pow(g, 1 / 2.4) - 0.055;
    b = b < 0.0031308 ? 12.92 * b : 1.055 * pow(b, 1 / 2.4) - 0.055;
    r = clamp(0.0, r, 1.0);
    g = clamp(0.0, g, 1.0);
    b = clamp(0.0, b, 1.0);
    return (colour_t){r,g,b};
}
static colour_t luv_srgb(colour_t in) { return xyz_srgb(luv_xyz(in)); }
static colour_t lchuv_srgb(colour_t in) { return luv_srgb(lchuv_luv(in)); }
static colour_t lab_srgb(colour_t in) { return xyz_srgb(lab_xyz(in)); }
static colour_t lchab_srgb(colour_t in) { return lab_srgb(lchab_lab(in)); }
static colour_t srgb_srgb(colour_t in) { return in; }
static colour_t rgb8_srgb(colour_t in) { return (colour_t){in.x / 255.0, in.y / 255.0, in.z / 255.0}; }

static unsigned rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000 + (r << 16) + (g << 8) + b;
}
static colour_t import_rgb(unsigned x) {
    return rgb8_srgb((colour_t){(x >> 16) & 255, (x >> 8) & 255, x & 255});
}
static unsigned export_rgb(colour_t c) {
    return rgb(c.x * 255.0, c.y * 255.0, c.z * 255.0);
}
static unsigned export_luv(colour_t in) { return export_rgb(luv_srgb(in)); }
static unsigned export_lab(colour_t in) { return export_rgb(lab_srgb(in)); }
static unsigned export_lchuv(colour_t in) { return export_rgb(lchuv_srgb(in)); }
static unsigned export_lchab(colour_t in) { return export_rgb(lchab_srgb(in)); }





static colour_t srgb_xyz(colour_t rgb) {
    double r = rgb.x <= 0.04045 ? rgb.x / 12.92 : pow((rgb.x + 0.055) / 1.055, 2.4);
    double g = rgb.y <= 0.04045 ? rgb.y / 12.92 : pow((rgb.y + 0.055) / 1.055, 2.4);
    double b = rgb.z <= 0.04045 ? rgb.z / 12.92 : pow((rgb.z + 0.055) / 1.055, 2.4);
    double X = r * +0.412435 + g * +0.357580 + b * +0.180423;
    double Y = r * +0.212671 + g * +0.715160 + b * +0.072169;
    double Z = r * +0.019334 + g * +0.119193 + b * +0.950227;
    return (colour_t){X * 100, Y * 100, Z * 100};
}
static colour_t xyz_luv(colour_t xyz) {
    double yr = xyz.y / D65.y;
    double up = (4.0 * xyz.x) / (xyz.x + 15.0 * xyz.y + 3.0 * xyz.z);
    double vp = (9.0 * xyz.y) / (xyz.x + 15.0 * xyz.y + 3.0 * xyz.z);
    double urp = (4.0 * D65.x) / (D65.x + 15.0 * D65.y + 3.0 * D65.z);
    double vrp = (9.0 * D65.y) / (D65.x + 15.0 * D65.y + 3.0 * D65.z);
    double l = yr > epsilon ? 116.0 * pow(yr, 1.0 / 3.0) - 16 : kappa * yr;
    double u = 13.0 * l * (up - urp);
    double v = 13.0 * l * (vp - vrp);
    return (colour_t){l, u, v};
}
static colour_t luv_lchuv(colour_t luv) {
    double c = sqrt(luv.u * luv.u + luv.v * luv.v);
    double h = atan2(luv.v, luv.u) * 180 / M_PI;
    return (colour_t){luv.l, c, h};
}
static colour_t srgb_lchuv(colour_t rgb) { return luv_lchuv(xyz_luv(srgb_xyz(rgb))); }

static colour_t xyz_lab(colour_t xyz) {
    double xr = xyz.x / D65.x;
    double yr = xyz.y / D65.y;
    double zr = xyz.z / D65.z;
    double fx = xr > epsilon ? pow(xr, 1.0 / 3.0) : (kappa * xr + 16.0) / 116.0;
    double fy = yr > epsilon ? pow(yr, 1.0 / 3.0) : (kappa * yr + 16.0) / 116.0;
    double fz = zr > epsilon ? pow(zr, 1.0 / 3.0) : (kappa * zr + 16.0) / 116.0;
    double l = 116.0 * fy - 16.0;
    double a = 500.0 * (fx - fy);
    double b = 200.0 * (fy - fz);
    return (colour_t){l, a, b};
}
static colour_t lab_lchab(colour_t lab) {
    double c = sqrt(lab.a * lab.a + lab.b * lab.b);
    double h = atan2(lab.b, lab.a) * 180 / M_PI;
    return (colour_t){lab.l, c, h};
}
static colour_t srgb_lchab(colour_t rgb) { return lab_lchab(xyz_lab(srgb_xyz(rgb))); }
static colour_t adjust_lch(colour_t lch, double l, double c, double h) {
    double newh = fmod(lch.h + h, 360.0);
    return (colour_t){
        clamp(0.0, lch.l + l, 100.0),
        clamp(0.0, lch.c + c, 100.0),
        newh < 0? newh + 360.0: newh};
}
static colour_t enhance_l(colour_t lch, double factor) {
    lch.l += (lch.l < 50? -1: 1) * fabs(factor) * (factor < 0? -1: 1) * 100;
    return lch;
}
static colour_t clamp_l(colour_t lch, double low, double high) {
    lch.l = clamp(low, lch.l, high);
    return lch;
}
static colour_t clamp_c(colour_t lch, double low, double high) {
    lch.c = clamp(low, lch.c, high);
    return lch;
}
static colour_t clamp_lc(colour_t lch, double ll, double hl, double lc, double hc) {
    return clamp_l(clamp_c(lch, lc, hc), ll, hl);
}


