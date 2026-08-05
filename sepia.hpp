#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Sepia {

// -- Public: scalar type aliases ------------------
using f32   = float;
using f64   = double;
using i32   = std::int32_t;
using i64   = std::int64_t;
using u8    = std::uint8_t;
using u32   = std::uint32_t;
using u64   = std::uint64_t;
using usize = std::size_t;

// ---------------------------------------------------------------------------
// Public: Physical size (millimetres) + resolution (dots per millimetre)
//
//   PhysicalSize{190.0, 120.0}  →  190 mm wide, 120 mm tall
//   Resolution{5.0}             →  5 dpmm  ≈ 127 DPI
//   Resolution::from_dpi(96)    →  convenience: 96 DPI → 3.7795 dpmm
//   Resolution::from_dpi(300)   →  300 DPI → 11.811 dpmm  (print quality)
//
// Pixel dimensions are derived:  pixels = round(mm * dpmm)
// All internal spacing constants (margins, tick sizes, font scale …) are
// multiplied by dpmm so the figure looks identical at any resolution.
// ---------------------------------------------------------------------------
struct PhysicalSize {
  f64 width_mm  = 190.0;   ///< figure width  in millimetres
  f64 height_mm = 120.0;   ///< figure height in millimetres
};

struct Resolution {
  f64 dpmm = 3.7795;       ///< dots (pixels) per millimetre

  /// Convenience: build from the more familiar DPI unit (dots per inch).
  /// 1 inch = 25.4 mm, so  dpmm = dpi / 25.4
  static constexpr Resolution from_dpi(f64 dpi) noexcept {
    return Resolution{ dpi / 25.4 };
  }

  /// Round-trip helper: convert stored dpmm back to DPI for display/logging.
  constexpr f64 to_dpi() const noexcept { return dpmm * 25.4; }
};

// --- detail: implementation plumbing, not part of the public API 
namespace detail {

inline void* aligned_alloc_impl(usize alignment, usize bytes) {
  void* ptr = nullptr;
#if defined(_WIN32)
  ptr = _aligned_malloc(bytes, alignment);
#else
  if (posix_memalign(&ptr, alignment, bytes) != 0) ptr = nullptr;
#endif
  if (!ptr) throw std::bad_alloc();
  return ptr;
}

inline void aligned_free_impl(void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

// Bump allocator for short-lived per-frame scratch work
class Arena {
public:
  explicit Arena(usize capacity)
    : capacity_(capacity)
    , buf_(static_cast<char*>(aligned_alloc_impl(64, capacity)))
  {}
  ~Arena() { aligned_free_impl(buf_); }

  Arena(const Arena&)            = delete;
  Arena& operator=(const Arena&) = delete;

  void* alloc(usize bytes, usize align = 8) {
    usize padding    = (align - (offset_ % align)) % align;
    usize new_offset = offset_ + padding + bytes;
    if (new_offset > capacity_) throw std::bad_alloc();
    void* ptr = buf_ + offset_ + padding;
    offset_ = new_offset;
    return ptr;
  }

  template <typename T, typename... Args>
  T* create(Args&&... args) {
    void* mem = alloc(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
  }

  void  reset()    { offset_ = 0; }
  usize used()     const { return offset_; }
  usize capacity() const { return capacity_; }

private:
  usize capacity_;
  usize offset_ = 0;
  char* buf_;
};

// Pixel-space rectangle; only used inside Figure layout and CoordTransform
struct Rect {
  f64 x = 0, y = 0, w = 0, h = 0;

  constexpr Rect() = default;
  constexpr Rect(f64 x, f64 y, f64 w, f64 h) : x(x), y(y), w(w), h(h) {}
};


static constexpr f64 BASELINE_DPMM = 4;

inline f64 scale(f64 base_px, f64 dpmm) noexcept {
  return base_px * (dpmm / BASELINE_DPMM);
}

} // NS detail

// --- Public: AlignedBuffer<T> ---------------------------------------
// Cache-line (64-byte) aligned contiguous buffer. Move-only
template <typename T, usize Alignment = 64>
class AlignedBuffer {
public:
  AlignedBuffer() = default;

  explicit AlignedBuffer(usize count)
    : size_(count)
    , data_(static_cast<T*>(detail::aligned_alloc_impl(Alignment, count * sizeof(T))))
  {}

  ~AlignedBuffer() { release(); }

  AlignedBuffer(const AlignedBuffer&)            = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;

  AlignedBuffer(AlignedBuffer&& o) noexcept
  : size_(o.size_), data_(o.data_) { o.data_ = nullptr; o.size_ = 0; }

  AlignedBuffer& operator=(AlignedBuffer&& o) noexcept {
    if (this != &o) { release(); size_ = o.size_; data_ = o.data_; o.data_ = nullptr; o.size_ = 0; }
    return *this;
  }

  void resize(usize count) {
    if (count == size_) return;
    release();
    size_ = count;
    data_ = static_cast<T*>(detail::aligned_alloc_impl(Alignment, count * sizeof(T)));
  }

  T*       data()        { return data_; }
  const T* data()  const { return data_; }
  usize    size()  const { return size_; }
  bool     empty() const { return size_ == 0; }

  T&       operator[](usize i)       { return data_[i]; }
  const T& operator[](usize i) const { return data_[i]; }

  T*       begin()       { return data_; }
  T*       end()         { return data_ + size_; }
  const T* begin() const { return data_; }
  const T* end()   const { return data_ + size_; }

private:
  void release() { if (data_) { detail::aligned_free_impl(data_); data_ = nullptr; size_ = 0; } }
  usize size_ = 0;
  T*    data_ = nullptr;
};

// --- Public: Color --------------------------------------
struct Color {
  u32 r = 0, g = 0, b = 0, a = 255;

  constexpr Color() = default;
  constexpr Color(u32 r, u32 g, u32 b, u32 a = 255) : r(r), g(g), b(b), a(a) {}

  static constexpr Color black()  { return {0,   0,   0};   }
  static constexpr Color white()  { return {255, 255, 255}; }
  static constexpr Color red()    { return {228, 26,  28};  }
  static constexpr Color blue()   { return {55,  126, 184}; }
  static constexpr Color green()  { return {77,  175, 74};  }
  static constexpr Color orange() { return {255, 127, 0};   }
  static constexpr Color purple() { return {152, 78,  163}; }
  static constexpr Color gray()   { return {150, 150, 150}; }

  constexpr Color with_alpha(u32 alpha) const { return {r, g, b, alpha}; }
};

// --- Public: BBox -------------------------------------------------
struct BBox {
  f64 x_min =  std::numeric_limits<f64>::max();
  f64 x_max = -std::numeric_limits<f64>::max();
  f64 y_min =  std::numeric_limits<f64>::max();
  f64 y_max = -std::numeric_limits<f64>::max();

  constexpr bool empty() const { return x_min > x_max; }

  void expand(f64 x, f64 y) {
    if (x < x_min) x_min = x;
    if (x > x_max) x_max = x;
    if (y < y_min) y_min = y;
    if (y > y_max) y_max = y;
  }

  void merge(const BBox& o) {
    if (o.empty()) return;
    expand(o.x_min, o.y_min);
    expand(o.x_max, o.y_max);
  }

  f64 width()  const { return x_max - x_min; }
  f64 height() const { return y_max - y_min; }
};

// --- Public: style enumerations ------------------------------------------------
enum class LineStyle : u32 {
  Solid, Dashed, Dotted, DashDot, None
};

enum class MarkerStyle : u32 {
  None, Circle, Square, Triangle, Cross, Diamond
};

enum class ScaleType : u32 {
  Linear, Log
};

// --- Public: params, aggregate style structs ------------------------------------------
namespace params {

struct DataStyle {
  Color color = Color::blue();
  f64   width = 1.5;
  f64   alpha = 1.0;
  LineStyle line_style = LineStyle::Solid;

  MarkerStyle marker      = MarkerStyle::None;
  f64         marker_size = 4.0;

  bool  fill       = false;
  Color fill_color = Color::blue().with_alpha(50);

  std::string label;
};

struct GridStyle {
  bool  show        = true;
  Color major_color = {220, 220, 220};
  Color minor_color = {240, 240, 240};
  f64   major_width = 1.0;
  f64   minor_width = 0.5;
  f64   major_alpha = 0.8;
  f64   minor_alpha = 0.4;
  bool  show_minor  = false;
};

struct AxisStyle {
  bool  show       = true;
  Color color      = Color::black();
  f64   width      = 1.0;
  f64   tick_size  = 5.0;
  Color tick_color = Color::black();

  ScaleType x_scale = ScaleType::Linear;
  f64       x_min   = 0.0;
  f64       x_max   = 0.0;

  ScaleType y_scale = ScaleType::Linear;
  f64       y_min   = 0.0;
  f64       y_max   = 0.0;
};

struct LegendStyle {
  bool  show     = true;
  Color bg_color = Color::white().with_alpha(230);
  Color border   = Color::gray();
  f64   padding  = 8.0;
  std::string position = "top-right";
};

struct TextStyle {
  Color       color     = Color::black();
  f64         font_size = 12.0;
  std::string font_face = "sans";
};

// ---------------------------------------------------------------------------
// LayoutStyle — margins are expressed in MILLIMETRES.
//
// The previous version used raw pixel values which made figures look different
// at different resolutions.  By storing mm values the rendering code converts
// them to pixels at render-time using the figure's dpmm.
//
// Backward-compat note: if you were passing pixel values directly (e.g.
// margin_left = 70) the visual result will be similar at 96 DPI (3.78 dpmm)
// because 70 px / 3.78 =~ 18.5 mm, which rounds back to ~70 px at that same
// resolution.  At higher DPI the margins will grow proportionally, which is
// the correct physical behaviour.
// ---------------------------------------------------------------------------
struct LayoutStyle {
  /// Margins in millimetres.  At 96 DPI (3.78 dpmm):
  ///   top=13 mm -> ~50 px   bottom=16 mm -> ~60 px
  ///   left=18.5 mm -> ~70 px   right=5.3 mm -> ~20 px
  f64   margin_top_mm    = 13.2;   ///< was 50 px @ 96 DPI baseline
  f64   margin_bottom_mm = 15.9;   ///< was 60 px
  f64   margin_left_mm   = 18.5;   ///< was 70 px
  f64   margin_right_mm  =  5.3;   ///< was 20 px
  Color background       = Color::white();

  // -----------------------------------------------------------------------
  // Legacy pixel-based setters, kept for source-level compatibility.
  // They accept pixel values and store them "as if at 96 DPI baseline" by
  // dividing by BASELINE_DPMM, so the physical size stays consistent.
  // -----------------------------------------------------------------------
  void set_margin_top_px   (f64 px) { margin_top_mm    = px / detail::BASELINE_DPMM; }
  void set_margin_bottom_px(f64 px) { margin_bottom_mm = px / detail::BASELINE_DPMM; }
  void set_margin_left_px  (f64 px) { margin_left_mm   = px / detail::BASELINE_DPMM; }
  void set_margin_right_px (f64 px) { margin_right_mm  = px / detail::BASELINE_DPMM; }
};

struct PerfParams {
  bool  lod_enable        = false;
  usize lod_target_points = 2000;
};

} // NS params

// --- Public: data, owning and non-owning series types --------------------------
namespace data {

struct DataView {
  const f64* ptr    = nullptr;
  usize      count  = 0;
  usize      stride = 1;

  f64  operator[](usize i) const { return ptr[i * stride]; }
  bool empty() const { return count == 0 || ptr == nullptr; }
};

class Series {
public:
  Series() = default;

  Series(const f64* x, const f64* y, usize n) : x_(n), y_(n) {
    std::memcpy(x_.data(), x, n * sizeof(f64));
    std::memcpy(y_.data(), y, n * sizeof(f64));
    recompute_bounds();
  }

  Series(AlignedBuffer<f64>&& x, AlignedBuffer<f64>&& y)
  : x_(std::move(x)), y_(std::move(y)) { recompute_bounds(); }

  DataView    x_view()  const { return {x_.data(), x_.size(), 1}; }
  DataView    y_view()  const { return {y_.data(), y_.size(), 1}; }
  usize       size()    const { return x_.size(); }
  const BBox& bounds()  const { return bounds_; }

  f64*       x_data()       { return x_.data(); }
  f64*       y_data()       { return y_.data(); }
  const f64* x_data() const { return x_.data(); }
  const f64* y_data() const { return y_.data(); }

  void recompute_bounds() {
    bounds_ = {};
    const usize n = x_.size();
    for (usize i = 0; i < n; ++i) bounds_.expand(x_[i], y_[i]);
  }

private:
  AlignedBuffer<f64> x_;
  AlignedBuffer<f64> y_;
  BBox bounds_;
};

class ExternalSeries {
public:
  ExternalSeries() = default;
  ExternalSeries(const f64* x, const f64* y, usize n, usize stride = 1)
  : x_{x, n, stride}, y_{y, n, stride} { recompute_bounds(); }

  DataView    x_view()  const { return x_; }
  DataView    y_view()  const { return y_; }
  usize       size()    const { return x_.count; }
  const BBox& bounds()  const { return bounds_; }

  void recompute_bounds() {
    bounds_ = {};
    for (usize i = 0; i < x_.count; ++i) bounds_.expand(x_[i], y_[i]);
  }

private:
  DataView x_, y_;
  BBox bounds_;
};

// Internal: LTTB decimator. Users configure via PerfParams; never call directly
class LttbDecimator {
public:
  static Series decimate(const DataView& xv, const DataView& yv, usize target) {
    const usize n = xv.count;
    if (target >= n || target < 3) {
      AlignedBuffer<f64> ox(n), oy(n);
      for (usize i = 0; i < n; ++i) { ox[i] = xv[i]; oy[i] = yv[i]; }
      return Series(std::move(ox), std::move(oy));
    }

    AlignedBuffer<f64> ox(target), oy(target);
    ox[0] = xv[0]; oy[0] = yv[0];

    const f64 bucket_size = static_cast<f64>(n - 2) / static_cast<f64>(target - 2);
    usize a = 0;

    for (usize i = 1; i < target - 1; ++i) {
      usize bucket_start = static_cast<usize>((i - 1) * bucket_size) + 1;
      usize bucket_end   = static_cast<usize>(i * bucket_size) + 1;
      if (bucket_end > n - 1) bucket_end = n - 1;

      usize next_start = static_cast<usize>(i * bucket_size) + 1;
      usize next_end   = static_cast<usize>((i + 1) * bucket_size) + 1;
      if (next_end > n) next_end = n;

      f64 avg_x = 0, avg_y = 0;
      usize next_count = next_end - next_start;
      for (usize j = next_start; j < next_end; ++j) { avg_x += xv[j]; avg_y += yv[j]; }
      if (next_count > 0) { avg_x /= next_count; avg_y /= next_count; }

      f64 max_area = -1.0; usize max_idx = bucket_start;
      f64 ax = xv[a], ay = yv[a];

      for (usize j = bucket_start; j < bucket_end; ++j) {
        f64 area = std::abs(
          (ax - avg_x) * (yv[j] - ay) - (ax - xv[j]) * (avg_y - ay)
        ) * 0.5;
        if (area > max_area) { max_area = area; max_idx = j; }
      }

      ox[i] = xv[max_idx]; oy[i] = yv[max_idx];
      a = max_idx;
    }

    ox[target - 1] = xv[n - 1]; oy[target - 1] = yv[n - 1];
    return Series(std::move(ox), std::move(oy));
  }
};

} // NS data

// --- rendering: Canvas is semi-public; everything else is internal ----------------------------
namespace rendering {

// Semi-public: accessible via Figure::canvas() for custom renderers
class Canvas {
public:
  Canvas() = default;

  Canvas(u32 width, u32 height)
    : width_(width), height_(height)
    , pixels_(static_cast<usize>(width) * height * 4)
  {
    clear({255, 255, 255, 255});
  }


  void clear(Color c) {
    u32* px     = reinterpret_cast<u32*>(pixels_.data());
    u32  packed = pack(c);
    usize total = static_cast<usize>(width_) * height_;
    std::fill(px, px + total, packed);
  }


  void set_pixel(i32 x, i32 y, Color c) {
    if (x < 0 || y < 0 || x >= static_cast<i32>(width_) || y >= static_cast<i32>(height_)) return;
    usize idx = (static_cast<usize>(y) * width_ + x) * 4;
    u8* dst = pixels_.data() + idx;

    if (c.a == 255) {
      dst[0] = static_cast<u8>(c.r); dst[1] = static_cast<u8>(c.g);
      dst[2] = static_cast<u8>(c.b); dst[3] = 255;

    } else {
      u32 sa = c.a, da = 255 - sa;
      dst[0] = static_cast<u8>((c.r * sa + dst[0] * da) / 255);
      dst[1] = static_cast<u8>((c.g * sa + dst[1] * da) / 255);
      dst[2] = static_cast<u8>((c.b * sa + dst[2] * da) / 255);
      dst[3] = static_cast<u8>(sa + (dst[3] * da) / 255);
    }
  }

  void draw_line(f64 x0, f64 y0, f64 x1, f64 y1, Color c, f64 width = 1.0, bool aa=true) {
    if(aa) draw_line_aa(x0, y0, x1, y1, c, width);
    else draw_line_bresenham(x0, y0, x1, y1, c, width);
  }


  void fill_rect(i32 x, i32 y, i32 w, i32 h, Color c) {
    i32 x0 = std::max(x, 0), y0 = std::max(y, 0);
    i32 x1 = std::min(x + w, static_cast<i32>(width_));
    i32 y1 = std::min(y + h, static_cast<i32>(height_));

    for (i32 py = y0; py < y1; ++py)
      for (i32 px = x0; px < x1; ++px) set_pixel(px, py, c);
  }


  void draw_rect(i32 x, i32 y, i32 w, i32 h, Color c, f64 line_w = 1.0) { // legend
    draw_line(x, y, x + w, y, c, line_w, false);
    draw_line(x + w, y, x + w, y + h, c, line_w, false);
    draw_line(x + w, y + h, x, y + h, c, line_w, false);
    draw_line(x, y + h, x, y, c, line_w, false);
  }


  void draw_circle(i32 cx, i32 cy, f64 radius, Color c) {
    i32 r = static_cast<i32>(radius);
    for (i32 dy = -r; dy <= r; ++dy)
      for (i32 dx = -r; dx <= r; ++dx)
        if (dx * dx + dy * dy <= r * r)
          set_pixel(cx + dx, cy + dy, c);
  }


  u8*       data()   { return pixels_.data(); }
  const u8* data()   const { return pixels_.data(); }
  u32       width()  const { return width_; }
  u32       height() const { return height_; }
  usize     stride() const { return static_cast<usize>(width_) * 4; }

private:
  static u32 pack(Color c) {
    return static_cast<u32>(c.r)
    | (static_cast<u32>(c.g) << 8)
    | (static_cast<u32>(c.b) << 16)
    | (static_cast<u32>(c.a) << 24);
  }

  void draw_line_aa(f64 x0, f64 y0, f64 x1, f64 y1, Color c, f64 /*width*/) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep)   { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

    f64 dx = x1 - x0, dy = y1 - y0;
    f64 gradient = (dx == 0.0) ? 1.0 : dy / dx;

    f64 xend = std::round(x0), yend = y0 + gradient * (xend - x0);
    f64 xgap = rfpart(x0 + 0.5);
    i32 xpxl1 = static_cast<i32>(xend), ypxl1 = static_cast<i32>(std::floor(yend));
    if (steep) {
      plot(ypxl1,     xpxl1, c, rfpart(yend) * xgap);
      plot(ypxl1 + 1, xpxl1, c,  fpart(yend) * xgap);
    } else {
      plot(xpxl1, ypxl1,     c, rfpart(yend) * xgap);
      plot(xpxl1, ypxl1 + 1, c,  fpart(yend) * xgap);
    }
    f64 intery = yend + gradient;

    xend = std::round(x1); yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5);
    i32 xpxl2 = static_cast<i32>(xend), ypxl2 = static_cast<i32>(std::floor(yend));
    if (steep) {
      plot(ypxl2,     xpxl2, c, rfpart(yend) * xgap);
      plot(ypxl2 + 1, xpxl2, c,  fpart(yend) * xgap);
    } else {
      plot(xpxl2, ypxl2,     c, rfpart(yend) * xgap);
      plot(xpxl2, ypxl2 + 1, c,  fpart(yend) * xgap);
    }

    for (i32 x = xpxl1 + 1; x < xpxl2; ++x) {
      i32 iy = static_cast<i32>(std::floor(intery));
      if (steep) {
        plot(iy,     x, c, rfpart(intery));
        plot(iy + 1, x, c,  fpart(intery));
      } else {
        plot(x, iy,     c, rfpart(intery));
        plot(x, iy + 1, c,  fpart(intery));
      }
      intery += gradient;
    }
  }

  void draw_line_bresenham(f64 x0, f64 y0, f64 x1, f64 y1, Color c, f64 width) {
    i32 x0i = static_cast<i32>(std::round(x0));
    i32 y0i = static_cast<i32>(std::round(y0));
    i32 x1i = static_cast<i32>(std::round(x1));
    i32 y1i = static_cast<i32>(std::round(y1));

    bool steep = std::abs(y1i - y0i) > std::abs(x1i - x0i);
    if (steep) {
        std::swap(x0i, y0i);
        std::swap(x1i, y1i);
    }
    if (x0i > x1i) {
        std::swap(x0i, x1i);
        std::swap(y0i, y1i);
    }

    i32 dx = x1i - x0i;
    i32 dy = std::abs(y1i - y0i);
    i32 err = dx / 2;
    i32 ystep = (y0i < y1i) ? 1 : -1;
    i32 y = y0i;

    for (i32 x = x0i; x <= x1i; ++x) {
        if (steep)
            set_pixel(y, x, c);
        else
            set_pixel(x, y, c);
        err -= dy;
        if (err < 0) {
            y += ystep;
            err += dx;
        }
    }
  }

  static f64 fpart(f64 x)  { return x - std::floor(x); }
  static f64 rfpart(f64 x) { return 1.0 - fpart(x); }

  void plot(i32 x, i32 y, Color c, f64 brightness) {
    set_pixel(x, y, {c.r, c.g, c.b, static_cast<u32>(c.a * brightness)});
  }

  u32 width_ = 0, height_ = 0;
  AlignedBuffer<u8> pixels_;
};

// Internal: 5*7 bitmap font, no external font dependency
namespace font {

struct Glyph { u8 rows[7]; };

inline const Glyph& get_glyph(char ch) {
  static const bool init = []() -> bool { return true; }();
  (void)init;

  static Glyph table[128] = {};
  static bool built = false;
  if (!built) {
    built = true;
    auto set = [&](char c, u8 r0, u8 r1, u8 r2, u8 r3, u8 r4, u8 r5, u8 r6) {
      auto& g = table[static_cast<u8>(c)];
      g.rows[0]=r0; g.rows[1]=r1; g.rows[2]=r2; g.rows[3]=r3;
      g.rows[4]=r4; g.rows[5]=r5; g.rows[6]=r6;
    };

    set('0', 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E);
    set('1', 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E);
    set('2', 0x0E,0x11,0x01,0x06,0x08,0x10,0x1F);
    set('3', 0x0E,0x11,0x01,0x06,0x01,0x11,0x0E);
    set('4', 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02);
    set('5', 0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E);
    set('6', 0x06,0x08,0x10,0x1E,0x11,0x11,0x0E);
    set('7', 0x1F,0x01,0x02,0x04,0x08,0x08,0x08);
    set('8', 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E);
    set('9', 0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C);

    set('A', 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11);
    set('B', 0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E);
    set('C', 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E);
    set('D', 0x1E,0x11,0x11,0x11,0x11,0x11,0x1E);
    set('E', 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F);
    set('F', 0x1F,0x10,0x10,0x1E,0x10,0x10,0x10);
    set('G', 0x0E,0x11,0x10,0x17,0x11,0x11,0x0F);
    set('H', 0x11,0x11,0x11,0x1F,0x11,0x11,0x11);
    set('I', 0x0E,0x04,0x04,0x04,0x04,0x04,0x0E);
    set('J', 0x07,0x02,0x02,0x02,0x02,0x12,0x0C);
    set('K', 0x11,0x12,0x14,0x18,0x14,0x12,0x11);
    set('L', 0x10,0x10,0x10,0x10,0x10,0x10,0x1F);
    set('M', 0x11,0x1B,0x15,0x15,0x11,0x11,0x11);
    set('N', 0x11,0x19,0x15,0x13,0x11,0x11,0x11);
    set('O', 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E);
    set('P', 0x1E,0x11,0x11,0x1E,0x10,0x10,0x10);
    set('Q', 0x0E,0x11,0x11,0x11,0x15,0x12,0x0D);
    set('R', 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11);
    set('S', 0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E);
    set('T', 0x1F,0x04,0x04,0x04,0x04,0x04,0x04);
    set('U', 0x11,0x11,0x11,0x11,0x11,0x11,0x0E);
    set('V', 0x11,0x11,0x11,0x11,0x0A,0x0A,0x04);
    set('W', 0x11,0x11,0x11,0x15,0x15,0x1B,0x11);
    set('X', 0x11,0x11,0x0A,0x04,0x0A,0x11,0x11);
    set('Y', 0x11,0x11,0x0A,0x04,0x04,0x04,0x04);
    set('Z', 0x1F,0x01,0x02,0x04,0x08,0x10,0x1F);

    set('a', 0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F);
    set('b', 0x10,0x10,0x1E,0x11,0x11,0x11,0x1E);
    set('c', 0x00,0x00,0x0E,0x11,0x10,0x11,0x0E);
    set('d', 0x01,0x01,0x0F,0x11,0x11,0x11,0x0F);
    set('e', 0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E);
    set('f', 0x06,0x08,0x08,0x1E,0x08,0x08,0x08);
    set('g', 0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E);
    set('h', 0x10,0x10,0x1E,0x11,0x11,0x11,0x11);
    set('i', 0x04,0x00,0x0C,0x04,0x04,0x04,0x0E);
    set('j', 0x02,0x00,0x06,0x02,0x02,0x12,0x0C);
    set('k', 0x10,0x10,0x12,0x14,0x18,0x14,0x12);
    set('l', 0x0C,0x04,0x04,0x04,0x04,0x04,0x0E);
    set('m', 0x00,0x00,0x1A,0x15,0x15,0x15,0x15);
    set('n', 0x00,0x00,0x1E,0x11,0x11,0x11,0x11);
    set('o', 0x00,0x00,0x0E,0x11,0x11,0x11,0x0E);
    set('p', 0x00,0x00,0x1E,0x11,0x1E,0x10,0x10);
    set('q', 0x00,0x00,0x0F,0x11,0x0F,0x01,0x01);
    set('r', 0x00,0x00,0x16,0x19,0x10,0x10,0x10);
    set('s', 0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E);
    set('t', 0x08,0x08,0x1E,0x08,0x08,0x09,0x06);
    set('u', 0x00,0x00,0x11,0x11,0x11,0x11,0x0F);
    set('v', 0x00,0x00,0x11,0x11,0x11,0x0A,0x04);
    set('w', 0x00,0x00,0x11,0x11,0x15,0x15,0x0A);
    set('x', 0x00,0x00,0x11,0x0A,0x04,0x0A,0x11);
    set('y', 0x00,0x00,0x11,0x11,0x0F,0x01,0x0E);
    set('z', 0x00,0x00,0x1F,0x02,0x04,0x08,0x1F);

    set('.', 0x00,0x00,0x00,0x00,0x00,0x00,0x04);
    set(',', 0x00,0x00,0x00,0x00,0x00,0x04,0x08);
    set(':', 0x00,0x00,0x04,0x00,0x00,0x04,0x00);
    set(';', 0x00,0x00,0x04,0x00,0x00,0x04,0x08);
    set('!', 0x04,0x04,0x04,0x04,0x04,0x00,0x04);
    set('?', 0x0E,0x11,0x01,0x06,0x04,0x00,0x04);
    set('-', 0x00,0x00,0x00,0x1F,0x00,0x00,0x00);
    set('+', 0x00,0x04,0x04,0x1F,0x04,0x04,0x00);
    set('=', 0x00,0x00,0x1F,0x00,0x1F,0x00,0x00);
    set('(', 0x02,0x04,0x08,0x08,0x08,0x04,0x02);
    set(')', 0x08,0x04,0x02,0x02,0x02,0x04,0x08);
    set('/', 0x01,0x02,0x02,0x04,0x08,0x08,0x10);
    set(' ', 0x00,0x00,0x00,0x00,0x00,0x00,0x00);
    set('_', 0x00,0x00,0x00,0x00,0x00,0x00,0x1F);

    auto& fb = table[127];
    fb.rows[0]=0x1F; fb.rows[1]=0x11; fb.rows[2]=0x11; fb.rows[3]=0x11;
    fb.rows[4]=0x11; fb.rows[5]=0x11; fb.rows[6]=0x1F;
  }

  u8 idx = static_cast<u8>(ch);
  if (idx < 32 || idx > 126) idx = 127;
  return table[idx];
}

} // NS font

// Internal: text rendering helpers used only by Figure render passes
inline void draw_text(Canvas& canvas, const std::string& text, i32 x, i32 y, Color color, i32 scale = 1) {
  i32 cursor_x = x;
  for (char ch : text) {
    const auto& g = font::get_glyph(ch);
    for (i32 row = 0; row < 7; ++row)
      for (i32 col = 0; col < 5; ++col)
        if (g.rows[row] & (0x10 >> col))
          for (i32 sy = 0; sy < scale; ++sy)
            for (i32 sx = 0; sx < scale; ++sx)
              canvas.set_pixel(
                cursor_x + col * scale + sx,
                y + row * scale + sy, 
                color
              );
    cursor_x += 6 * scale;
  }
}

inline void draw_text_vertical(
  Canvas& canvas, 
  const std::string& text,
  i32 x, i32 y, Color color, i32 scale = 1
) {

  i32 lh = static_cast<i32>(text.size()) * 6 * scale - scale;
  i32 cursor_y = y + lh - 5 * scale;
  for (char ch : text) {
    const auto& g = font::get_glyph(ch);
    for (i32 row = 0; row < 7; ++row)
      for (i32 col = 0; col < 5; ++col)
        if (g.rows[row] & (0x10 >> col))
          for (i32 sy = 0; sy < scale; ++sy)
            for (i32 sx = 0; sx < scale; ++sx)
              canvas.set_pixel(
                x + row * scale + sx,
                cursor_y + (4 - col) * scale + sy, 
                color
              );
    cursor_y -= 6 * scale;
  }
}

inline i32 text_width(const std::string& text, i32 scale = 1) {
  if (text.empty()) return 0;
  return static_cast<i32>(text.size()) * 6 * scale - scale;
}
inline i32 text_height(i32 scale = 1) { return 7 * scale; }
inline i32 text_width_vertical(i32 /*text_len*/, i32 scale = 1) { return 7 * scale; }
inline i32 text_height_vertical(const std::string& text, i32 scale = 1) {
  if (text.empty()) return 0;
  return static_cast<i32>(text.size()) * 6 * scale - scale;
}

// Internal: tick generation for axes.
struct Tick {
  f64         value;
  std::string label;
  bool        is_minor = false;
};

class TickEngine {
public:
  static std::vector<Tick> compute(f64 lo, f64 hi, usize target_ticks = 6, bool include_minor = false, ScaleType scale = ScaleType::Linear) {
    if (scale == ScaleType::Log) return compute_log(lo, hi, include_minor);
    return compute_linear(lo, hi, target_ticks, include_minor);
  }

private:
  static std::vector<Tick> compute_linear(f64 lo, f64 hi, usize target_ticks, bool include_minor) {
    std::vector<Tick> ticks;
    if (hi <= lo) return ticks;

    f64 range      = hi - lo;
    f64 rough_step = range / static_cast<f64>(target_ticks);
    f64 mag        = std::pow(10.0, std::floor(std::log10(rough_step)));
    f64 norm       = rough_step / mag;

    f64 nice;
    if      (norm < 1.5) nice = 1.0;
    else if (norm < 3.0) nice = 2.0;
    else if (norm < 7.0) nice = 5.0;
    else                 nice = 10.0;

    f64 step  = nice * mag;
    f64 start = std::floor(lo / step) * step;

    for (f64 v = start; v <= hi + step * 0.001; v += step) {
      if (v < lo - step * 0.001) continue;
      ticks.push_back({v, format_value(v, step), false});

      if (include_minor) {
        f64 minor_step = step / 5.0;
        for (int m = 1; m < 5; ++m) {
          f64 mv = v + m * minor_step;

          if (mv > lo && mv < hi) ticks.push_back({mv, "", true});
        }
      }
    }
    return ticks;
  }

  static std::vector<Tick> compute_log(f64 lo, f64 hi, bool include_minor) {
    std::vector<Tick> ticks;
    if (hi <= 0 || lo <= 0) return ticks;

    int exp_lo = static_cast<int>(std::floor(std::log10(lo)));
    int exp_hi = static_cast<int>(std::ceil(std::log10(hi)));

    for (int e = exp_lo; e <= exp_hi; ++e) {
      f64 val = std::pow(10.0, e);
      if (val >= lo && val <= hi) ticks.push_back({val, format_log_value(val), false});
      if (include_minor)
        for (int m = 2; m <= 9; ++m) {
          f64 mv = m * val;
          if (mv > lo && mv < hi) ticks.push_back({mv, "", true});
        }
    }
    return ticks;
  }

  static std::string format_value(f64 v, f64 step) {
    char buf[64];
    if (step >= 1.0 && std::abs(v) < 1e12) {
      if (std::abs(v - std::round(v)) < 1e-9) 
        std::snprintf(buf, sizeof(buf), "%.0f", v);
      else 
        std::snprintf(buf, sizeof(buf), "%.1f", v);
    } else if (step >= 0.01) {
      std::snprintf(buf, sizeof(buf), "%.2f", v);
    } else {
      std::snprintf(buf, sizeof(buf), "%.4g", v);
    }
    return buf;
  }

  static std::string format_log_value(f64 v) {
    char buf[64];
    if (v >= 1.0 && v < 1e7) 
      std::snprintf(buf, sizeof(buf), "%.0f", v);
    else
      std::snprintf(buf, sizeof(buf), "%.0e", v);
    return buf;
  }
};

// Internal: data-space -> pixel-space coordinate mapping. Hot path per point.
class CoordTransform {
public:
  CoordTransform() = default;

  void set(const BBox& data_box, const detail::Rect& pixel_rect, 
           ScaleType x_scale = ScaleType::Linear, 
           ScaleType y_scale = ScaleType::Linear ) {

    data_  = data_box;
    rect_  = pixel_rect;
    x_log_ = (x_scale == ScaleType::Log);
    y_log_ = (y_scale == ScaleType::Log);

    f64 x_lo = x_log_ ? safe_log10(data_box.x_min) : data_box.x_min;
    f64 x_hi = x_log_ ? safe_log10(data_box.x_max) : data_box.x_max;
    f64 y_lo = y_log_ ? safe_log10(data_box.y_min) : data_box.y_min;
    f64 y_hi = y_log_ ? safe_log10(data_box.y_max) : data_box.y_max;

    f64 dw = x_hi - x_lo, dh = y_hi - y_lo;
    sx_ = (dw > 0) ? pixel_rect.w / dw : 1.0;
    sy_ = (dh > 0) ? pixel_rect.h / dh : 1.0;
    log_x_lo_ = x_lo; log_y_lo_ = y_lo;
  }

  inline f64 to_px_x(f64 x) const {
    f64 v = x_log_ ? safe_log10(x) : x;
    return rect_.x + (v - log_x_lo_) * sx_;
  }
  inline f64 to_px_y(f64 y) const {
    f64 v = y_log_ ? safe_log10(y) : y;
    return rect_.y + rect_.h - (v - log_y_lo_) * sy_;
  }
  inline f64 to_data_x(f64 px) const {
    f64 v = log_x_lo_ + (px - rect_.x) / sx_;
    return x_log_ ? std::pow(10.0, v) : v;
  }
  inline f64 to_data_y(f64 py) const {
    f64 v = log_y_lo_ + (rect_.y + rect_.h - py) / sy_;
    return y_log_ ? std::pow(10.0, v) : v;
  }

  const BBox& data_box() const { return data_; }
  const detail::Rect& pixel_rect() const { return rect_; }

private:
  static inline f64 safe_log10(f64 v) { return std::log10(v > 1e-300 ? v : 1e-300); }

  BBox        data_;
  detail::Rect rect_;
  f64  sx_ = 1.0, sy_ = 1.0;
  f64  log_x_lo_ = 0.0, log_y_lo_ = 0.0;
  bool x_log_ = false, y_log_ = false;
};

} // NS rendering

// Internal: PPM file output, called only through Figure::save_ppm()
namespace output {

inline bool write_ppm(const rendering::Canvas& canvas, const std::string& path) {

  std::ofstream f(path, std::ios::binary);
  if (!f.is_open()) return false;

  f << "P6\n" << canvas.width() << " " << canvas.height() << "\n255\n";

  const u8* data = canvas.data();
  usize w = canvas.width(), h = canvas.height();

  for (usize y = 0; y < h; ++y)
    for (usize x = 0; x < w; ++x) {
      usize idx = (y * w + x) * 4;
      f.put(static_cast<char>(data[idx + 0]));
      f.put(static_cast<char>(data[idx + 1]));
      f.put(static_cast<char>(data[idx + 2]));
    }

  return f.good();
}

} // NS output

// --- Public: plot2d -------------------------------------------------------
namespace plot2d {

using SeriesVariant = std::variant<data::Series, data::ExternalSeries>;

struct PlotEntry {
  SeriesVariant     series;
  params::DataStyle style;

  data::DataView x_view() const { return std::visit([](auto& s){ return s.x_view(); }, series); }
  data::DataView y_view() const { return std::visit([](auto& s){ return s.y_view(); }, series); }
  usize          size()   const { return std::visit([](auto& s){ return s.size();   }, series); }
  BBox           bounds() const { return std::visit([](auto& s){ return s.bounds(); }, series); }
};

class Figure;

class PlotCommand {
public:
  PlotCommand(Figure& fig, data::Series&& series) : 
    fig_(fig), entry_{std::move(series), {}} 
  {}

  PlotCommand(Figure& fig, data::ExternalSeries&& series) : 
    fig_(fig), entry_{std::move(series), {}} 
  {}

  PlotCommand& data(const params::DataStyle& style) { entry_.style = style; return *this; }

  PlotCommand& color(Color c)              { entry_.style.color      = c;    return *this; }
  PlotCommand& width(f64 w)                { entry_.style.width      = w;    return *this; }
  PlotCommand& alpha(f64 a)                { entry_.style.alpha      = a;    return *this; }
  PlotCommand& label(const std::string& l) { entry_.style.label      = l;    return *this; }
  PlotCommand& line(LineStyle s)           { entry_.style.line_style = s;    return *this; }

  PlotCommand& marker(MarkerStyle m, f64 size = 4.0) {
    entry_.style.marker      = m;
    entry_.style.marker_size = size;
    return *this;
  }

  PlotCommand& fill(bool on = true, Color c = Color::blue().with_alpha(50)) {
    entry_.style.fill       = on;
    entry_.style.fill_color = c;
    return *this;
  }

  ~PlotCommand();

private:
  Figure&   fig_;
  PlotEntry entry_;
  bool      committed_ = false;

  void commit();
  friend class Figure;
};

// ===========================================================================
// Figure
// ===========================================================================
class Figure {
public:
  // -------------------------------------------------------------------------
  // Constructor B: physical size (mm) + resolution (dpmm).
  //
  //   Figure({190.0, 120.0}, {5.0})
  //     → 190 mm wide, 120 mm tall at 5 dpmm = 950×600 pixels
  //
  //   Figure({210.0, 297.0}, Resolution::from_dpi(300))
  //     → A4 page at 300 DPI = 2480×3508 pixels
  //
  // All spacing (margins, tick size, font scale, line widths, legend padding)
  // is proportionally scaled so the figure looks identical at any resolution.
  // -------------------------------------------------------------------------
  Figure(PhysicalSize size, Resolution res = Resolution{})
    : dpmm_(res.dpmm)
    , phys_(size)
    , canvas_(static_cast<u32>(std::round(size.width_mm  * res.dpmm)),
              static_cast<u32>(std::round(size.height_mm * res.dpmm)))
    , fig_w_(size.width_mm  * res.dpmm)
    , fig_h_(size.height_mm * res.dpmm)
  {}

  // -------------------------------------------------------------------------
  // Constructor C: flat (width_mm, height_mm, dpmm) — most concise form.
  //
  //   Figure(200.0, 150.0, 4.0)
  //     → 200 mm wide, 150 mm tall at 4 dpmm ≈ 102 DPI = 800×600 pixels
  //
  // The third argument defaults to BASELINE_DPMM (≈ 96 DPI) so that
  //   Figure(185.3, 119.1)  behaves like the old  Figure(700, 450).
  // -------------------------------------------------------------------------
  Figure(f64 width_mm, f64 height_mm, f64 dpmm = detail::BASELINE_DPMM)
    : dpmm_(dpmm)
    , phys_{ width_mm, height_mm }
    , canvas_(static_cast<u32>(std::round(width_mm  * dpmm)),
              static_cast<u32>(std::round(height_mm * dpmm)))
    , fig_w_(width_mm  * dpmm)
    , fig_h_(height_mm * dpmm)
  {}

  // -------------------------------------------------------------------------
  // Accessors: physical dimensions and resolution
  // -------------------------------------------------------------------------
  f64 width_mm()   const { return phys_.width_mm;  }
  f64 height_mm()  const { return phys_.height_mm; }
  f64 dpmm()       const { return dpmm_;            }
  f64 dpi()        const { return dpmm_ * 25.4;     }
  u32 width_px()   const { return canvas_.width();  }
  u32 height_px()  const { return canvas_.height(); }

  void set_title(const std::string& t)  { title_  = t; }
  void set_xlabel(const std::string& l) { xlabel_ = l; }
  void set_ylabel(const std::string& l) { ylabel_ = l; }

  void grid(const params::GridStyle& g)     { grid_style_   = g; }
  void axis(const params::AxisStyle& a)     { axis_style_   = a; }
  void legend(const params::LegendStyle& l) { legend_style_ = l; }
  void layout(const params::LayoutStyle& l) { layout_style_ = l; }
  void text(const params::TextStyle& t)     { text_style_   = t; }
  void perf(const params::PerfParams& p)    { perf_         = p; }

  // LINE PLOTS
  inline PlotCommand plot(const f64* x, const f64* y, usize n) {
    return PlotCommand(*this, data::Series(x, y, n));
  }
  inline PlotCommand plot(data::Series&& s) {
    return PlotCommand(*this, std::move(s));
  }
  inline PlotCommand plot_ref(const f64* x, const f64* y, usize n, usize stride = 1) {
    return PlotCommand(*this, data::ExternalSeries(x, y, n, stride));
  }

  void render() {
    canvas_.clear(layout_style_.background);
    compute_plot_area();
    compute_data_bounds();
    setup_transform();
    render_grid();
    render_axes();
    render_data();
    render_legend();
    render_title_and_labels();
  }

  const rendering::Canvas& canvas() const { return canvas_; }
  rendering::Canvas& canvas()       { return canvas_; }

  bool save_ppm(const std::string& path) const {
    return output::write_ppm(canvas_, path);
  }

  const std::vector<PlotEntry>& entries() const { return entries_; }

private:
  friend class PlotCommand;

  void add_entry(PlotEntry&& e) { entries_.push_back(std::move(e)); }

  // ------------------------------------------------------------------
  // sc(base_px) — scale a baseline-96DPI pixel value to current dpmm.
  // Use for all hardcoded pixel constants: tick sizes, offsets, radii …
  // ------------------------------------------------------------------
  inline f64 sc(f64 base_px) const { return detail::scale(base_px, dpmm_); }

  // Font scale: nearest integer ≥ 1 (bitmap font scales by integer steps).
  // At 96 DPI (dpmm ≈ 3.78) → scale 1.  At 192 DPI → scale 2.  At 300 DPI → scale 3.
  inline i32 font_scale(i32 base_scale = 1) const {
    return std::max(1, static_cast<i32>(std::round(
      base_scale * dpmm_ / detail::BASELINE_DPMM
    )));
  }

  void compute_plot_area() {
    // Convert mm-based margins to pixels using current dpmm
    f64 ml = layout_style_.margin_left_mm   * dpmm_;
    f64 mt = layout_style_.margin_top_mm    * dpmm_;
    f64 mr = layout_style_.margin_right_mm  * dpmm_;
    f64 mb = layout_style_.margin_bottom_mm * dpmm_;

    plot_area_ = {
      ml,
      mt,
      fig_w_ - ml - mr,
      fig_h_ - mt  - mb
    };
  }

  void compute_data_bounds() {
    data_bounds_ = {};
    for (auto& e : entries_) data_bounds_.merge(e.bounds());

    if (std::bit_cast<u64>(axis_style_.x_min)) data_bounds_.x_min = axis_style_.x_min;
    if (std::bit_cast<u64>(axis_style_.x_max)) data_bounds_.x_max = axis_style_.x_max;
    if (std::bit_cast<u64>(axis_style_.y_min)) data_bounds_.y_min = axis_style_.y_min;
    if (std::bit_cast<u64>(axis_style_.y_max)) data_bounds_.y_max = axis_style_.y_max;

    if (!data_bounds_.empty()) {
      if (axis_style_.x_scale == ScaleType::Log && data_bounds_.x_min > 0) {
        f64 log_lo = std::log10(data_bounds_.x_min);
        f64 log_hi = std::log10(data_bounds_.x_max);
        f64 pad = (log_hi - log_lo) * 0.05; if (pad == 0) pad = 0.15;
        data_bounds_.x_min = std::pow(10.0, log_lo - pad);
        data_bounds_.x_max = std::pow(10.0, log_hi + pad);
      } else {
        f64 xpad = data_bounds_.width() * 0.05; if (xpad == 0) xpad = 1.0;
        data_bounds_.x_min -= xpad; data_bounds_.x_max += xpad;
      }

      if (axis_style_.y_scale == ScaleType::Log && data_bounds_.y_min > 0) {
        f64 log_lo = std::log10(data_bounds_.y_min);
        f64 log_hi = std::log10(data_bounds_.y_max);
        f64 pad = (log_hi - log_lo) * 0.05; if (pad == 0) pad = 0.15;
        data_bounds_.y_min = std::pow(10.0, log_lo - pad);
        data_bounds_.y_max = std::pow(10.0, log_hi + pad);
      } else {
        f64 ypad = data_bounds_.height() * 0.05; if (ypad == 0) ypad = 1.0;
        data_bounds_.y_min -= ypad; data_bounds_.y_max += ypad;
      }
    }
  }

  void setup_transform() {
    transform_.set(data_bounds_, plot_area_, axis_style_.x_scale, axis_style_.y_scale);
  }

  void render_grid() {
    if (!grid_style_.show) return;

    auto xticks = rendering::TickEngine::compute(
      data_bounds_.x_min, data_bounds_.x_max, 8,
      grid_style_.show_minor, axis_style_.x_scale);
    auto yticks = rendering::TickEngine::compute(
      data_bounds_.y_min, data_bounds_.y_max, 6,
      grid_style_.show_minor, axis_style_.y_scale);

    for (auto& t : xticks) {
      f64 px = transform_.to_px_x(t.value);
      Color c  = t.is_minor ? grid_style_.minor_color : grid_style_.major_color;
      f64 w  = t.is_minor ? grid_style_.minor_width : grid_style_.major_width;
      f64 a  = t.is_minor ? grid_style_.minor_alpha : grid_style_.major_alpha;
      c = c.with_alpha(static_cast<u32>(a * 255));
      canvas_.draw_line(px, plot_area_.y, px, plot_area_.y + plot_area_.h,
                        c, sc(w), false);
    }

    for (auto& t : yticks) {
      f64 py = transform_.to_px_y(t.value);
      Color c  = t.is_minor ? grid_style_.minor_color : grid_style_.major_color;
      f64 w  = t.is_minor ? grid_style_.minor_width : grid_style_.major_width;
      f64 a  = t.is_minor ? grid_style_.minor_alpha : grid_style_.major_alpha;
      c = c.with_alpha(static_cast<u32>(a * 255));
      canvas_.draw_line(plot_area_.x, py, plot_area_.x + plot_area_.w, py,
                        c, sc(w), false);
    }
  }

  void render_axes() {
    if (!axis_style_.show) return;
    Color c = axis_style_.color;
    f64 w = sc(axis_style_.width);
    f64 x0 = plot_area_.x, y0 = plot_area_.y;
    f64 x1 = x0 + plot_area_.w, y1 = y0 + plot_area_.h;

    canvas_.draw_line(x0, y0, x1, y0, c, w, false);
    canvas_.draw_line(x1, y0, x1, y1, c, w, false);
    canvas_.draw_line(x1, y1, x0, y1, c, w, false);
    canvas_.draw_line(x0, y1, x0, y0, c, w, false);

    const f64 tick_sz  = sc(axis_style_.tick_size);
    const i32 fscale   = font_scale(1);
    const f64 tick_gap = sc(3.0);   // gap between tick end and label

    auto xticks = rendering::TickEngine::compute(
      data_bounds_.x_min, data_bounds_.x_max, 8, false, axis_style_.x_scale);

    for (auto& t : xticks) {
      f64 px = transform_.to_px_x(t.value);
      canvas_.draw_line(px, y1, px, y1 + tick_sz, c, w, false);

      if (!t.label.empty()) {
        i32 tw = rendering::text_width(t.label, fscale);
        rendering::draw_text(
          canvas_, t.label,
          static_cast<i32>(px) - tw / 2,
          static_cast<i32>(y1 + tick_sz + tick_gap),
          text_style_.color, fscale);
      }
    }

    auto yticks = rendering::TickEngine::compute(
      data_bounds_.y_min, data_bounds_.y_max, 6, false, axis_style_.y_scale);

    for (auto& t : yticks) {
      f64 py = transform_.to_px_y(t.value);
      canvas_.draw_line(x0 - tick_sz, py, x0, py, c, w, false);

      if (!t.label.empty()) {
        i32 tw = rendering::text_width(t.label, fscale);
        rendering::draw_text(
          canvas_, t.label,
          static_cast<i32>(x0 - tick_sz - tick_gap) - tw,
          static_cast<i32>(py) - rendering::text_height(fscale) / 2,
          text_style_.color, fscale);
      }
    }
  }

  void render_data() {
    for (auto& entry : entries_) {
      auto xv = entry.x_view(), yv = entry.y_view();
      const auto& st = entry.style;

      data::DataView rx = xv, ry = yv;
      data::Series decimated;

      if (perf_.lod_enable) {
        if (xv.count >= perf_.lod_target_points) {
          decimated = data::LttbDecimator::decimate(xv, yv, perf_.lod_target_points);
          rx = decimated.x_view();
          ry = decimated.y_view();
        }
      }

      Color col = st.color;
      if (st.alpha < 1.0) col = col.with_alpha(static_cast<u32>(st.alpha * 255));

      if (st.line_style != LineStyle::None && rx.count > 1) {
        for (usize i = 1; i < rx.count; ++i) {
          canvas_.draw_line(
            transform_.to_px_x(rx[i-1]), transform_.to_px_y(ry[i-1]),
            transform_.to_px_x(rx[i]),   transform_.to_px_y(ry[i]),
            col,
            sc(st.width)
          );
        }
      }

      if (st.fill && rx.count > 1) {
        f64 base_py = transform_.to_px_y(data_bounds_.y_min);
        for (usize i = 0; i < rx.count; ++i) {
          i32 ix     = static_cast<i32>(transform_.to_px_x(rx[i]));
          i32 iy_top = static_cast<i32>(transform_.to_px_y(ry[i]));
          i32 iy_bot = static_cast<i32>(base_py);
          for (i32 y = iy_top; y <= iy_bot; ++y)
            canvas_.set_pixel(ix, y, st.fill_color);
        }
      }

      if (st.marker != MarkerStyle::None) {
        for (usize i = 0; i < rx.count; ++i) {
          canvas_.draw_circle(
            static_cast<i32>(transform_.to_px_x(rx[i])),
            static_cast<i32>(transform_.to_px_y(ry[i])),
            sc(st.marker_size), col
          );
        }
      }
    }
  }

  void render_legend() {
    if (!legend_style_.show) return;
    std::vector<const PlotEntry*> labeled;
    for (auto& e : entries_) if (!e.style.label.empty()) labeled.push_back(&e);
    if (labeled.empty()) return;

    const i32 fscale  = font_scale(1);
    i32 line_h = rendering::text_height(fscale) + static_cast<i32>(sc(4.0));
    i32 max_w  = 0;

    for (auto* e : labeled) {
      i32 w = rendering::text_width(e->style.label, fscale);
      if (w > max_w) max_w = w;
    }

    i32 pad   = static_cast<i32>(legend_style_.padding * (dpmm_ / detail::BASELINE_DPMM));
    i32 swatch_w = static_cast<i32>(sc(18.0));  // colour swatch width
    i32 swatch_gap = static_cast<i32>(sc(6.0)); // gap between swatch and text

    i32 box_w = max_w + swatch_w + swatch_gap + pad * 2;
    i32 box_h = static_cast<i32>(labeled.size()) * line_h + pad * 2;

    i32 bx = static_cast<i32>(plot_area_.x + plot_area_.w) - box_w - static_cast<i32>(sc(8.0));
    i32 by = static_cast<i32>(plot_area_.y) + static_cast<i32>(sc(8.0));

    canvas_.fill_rect(bx, by, box_w, box_h, legend_style_.bg_color);
    canvas_.draw_rect(bx, by, box_w, box_h, legend_style_.border);

    i32 ty = by + pad;
    for (auto* e : labeled) {
      i32 lx = bx + pad;

      canvas_.draw_line(
        lx, ty + line_h / 2.0,
        lx + swatch_w, ty + line_h / 2.0,
        e->style.color, sc(2.0), false);

      rendering::draw_text(canvas_, e->style.label,
        lx + swatch_w + swatch_gap, ty,
        text_style_.color, fscale);
      ty += line_h;
    }
  }

  void render_title_and_labels() {
    const i32 title_scale  = font_scale(2);
    const i32 label_scale  = font_scale(1);

    if (!title_.empty()) {
      i32 tw = rendering::text_width(title_, title_scale);
      i32 tx = static_cast<i32>(fig_w_ / 2) - tw / 2;
      rendering::draw_text(canvas_, title_, tx, static_cast<i32>(sc(8.0)),
                           text_style_.color, title_scale);
    }
    if (!xlabel_.empty()) {
      i32 tw = rendering::text_width(xlabel_, label_scale);
      i32 tx = static_cast<i32>(plot_area_.x + plot_area_.w / 2) - tw / 2;
      i32 ty = static_cast<i32>(fig_h_) - static_cast<i32>(sc(15.0));
      rendering::draw_text(canvas_, xlabel_, tx, ty, text_style_.color, label_scale);
    }
    if (!ylabel_.empty()) {
      i32 lh = rendering::text_height_vertical(ylabel_, label_scale);
      i32 tx = static_cast<i32>(sc(5.0));
      i32 ty = static_cast<i32>(plot_area_.y + plot_area_.h / 2) - lh / 2;
      rendering::draw_text_vertical(canvas_, ylabel_, tx, ty,
                                    text_style_.color, label_scale);
    }
  }

  // -------------------------------------------------------------------------
  // Member data
  // -------------------------------------------------------------------------
  f64          dpmm_;    ///< dots (pixels) per millimetre — the resolution
  PhysicalSize phys_;    ///< physical dimensions in mm

  rendering::Canvas         canvas_;
  f64                       fig_w_, fig_h_;  ///< pixel dimensions (derived)
  detail::Rect              plot_area_;
  BBox                      data_bounds_;
  rendering::CoordTransform transform_;

  std::string title_, xlabel_, ylabel_;

  params::GridStyle   grid_style_;
  params::AxisStyle   axis_style_;
  params::LegendStyle legend_style_;
  params::LayoutStyle layout_style_;
  params::TextStyle   text_style_;
  params::PerfParams  perf_;

  std::vector<PlotEntry> entries_;
};

inline PlotCommand::~PlotCommand() { if (!committed_) commit(); }
inline void PlotCommand::commit()  { fig_.add_entry(std::move(entry_)); committed_ = true; }

} // NS plot2d

} // NS Sepia
