// Amazing Small Graphics Library
#include <stdbool.h>
#include <stdint.h>

typedef struct { float x, y; } AsgPoint;
typedef struct { AsgPoint a, b; } AsgRect;
typedef struct { float a, b, c, d, e, f; } AsgMatrix;
typedef enum { ASG_NONZERO_WINDING, ASG_EVENODD_WINDING } AsgFillRule;
typedef struct {
    uint32_t    *buf;
    int         width;
    int         height;
    float       flatness;
    float       subsamples;
    AsgMatrix   ctm;
} Asg;

typedef enum {
    ASG_PATH_SUBPATH    = 0,
    ASG_PATH_LINE       = 1,
    ASG_PATH_BEZIER3    = 2,
    ASG_PATH_BEZIER4    = 3,
} AsgPathPartType;
static int asg_path_part_type_args(AsgPathPartType type) {
    static int args[] = { 1, 1, 2, 3 };
    return args[type & 3];
}

typedef enum {
    ASG_FONT_WEIGHT_THIN = 100,
    ASG_FONT_WEIGHT_EXTRA_LIGHT = 200,
    ASG_FONT_WEIGHT_LIGHT = 300,
    ASG_FONT_WEIGHT_REGULAR = 400,
    ASG_FONT_WEIGHT_MEDIUM = 500,
    ASG_FONT_WEIGHT_SEMIBOLD = 600,
    ASG_FONT_WEIGHT_BOLD = 700,
    ASG_FONT_WEIGHT_EXTRABOLD = 800,
    ASG_FONT_WEIGHT_BLACK = 900,
} AsgFontWeight;
typedef enum {
    ASG_FONT_STRETCH_ULTRA_CONDENSED = 1,
    ASG_FONT_STRETCH_EXTRA_CONDENSED = 2,
    ASG_FONT_STRETCH_CONDENSED = 3,
    ASG_FONT_STRETCH_SEMI_CONDENSED = 4,
    ASG_FONT_STRETCH_NORMAL = 5,
    ASG_FONT_STRETCH_SEMI_EXPANDED = 6,
    ASG_FONT_STRETCH_EXPANDED = 7,
    ASG_FONT_STRETCH_EXTRA_EXPANDED = 8,
    ASG_FONT_STRETCH_ULTRA_EXPANDED = 9,
} AsgFontStretch;

typedef struct {
    int             nparts;
    int             npoints;
    int             cap;
    AsgPathPartType *types;
    AsgPoint        *points;
    AsgPoint        start;
    AsgFillRule     fill_rule;
} AsgPath;

typedef struct {
    float       scale_x;
    float       scale_y;
    
    int         nglyphs;
    int         nhmtx;
    int         long_loca;
    
    // Table pointers
    const void  *file;
    const void  *glyf;
    const void  *loca;
    const uint16_t *hmtx;
    
    // Metrics
    float       em;
    float       ascender;
    float       descender;
    float       leading;
    float       x_height;
    float       cap_height;
    AsgRect     subscript_box;
    AsgRect     superscript_box;
    
    // Style
    int         weight;
    int         stretch;
    uint8_t     panose[10];
    bool        is_italic;
    const wchar_t *family;
    const wchar_t *style_name;
    const wchar_t *name;
    
    uint16_t    cmap[65536];
} AsgOTF;

typedef struct { AsgOTF otf; } AsgFont;

const static AsgMatrix AsgIdentityMatrix = { 1, 0, 0, 1, 0, 0 };

static AsgPoint asg_pt(float x, float y) { AsgPoint p = { x, y }; return p; }
static AsgRect asg_rect(AsgPoint a, AsgPoint b) { AsgRect r; r.a=a;r.b=b; return r; }
static uint32_t asg_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000 | r << 16 | g << 8 | b;
}
uint32_t asg_blend(uint32_t bg, uint32_t fg, uint32_t a);

// GRAPHICS STATE MANAGEMENT
Asg *asg_new(void *buf, int width, int height);
void asg_free(Asg *gs);
void asg_load_identity(Asg *gs);
void asg_translate(Asg *gs, float x, float y);
void asg_scale(Asg *gs, float x, float y);
void asg_rotate(Asg *gs, float rad);
AsgPoint asg_transform_point(const AsgMatrix *ctm, AsgPoint p);
void asg_multiply_matrix(Asg *gs, const AsgMatrix * __restrict mat);

// MATRIX
void asg_matrix_identity(AsgMatrix *mat);
void asg_matrix_translate(AsgMatrix *mat, float x, float y);
void asg_matrix_scale(AsgMatrix *mat, float x, float y);
void asg_matrix_rotate(AsgMatrix *mat, float rad);
void asg_matrix_multiply(AsgMatrix * __restrict a, const AsgMatrix * __restrict b);

// IMMEDIATE MODE DRAWING
void asg_clear(
    const Asg *gs,
    uint32_t color);
void asg_stroke_line(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    uint32_t color);
void asg_stroke_rectangle(
    const Asg *gs,
    AsgPoint nw,
    AsgPoint se,
    uint32_t color);
void asg_fill_rectangle(
    const Asg *gs,
    AsgPoint nw,
    AsgPoint se,
    uint32_t color);
void asg_stroke_circle(
    const Asg *gs,
    AsgPoint at,
    float radius,
    uint32_t color);
void asg_fill_circle(
    const Asg *gs,
    AsgPoint centre,
    float radius,
    uint32_t color);
void asg_stroke_bezier3(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    uint32_t color);
void asg_stroke_bezier4(
    const Asg *gs,
    AsgPoint a,
    AsgPoint b,
    AsgPoint c,
    AsgPoint d,
    uint32_t color);

// PATHS
AsgPath *asg_new_path(void);
void asg_free_path(AsgPath *path);
AsgRect asg_get_bounding_box(AsgPath *path);
void asg_add_subpath(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint p);
void asg_close_subpath(AsgPath *path);
void asg_add_line(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint b);
void asg_add_bezier3(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint b,
    AsgPoint c);
void asg_add_bezier4(
    AsgPath *path,
    const AsgMatrix *ctm,
    AsgPoint b,
    AsgPoint c,
    AsgPoint d);
void asg_stroke_path(
    const Asg *gs,
    const AsgPath *path,
    uint32_t color);
void asg_fill_path(
    const Asg *gs,
    const AsgPath *path,
    uint32_t color);


// FONTS & TEXT
    AsgFont *asg_open_font(const wchar_t *name);
    AsgFont *asg_open_font_variant(const wchar_t *family, AsgFontWeight weight, bool italic, AsgFontStretch stretch);
    wchar_t **asg_list_fonts(int *countp);
    AsgFont *asg_load_font(const void *file, int font_index);
    void asg_free_font(AsgFont *font);
    void asg_scale_font(AsgFont *font, float height, float width);
    float asg_get_font_ascender(const AsgFont *font);
    float asg_get_font_descender(const AsgFont *font);
    float asg_get_font_leading(const AsgFont *font);
    float asg_get_font_em(const AsgFont *font);
    AsgFontWeight asg_get_font_weight(const AsgFont *font);
    AsgFontStretch asg_get_font_stretch(const AsgFont *font);
    AsgRect asg_get_font_subscript(const AsgFont *font);
    AsgRect asg_get_font_superscript(const AsgFont *font);
    bool asg_is_font_monospaced(const AsgFont *font);
    bool asg_is_font_italic(const AsgFont *font);
    const wchar_t *asg_get_font_family(const AsgFont *font);
    const wchar_t *asg_get_font_name(const AsgFont *font);
    const wchar_t *asg_get_font_style_name(const AsgFont *font);

    // CHARACTER BASED
        float asg_get_char_lsb(const AsgFont *font, unsigned c);
        float asg_get_char_width(const AsgFont *font, unsigned c);
        float asg_get_chars_width(const AsgFont *font, const char chars[], int len);
        float asg_get_wchars_width(const AsgFont *font, const wchar_t chars[], int len);
        float asg_fill_char(
            Asg *gs,
            const AsgFont *font,
            AsgPoint at,
            unsigned g,
            uint32_t color);
        float asg_fill_string_utf8(
            Asg *gs,
            const AsgFont *font,
            AsgPoint at,
            const uint8_t chars[],
            int len,
            uint32_t color);
        float asg_fill_string(
            Asg *gs,
            const AsgFont *font,
            AsgPoint at,
            const uint16_t chars[],
            int len,
            uint32_t color);
        AsgPath *asg_get_char_path(
            const AsgFont *font,
            const AsgMatrix *ctm,
            unsigned c);
    // GLYPH BASED
        int asg_get_glyph(const AsgFont *font, unsigned c);
        float asg_get_glyph_lsb(const AsgFont *font, unsigned g);
        float asg_get_glyph_width(const AsgFont *font, unsigned g);
        float asg_fill_glyph(
            Asg *gs,
            const AsgFont *font,
            AsgPoint at,
            unsigned g,
            uint32_t color);
        AsgPath *asg_get_glyph_path(
            const AsgFont *font,
            const AsgMatrix *ctm,
            unsigned g);

// EXTERNAL FORMATS

// UNICODE
    uint16_t *asg_utf8_to_utf16(const uint8_t *input, int len, int *lenp);
    uint8_t *asg_utf16_to_utf8(const uint16_t *input, int len, int *lenp);

// OPENTYPE FORMAT (OTF) / TRUE TYPE FORMAT (TTF) FONTS
    AsgOTF *asg_load_otf(const void *file, int font_index);
    void asg_free_otf(AsgOTF *font);
    void asg_scale_otf(AsgOTF *font, float height, float width);
    float asg_get_otf_ascender(const AsgOTF *font);
    float asg_get_otf_descender(const AsgOTF *font);
    float asg_get_otf_leading(const AsgOTF *font);
    float asg_get_otf_em(const AsgOTF *font);
    AsgFontWeight asg_get_otf_weight(const AsgOTF *font);
    AsgFontStretch asg_get_otf_stretch(const AsgOTF *font);
    AsgRect asg_get_otf_subscript(const AsgOTF *font);
    AsgRect asg_get_otf_superscript(const AsgOTF *font);
    bool asg_otf_is_monospaced(const AsgOTF *font);
    bool asg_otf_is_italic(const AsgOTF *font);
    const wchar_t *asg_get_otf_family(const AsgOTF *font);
    const wchar_t *asg_get_otf_name(const AsgOTF *font);
    const wchar_t *asg_get_otf_style_name(const AsgOTF *font);
    

    // CHARACTER-BASED
    float asg_get_otf_char_lsb(const AsgOTF *font, unsigned c);
    float asg_get_otf_char_width(const AsgOTF *font, unsigned c);
    float asg_otf_draw_char(
        Asg *gs,
        const AsgOTF *font,
        AsgPoint at,
        unsigned c,
        uint32_t color);
    float asg_otf_fill_string_utf8(
        Asg *gs,
        const AsgOTF *font,
        AsgPoint at,
        const char chars[],
        int len,
        uint32_t color);
    float asg_otf_fill_string(
        Asg *gs,
        const AsgOTF *font,
        AsgPoint at,
        const wchar_t chars[],
        int len,
        uint32_t color);
    AsgPath *asg_get_otf_glyph_path(
        const AsgOTF *font,
        const AsgMatrix *ctm,
        unsigned c);

    // GLYPH-BASED
    int asg_get_otf_glyph(const AsgOTF *font, unsigned c);
    float asg_get_otf_glyph_lsb(const AsgOTF *font, unsigned g);
    float asg_get_otf_glyph_width(const AsgOTF *font, unsigned g);
    float asg_otf_draw_glyph(
        Asg *gs,
        const AsgOTF *font,
        AsgPoint at,
        unsigned g,
        uint32_t color);
    AsgPath *asg_get_otf_glyph_path(
        const AsgOTF *font,
        const AsgMatrix *ctm,
        unsigned g);



// SVG
    AsgPath *asg_get_svg_path(
        const char *svg,
        const AsgMatrix *initial_ctm);