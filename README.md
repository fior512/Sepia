# 🖌️ Sepia
>  Sepia: named after the cuttlefish (`Sepia officinalis`), which releases ink, here we "ink" pixels onto the plots.

![GitHub last commit](https://img.shields.io/github/last-commit/MoonFlowww/Sepia?logo=github)
![Unique Cloners](https://img.shields.io/badge/Unique_Cloners-214-blue?logo=github)

A native C++20 plotting framework for publication-quality 2D visualizations. Single-Header and Zero external dependencies.

Handles datasets from a few points to **tens of millions** through built-in LTTB (Largest-Triangle-Three-Buckets) decimation.

## Features

- 2D line plots with multiple series, colors, line styles, markers, and fill
- Automatic axis generation with "nice" tick marks and grid rendering
- Built-in 5x7 bitmap font;  no FreeType or external font libraries
- Xiaolin Wu anti-aliased line drawing
- LTTB Level-of-Detail decimation for mega-datasets
- Cache-line aligned memory buffers (SIMD-friendly)
- PPM image output; no libpng, no external image libraries
- Single-Header

## Download

```bash
git clone https://github.com/MoonFlowww/Sepia.git
cd Sepia
```

## Building

Sepia is Single-Header. Only need to copy-paste `sepia.hpp` into your project.


## Quick Start

```cpp
#include "sepia.hpp"
#include <cmath>
#include <vector>

int main() {
  const size_t N = 200;
  std::vector<Sepia::f64> x(N), y(N);
  for (size_t i = 0; i < N; ++i) { // generate dummy data
    x[i] = static_cast<double>(i) * 0.05;
    y[i] = std::sin(x[i]);
  }

  Sepia::plot2d::Figure figure(700.0, 450.0); // Plot Init
  figure.set_title("Sine Wave");
  figure.set_xlabel("Time");
  figure.set_ylabel("Amplitude");

  figure.plot(x.data(), y.data(), N)
    .data({.color = Sepia::Color::blue(), .width = 2.0, .label = "sin(x)"}); // optional parameters

  figure.grid({.show = true, .major_color = {210, 210, 210}}); // optional parameters

  figure.render(); // plot
  figure.save_ppm("basic.ppm"); // save image
  return 0;
}
```

## API Reference

### Figure

The central user-facing class. Create one, configure it, plot data, render, and save.

```cpp
Sepia::plot2d::Figure figure(width, height);   // dimensions in pixels

// Titles and labels
figure.set_title("My Plot");
figure.set_xlabel("X Axis");
figure.set_ylabel("Y Axis");

// Style configuration (all use aggregate initialization)
figure.grid({.show = true, .show_minor = true});
figure.axis({.show = true, .tick_size = 5.0});
figure.legend({.show = true, .position = "top-right"});
figure.layout({.margin_left = 80.0, .background = Sepia::Color::white()});
figure.text({.color = Sepia::Color::black(), .font_size = 12.0});
figure.perf({.lod_enable = true, .lod_target_points = 2000});


// Render and export
figure.render();
figure.save_ppm("output.ppm");
```

### Plotting Data

Three ways to add data to a figure:

```cpp
// 1. Owning (copies data into aligned storage)
figure.plot(x_ptr, y_ptr, count)
  .data({.color = Sepia::Color::red(), .width = 2.0, .label = "my curve"});

// 2. Owning (move a pre-built Series)
figure.plot(sepia::data::Series(std::move(x_buf), std::move(y_buf)))
  .data({.color = Sepia::Color::blue()});

// 3. Non-owning (zero-copy reference to your memory)
figure.plot_ref(x_ptr, y_ptr, count)
  .data({.color = Sepia::Color::green()});
```

### PlotCommand (Fluent Builder)

`figure.plot(...)` returns a `PlotCommand` that supports chaining:

```cpp
figure.plot(x, y, n)
  .color(Sepia::Color::orange())
  .width(2.5)
  .alpha(0.8)
  .label("signal")
  .line(Sepia::LineStyle::Dashed)
  .marker(Sepia::MarkerStyle::Circle, 3.0)
  .fill(true, Sepia::Color::orange().with_alpha(50));
```

Or pass everything at once via `DataStyle`:

```cpp
figure.plot(x, y, n)
  .data({
    .color      = Sepia::Color::red(),
    .width      = 1.5,
    .alpha      = 0.9,
    .line_style = Sepia::LineStyle::Solid,
    .marker     = Sepia::MarkerStyle::Circle,
.marker_size = 4.0,
    .fill       = false,
    .label      = "my data"
  });
```

### Style Structs

| Struct | Key fields |
|--------|-----------|
| `DataStyle` | `color`, `width`, `alpha`, `line_style`, `marker`, `marker_size`, `fill`, `fill_color`, `label` |
| `GridStyle` | `show`, `major_color`, `minor_color`, `major_width`, `minor_width`, `show_minor` |
| `AxisStyle` | `show`, `color`, `width`, `tick_size`, `x_min/x_max/y_min/y_max` (-1=no default) |
| `LegendStyle` | `show`, `bg_color`, `border`, `padding`, `position` |
| `LayoutStyle` | `margin_top/bottom/left/right`, `background` |
| `TextStyle` | `color`, `font_size`, `font_face` |
| `PerfParams` | `lod_enable`, `lod_target_points` |

### Colors

Built-in presets:

```cpp
Sepia::Color::black()    Sepia::Color::white()
Sepia::Color::red()      Sepia::Color::blue()
Sepia::Color::green()    Sepia::Color::orange()
Sepia::Color::purple()   Sepia::Color::gray()

// Custom
Sepia::Color(r, g, b);             // opaque
Sepia::Color(r, g, b, a);          // with alpha
color.with_alpha(128);             // semi-transparent variant
```

### Line Styles & Markers

```
LineStyle:   Solid, Dashed, Dotted, DashDot, None
MarkerStyle: None, Circle, Square, Triangle, Cross, Diamond
```

## Data Ownership

Sepia offers three ownership models depending on your performance and lifetime needs:

| Method | Ownership | Copy? | When to use |
|--------|-----------|-------|-------------|
| `figure.plot(ptr, ptr, n)` | **Figure owns** | Yes,  data is copied into cache-aligned `AlignedBuffer` | Default. Safe after your arrays go out of scope. |
| `figure.plot(Series&&)` | **Figure owns** | No,  moved in | You already have a `Series` or `AlignedBuffer`. Zero-copy transfer. |
| `figure.plot_ref(ptr, ptr, n)` | **You own** | No, zero-copy view | Performance-critical. **You must keep your arrays alive** until after `render()`. |

### AlignedBuffer

`Sepia::AlignedBuffer<T>` is a cache-line aligned (64-byte) contiguous buffer. Used internally for all owned data. SIMD-friendly.

```cpp
Sepia::AlignedBuffer<sepia::f64> x(N), y(N);
// Fill x, y...
auto series = Sepia::data::Series(std::move(x), std::move(y));
figure.plot(std::move(series));
```

### LTTB Decimation (Level-of-Detail)

When `enable_lod` is `true` (the default) automatically downsampled to `lod_target_points` using the **Largest-Triangle-Three-Buckets** algorithm before rendering. This preserves visual shape while keeping render times constant regardless of input size.

```cpp
// Defaults (in PerfParams):
//   lod_enable        = true
//   lod_target_points = 2000

// Custom tuning
figure.perf({.lod_enable = true, .lod_target_points = 5000});

// Disable decimation entirely
figure.perf({.enable_lod = false});
```

The decimation is lazy, it happens during `render()` and does not modify your original data.

## Output Format

Sepia outputs **PPM (Portable Pixmap)** files. To convert to PNG:


```bash
# Using ImageMagick
convert output.ppm output.png

# Using ffmpeg
ffmpeg -i output.ppm output.png
```

## Requirements

- C++20 compiler (GCC 10+, Clang 10+, MSVC 2019 16.8+)
- No external libraries

## Stress Test Results
```
[cmd]: g++ -std=c++20 -O3 -mavx2 -mfma -ffast-math -fno-trapping-math -fno-math-errno -march=native stress/stresstest.cpp -o ./bin/stress
```

```
[cmd]: time ./a.out

Dataset Size     With LTTB[2k] (ms)  Without LTTB (ms)
------------     ------------------  -----------------
100                         0.13             0.14
500                         0.18             0.18
1000                        0.23             0.22
5000                        0.26             0.31
10000                       0.26             0.41
50000                       0.30             1.45
100000                      0.34             2.80
500000                      0.65            13.56
1000000                     1.03            27.01
5000000                     4.66           129.34
10000000                    8.47           235.06
50000000                   40.65          1176.12
100000000                  81.09          2347.70
500000000                 405.73         11741.75
1000000000                822.99         23520.82

Results plotted to stresstest_results.ppm

--- Summary ---
At 1B points:
  With LTTB[2k]:  822.99 ms
  Without LTTB:   23520.82 ms
  Speedup:        28.6x
./bin/stress  169,42s user 1,76s system 99% cpu 2:51,36 total
```

![Image](stresstest_results.png)

> *Log Log scale*
