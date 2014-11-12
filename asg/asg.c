// TODO: Handle OpenType
// TODO: Handle transformed coordinates in immediate mode drawing
// TODO: Get family names for matching weights and variants
#define BEZIER_RECURSION_LIMIT 10
#define _USE_MATH_DEFINES
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <emmintrin.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "asg.h"

#define MIN(X,Y) ((X) < (Y)? (X): (Y))
#define MAX(X,Y) ((X) > (Y)? (X): (Y))

static bool within_int(int a, int b, int c) {
    return a <= b && b < c;
}
static int clamp_int(int a, int b, int c) {
    return b <= a? a: b >= c? c: b;
}
static float distance(AsgPoint a, AsgPoint b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrt(dx*dx + dy*dy);
}
static float dist(float x, float y) {
    return sqrt(x*x + y*y);
}
static AsgPoint mid(AsgPoint a, AsgPoint b) {
    return asg_pt((a.x + b.x) / 2, (a.y + b.y) / 2);
}

static float fraction(float x) {
    return x - floor(x);
}
uint32_t asg_blend(uint32_t bg, uint32_t fg, uint32_t a) {
    if (a == 0xff)
        return fg;
    else if (a == 0)
        return bg;
    unsigned na = 255 - a;
    unsigned rb = ((( fg & 0x00ff00ff) * a) +
                    ((bg & 0x00ff00ff) * na)) &
                    0xff00ff00;
    unsigned g = (((  fg & 0x0000ff00) * a) +
                    ((bg & 0x0000ff00) * na)) &
                    0x00ff0000;
    return (rb | g) >> 8;
}
Asg *asg_new(void *buf, int width, int height) {
    Asg *gs = malloc(sizeof *gs);
    gs->width = width;
    gs->height = height;
    if (buf)
        gs->buf = buf;
    else
        gs->buf = malloc(width * height * sizeof *gs->buf);
    gs->flatness = 1.001f;
    gs->subsamples = 3;
    asg_load_identity(gs);
    return gs;
}
void asg_free(Asg *gs) {
    if (gs) {
        free(gs->buf);
        free(gs);
    }
}
void asg_matrix_identity(AsgMatrix *mat) {
    mat->a = 1;
    mat->b = 0;
    mat->c = 0;
    mat->d = 1;
    mat->e = 0;
    mat->f = 0;
}
void asg_matrix_translate(AsgMatrix *mat, float x, float y) {
    mat->e += x;
    mat->f += y;
}
void asg_matrix_scale(AsgMatrix *mat, float x, float y) {
    mat->a *= x;
    mat->c *= x;
    mat->e *= x;
    mat->b *= y;
    mat->d *= y;
    mat->f *= y;
}
void asg_matrix_rotate(AsgMatrix *mat, float rad) {
    AsgMatrix old = *mat;
    float m = cos(rad);
    float n = sin(rad);
    mat->a = old.a * m - old.b * n;
    mat->b = old.a * n + old.b * m;
    mat->c = old.c * m - old.d * n;
    mat->d = old.c * n + old.d * m;
    mat->e = old.e * m - old.f * n;
    mat->f = old.e * n + old.f * m;
}
void asg_matrix_apply(AsgMatrix * __restrict a, const AsgMatrix * __restrict b) {
    AsgMatrix old = *a;
    
    a->a = old.a * b->a + old.b * b->c;
    a->c = old.c * b->a + old.d * b->c;
    a->e = old.e * b->a + old.f * b->c + b->e;
    
    a->b = old.a * b->b + old.b * b->d;
    a->d = old.c * b->b + old.d * b->d;
    a->f = old.e * b->b + old.f * b->d + b->f;
}
void asg_load_identity(Asg *gs) {
    asg_matrix_identity(&gs->ctm);
}
void asg_translate(Asg *gs, float x, float y) {
    asg_matrix_translate(&gs->ctm, x, y);
}
void asg_scale(Asg *gs, float x, float y) {
    asg_matrix_scale(&gs->ctm, x, y);
}
void asg_rotate(Asg *gs, float rad) {
    asg_matrix_rotate(&gs->ctm, rad);
}
void asg_apply_matrix(Asg *asg, const AsgMatrix *mat) {
    asg_matrix_apply(&asg->ctm, mat);
}
AsgPoint asg_transform_point(const AsgMatrix *ctm, AsgPoint p) {
    AsgPoint out = {
        ctm->a * p.x + ctm->c * p.y + ctm->e,
        ctm->b * p.x + ctm->d * p.y + ctm->f
    };
    return out;
}
void asg_clear(
    const Asg *gs,
    uint32_t color)
{
    uint32_t *p = gs->buf;
    uint32_t *end = gs->buf + gs->width * gs->height;
    while (p < end) *p++ = color;
}
void asg_stroke_line(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    uint32_t color)
{
   int x0 = a.x;
   int y0 = a.y;
   int x1 = b.x;
   int y1 = b.y;
   
   int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
   int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; 
   int err = dx-dy, e2, x2;                       /* error value e_xy */
   int ed = dx+dy == 0 ? 1 : sqrt((float)dx*dx+(float)dy*dy);

   uint32_t *scr = gs->buf + y0 * gs->width + x0;
   for ( ; ; ) {                                         /* pixel loop */
      if (within_int(0, x0, gs->width) && within_int(0, y0, gs->height))
        *scr = asg_blend(color, *scr, 255*abs(err-dx+dy)/ed);
      e2 = err; x2 = x0;
      if (2*e2 >= -dx) {                                    /* x step */
         if (x0 == x1) break;
         if (e2+dy < ed)
            if (within_int(0, x0, gs->width) && within_int(0, y0+sy, gs->height)) {
                uint32_t *tmp = scr + (sy > 0? gs->width: -gs->width);
                *tmp = asg_blend(color, *tmp, 255*(e2+dy)/ed);
            }
         err -= dy; x0 += sx;
         scr += sx;
      } 
      if (2*e2 <= dy) {                                     /* y step */
         if (y0 == y1) break;
         if (dx-e2 < ed)
            if (within_int(0, x2+sx, gs->width) && within_int(0, y0, gs->height)) {
                uint32_t *tmp = scr + (x2 - x0) + sx;
                *tmp = asg_blend(color, *tmp, 255*(dx-e2)/ed);
            }
         err += dx; y0 += sy; 
         scr += sy > 0? gs->width: -gs->width;
      }
    }
}
void asg_stroke_rectangle(
    const Asg *gs,
    AsgPoint nw,
    AsgPoint se,
    uint32_t color)
{
    int x0 = clamp_int(0, nw.x, gs->width - 1);
    int x1 = clamp_int(0, se.x, gs->width - 1);
    int y0 = clamp_int(0, nw.y, gs->height - 1);
    int y1 = clamp_int(0, se.y, gs->height - 1);
    
    int a = color >> 24;
    uint32_t *screen = gs->buf + y0 * gs->width;
    
    if (within_int(0, nw.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = asg_blend(screen[x], color, a * (1 - fraction(nw.y)));
    screen += gs->width;
    if (within_int(0, nw.y + 1, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = asg_blend(screen[x], color, a * fraction(nw.y));
    
    
    for (int y = y0; y <= y1; y++, screen += gs->width) {
        if (within_int(0, nw.x, gs->width))
            screen[x0] = asg_blend(screen[x0], color, a * (1 - fraction(nw.x)));
        if (within_int(0, nw.x+1, gs->width))
            screen[x0+1] = asg_blend(screen[x0+1], color, a * fraction(nw.x));
        if (within_int(0, se.x, gs->width))
            screen[x1] = asg_blend(screen[x1], color, a * (1 - fraction(se.x)));
        if (within_int(0, se.x-1, gs->width))
            screen[x1-1] = asg_blend(screen[x1-1], color, a * fraction(se.x));
    }
    
    screen -= gs->width;
    if (within_int(0, se.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = asg_blend(screen[x], color, a * (1 - fraction(se.y)));
    screen += gs->width;
    if (within_int(0, se.y + 1, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = asg_blend(screen[x], color, a * fraction(se.y));
}

void asg_fill_rectangle(
    const Asg *gs,
    AsgPoint nw,
    AsgPoint se,
    uint32_t color)
{
    int x0 = clamp_int(0, nw.x, gs->width - 1);
    int x1 = clamp_int(0, se.x, gs->width - 1);
    int y0 = clamp_int(0, nw.y, gs->height - 1);
    int y1 = clamp_int(0, se.y, gs->height - 1);
    
    int a = color >> 24;
    uint32_t *screen = gs->buf + y0 * gs->width;
    
    if (within_int(0, nw.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = asg_blend(screen[x], color, a * (1 - fraction(nw.y)));
    screen += gs->width;
    for (int y = y0; y < y1; y++, screen += gs->width) {
        if (within_int(0, nw.x, gs->width))
            screen[x0] = asg_blend(screen[x0], color, a * (1 - fraction(nw.x)));
        for (int x = x0 + 1; x < x1; x++)
            screen[x] = asg_blend(screen[x], color, a);
        if (within_int(0, se.x-1, gs->width))
            screen[x1-1] = asg_blend(screen[x1-1], color, a * fraction(se.x));
    }
    if (within_int(0, se.y, gs->height))
        for (int x = x0; x <= x1; x++)
            screen[x] = asg_blend(screen[x], color, a * fraction(se.y));
}

void asg_stroke_circle(
    const Asg *gs,
    AsgPoint centre,
    float radius,
    uint32_t color)
{
    int x = -radius, y = 0, e = 2 - 2 * radius;
    int cx = centre.x;
    int cy = centre.y;
    uint32_t *screen = gs->buf + cy * gs->width + cx;
    do {
        if (within_int(0, cx - x, gs->width) && within_int(0, cy + y, gs->height))
            screen[-x + y*gs->width] = color;
        if (within_int(0, cx - y, gs->width) && within_int(0, cy - x, gs->height))
            screen[-y - x*gs->width] = color;
        if (within_int(0, cx + x, gs->width) && within_int(0, cy - y, gs->height))
            screen[x - y*gs->width] = color;
        if (within_int(0, cx + y, gs->width) && within_int(0, cy + x, gs->height))
            screen[y + x*gs->width] = color;
        radius = e;
        if (radius <= y) {
            y++;
            e += y * 2 + 1;
        }
        if (radius > x || e > y) {
            x++;
            e += x * 2 + 1;
        }
    } while (x < 0);
}
void asg_fill_circle(
    const Asg *gs,
    AsgPoint centre,
    float radius,
    uint32_t color)
{
    // TODO Use a real circle algorithm
    int cx = centre.x;
    int cy = centre.y;
    int min_y = clamp_int(0, centre.y - radius, gs->height - 1);
    int max_y = clamp_int(0, centre.y + radius, gs->height - 1);
    uint32_t *screen = gs->buf + min_y * gs->width;
    for (int y = min_y; y < max_y; y++) {
        int min_x = clamp_int(0, centre.x - radius, gs->width - 1);
        int max_x = clamp_int(0, centre.x + radius, gs->width - 1);
        bool first = false;
        
        for (int x = min_x; x < max_x; x++) {
            float dx = (cx - x);
            float dy = (cy - y);
            float d = sqrt(dx*dx + dy*dy);
            if (d < radius)
                if (within_int(0,x,gs->width) && within_int(0,y,gs->height)) {
                    if (!first) {
                        screen[x] = asg_blend(screen[x],
                            color,
                            (color >> 24) * (1 - fraction(centre.x - radius)));
                        first = true;
                    } else
                        screen[x] = asg_blend(screen[x], color, color >> 24);
                }
        }
        if (max_x == centre.x + radius)
            screen[max_x] = asg_blend(screen[max_x],
                color,
                (color >> 24) * fraction(centre.x + radius));
        screen += gs->width;
    }
}

static void stroke_bezier3(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    uint32_t color,
    int n)
{
    // TODO Make quadratic and cubic agree on what flatness is
    if (!n) {
        asg_stroke_line(gs, a, c, color);
        return;
    }
    AsgPoint m = { (a.x + 2 * b.x + c.x) / 4, (a.y + 2 * b.y + c.y) / 4 };
    AsgPoint d = { (a.x + c.x) / 2 - m.x, (a.y + c.y) / 2 - m.y };
    if (d.x * d.x + d.y * d.y > .05) {
        stroke_bezier3(gs, a, asg_pt((a.x + b.x) / 2, (a.y + b.y) / 2), m, color, n - 1);
        stroke_bezier3(gs, m, asg_pt((b.x + c.x) / 2, (b.y + c.y) / 2), c, color, n - 1);
    } else
        asg_stroke_line(gs, a, c, color);
}

void asg_stroke_bezier3(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    uint32_t color)
{
    stroke_bezier3(gs, a, b, c, color, BEZIER_RECURSION_LIMIT);
}
static void stroke_bezier4(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    AsgPoint d,
    uint32_t color,
    int n)
{
    if (!n) {
        asg_stroke_line(gs, a, d, color);
        return;
    }
    float d1 = dist(a.x - b.x, a.y - b.y);
    float d2 = dist(b.x - c.x, b.y - c.y);
    float d3 = dist(c.x - d.x, c.y - d.y);
    float d4 = dist(a.x - d.x, a.y - d.y);
    if (d1 + d2 + d3 < gs->flatness * d4)
        asg_stroke_line(gs, a, d, color);
    else {
        AsgPoint mab = mid(a, b);
        AsgPoint mbc = mid(b, c);
        AsgPoint mcd = mid(c, d);
        AsgPoint mabc = mid(mab, mbc);
        AsgPoint mbcd = mid(mbc, mcd);
        AsgPoint mabcd = mid(mabc, mbcd);
        stroke_bezier4(gs, a, mab, mabc, mabcd, color, n - 1);
        stroke_bezier4(gs, mabcd, mbcd, mcd, d, color, n - 1);
    }
}
void asg_stroke_bezier4(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    AsgPoint d,
    uint32_t color)
{
    stroke_bezier4(gs, a, b, c, d, color, BEZIER_RECURSION_LIMIT);
}

AsgPath *asg_new_path(void) {
    AsgPath *path = calloc(1, sizeof *path);
    return path;
}

void asg_free_path(AsgPath *path) {
    if (path) {
        free(path->types);
        free(path->points);
        free(path);
    }
}

static void add_part(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPathPartType type,
    ...)
{
    if (path->nparts + 1 >= path->cap) {
        path->cap = path->cap? path->cap * 2: 16;
        path->types = realloc(path->types, path->cap * sizeof *path->types);
        path->points = realloc(path->points, path->cap * 4 * sizeof *path->points);
    }
    
    va_list ap;
    va_start(ap, type);
    path->types[path->nparts] = type;
    for (int i = 0; i < asg_path_part_type_args(type); i++)
        path->points[path->npoints + i] = asg_transform_point(ctm, *va_arg(ap, AsgPoint*));
    va_end(ap);
    path->nparts++;
    path->npoints += asg_path_part_type_args(type);
}

void asg_add_subpath(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint p)
{
    path->start = asg_transform_point(ctm, p);
    add_part(path, ctm, ASG_PATH_SUBPATH, &p);
}

void asg_close_subpath(AsgPath *path) {
    add_part(path, &AsgIdentityMatrix, ASG_PATH_LINE, &path->start);
}
void asg_add_line(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint b)
{
    add_part(path, ctm, ASG_PATH_LINE, &b);
}
void asg_add_bezier3(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint b,
    AsgPoint c)
{
    add_part(path, ctm, ASG_PATH_BEZIER3, &b, &c);
}
void asg_add_bezier4(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint b,
    AsgPoint c,
    AsgPoint d)
{
    add_part(path, ctm, ASG_PATH_BEZIER4, &b, &c, &d);
}

void asg_stroke_path(
    const Asg *gs,
    const AsgPath *path,
    uint32_t color)
{    
    AsgPoint a = {0, 0};
    for (int i = 0, ip = 0; i < path->nparts; ip += asg_path_part_type_args(path->types[i]), i++)
        switch (path->types[i]) {
        case ASG_PATH_SUBPATH:
            a = path->points[ip];
            break;
        case ASG_PATH_LINE:
            asg_stroke_line(gs, a, path->points[ip], color);
            a = path->points[ip];
            break;
        case ASG_PATH_BEZIER3:
            asg_stroke_bezier3(gs, a, path->points[ip], path->points[ip+1], color);
            a = path->points[ip+1];
            break;
        case ASG_PATH_BEZIER4:
            asg_stroke_bezier4(gs, a, path->points[ip], path->points[ip+1], path->points[ip+2], color);
            a = path->points[ip+2];
            break;
        }
}

AsgRect asg_get_bounding_box(AsgPath *path) {
    AsgRect r = { {FLT_MAX,FLT_MAX}, {FLT_MIN,FLT_MIN} };
    // TODO box is not tight since it just goes by curve control points
    for (int i = 0; i < path->npoints; i++) {
        r.a.x = MIN(r.a.x, path->points[i].x);
        r.a.y = MIN(r.a.y, path->points[i].y);
        r.b.x = MAX(r.b.x, path->points[i].x);
        r.b.y = MAX(r.b.y, path->points[i].y);
    }
    return r;
}

typedef struct {
    AsgPoint    a;
    AsgPoint    b;
    float       m;
    float       dir;
} Segment;
typedef struct {
    int     n;
    int     cap;
    Segment *segs;
} SegList;


static void add_seg(
    SegList *list,
    AsgPoint a,
    AsgPoint b)
{
    if (list->n + 1 >= list->cap) {
        list->cap = list->cap? list->cap * 2: 128;
        list->segs = realloc(list->segs, list->cap * sizeof *list->segs);
    }
    list->segs[list->n].a = a.y < b.y? a: b;
    list->segs[list->n].b = a.y < b.y? b: a;
    list->segs[list->n].dir = a.y < b.y? -1: 1;
    list->n++;
}
static void decomp_bezier3(
    SegList *list,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    float flatness,
    int n)
{    
    if (!n) {
        add_seg(list, a, c);
        return;
    }
    AsgPoint m = { (a.x + 2 * b.x + c.x) / 4, (a.y + 2 * b.y + c.y) / 4 };
    AsgPoint d = { (a.x + c.x) / 2 - m.x, (a.y + c.y) / 2 - m.y };
    if (d.x * d.x + d.y * d.y > .05) {
        decomp_bezier3(list, a, asg_pt((a.x + b.x) / 2, (a.y + b.y) / 2), m, flatness, n - 1);
        decomp_bezier3(list, m, asg_pt((b.x + c.x) / 2, (b.y + c.y) / 2), c, flatness, n - 1);
    } else
        add_seg(list, a, c);
}
static void decomp_bezier4(
    SegList *list,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    AsgPoint d,
    float flatness,
    int n)
{    
    if (!n) {
        add_seg(list, a, d);
        return;
    }
    float d1 = dist(a.x - b.x, a.y - b.y);
    float d2 = dist(b.x - c.x, b.y - c.y);
    float d3 = dist(c.x - d.x, c.y - d.y);
    float d4 = dist(a.x - d.x, a.y - d.y);
    if (d1 + d2 + d3 >= flatness * d4) {
        AsgPoint mab = mid(a, b);
        AsgPoint mbc = mid(b, c);
        AsgPoint mcd = mid(c, d);
        AsgPoint mabc = mid(mab, mbc);
        AsgPoint mbcd = mid(mbc, mcd);
        AsgPoint mabcd = mid(mabc, mbcd);
        decomp_bezier4(list, a, mab, mabc, mabcd, flatness, n - 1);
        decomp_bezier4(list, mabcd, mbcd, mcd, d, flatness, n - 1);
    } else
        add_seg(list, a, d);
}
static int sort_tops(const void *ap, const void *bp) {
    const Segment * __restrict a = ap;
    const Segment * __restrict b = bp;
    return  a->a.y < b->a.y? -1:
            a->a.y > b->a.y? 1:
            a->a.x < b->a.x? -1:
            a->a.x > b->a.x? 1:
            0;
}
static void fill_evenodd(const Asg *gs, const Segment *segs, int nsegs, uint32_t color) {
    typedef struct {
        float y0;
        float y1;
        float x;
        float m;
    } Edge;
    int                     max_y = INT_MIN;
    int                     min_y = INT_MAX; // calculated later
    int                     min_x = 0; // per-line minimum used x
    int                     max_x = gs->width - 1; // per-line maximum used x
    for (int i = 0; i < nsegs; i++) {
        if (segs[i].a.y < min_y) min_y = segs[i].a.y;
        if (segs[i].b.y > max_y) max_y = segs[i].b.y;
    }
    min_y = clamp_int(0, min_y / gs->subsamples, gs->height - 1);
    max_y = clamp_int(0, max_y / gs->subsamples, gs->height - 1);
    
    uint8_t * __restrict    buffer = malloc(gs->width);
    Edge * __restrict       edges = calloc(1, nsegs * sizeof *edges);
        
    // Rasterise each line
    int nedges = 0;
    float max_alpha = (color >> 24) / gs->subsamples;
    for (int scan_y = min_y, min_seg = 0; scan_y <= max_y; scan_y++) {
        // Clear line buffer
        if (min_x <= max_x)
            memset(buffer + min_x, 0, max_x - min_x + 1);
        min_x = gs->width;
        max_x = 0;
        
        
        for (int s = -gs->subsamples / 2; s < gs->subsamples / 2; s++) {
            float y = gs->subsamples * scan_y + s + .5;
            
            int old_nedges = nedges;
            nedges = 0;
            for (int i = 0; i < old_nedges; i++)
                if (y < edges[i].y1) {
                    if (i != nedges)
                        edges[nedges] = edges[i];
                    edges[nedges].x += edges[nedges].m;
                    nedges++;
                }
            for ( ; min_seg < nsegs; min_seg++)
                if (segs[min_seg].a.y <= y) { // starts on or just before this scanline
                    if (y < segs[min_seg].b.y) { // ends after this scanline
                        edges[nedges].y0 = segs[min_seg].a.y;
                        edges[nedges].y1 = segs[min_seg].b.y;
                        edges[nedges].m = segs[min_seg].m;
                        edges[nedges].x = segs[min_seg].a.x +
                            segs[min_seg].m * (y - segs[min_seg].a.y);
                        nedges++;
                    } // starts and ends before this scanline
                } else // starts after this scanline
                    break;
            
            // Sort edges from left to right
            for (int i = 1; i < nedges; i++)
                for (int j = i; j > 0 && edges[j - 1].x > edges[j].x; j--) {
                    Edge tmp = edges[j];
                    edges[j] = edges[j - 1];
                    edges[j - 1] = tmp;
                }
        
            // Render edges
            for (int i = 1; i < nedges; i += 2) {
                int start = clamp_int(0, edges[i - 1].x, gs->width - 1);
                int end = clamp_int(0, edges[i].x, gs->width - 1);
                if (start < min_x) min_x = start;
                if (end > max_x) max_x = end;
                
                if (start == end)
                    if (start == (int)edges[i-1].x)
                        buffer[start] += (edges[i].x - edges[i - 1].x) * max_alpha;
                    else;
                else {
                    if (start == (int)edges[i-1].x)
                        buffer[start] += (1 - fraction(edges[i-1].x)) * max_alpha;
                    for (int i = start + 1; i < end; i++)
                        buffer[i] += max_alpha;
                    if (end == (int)edges[i].x)
                        buffer[end] += fraction(edges[i].x) * max_alpha;
                }
            }
        }

        // Copy buffer to screen
        if ((gs->width & 3) == 0) {
            if (min_x & 3)
                min_x &= ~3;
            if (max_x & 3)
                max_x = clamp_int(0, max_x + 3 & ~3, gs->width);
            else
                max_x = clamp_int(0, max_x + 4, gs->width);
                
            uint32_t * __restrict   screen = gs->buf + scan_y * gs->width;
            __m128i fg = _mm_set_epi32(color, color, color, color);
            for (int i = min_x; i < max_x; i += 4) {
                __m128i a = _mm_setr_epi32(buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
                __m128i na = _mm_sub_epi32(_mm_set1_epi32(255), a);
                __m128i bg = _mm_load_si128((__m128i*)(screen + i));
                
                __m128i rb_fg = _mm_and_si128(fg, _mm_set1_epi32(0x00ff00ff));
                __m128i rb_bg = _mm_and_si128(bg, _mm_set1_epi32(0x00ff00ff));
                __m128i  g_fg = _mm_and_si128(fg, _mm_set1_epi32(0x0000ff00));
                __m128i  g_bg = _mm_and_si128(bg, _mm_set1_epi32(0x0000ff00));
                
                rb_fg = _mm_or_si128(
                            _mm_mul_epu32(rb_fg, a),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(rb_fg, 4), _mm_srli_si128(a, 4)), 4));
                rb_bg = _mm_or_si128(
                            _mm_mul_epu32(rb_bg, na),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(rb_bg, 4), _mm_srli_si128(na, 4)), 4));
                __m128i rb = _mm_and_si128(_mm_add_epi32(rb_bg, rb_fg), _mm_set1_epi32(0xff00ff00));
                
                g_fg = _mm_or_si128(
                            _mm_mul_epu32(g_fg, a),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(g_fg, 4), _mm_srli_si128(a, 4)), 4));
                g_bg = _mm_or_si128(
                            _mm_mul_epu32(g_bg, na),
                            _mm_slli_si128(_mm_mul_epu32(_mm_srli_si128(g_bg, 4), _mm_srli_si128(na, 4)), 4));
                __m128i g = _mm_and_si128(_mm_add_epi32(g_bg, g_fg), _mm_set1_epi32(0x00ff0000));
                
                *(__m128i*)(screen + i) = _mm_srli_epi32(_mm_or_si128(rb, g), 8);
            }
        } else {
            uint32_t * __restrict   screen = gs->buf + scan_y * gs->width;
            for (int i = min_x; i <= max_x; i++)
                if (buffer[i]) screen[i] = asg_blend(screen[i], color, buffer[i]);
        }
    }
    free(buffer);
    free(edges);
}
static void fill_nonzero(const Asg *gs, const Segment *segs, int nsegs, uint32_t color) {
    // TODO Do even-odd fill rule
    fill_evenodd(gs, segs, nsegs, color);
}

void asg_fill_path(
    const Asg *gs,
    const AsgPath *path,
    uint32_t color)
{
    if (path->nparts == 0) return;
    
    SegList list = { 0 };
    
    // Decompose curves into a list of lines
    AsgPoint a = {0, 0};
    for (int i = 0, ip = 0; i < path->nparts; ip += asg_path_part_type_args(path->types[i]), i++)
        switch (path->types[i]) {
        case ASG_PATH_SUBPATH:
            a = path->points[ip];
            break;
        case ASG_PATH_LINE:
            add_seg(&list, a, path->points[ip]);
            a = path->points[ip];
            break;
        case ASG_PATH_BEZIER3:
            decomp_bezier3(&list, a, path->points[ip], path->points[ip+1], gs->flatness, BEZIER_RECURSION_LIMIT);
            a = path->points[ip+1];
            break;
        case ASG_PATH_BEZIER4:
            decomp_bezier4(&list, a, path->points[ip], path->points[ip+1], path->points[ip+2], gs->flatness, BEZIER_RECURSION_LIMIT);
            a = path->points[ip+2];
            break;
        }
        
    // Subsample in Y direction
    for (int i = 0; i < list.n; i++) {
        list.segs[i].a.y *= gs->subsamples;
        list.segs[i].b.y *= gs->subsamples;
        list.segs[i].m = list.segs[i].a.y == list.segs[i].b.y
            ? 0
            : (list.segs[i].b.x - list.segs[i].a.x) / (list.segs[i].b.y - list.segs[i].a.y);
    }
    
    // Sort line segments by their tops
    const Segment *const __restrict segs = list.segs;
    const int nsegs = list.n;
    qsort(list.segs, nsegs, sizeof *segs, sort_tops);
    
    if (path->fill_rule == ASG_EVENODD_WINDING)
        fill_evenodd(gs, segs, nsegs, color);
    else
        fill_nonzero(gs, segs, nsegs, color);
    
    free(list.segs);
}

#ifdef _MSC_VER
    #define be32(x) _byteswap_ulong(x)
    #define be16(x) _byteswap_ushort(x)
#else
    #define be32(x) __builtin_bswap32(x)
    #define be16(x) __builtin_bswap16(x)
#endif
static void unpack(const void **datap, const char *fmt, ...) {
    const uint8_t *data = *datap;
    va_list ap;
    va_start(ap, fmt);
    for ( ; *fmt; fmt++)
        switch (*fmt) {
        case 'b': (uint8_t*)data; data++; break;
        case 's': (uint16_t*)data; data += 2; break;
        case 'l': (uint32_t*)data; data += 4; break;
        case 'B': *va_arg(ap, uint8_t*) = *(uint8_t*)data; data++; break;
        case 'S': *va_arg(ap, uint16_t*) = be16(*(uint16_t*)data); data += 2; break;
        case 'L': *va_arg(ap, uint32_t*) = be32(*(uint32_t*)data); data += 4; break;
        }
    va_end(ap);
    *datap = data;
}

AsgOTF *asg_otf_load(const void *file, int font_index) {

    // Check that this is an OTF
    uint32_t ver;
    uint16_t ntab;
    const void *header = file;
collection_item:
    unpack(&header, "LSsss", &ver, &ntab);
    
    if (ver == 0x00010000) ;
    else if (ver == 'ttcf') { // TrueType Collection
        header = file;
        uint32_t nfonts;
        unpack(&header, "lLL", &ver, &nfonts);
        if (ver != 0x00010000 && ver != 0x00020000)
            return NULL;
        if (font_index >= nfonts)
            return NULL;
        header = (const char*)file + be32(((uint32_t*)header)[font_index]);
        goto collection_item;
    } else
        return NULL;
    
    // Find the tables we need
    const void *head = NULL;
    const void *hhea = NULL;
    const void *maxp = NULL;
    const void *cmap = NULL;
    const void *hmtx = NULL;
    const void *glyf = NULL;
    const void *loca = NULL;
    const void *os2  = NULL;
    for (unsigned i = 0; i < ntab; i++) {
        uint32_t tag, offset;
        unpack(&header, "LlLl", &tag, &offset);
        const void *address = (char*)file + offset;
        switch (tag) {
        case 'head': head = address; break;
        case 'hhea': hhea = address; break;
        case 'maxp': maxp = address; break;
        case 'hmtx': hmtx = address; break;
        case 'glyf': glyf = address; break;
        case 'loca': loca = address; break;
        case 'cmap': cmap = address; break;
        case 'OS/2': os2  = address; break;
        }
    }
    
    AsgOTF *font = calloc(1, sizeof *font);
    font->hmtx = hmtx;
    font->glyf = glyf;
    font->loca = loca;
    
    // 'head' table
    uint16_t em, long_loca;
    unpack(&head, "llllsSllllsssssssSs", &em, &long_loca);
    font->em = em;
    font->long_loca = long_loca;
    
    // 'hhea' table
    uint16_t nhmtx;
    unpack(&hhea, "lsssssssssssssssS", &nhmtx);
    font->nhmtx = nhmtx;
    
    // 'os/2' table
    int16_t ascender, descender, leading;
    unpack(&os2, "ssssssssssssssssbbbbbbbbbbllllbbbbsssSSS", &ascender, &descender, &leading);
    font->ascender = ascender;
    font->descender = descender;
    font->leading = leading
        ? leading
        : (font->em - (font->ascender - font->descender)) + font->em * .1f;
    
    // 'maxp' table
    uint16_t nglyphs;
    unpack(&maxp, "lSsssssssssssss", &nglyphs);
    font->nglyphs = nglyphs;
    
    // cmap table
    uint16_t nencodings;
    const void *encodings = cmap;
    const void *encoding_table = NULL;
    unpack(&encodings, "sS", &nencodings);
    for (int i = 0; i < nencodings; i++) {
        uint16_t platform, encoding;
        uint32_t offset;
        unpack(&encodings, "SSL", &platform, &encoding, &offset);
        
        if ((platform == 3 || platform == 0) && encoding == 1) // Windows UCS (preferred)
            encoding_table = (char*)cmap + offset;
        else if (!encoding_table && platform == 1 && encoding == 0) // Symbol
            encoding_table = (char*)cmap + offset;
    }
    
    if (encoding_table == NULL);
    switch (encoding_table? be16(*(uint16_t*)encoding_table): -1) {
    case 0: {
            uint16_t len;
            unpack(&encoding_table, "sssSsss", &len);
            len -= 6;
            for (int i = 0; i < len; i++)
                font->cmap[i] = ((uint8_t*)encoding_table)[i];
            break;
        }
    case 4: {
            uint16_t nseg;
            unpack(&encoding_table, "sssSsss", &nseg);
            nseg /= 2;
            
            const uint16_t *endp    = encoding_table;
            const uint16_t *startp  = endp + nseg + 1;
            const uint16_t *deltap  = startp + nseg;
            const uint16_t *offsetp = deltap + nseg;
            for (int i = 0; i < nseg; i++) {
                int end     = be16(endp[i]);
                int start   = be16(startp[i]);
                int delta   = be16(deltap[i]);
                int offset  = be16(offsetp[i]);
                if (offset)
                    for (int c = start; c <= end; c++) {
                        int g = be16(offsetp[offset/2 + (c - start) + i]);
                        font->cmap[c] = g? g + delta: 0;
                    }
                else
                    for (int c = start; c <= end; c++)
                        font->cmap[c] = c + delta;
            }
        }
        break;
    }
    
    
    font->file = file;
    font->scale_x = font->scale_y = 1;
    return font;
}
void asg_otf_free(AsgOTF *font) {
    if (font) {
        free((void*)font->file);
        free(font);
    }
}
void asg_otf_scale(AsgOTF *font, float height, float width) {
    float s = font->ascender - font->descender;
    if (width <= 0)
        width = height;
    font->scale_x = width / font->em;
    font->scale_y = height / font->em;
}

float asg_otf_get_ascender(const AsgOTF *font) {
    return font->ascender * font->scale_y;
}
float asg_otf_get_descender(const AsgOTF *font) {
    return font->descender * font->scale_y;
}
float asg_otf_get_leading(const AsgOTF *font) {
    return font->leading * font->scale_y;
}

float asg_otf_get_glyph_lsb(const AsgOTF *font, unsigned g) {
    return  g < font->nhmtx?    be16(font->hmtx[g * 2 + 1]) * font->scale_x:
            g < font->nglyphs?  be16(font->hmtx[(font->nhmtx - 1) * 2 + 1]) * font->scale_x:
            0;
}
float asg_otf_get_glyph_width(const AsgOTF *font, unsigned g) {
    return  g < font->nhmtx?    be16(font->hmtx[g * 2]) * font->scale_x:
            g < font->nglyphs?  be16(font->hmtx[(font->nhmtx - 1) * 2]) * font->scale_x:
            0;
}

void glyph_path(AsgPath *path, const AsgOTF *font, const AsgMatrix *ctm, unsigned g) {
    const void          *data;
    {
        const uint32_t      *loca32 = font->loca;
        const uint16_t      *loca16 = font->loca;
        g &= 0xffff;
        data = font->long_loca?
                loca32[g] == loca32[g + 1]? NULL:
                (char*)font->glyf + be32(loca32[g]):
            loca16[g] == loca16[g + 1]? NULL:
                (char*)font->glyf + be16(loca16[g]) * 2;
        if (!data)
            return;
    }
    
    int16_t ncontours;
    unpack(&data, "Sssss", &ncontours);
    
    if (ncontours < 0) {
        uint16_t flags, glyph;
        int16_t arg1, arg2;
        do {
            unpack(&data, "SS", &flags, &glyph);
            
            AsgMatrix new_ctm = AsgIdentityMatrix;
            
            if (flags & 1) // args are words
                unpack(&data, "SS", &arg1, &arg2);
            else { // args are bytes
                uint8_t a, b;
                unpack(&data, "BB", &a, &b);
                arg1 = a;
                arg2 = b;
            }
            
            if (flags & 2) // args are x & y
                asg_matrix_translate(&new_ctm, arg1, arg2);
            else {
                // TODO: "matching points"
            }
            
            int16_t sx = 1, sy = 1;
            if (flags & 8) { // scale
                unpack(&data, "S", &sx);
                sy = sx;
            } else if (flags & 64) // x & y scale
                unpack(&data, "SS", &sx, &sy);
            else if (flags & 128) // 2x2 matrix
                unpack(&data, "SS", &sx, &sy);
                // TODO: handle a, b scales
            
            asg_matrix_scale(&new_ctm, sx, sy);
            asg_matrix_apply(&new_ctm, ctm);
            glyph_path(path, font, &new_ctm, glyph);
        } while (flags & 32);
    } else {
        const uint16_t  *ends = data;
        int             npoints = be16(ends[ncontours - 1]) + 1;
        int             ninstr = be16(ends[ncontours]);
        const uint8_t   *flags = (const uint8_t*)(ends + ncontours + 1) + ninstr;
        const uint8_t   *f = flags;
        int             xsize = 0;
        
        // Get length of flags & x coordinates
        for (int i = 0; i < npoints; i++) {
            int dx =    *f & 2? 1:
                        *f & 16? 0:
                        2;
            if (*f & 8) {
                i += f[1];
                dx *= f[1] + 1;
                f += 2;
            } else
                f++;
            xsize += dx;
        }
        const uint8_t   *xs = f;
        const uint8_t   *ys = xs + xsize;
        AsgPoint a = {0, 0};
        AsgPoint b;
        bool in_curve = false;
        
        
        int vx = 0;
        int vy = 0;
        AsgPoint start;
        for (int i = 0, rep = 0, end = 0; i < npoints; i++) {
            int dx =    *flags & 2? *flags & 16? *xs++: -*xs++:
                        *flags & 16? 0:
                        (int16_t)be16(((int16_t*)(xs+=2))[-1]);
            int dy =    *flags & 4? *flags & 32? *ys++: -*ys++:
                        *flags & 32? 0:
                        (int16_t)be16(((int16_t*)(ys+=2))[-1]);
            vx += dx;
            vy += dy;
            AsgPoint p = {vx, vy};
    
            
            if (i == end) {
                end = be16(*ends++) + 1;
                if (in_curve)
                    asg_add_bezier3(path, ctm, b, start);
                else
                    asg_close_subpath(path);
                if (i != npoints - 1)
                    asg_add_subpath(path, ctm, p);
                start = a = p;
                in_curve = false;
            } else if (~*flags & 1) // curve
                if (in_curve) {
                    AsgPoint q = mid(b, p);
                    asg_add_bezier3(path, ctm, b, q);
                    a = q;
                    b = p;
                } else {
                    b = p;
                    in_curve = true;
                }
            else if (in_curve) { // curve to line
                asg_add_bezier3(path, ctm, b, p);
                a = p;
                in_curve = false;
            } else { // line to line
                asg_add_line(path, ctm, p);
                a = p;
            }
            
            if (rep) {
                if (--rep == 0)
                    flags += 2;
            } else if (*flags & 8) {
                rep = flags[1];
                if (!rep) flags += 2;
            } else
                flags++;
        }
        if (in_curve)
            asg_add_bezier3(path, ctm, b, start);
        else
            asg_close_subpath(path);
    }
}
AsgPath *asg_otf_get_glyph_path(
    const AsgOTF *font,
    const AsgMatrix *ctm,
    unsigned g)
{
    AsgPath *path = asg_new_path();
    AsgMatrix new_ctm = {1,0,0, 1,0,0};
    asg_matrix_translate(&new_ctm, 0, -font->ascender);
    asg_matrix_scale(&new_ctm, font->scale_x, -font->scale_y);
    asg_matrix_apply(&new_ctm, ctm);
    glyph_path(path, font, &new_ctm, g);
    return path;
}
float asg_otf_draw_glyph(
    Asg *gs,
    const AsgOTF *font,
    AsgPoint at,
    unsigned g,
    uint32_t color)
{
    AsgMatrix ctm = gs->ctm;
    asg_matrix_translate(&ctm, at.x, at.y);
    AsgPath *path = asg_otf_get_glyph_path(font, &ctm, g);
    if (path) {
        asg_fill_path(gs, path, color);
        asg_free_path(path);
    }
    return asg_otf_get_glyph_width(font, g);
}

int asg_otf_get_glyph(const AsgOTF *font, unsigned c) {
    return font->cmap[c & 0xffff];
}
float asg_otf_get_char_lsb(const AsgOTF *font, unsigned c) {
    return asg_otf_get_glyph_lsb(font, asg_otf_get_glyph(font, c));
}
float asg_otf_get_char_width(const AsgOTF *font, unsigned c) {
    return asg_otf_get_glyph_width(font, asg_otf_get_glyph(font, c));
}
AsgPath *asg_otf_get_char_path(const AsgOTF *font, const AsgMatrix *ctm, unsigned c) {
    return asg_otf_get_glyph_path(font, ctm, asg_otf_get_glyph(font, c));
}
float asg_otf_draw_char(
    Asg *gs,
    const AsgOTF *font,
    AsgPoint at,
    unsigned c,
    uint32_t color)
{
    return asg_otf_draw_glyph(gs, font, at, asg_otf_get_glyph(font, c), color);
}

#define trailing(n) ((utf[n] & 0xC0) == 0x80)
#define overlong(n) do { if (c < n) c = 0xfffd; } while(0)
float asg_otf_draw_chars(
    Asg *gs,
    const AsgOTF *font,
    AsgPoint at,
    const char chars[],
    int len,
    uint32_t color)
{
    if (len < 0) len = strlen(chars);
    const uint8_t *utf = chars;
    const uint8_t *end = chars + len;
    float org = at.x;
    
    while (utf < end) {
        unsigned c;
        
        if (*utf < 0x80)
            c = *utf++;
        else if (~*utf & 0x20 && utf + 1 < end && trailing(1)) { // two byte
            c =     (utf[0] & 0x1f) << 6
                    |(utf[1] & 0x3f);
            utf += 2;
            overlong(0x80);
        } else if (~*utf & 0x10 && utf + 2 < end && trailing(1) && trailing(2)) { // three byte
            c =     (utf[0] & 0x0f) << 12
                    |(utf[1] & 0x3f) << 6
                    |(utf[2] & 0x3f);
            utf += 3;
            overlong(0x800);
        } else {
            // Discard malformed or non-BMP characters
            do
                utf++;
            while ((*utf & 0xc0) == 0x80);
            c = 0xfffd; /* replacement char */
        }
        
        at.x += asg_otf_draw_char(gs, font, at, c, color);
    }
    return at.x - org;
}
float asg_otf_draw_wchars(
    Asg *gs,
    const AsgOTF *font,
    AsgPoint at,
    const wchar_t chars[],
    int len,
    uint32_t color)
{
    float org = at.x;
    if (len < 0) len = wcslen(chars);
    for (int i = 0; i < len; i++)
        at.x += asg_otf_draw_char(gs, font, at, chars[i], color);
    return at.x - org;
}

void *platform_open_font(const wchar_t *name);
wchar_t **platform_list_fonts(int *countp);

AsgFont *asg_open_font(const wchar_t *name) {
    return platform_open_font(name);
}
wchar_t **asg_list_fonts(int *countp) {
    return platform_list_fonts(countp);
}

AsgFont *asg_load_font(const void *file, int font_index) {
    return (void*)asg_otf_load((void*)file, font_index);
}
void asg_free_font(AsgFont *font) {
    asg_otf_free((void*)font);
}
void asg_scale_font(AsgFont *font, float height, float width) {
    asg_otf_scale((void*)font, height, width);
}
float asg_get_font_ascender(const AsgFont *font) {
    return asg_otf_get_ascender((void*)font);
}
float asg_get_font_descender(const AsgFont *font) {
    return asg_otf_get_descender((void*)font);
}
float asg_get_font_leading(const AsgFont *font) {
    return asg_otf_get_leading((void*)font);
}

float asg_get_char_lsb(const AsgFont *font, unsigned c) {
    return asg_otf_get_char_lsb((void*)font, c);
}
float asg_get_char_width(const AsgFont *font, unsigned c) {
    return asg_otf_get_char_width((void*)font, c);
}
float asg_get_chars_width(const AsgFont *font, const char chars[], int len) {
    float width = 0;
    if (len < 0) len = strlen(chars);
    for (int i = 0; i < len; i++) width += asg_get_char_width(font, chars[i]);
    return width;
}
float asg_get_wchars_width(const AsgFont *font, const wchar_t chars[], int len) {
    float width = 0;
    if (len < 0) len = wcslen(chars);
    for (int i = 0; i < len; i++) width += asg_get_char_width(font, chars[i]);
    return width;
}
float asg_draw_char(
    Asg *gs,
    const AsgFont *font,
    AsgPoint at,
    unsigned c,
    uint32_t color)
{
    return asg_otf_draw_char(gs, (void*)font, at, c, color);
}
float asg_draw_chars(
    Asg *gs,
    const AsgFont *font,
    AsgPoint at,
    const char chars[],
    int len,
    uint32_t color)
{
    return asg_otf_draw_chars(gs, (void*)font, at, chars, len, color);
}
float asg_draw_wchars(
    Asg *gs,
    const AsgFont *font,
    AsgPoint at,
    const wchar_t chars[],
    int len,
    uint32_t color)
{
    return asg_otf_draw_wchars(gs, (void*)font, at, chars, len, color);
}
AsgPath *asg_get_char_path(const AsgFont *font, const AsgMatrix *ctm, unsigned c) {
    return asg_otf_get_char_path((void*)font, ctm, c);
}
int asg_get_glyph(const AsgFont *font, unsigned c) {
    return asg_otf_get_glyph((void*)font, c);
}
float asg_get_glyph_lsb(const AsgFont *font, unsigned g) {
    return asg_otf_get_glyph_lsb((void*)font, g);
}
float asg_get_glyph_width(const AsgFont *font, unsigned g) {
    return asg_otf_get_glyph_width((void*)font, g);
}
float asg_draw_glyph(
    Asg *gs,
    const AsgFont *font,
    AsgPoint at,
    unsigned g,
    uint32_t color)
{
    return asg_otf_draw_glyph(gs, (void*)font, at, g, color);
}
AsgPath *asg_get_glyph_path(const AsgFont *font, const AsgMatrix *ctm, unsigned g) {
    return asg_otf_get_glyph_path((void*)font, ctm, g);
}

AsgPath *asg_get_svg_path(const char *svg, const AsgMatrix *initial_ctm) {
    static char params[256];
    if (!params['m']) {
        char *name = "mzlhvcsqt";
        int n[] = { 2, 0, 2, 1, 1, 6, 4, 4, 2 };
        memset(params, -1, 256);
        for (int i = 0; name[i]; i++)
            params[name[i]] = params[toupper(name[i])] = n[i];
    }
    
    AsgMatrix   ctm = initial_ctm? *initial_ctm: AsgIdentityMatrix;
    AsgPath     *path = asg_new_path();
    AsgPoint    cur = {0,0};
    AsgPoint    start = {0,0};
    AsgPoint    reflect = {0,0};
    AsgPoint    b, c;
    int         cmd;
    float       a[6];
    while (*svg) {
        while (isspace(*svg) || *svg==',') svg++;
        if (params[*svg] >= 0) // continue last command
            cmd = *svg++;
        
        for (int i = 0; i < params[cmd]; i++) {
            while (isspace(*svg) || *svg==',') svg++;
            a[i] = strtof(svg, (char**)&svg);
        }
        
//        printf("%c", cmd);
//        for (int i = 0; i < params[cmd]; i++)
//            printf(" %g", a[i]);
//        putchar('\n');

        switch (cmd) {
        case 'm':
            start = cur = asg_pt(cur.x + a[0], cur.y + a[1]);
            asg_add_subpath(path, &ctm, cur);
            break;
        case 'M':
            start = cur = asg_pt(a[0], a[1]);
            asg_add_subpath(path, &ctm, cur);
            break;
        case 'Z':
        case 'z':
            asg_close_subpath(path);
            cur = start;
            break;
        case 'L':
            cur = asg_pt(a[0], a[1]);
            asg_add_line(path, &ctm, cur);
            break;
        case 'l':
            cur = asg_pt(cur.x + a[0], cur.y + a[1]);
            asg_add_line(path, &ctm, cur);
            break;
        case 'h':
            cur = asg_pt(cur.x + a[0], cur.y);
            asg_add_line(path, &ctm, cur);
            break;
        case 'H':
            cur = asg_pt(a[0], cur.y);
            asg_add_line(path, &ctm, cur);
            break;
        case 'v':
            cur = asg_pt(cur.x, cur.y + a[0]);
            asg_add_line(path, &ctm, cur);
            break;
        case 'V':
            cur = asg_pt(cur.x, a[0]);
            asg_add_line(path, &ctm, cur);
            break;
        case 'c':
            b = asg_pt(cur.x + a[0], cur.y + a[1]);
            reflect = asg_pt(cur.x + a[2], cur.y + a[3]);
            cur = asg_pt(cur.x + a[4], cur.y + a[5]);
            asg_add_bezier4(path, &ctm, b, reflect, cur);
            break;
        case 'C':
            b = asg_pt(a[0], a[1]);
            reflect = asg_pt(a[2], a[3]);
            cur = asg_pt(a[4], a[5]);
            asg_add_bezier4(path, &ctm, b, reflect, cur);
            break;
        case 's':
            b = asg_pt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = asg_pt(cur.x + a[0], cur.y + a[1]);
            cur = asg_pt(cur.x + a[2], cur.y + a[3]);
            asg_add_bezier4(path, &ctm, b, reflect, cur);
            break;
        case 'S':
            b = asg_pt( cur.x + (cur.x - reflect.x),
                    cur.y + (cur.y - reflect.y));
            reflect = asg_pt(a[0], a[1]);
            cur = asg_pt(a[2], a[3]);
            asg_add_bezier4(path, &ctm, b, reflect, cur);
            break;
        case 'q':
            reflect = asg_pt(cur.x + a[0], cur.y + a[1]);
            cur = asg_pt(cur.x + a[2], cur.y + a[3]);
            asg_add_bezier3(path, &ctm, reflect, cur);
            break;
        case 'Q':
            reflect = asg_pt(a[0], a[1]);
            cur = asg_pt(a[2], a[3]);
            asg_add_bezier3(path, &ctm, reflect, cur);
            break;
        case 't':
            reflect = asg_pt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = asg_pt(cur.x + a[0], cur.y + a[1]);
            asg_add_bezier3(path, &ctm, reflect, cur);
            break;
        case 'T':
            reflect = asg_pt(cur.x + (cur.x - reflect.x),
                         cur.y + (cur.y - reflect.y));
            cur = asg_pt(a[0], a[1]);
            asg_add_bezier3(path, &ctm, reflect, cur);
            break;
        }
    }
    return path;
}
