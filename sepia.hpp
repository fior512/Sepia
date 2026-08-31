#pragma once

#include <algorithm>
#include <array>
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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Sepia {

// Scalar types are the standard C++ types (float, double, std::int32_t, ...). Users pick any
// arithmetic type as the template parameter; no framework-specific aliases are exported.

// Public: static default scalar for all data containers. Specialize scalar_traits<void>
// (or another tag) before first use to switch a project to float for the speed-accuracy knob.
template <typename Tag = void>
struct scalar_traits { using type = double; };

// --- detail: implementation plumbing, not part of the public API 
namespace detail {

inline void* aligned_alloc_impl(std::size_t alignment, std::size_t bytes) {
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
  explicit Arena(std::size_t capacity)
    : capacity_(capacity)
    , buf_(static_cast<char*>(aligned_alloc_impl(64, capacity)))
  {}
  ~Arena() { aligned_free_impl(buf_); }

  Arena(const Arena&)            = delete;
  Arena& operator=(const Arena&) = delete;

  void* alloc(std::size_t bytes, std::size_t align = 8) {
    std::size_t padding    = (align - (offset_ % align)) % align;
    std::size_t new_offset = offset_ + padding + bytes;
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
  std::size_t used()     const { return offset_; }
  std::size_t capacity() const { return capacity_; }

private:
  std::size_t capacity_;
  std::size_t offset_ = 0;
  char* buf_;
};

// Pixel-space rectangle; only used inside Figure layout and CoordTransform
struct Rect {
  double x = 0, y = 0, w = 0, h = 0;

  constexpr Rect() = default;
  constexpr Rect(double x, double y, double w, double h) : x(x), y(y), w(w), h(h) {}
};

} // NS detail

// --- Public: AlignedBuffer<T> ---------------------------------------
// Cache-line (64-byte) aligned contiguous buffer. Move-only
template <typename T, std::size_t Alignment = 64>
class AlignedBuffer {
public:
  AlignedBuffer() = default;

  explicit AlignedBuffer(std::size_t count)
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

  void resize(std::size_t count) {
    if (count == size_) return;
    release();
    size_ = count;
    data_ = static_cast<T*>(detail::aligned_alloc_impl(Alignment, count * sizeof(T)));
  }

  T*       data()        { return data_; }
  const T* data()  const { return data_; }
  std::size_t    size()  const { return size_; }
  bool     empty() const { return size_ == 0; }

  T&       operator[](std::size_t i)       { return data_[i]; }
  const T& operator[](std::size_t i) const { return data_[i]; }

  T*       begin()       { return data_; }
  T*       end()         { return data_ + size_; }
  const T* begin() const { return data_; }
  const T* end()   const { return data_ + size_; }

private:
  void release() { if (data_) { detail::aligned_free_impl(data_); data_ = nullptr; size_ = 0; } }
  std::size_t size_ = 0;
  T*    data_ = nullptr;
};

// --- Public: Color --------------------------------------
struct Color {
  std::uint32_t r = 0, g = 0, b = 0, a = 255;

  constexpr Color() = default;
  constexpr Color(std::uint32_t r, std::uint32_t g, std::uint32_t b, std::uint32_t a = 255) : r(r), g(g), b(b), a(a) {}

  static constexpr Color black()  { return {0,   0,   0};   }
  static constexpr Color white()  { return {255, 255, 255}; }
  static constexpr Color red()    { return {228, 26,  28};  }
  static constexpr Color blue()   { return {55,  126, 184}; }
  static constexpr Color green()  { return {77,  175, 74};  }
  static constexpr Color orange() { return {255, 127, 0};   }
  static constexpr Color purple() { return {152, 78,  163}; }
  static constexpr Color gray()   { return {150, 150, 150}; }
  static constexpr Color teal()   { return {0,   150, 136}; }
  static constexpr Color magenta(){ return {231, 41,  138}; }
  static constexpr Color brown()  { return {121, 85,  72};  }
  static constexpr Color olive()  { return {128, 128, 0};   }

  constexpr bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
  constexpr bool operator!=(const Color& o) const { return !(*this == o); }

  constexpr Color with_alpha(std::uint32_t alpha) const { return {r, g, b, alpha}; }
};

// Auto palette for series without an explicit color; excludes black and white.
inline constexpr std::array<Color, 10> kAutoPalette = {{
  Color::blue(), Color::red(), Color::green(), Color::orange(), Color::purple(),
  Color::teal(), Color::magenta(), Color::brown(), Color::olive(), Color::gray()
}};

// --- Public: BBox -------------------------------------------------
struct BBox {
  double x_min =  std::numeric_limits<double>::max();
  double x_max = -std::numeric_limits<double>::max();
  double y_min =  std::numeric_limits<double>::max();
  double y_max = -std::numeric_limits<double>::max();

  constexpr bool empty() const { return x_min > x_max; }

  void expand(double x, double y) {
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

  double width()  const { return x_max - x_min; }
  double height() const { return y_max - y_min; }
};

// --- Public: style enumerations ------------------------------------------------
enum class LineStyle : std::uint32_t {
  Solid, Dashed, Dotted, DashDot, None
};

enum class MarkerStyle : std::uint32_t {
  None, Circle, Square, Triangle, Cross, Diamond
};

enum class ScaleType : std::uint32_t {
  Linear, Log
};

// --- Public: params, aggregate style structs ------------------------------------------
namespace params {

struct DataStyle {
  Color color = Color::blue();
  bool  auto_color = true;
  double width = 1.5;
  double alpha = 1.0;
  LineStyle line_style = LineStyle::Solid;

  MarkerStyle marker      = MarkerStyle::None;
  double         marker_size = 4.0;

  bool  fill       = false;
  Color fill_color = Color::blue().with_alpha(50);

  std::string label;
};

struct GridStyle {
  bool  show        = true;
  Color major_color = {220, 220, 220};
  Color minor_color = {240, 240, 240};
  double   major_width = 1.0;
  double   minor_width = 0.5;
  double   major_alpha = 0.8;
  double   minor_alpha = 0.4;
  bool  show_minor  = false;
};

struct AxisStyle {
  bool  show       = true;
  Color color      = Color::black();
  double   width      = 1.0;
  double   tick_size  = 5.0;
  Color tick_color = Color::black();

  ScaleType x_scale = ScaleType::Linear;
  double       x_min   = 0.0;
  double       x_max   = 0.0;

  ScaleType y_scale = ScaleType::Linear;
  double       y_min   = 0.0;
  double       y_max   = 0.0;
};

struct LegendStyle {
  bool  show     = true;
  Color bg_color = Color::white().with_alpha(230);
  Color border   = Color::gray();
  double   padding  = 8.0;
  std::string position = "top-right";
};

struct TextStyle {
  Color       color     = Color::black();
  double         font_size = 12.0;
  std::string font_face = "sans";
};

struct LayoutStyle {
  double   margin_top    = 50.0;
  double   margin_bottom = 60.0;
  double   margin_left   = 70.0;
  double   margin_right  = 20.0;
  Color background    = Color::white();
};

struct PerfParams {
  bool  lod_enable        = false;
  std::size_t lod_target_points = 2000;
};

} // NS params

// --- Public: data, owning and non-owning series types --------------------------
namespace data {

template <typename T = typename scalar_traits<>::type>
struct DataView {
  const T* ptr    = nullptr;
  std::size_t    count  = 0;
  std::size_t    stride = 1;

  DataView() = default;
  DataView(const T* p, std::size_t c, std::size_t s = 1) : ptr(p), count(c), stride(s) {}

  T     operator[](std::size_t i) const { return ptr[i * stride]; }
  bool  empty() const { return count == 0 || ptr == nullptr; }
};

// T is the stored/computed scalar, S is the input scalar. S defaults to T; different S
// compresses or widens on ingest (e.g. Series<float, double> stores double input as float).
template <typename T = typename scalar_traits<>::type>
class Series {
public:
  Series() = default;

  Series(const T* x, const T* y, std::size_t n) : x_(n), y_(n) {
    std::memcpy(x_.data(), x, n * sizeof(T));
    std::memcpy(y_.data(), y, n * sizeof(T));
    recompute_bounds();
  }

  // Input type S is deduced from the pointers and converted to T (compress or widen).
  template <typename S>
  Series(const S* x, const S* y, std::size_t n) : x_(n), y_(n) {
    for (std::size_t i = 0; i < n; ++i) { x_[i] = static_cast<T>(x[i]); y_[i] = static_cast<T>(y[i]); }
    recompute_bounds();
  }

  Series(AlignedBuffer<T>&& x, AlignedBuffer<T>&& y)
  : x_(std::move(x)), y_(std::move(y)) { recompute_bounds(); }

  DataView<T> x_view()  const { return {x_.data(), x_.size(), 1}; }
  DataView<T> y_view()  const { return {y_.data(), y_.size(), 1}; }
  std::size_t       size()    const { return x_.size(); }
  const BBox& bounds()  const { return bounds_; }

  T*       x_data()       { return x_.data(); }
  T*       y_data()       { return y_.data(); }
  const T* x_data() const { return x_.data(); }
  const T* y_data() const { return y_.data(); }

  void recompute_bounds() {
    bounds_ = {};
    const std::size_t n = x_.size();
    for (std::size_t i = 0; i < n; ++i) bounds_.expand(static_cast<double>(x_[i]), static_cast<double>(y_[i]));
  }

private:
  AlignedBuffer<T> x_;
  AlignedBuffer<T> y_;
  BBox bounds_;
};

template <typename T = typename scalar_traits<>::type>
class ExternalSeries {
public:
  ExternalSeries() = default;
  ExternalSeries(const T* x, const T* y, std::size_t n, std::size_t stride = 1)
  : x_{x, n, stride}, y_{y, n, stride} { recompute_bounds(); }

  DataView<T> x_view()  const { return x_; }
  DataView<T> y_view()  const { return y_; }
  std::size_t       size()    const { return x_.count; }
  const BBox& bounds()  const { return bounds_; }

  void recompute_bounds() {
    bounds_ = {};
    for (std::size_t i = 0; i < x_.count; ++i) bounds_.expand(static_cast<double>(x_[i]), static_cast<double>(y_[i]));
  }

private:
  DataView<T> x_, y_;
  BBox bounds_;
};

// Internal: LTTB decimator. Users configure via PerfParams; never call directly
template <typename T = typename scalar_traits<>::type>
class LttbDecimator {
public:
  static Series<T> decimate(const DataView<T>& xv, const DataView<T>& yv, std::size_t target) {
    const std::size_t n = xv.count;
    if (target >= n || target < 3) {
      AlignedBuffer<T> ox(n), oy(n);
      for (std::size_t i = 0; i < n; ++i) { ox[i] = xv[i]; oy[i] = yv[i]; }
      return Series<T>(std::move(ox), std::move(oy));
    }

    AlignedBuffer<T> ox(target), oy(target);
    ox[0] = xv[0]; oy[0] = yv[0];

    const double bucket_size = static_cast<double>(n - 2) / static_cast<double>(target - 2);
    std::size_t a = 0;

    for (std::size_t i = 1; i < target - 1; ++i) {
      std::size_t bucket_start = static_cast<std::size_t>((i - 1) * bucket_size) + 1;
      std::size_t bucket_end   = static_cast<std::size_t>(i * bucket_size) + 1;
      if (bucket_end > n - 1) bucket_end = n - 1;

      std::size_t next_start = static_cast<std::size_t>(i * bucket_size) + 1;
      std::size_t next_end   = static_cast<std::size_t>((i + 1) * bucket_size) + 1;
      if (next_end > n) next_end = n;

      T avg_x = 0, avg_y = 0;
      std::size_t next_count = next_end - next_start;
      for (std::size_t j = next_start; j < next_end; ++j) { avg_x += xv[j]; avg_y += yv[j]; }
      if (next_count > 0) { avg_x /= static_cast<T>(next_count); avg_y /= static_cast<T>(next_count); }

      T max_area = static_cast<T>(-1); std::size_t max_idx = bucket_start;
      T ax = xv[a], ay = yv[a];

      for (std::size_t j = bucket_start; j < bucket_end; ++j) {
        T area = std::abs(
          (ax - avg_x) * (yv[j] - ay) - (ax - xv[j]) * (avg_y - ay)
        ) * static_cast<T>(0.5);
        if (area > max_area) { max_area = area; max_idx = j; }
      }

      ox[i] = xv[max_idx]; oy[i] = yv[max_idx];
      a = max_idx;
    }

    ox[target - 1] = xv[n - 1]; oy[target - 1] = yv[n - 1];
    return Series<T>(std::move(ox), std::move(oy));
  }
};

} // NS data

// --- rendering: Canvas is semi-public; everything else is internal ----------------------------
namespace rendering {

// Semi-public: accessible via Figure::canvas() for custom renderers
class Canvas {
public:
  Canvas() = default;

  Canvas(std::uint32_t width, std::uint32_t height)
    : width_(width), height_(height)
    , pixels_(static_cast<std::size_t>(width) * height * 4)
  {
    clear({255, 255, 255, 255});
  }


  void clear(Color c) {
    std::uint32_t* px     = reinterpret_cast<std::uint32_t*>(pixels_.data());
    std::uint32_t  packed = pack(c);
    std::size_t total = static_cast<std::size_t>(width_) * height_;
    std::fill(px, px + total, packed);
  }


  void set_pixel(std::int32_t x, std::int32_t y, Color c) {
    if (x < 0 || y < 0 || x >= static_cast<std::int32_t>(width_) || y >= static_cast<std::int32_t>(height_)) return;
    std::size_t idx = (static_cast<std::size_t>(y) * width_ + x) * 4;
    std::uint8_t* dst = pixels_.data() + idx;

    if (c.a == 255) {
      dst[0] = static_cast<std::uint8_t>(c.r); dst[1] = static_cast<std::uint8_t>(c.g);
      dst[2] = static_cast<std::uint8_t>(c.b); dst[3] = 255;

    } else {
      std::uint32_t sa = c.a, da = 255 - sa;
      dst[0] = static_cast<std::uint8_t>((c.r * sa + dst[0] * da) / 255);
      dst[1] = static_cast<std::uint8_t>((c.g * sa + dst[1] * da) / 255);
      dst[2] = static_cast<std::uint8_t>((c.b * sa + dst[2] * da) / 255);
      dst[3] = static_cast<std::uint8_t>(sa + (dst[3] * da) / 255);
    }
  }

  void draw_line(double x0, double y0, double x1, double y1, Color c, double width = 1.0, bool aa=true) {
    if(aa) draw_line_aa(x0, y0, x1, y1, c, width);
    else draw_line_bresenham(x0, y0, x1, y1, c, width);
  }


  void fill_rect(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, Color c) {
    std::int32_t x0 = std::max(x, 0), y0 = std::max(y, 0);
    std::int32_t x1 = std::min(x + w, static_cast<std::int32_t>(width_));
    std::int32_t y1 = std::min(y + h, static_cast<std::int32_t>(height_));

    for (std::int32_t py = y0; py < y1; ++py)
      for (std::int32_t px = x0; px < x1; ++px) set_pixel(px, py, c);
  }


  void draw_rect(std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h, Color c, double line_w = 1.0) { // legend
    draw_line(x, y, x + w, y, c, line_w, false);
    draw_line(x + w, y, x + w, y + h, c, line_w, false);
    draw_line(x + w, y + h, x, y + h, c, line_w, false);
    draw_line(x, y + h, x, y, c, line_w, false);
  }


  void draw_circle(std::int32_t cx, std::int32_t cy, double radius, Color c) {
    std::int32_t r = static_cast<std::int32_t>(radius);
    for (std::int32_t dy = -r; dy <= r; ++dy)
      for (std::int32_t dx = -r; dx <= r; ++dx)
        if (dx * dx + dy * dy <= r * r)
          set_pixel(cx + dx, cy + dy, c);
  }


  std::uint8_t*       data()   { return pixels_.data(); }
  const std::uint8_t* data()   const { return pixels_.data(); }
  std::uint32_t       width()  const { return width_; }
  std::uint32_t       height() const { return height_; }
  std::size_t     stride() const { return static_cast<std::size_t>(width_) * 4; }

private:
  static std::uint32_t pack(Color c) {
    return static_cast<std::uint32_t>(c.r)
    | (static_cast<std::uint32_t>(c.g) << 8)
    | (static_cast<std::uint32_t>(c.b) << 16)
    | (static_cast<std::uint32_t>(c.a) << 24);
  }

  void draw_line_aa(double x0, double y0, double x1, double y1, Color c, double /*width*/) {
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep)   { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

    double dx = x1 - x0, dy = y1 - y0;
    double gradient = (dx == 0.0) ? 1.0 : dy / dx;

    double xend = std::round(x0), yend = y0 + gradient * (xend - x0);
    double xgap = rfpart(x0 + 0.5);
    std::int32_t xpxl1 = static_cast<std::int32_t>(xend), ypxl1 = static_cast<std::int32_t>(std::floor(yend));
    if (steep) {
      plot(ypxl1,     xpxl1, c, rfpart(yend) * xgap);
      plot(ypxl1 + 1, xpxl1, c,  fpart(yend) * xgap);
    } else {
      plot(xpxl1, ypxl1,     c, rfpart(yend) * xgap);
      plot(xpxl1, ypxl1 + 1, c,  fpart(yend) * xgap);
    }
    double intery = yend + gradient;

    xend = std::round(x1); yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5);
    std::int32_t xpxl2 = static_cast<std::int32_t>(xend), ypxl2 = static_cast<std::int32_t>(std::floor(yend));
    if (steep) {
      plot(ypxl2,     xpxl2, c, rfpart(yend) * xgap);
      plot(ypxl2 + 1, xpxl2, c,  fpart(yend) * xgap);
    } else {
      plot(xpxl2, ypxl2,     c, rfpart(yend) * xgap);
      plot(xpxl2, ypxl2 + 1, c,  fpart(yend) * xgap);
    }

    for (std::int32_t x = xpxl1 + 1; x < xpxl2; ++x) {
      std::int32_t iy = static_cast<std::int32_t>(std::floor(intery));
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

  void draw_line_bresenham(double x0, double y0, double x1, double y1, Color c, double width) {
    // Convert to integer coordinates (rounding)
    std::int32_t x0i = static_cast<std::int32_t>(std::round(x0));
    std::int32_t y0i = static_cast<std::int32_t>(std::round(y0));
    std::int32_t x1i = static_cast<std::int32_t>(std::round(x1));
    std::int32_t y1i = static_cast<std::int32_t>(std::round(y1));

    // Width > 1 is more complex; for simplicity, we ignore width > 1 in this example
    // (you could draw multiple parallel lines later)

    // Bresenham algorithm
    bool steep = std::abs(y1i - y0i) > std::abs(x1i - x0i);
    if (steep) {
        std::swap(x0i, y0i);
        std::swap(x1i, y1i);
    }
    if (x0i > x1i) {
        std::swap(x0i, x1i);
        std::swap(y0i, y1i);
    }

    std::int32_t dx = x1i - x0i;
    std::int32_t dy = std::abs(y1i - y0i);
    std::int32_t err = dx / 2;
    std::int32_t ystep = (y0i < y1i) ? 1 : -1;
    std::int32_t y = y0i;

    for (std::int32_t x = x0i; x <= x1i; ++x) {
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

  static double fpart(double x)  { return x - std::floor(x); }
  static double rfpart(double x) { return 1.0 - fpart(x); }

  void plot(std::int32_t x, std::int32_t y, Color c, double brightness) {
    set_pixel(x, y, {c.r, c.g, c.b, static_cast<std::uint32_t>(c.a * brightness)});
  }

  std::uint32_t width_ = 0, height_ = 0;
  AlignedBuffer<std::uint8_t> pixels_;
};

// Internal: 5*7 bitmap font, no external font dependency
namespace font {

struct Glyph { std::uint8_t rows[7]; };

inline const Glyph& get_glyph(char ch) {
  static const bool init = []() -> bool { return true; }();
  (void)init;

  static Glyph table[128] = {};
  static bool built = false;
  if (!built) {
    built = true;
    auto set = [&](char c, std::uint8_t r0, std::uint8_t r1, std::uint8_t r2, std::uint8_t r3, std::uint8_t r4, std::uint8_t r5, std::uint8_t r6) {
      auto& g = table[static_cast<std::uint8_t>(c)];
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

  std::uint8_t idx = static_cast<std::uint8_t>(ch);
  if (idx < 32 || idx > 126) idx = 127;
  return table[idx];
}

} // NS font

// Internal: text rendering helpers used only by Figure render passes
inline void draw_text(Canvas& canvas, const std::string& text, std::int32_t x, std::int32_t y, Color color, std::int32_t scale = 1) {
  std::int32_t cursor_x = x;
  for (char ch : text) {
    const auto& g = font::get_glyph(ch);
    for (std::int32_t row = 0; row < 7; ++row)
      for (std::int32_t col = 0; col < 5; ++col)
        if (g.rows[row] & (0x10 >> col))
          for (std::int32_t sy = 0; sy < scale; ++sy)
            for (std::int32_t sx = 0; sx < scale; ++sx)
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
  std::int32_t x, std::int32_t y, Color color, std::int32_t scale = 1
) {

  std::int32_t lh = static_cast<std::int32_t>(text.size()) * 6 * scale - scale;
  std::int32_t cursor_y = y + lh - 5 * scale;
  for (char ch : text) {
    const auto& g = font::get_glyph(ch);
    for (std::int32_t row = 0; row < 7; ++row)
      for (std::int32_t col = 0; col < 5; ++col)
        if (g.rows[row] & (0x10 >> col))
          for (std::int32_t sy = 0; sy < scale; ++sy)
            for (std::int32_t sx = 0; sx < scale; ++sx)
              canvas.set_pixel(
                x + row * scale + sx,
                cursor_y + (4 - col) * scale + sy, 
                color
              );
    cursor_y -= 6 * scale;
  }
}

inline std::int32_t text_width(const std::string& text, std::int32_t scale = 1) {
  if (text.empty()) return 0;
  return static_cast<std::int32_t>(text.size()) * 6 * scale - scale;
}
inline std::int32_t text_height(std::int32_t scale = 1) { return 7 * scale; }
inline std::int32_t text_width_vertical(std::int32_t /*text_len*/, std::int32_t scale = 1) { return 7 * scale; }
inline std::int32_t text_height_vertical(const std::string& text, std::int32_t scale = 1) {
  if (text.empty()) return 0;
  return static_cast<std::int32_t>(text.size()) * 6 * scale - scale;
}

// Internal: tick generation for axes.
struct Tick {
  double         value;
  std::string label;
  bool        is_minor = false;
};

class TickEngine {
public:
  static std::vector<Tick> compute(double lo, double hi, std::size_t target_ticks = 6, bool include_minor = false, ScaleType scale = ScaleType::Linear) {
    if (scale == ScaleType::Log) return compute_log(lo, hi, include_minor);
    return compute_linear(lo, hi, target_ticks, include_minor);
  }

private:
  static std::vector<Tick> compute_linear(double lo, double hi, std::size_t target_ticks, bool include_minor) {
    std::vector<Tick> ticks;
    if (hi <= lo) return ticks;

    double range      = hi - lo;
    double rough_step = range / static_cast<double>(target_ticks);
    double mag        = std::pow(10.0, std::floor(std::log10(rough_step)));
    double norm       = rough_step / mag;

    double nice;
    if      (norm < 1.5) nice = 1.0;
    else if (norm < 3.0) nice = 2.0;
    else if (norm < 7.0) nice = 5.0;
    else                 nice = 10.0;

    double step  = nice * mag;
    double start = std::floor(lo / step) * step;

    for (double v = start; v <= hi + step * 0.001; v += step) {
      if (v < lo - step * 0.001) continue;
      ticks.push_back({v, format_value(v, step), false});

      if (include_minor) {
        double minor_step = step / 5.0;
        for (int m = 1; m < 5; ++m) {
          double mv = v + m * minor_step;

          if (mv > lo && mv < hi) ticks.push_back({mv, "", true});
        }
      }
    }
    return ticks;
  }

  static std::vector<Tick> compute_log(double lo, double hi, bool include_minor) {
    std::vector<Tick> ticks;
    if (hi <= 0 || lo <= 0) return ticks;

    int exp_lo = static_cast<int>(std::floor(std::log10(lo)));
    int exp_hi = static_cast<int>(std::ceil(std::log10(hi)));

    for (int e = exp_lo; e <= exp_hi; ++e) {
      double val = std::pow(10.0, e);
      if (val >= lo && val <= hi) ticks.push_back({val, format_log_value(val), false});
      if (include_minor)
        for (int m = 2; m <= 9; ++m) {
          double mv = m * val;
          if (mv > lo && mv < hi) ticks.push_back({mv, "", true});
        }
    }
    return ticks;
  }

  static std::string format_value(double v, double step) {
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

  static std::string format_log_value(double v) {
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

    double x_lo = x_log_ ? safe_log10(data_box.x_min) : data_box.x_min;
    double x_hi = x_log_ ? safe_log10(data_box.x_max) : data_box.x_max;
    double y_lo = y_log_ ? safe_log10(data_box.y_min) : data_box.y_min;
    double y_hi = y_log_ ? safe_log10(data_box.y_max) : data_box.y_max;

    double dw = x_hi - x_lo, dh = y_hi - y_lo;
    sx_ = (dw > 0) ? pixel_rect.w / dw : 1.0;
    sy_ = (dh > 0) ? pixel_rect.h / dh : 1.0;
    log_x_lo_ = x_lo; log_y_lo_ = y_lo;
  }

  inline double to_px_x(double x) const {
    double v = x_log_ ? safe_log10(x) : x;
    return rect_.x + (v - log_x_lo_) * sx_;
  }
  inline double to_px_y(double y) const {
    double v = y_log_ ? safe_log10(y) : y;
    return rect_.y + rect_.h - (v - log_y_lo_) * sy_;
  }
  inline double to_data_x(double px) const {
    double v = log_x_lo_ + (px - rect_.x) / sx_;
    return x_log_ ? std::pow(10.0, v) : v;
  }
  inline double to_data_y(double py) const {
    double v = log_y_lo_ + (rect_.y + rect_.h - py) / sy_;
    return y_log_ ? std::pow(10.0, v) : v;
  }

  const BBox& data_box() const { return data_; }
  const detail::Rect& pixel_rect() const { return rect_; }

private:
  static inline double safe_log10(double v) { return std::log10(v > 1e-300 ? v : 1e-300); }

  BBox        data_;
  detail::Rect rect_;
  double  sx_ = 1.0, sy_ = 1.0;
  double  log_x_lo_ = 0.0, log_y_lo_ = 0.0;
  bool x_log_ = false, y_log_ = false;
};

} // NS rendering

// Internal: PPM file output, called only through Figure::save_ppm()
namespace output {

inline bool write_ppm(const rendering::Canvas& canvas, const std::string& path) {

  std::ofstream f(path, std::ios::binary);
  if (!f.is_open()) return false;

  const std::uint8_t* data = canvas.data();
  std::size_t w = canvas.width(), h = canvas.height();
  std::size_t row_bytes = w * 3;
  std::vector<std::uint8_t> row(row_bytes);

  f << "P6\n" << w << " " << h << "\n255\n";

  // Assemble each RGB row in memory, then write it in one call instead of three put() per pixel.
  for (std::size_t y = 0; y < h; ++y) {
    const std::uint8_t* src = data + (y * w) * 4;
    for (std::size_t x = 0; x < w; ++x) {
      std::size_t s = x * 4, d = x * 3;
      row[d + 0] = src[s + 0];
      row[d + 1] = src[s + 1];
      row[d + 2] = src[s + 2];

    }
    f.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row_bytes));
  }

  return f.good();
}

} // NS output

// --- Public: plot2d -------------------------------------------------------
namespace plot2d {

template <typename T = typename scalar_traits<>::type>
using SeriesVariant = std::variant<data::Series<T>, data::ExternalSeries<T>>;

template <typename T = typename scalar_traits<>::type>
struct PlotEntry {
  SeriesVariant<T> series;
  params::DataStyle style;

  data::DataView<T> x_view() const { return std::visit([](auto& s){ return s.x_view(); }, series); }
  data::DataView<T> y_view() const { return std::visit([](auto& s){ return s.y_view(); }, series); }
  std::size_t       size()   const { return std::visit([](auto& s){ return s.size();   }, series); }
  BBox              bounds() const { return std::visit([](auto& s){ return s.bounds(); }, series); }
};

template <typename T>
class Figure;

template <typename T = typename scalar_traits<>::type>
class PlotCommand {
public:
  PlotCommand(Figure<T>& fig, data::Series<T>&& series) : 
    fig_(fig), entry_{std::move(series), {}} 
  {}

  PlotCommand(Figure<T>& fig, data::ExternalSeries<T>&& series) : 
    fig_(fig), entry_{std::move(series), {}} 
  {}

  PlotCommand& data(const params::DataStyle& style) {
    entry_.style = style;
    if (style.color != Color::blue()) entry_.style.auto_color = false;
    return *this;
  }

  PlotCommand& color(Color c)              { entry_.style.color      = c; entry_.style.auto_color = false; return *this; }
  PlotCommand& width(double w)             { entry_.style.width      = w; return *this; }
  PlotCommand& alpha(double a)             { entry_.style.alpha      = a; return *this; }
  PlotCommand& label(const std::string& l) { entry_.style.label      = l;    return *this; }
  PlotCommand& line(LineStyle s)           { entry_.style.line_style = s;    return *this; }

  PlotCommand& marker(MarkerStyle m, double size = 4.0) {
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
  Figure<T>& fig_;
  PlotEntry<T> entry_;
  bool      committed_ = false;

  void commit();
  friend class Figure<T>;
};

template <typename T = typename scalar_traits<>::type>
class Figure {
public:
  Figure(double width, double height) : 
    canvas_(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)), 
    fig_w_(width), fig_h_(height) 
  {}

  void set_title(const std::string& t)  { title_  = t; }
  void set_xlabel(const std::string& l) { xlabel_ = l; }
  void set_ylabel(const std::string& l) { ylabel_ = l; }

  void grid(const params::GridStyle& g)     { grid_style_   = g; }
  void axis(const params::AxisStyle& a)     { axis_style_   = a; }
  void legend(const params::LegendStyle& l) { legend_style_ = l; }
  void layout(const params::LayoutStyle& l) { layout_style_ = l; }
  void text(const params::TextStyle& t)     { text_style_   = t; }
  void perf(const params::PerfParams& p)    { perf_         = p; }

  
  // LINE PLOTS. Input type S is deduced from the pointers and stored/computed in T.
  template <typename S>
  inline PlotCommand<T> plot(const S* x, const S* y, std::size_t n) {
    return PlotCommand<T>(*this, data::Series<T>(x, y, n));
  }
  inline PlotCommand<T> plot(data::Series<T>&& s) {
    return PlotCommand<T>(*this, std::move(s));
  }
  inline PlotCommand<T> plot_ref(const T* x, const T* y, std::size_t n, std::size_t stride = 1) {
    return PlotCommand<T>(*this, data::ExternalSeries<T>(x, y, n, stride));
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

  const std::vector<PlotEntry<T>>& entries() const { return entries_; }

private:
  friend class PlotCommand<T>;

  void add_entry(PlotEntry<T>&& e) {
    if (e.style.auto_color) {
      e.style.color = kAutoPalette[auto_color_index_ % kAutoPalette.size()];
      e.style.auto_color = false;
      ++auto_color_index_;
    }
    entries_.push_back(std::move(e));
  }

  void compute_plot_area() {
    plot_area_ = {
      layout_style_.margin_left,
      layout_style_.margin_top,
      fig_w_ - layout_style_.margin_left - layout_style_.margin_right,
      fig_h_ - layout_style_.margin_top  - layout_style_.margin_bottom
    };
  }

  void compute_data_bounds() {
    data_bounds_ = {};
    for (auto& e : entries_) data_bounds_.merge(e.bounds());

    if (std::bit_cast<std::uint64_t>(axis_style_.x_min)) data_bounds_.x_min = axis_style_.x_min;
    if (std::bit_cast<std::uint64_t>(axis_style_.x_max)) data_bounds_.x_max = axis_style_.x_max;
    if (std::bit_cast<std::uint64_t>(axis_style_.y_min)) data_bounds_.y_min = axis_style_.y_min;
    if (std::bit_cast<std::uint64_t>(axis_style_.y_max)) data_bounds_.y_max = axis_style_.y_max;

    if (!data_bounds_.empty()) {
      if (axis_style_.x_scale == ScaleType::Log && data_bounds_.x_min > 0) {
        double log_lo = std::log10(data_bounds_.x_min);
        double log_hi = std::log10(data_bounds_.x_max);
        double pad = (log_hi - log_lo) * 0.05; if (pad == 0) pad = 0.15;

        data_bounds_.x_min = std::pow(10.0, log_lo - pad);
        data_bounds_.x_max = std::pow(10.0, log_hi + pad);
      } else {
        double xpad = data_bounds_.width() * 0.05; if (xpad == 0) xpad = 1.0;
        data_bounds_.x_min -= xpad; data_bounds_.x_max += xpad;
      }

      if (axis_style_.y_scale == ScaleType::Log && data_bounds_.y_min > 0) {
        double log_lo = std::log10(data_bounds_.y_min);
        double log_hi = std::log10(data_bounds_.y_max);
        double pad = (log_hi - log_lo) * 0.05; if (pad == 0) pad = 0.15;

        data_bounds_.y_min = std::pow(10.0, log_lo - pad);
        data_bounds_.y_max = std::pow(10.0, log_hi + pad);
      } else {
        double ypad = data_bounds_.height() * 0.05; if (ypad == 0) ypad = 1.0;
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
      data_bounds_.x_min, 
      data_bounds_.x_max, 
      8, 
      grid_style_.show_minor, 
      axis_style_.x_scale
    );

    auto yticks = rendering::TickEngine::compute(
      data_bounds_.y_min, 
      data_bounds_.y_max, 
      6, 
      grid_style_.show_minor, 
      axis_style_.y_scale
    );


    for (auto& t : xticks) {
      double px = transform_.to_px_x(t.value);
      Color c  = t.is_minor ? grid_style_.minor_color : grid_style_.major_color;
      double w  = t.is_minor ? grid_style_.minor_width : grid_style_.major_width;
      double a  = t.is_minor ? grid_style_.minor_alpha : grid_style_.major_alpha;

      c = c.with_alpha(static_cast<std::uint32_t>(a * 255));
      canvas_.draw_line(px, plot_area_.y, px, plot_area_.y + plot_area_.h, c, w, false);
    }


    for (auto& t : yticks) {
      double py = transform_.to_px_y(t.value);
      Color c  = t.is_minor ? grid_style_.minor_color : grid_style_.major_color;
      double w  = t.is_minor ? grid_style_.minor_width : grid_style_.major_width;
      double a  = t.is_minor ? grid_style_.minor_alpha : grid_style_.major_alpha;

      c = c.with_alpha(static_cast<std::uint32_t>(a * 255));
      canvas_.draw_line(plot_area_.x, py, plot_area_.x + plot_area_.w, py, c, w, false);
    }
  }

  void render_axes() {
    if (!axis_style_.show) return;
    Color c = axis_style_.color;
    double w = axis_style_.width;
    double x0 = plot_area_.x, y0 = plot_area_.y;
    double x1 = x0 + plot_area_.w, y1 = y0 + plot_area_.h;

    canvas_.draw_line(x0, y0, x1, y0, c, w, false);
    canvas_.draw_line(x1, y0, x1, y1, c, w, false);
    canvas_.draw_line(x1, y1, x0, y1, c, w, false);
    canvas_.draw_line(x0, y1, x0, y0, c, w, false);


    auto xticks = rendering::TickEngine::compute(
      data_bounds_.x_min, 
      data_bounds_.x_max, 
      8, 
      false, 
      axis_style_.x_scale
    );


    for (auto& t : xticks) {
      double px = transform_.to_px_x(t.value);
      canvas_.draw_line(px, y1, px, y1 + axis_style_.tick_size, c, w, false);

      if (!t.label.empty()) {
        std::int32_t tw = rendering::text_width(t.label, 1);

        rendering::draw_text(
          canvas_, 
          t.label,
          static_cast<std::int32_t>(px) - tw / 2,
          static_cast<std::int32_t>(y1 + axis_style_.tick_size + 3),
          text_style_.color, 
          1
        );
      }
    }

    auto yticks = rendering::TickEngine::compute(
      data_bounds_.y_min, 
      data_bounds_.y_max, 
      6, 
      false, 
      axis_style_.y_scale
    );


    for (auto& t : yticks) {
      double py = transform_.to_px_y(t.value);
      canvas_.draw_line(x0 - axis_style_.tick_size, py, x0, py, c, w, false);


      if (!t.label.empty()) {
        std::int32_t tw = rendering::text_width(t.label, 1);
        rendering::draw_text(
          canvas_, 
          t.label,
          static_cast<std::int32_t>(x0 - axis_style_.tick_size - 3) - tw,
          static_cast<std::int32_t>(py) - 3,
          text_style_.color, 
          1
        );
      }
    }
  }

  void render_data() {
    for (auto& entry : entries_) {
      auto xv = entry.x_view(), yv = entry.y_view();
      const auto& st = entry.style;

      data::DataView<T> rx = xv, ry = yv;
      data::Series<T> decimated;

      if (perf_.lod_enable) {
        if (xv.count >= perf_.lod_target_points) {
          decimated = data::LttbDecimator<T>::decimate(xv, yv, perf_.lod_target_points);
          rx = decimated.x_view();
          ry = decimated.y_view();
        }
      }

      Color c = st.color;
      if (st.alpha < 1.0) c = c.with_alpha(static_cast<std::uint32_t>(st.alpha * 255));

      if (st.line_style != LineStyle::None && rx.count > 1) {
        for (std::size_t i = 1; i < rx.count; ++i) {
          double x0 = static_cast<double>(rx[i-1]), y0 = static_cast<double>(ry[i-1]);
          double x1 = static_cast<double>(rx[i]), y1 = static_cast<double>(ry[i]);
          if (std::isnan(x0) || std::isnan(y0) || std::isnan(x1) || std::isnan(y1)) continue;
          canvas_.draw_line(
            transform_.to_px_x(x0), transform_.to_px_y(y0),
            transform_.to_px_x(x1), transform_.to_px_y(y1),
            c,
            st.width
          );
        }
      }

      if (st.fill && rx.count > 1) {
        double base_py = transform_.to_px_y(data_bounds_.y_min);
        for (std::size_t i = 0; i < rx.count; ++i) {
          if (std::isnan(rx[i]) || std::isnan(ry[i])) continue;
          std::int32_t ix     = static_cast<std::int32_t>(transform_.to_px_x(static_cast<double>(rx[i])));
          std::int32_t iy_top = static_cast<std::int32_t>(transform_.to_px_y(static_cast<double>(ry[i])));
          std::int32_t iy_bot = static_cast<std::int32_t>(base_py);
          for (std::int32_t y = iy_top; y <= iy_bot; ++y)
            canvas_.set_pixel(ix, y, st.fill_color);
        }
      }

      if (st.marker != MarkerStyle::None) {
        for (std::size_t i = 0; i < rx.count; ++i) {
          if (std::isnan(rx[i]) || std::isnan(ry[i])) continue;
          canvas_.draw_circle(
            static_cast<std::int32_t>(transform_.to_px_x(static_cast<double>(rx[i]))),
            static_cast<std::int32_t>(transform_.to_px_y(static_cast<double>(ry[i]))),
            st.marker_size, 
            c
          );
        }
      }
    }
  }

  void render_legend() {
    if (!legend_style_.show) return;
    std::vector<const PlotEntry<T>*> labeled;
    for (auto& e : entries_) if (!e.style.label.empty()) labeled.push_back(&e);
    if (labeled.empty()) return;

    std::int32_t scale  = 1;
    std::int32_t line_h = rendering::text_height(scale) + 4;
    std::int32_t max_w  = 0;


    for (auto* e : labeled) {
      std::int32_t w = rendering::text_width(e->style.label, scale);
      if (w > max_w) max_w = w;
    }

    std::int32_t box_w = max_w + 30 + static_cast<std::int32_t>(legend_style_.padding * 2);
    std::int32_t box_h = static_cast<std::int32_t>(labeled.size()) * line_h
      + static_cast<std::int32_t>(legend_style_.padding * 2);


    std::int32_t bx = static_cast<std::int32_t>(plot_area_.x + plot_area_.w) - box_w - 8;
    std::int32_t by = static_cast<std::int32_t>(plot_area_.y) + 8;

    canvas_.fill_rect(bx, by, box_w, box_h, legend_style_.bg_color);
    canvas_.draw_rect(bx, by, box_w, box_h, legend_style_.border);


    std::int32_t ty = by + static_cast<std::int32_t>(legend_style_.padding);
    for (auto* e : labeled) {
      std::int32_t lx = bx + static_cast<std::int32_t>(legend_style_.padding);

      canvas_.draw_line(
        lx, 
        ty + line_h / 2.0, 
        lx + 18, 
        ty + line_h / 2.0,
        e->style.color, 
        2.0,
        false
      );

      rendering::draw_text(canvas_, e->style.label, lx + 24, ty, text_style_.color, scale);
      ty += line_h;
    }
  }

  void render_title_and_labels() {
    std::int32_t scale = 2;
    if (!title_.empty()) {
      std::int32_t tw = rendering::text_width(title_, scale);
      std::int32_t tx = static_cast<std::int32_t>(fig_w_ / 2) - tw / 2;
      rendering::draw_text(canvas_, title_, tx, 8, text_style_.color, scale);
    }
    scale = 1;
    if (!xlabel_.empty()) {
      std::int32_t tw = rendering::text_width(xlabel_, scale);
      std::int32_t tx = static_cast<std::int32_t>(plot_area_.x + plot_area_.w / 2) - tw / 2;
      std::int32_t ty = static_cast<std::int32_t>(fig_h_) - 15;
      rendering::draw_text(canvas_, xlabel_, tx, ty, text_style_.color, scale);
    }
    if (!ylabel_.empty()) {
      std::int32_t lh = rendering::text_height_vertical(ylabel_, scale);
      std::int32_t tx = 5;
      std::int32_t ty = static_cast<std::int32_t>(plot_area_.y + plot_area_.h / 2) - lh / 2;
      rendering::draw_text_vertical(canvas_, ylabel_, tx, ty, text_style_.color, scale);
    }
  }

  rendering::Canvas         canvas_;
  double                       fig_w_, fig_h_;
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

  std::vector<PlotEntry<T>> entries_;
  std::size_t auto_color_index_ = 0; // counts auto-assigned series, independent of total entries
};


template <typename T>
inline PlotCommand<T>::~PlotCommand() { if (!committed_) commit(); }
template <typename T>
inline void PlotCommand<T>::commit()  { fig_.add_entry(std::move(entry_)); committed_ = true; }

} // NS plot2d

} // NS Sepia
