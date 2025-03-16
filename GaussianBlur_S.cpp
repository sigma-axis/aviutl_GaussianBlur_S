/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>
#include <vector>
#include <numeric>
#include <cmath>
#include <numbers>
#include <bit>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>
#include "multi_thread.hpp"


////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
constinit struct ExEdit092 {
	void init(AviUtl::FilterPlugin* efp) {
		if (!is_initialized()) init_pointers(efp);
	}
	constexpr bool is_initialized() const { return fp != nullptr; }
	AviUtl::FilterPlugin* fp = nullptr;

	void** memory_ptr;	// 0x1a5328 // 少なくとも最大画像サイズと同サイズは保証されるっぽい．

	int32_t	yca_max_w;	// 0x196748
	int32_t	yca_max_h;	// 0x1920e0

private:
	void init_pointers(AviUtl::FilterPlugin* efp)
	{
		fp = efp;

		auto pick_addr = [exedit_base = reinterpret_cast<intptr_t>(efp->dll_hinst), this]
			<class T>(T & target, ptrdiff_t offset) { target = reinterpret_cast<T>(exedit_base + offset); };
		auto pick_val = [exedit_base = reinterpret_cast<int32_t>(efp->dll_hinst), this]
			<class T>(T & target, ptrdiff_t offset) { target = *reinterpret_cast<T*>(exedit_base + offset); };

		pick_addr(memory_ptr,	0x1a5328);

		pick_val(yca_max_w,		0x196748);
		pick_val(yca_max_h,		0x1920e0);

		// make ready the multi-thread function feature.
		constexpr ptrdiff_t ofs_num_threads_address = 0x086384;
		auto aviutl_base = reinterpret_cast<uintptr_t>(fp->hinst_parent);
		multi_thread.init(fp->exfunc->exec_multi_thread_func,
			reinterpret_cast<int32_t*>(aviutl_base + ofs_num_threads_address));
	}
} exedit{};


////////////////////////////////
// 仕様書．
////////////////////////////////
#define PLUGIN_VERSION	"v1.00"
#define PLUGIN_AUTHOR	"sigma-axis"
#define FILTER_INFO_FMT(name, ver, author)	(name##" "##ver##" by "##author)
#define FILTER_INFO(name)	static constexpr char filter_name[] = name, info[] = FILTER_INFO_FMT(name, PLUGIN_VERSION, PLUGIN_AUTHOR)
FILTER_INFO("ガウスぼかしσ");

// trackbars.
constexpr const char* track_names[]
	= { "標準偏差", "範囲倍率", "縦横比", "輝度γ補正"};
constexpr int32_t
	track_den[]			= {   100,   10,    100,   100 },
	track_min[]			= {     0,    0, -10000,  1250 },
	track_min_drag[]	= {     0, 2500, -10000,  5000 },
	track_default[]		= {   200, 3000,      0, 10000 },
	track_max_drag[]	= { 10000, 3500,  10000, 20000 },
	track_max[]			= { 50000, 4000,  10000, 80000 };
namespace idx_track
{
	enum id : int {
		sigma,
		range,
		aspect,
		gamma,
	};
	constexpr int count_entries = std::size(track_names);
};
static_assert(
	std::size(track_names) == std::size(track_den) &&
	std::size(track_names) == std::size(track_min) &&
	std::size(track_names) == std::size(track_min_drag) &&
	std::size(track_names) == std::size(track_default) &&
	std::size(track_names) == std::size(track_max_drag) &&
	std::size(track_names) == std::size(track_max));

// checks.
struct check_data {
	enum def_value : int32_t {
		unchecked = 0,
		checked = 1,
		button = -1,
		dropdown = -2,
	};
};
constexpr const char* check_names[] = { "サイズ固定" };
constexpr int32_t check_default[] = { check_data::unchecked };
namespace idx_check
{
	enum id : int {
		fixed_size,
	};
	constexpr int count_entries = std::size(check_names);
};
static_assert(std::size(check_names) == std::size(check_default));


// callbacks.
template<bool is_effect>
static BOOL func_proc(ExEdit::Filter* efp, ExEdit::FilterProcInfo* efpip);

consteval ExEdit::Filter filter_info(bool is_effect, bool do_init = false) {
	using Flag = ExEdit::Filter::Flag;
	return {
		.flag			= is_effect ? Flag::Effect | Flag{ 1 << 6 } : Flag{},
		.name			= const_cast<char*>(filter_name),
		.track_n		= std::size(track_names),
		.track_name		= const_cast<char**>(track_names),
		.track_default	= const_cast<int*>(track_default),
		.track_s		= const_cast<int*>(track_min),
		.track_e		= const_cast<int*>(track_max),
		.check_n		= is_effect ? static_cast<int>(std::size(check_names)) : 0,
		.check_name		= is_effect ? const_cast<char**>(check_names) : nullptr,
		.check_default	= is_effect ? const_cast<int*>(check_default) : nullptr,
		.func_proc		= is_effect ? &func_proc<true> : &func_proc<false>,
		.func_init		= do_init ? [](ExEdit::Filter* efp) { exedit.init(efp->exedit_fp); return TRUE; } : nullptr,
		.information	= const_cast<char*>(info),
		.track_scale	= const_cast<int*>(track_den),
		.track_drag_min	= const_cast<int*>(track_min_drag),
		.track_drag_max	= const_cast<int*>(track_max_drag),
	};
}
inline constinit auto
	filter = filter_info(false),
	effect = filter_info(true, true);


////////////////////////////////
// 分布関数の計算．
////////////////////////////////
template<bool calc_sum, class ScalarT>
static inline ScalarT const* calc_normal_dist(float sigma, int ker_size, ScalarT* buffer)
{
	constexpr auto round = [](float val) -> ScalarT {
		if constexpr (std::integral<ScalarT>)
			return std::lroundf(val);
		else return val;
	};

	namespace num = std::numbers;
	constexpr int denom = 1 << 16;
	float const a = static_cast<float>(denom * num::inv_sqrtpi / num::sqrt2) / sigma,
		s = 0.5f / sigma / sigma;
	int const center = (ker_size - 1) >> 1;

	ScalarT sum = 0;
	for (int i = 0; i < center; i++) {
		int const x = i - center;
		buffer[i] = buffer[ker_size - 1 - i] = round(a * std::exp(-s * x * x));
		sum += buffer[i];
	}
	// `a` can be extremely large that can eventually overflow.
	buffer[center] = round(std::min(a, static_cast<float>(std::max<ScalarT>(denom - 2 * sum, 1))));
	sum = buffer[center] + 2 * sum;

	if constexpr (calc_sum) {
		auto buf_sum = buffer + ker_size;
		buf_sum[0] = 0;
		for (int i = 1; i <= center; i++) {
			buf_sum[i] = buf_sum[i - 1] + buffer[i - 1];
			buf_sum[ker_size - i] = sum - buf_sum[i];
		}
	}
	return buffer;
}


////////////////////////////////
// ピクセル操作関数．
////////////////////////////////
constexpr static int max_alpha_bits = 12;
constexpr static int16_t max_alpha = 1 << max_alpha_bits;
constexpr static float max_luma_f = max_alpha;

// 輝度の尺度を変えたピクセル形式．
#pragma pack(push, 2)
struct PixelYC_fbb { // "float-byte-byte".
	float y;
	int8_t cb, cr;
};
#pragma pack(pop)
static_assert(sizeof(PixelYC_fbb) == sizeof(ExEdit::PixelYC));
struct PixelYCA_fbbs { // "float-byte-byte-short".
	float y;
	int8_t cb, cr;
	int16_t a;

	constexpr PixelYCA_fbbs& operator=(PixelYC_fbb const& rhs) {
		y  = rhs.y;
		cb = rhs.cb;
		cr = rhs.cr;
		a  = max_alpha;
		return *this;
	}
};

template<class PixelT>
concept pixel_type = std::same_as<PixelT, ExEdit::PixelYCA> || std::same_as<PixelT, ExEdit::PixelYC>
	|| std::same_as<PixelT, PixelYCA_fbbs> || std::same_as<PixelT, PixelYC_fbb>;

// 各種ピクセル形式の平均計算．アルファ値は事前に各色チャンネルに乗算されているものとする．
template<pixel_type SrcT, pixel_type DstT = SrcT> struct pixel_sum {};
namespace round_div
{
	static constexpr int s(int a, int b) {
		if (b <= 0) std::unreachable();
		return (a + ((a < 0 ? -b : b) >> 1)) / b;
	}
	static constexpr int u(int a, int b) {
		if (a < 0 || b <= 0) std::unreachable();
		return (a + (b >> 1)) / b;
	}
}
template<> struct pixel_sum<ExEdit::PixelYCA> {
	using src_pixel = ExEdit::PixelYCA;
	using dst_pixel = src_pixel;
	using weight_type = int;

	weight_type wt, sum_y, sum_cb, sum_cr, sum_a;
	void reset() { sum_y = sum_cb = sum_cr = sum_a = wt = 0; }
	void add(weight_type weight, src_pixel const& px) {
		wt += weight;
		if (px.a <= 0) return;

		sum_y  += weight * px.y;
		sum_cb += weight * px.cb;
		sum_cr += weight * px.cr;
		sum_a  += weight * px.a;
	}
	void add(weight_type weight) { wt += weight; }
	dst_pixel mean() const
	{
		if (sum_a <= 0) return { .a = 0 };
		return {
			.y  = static_cast<int16_t>(round_div::u(sum_y,  wt)),
			.cb = static_cast<int16_t>(round_div::s(sum_cb, wt)),
			.cr = static_cast<int16_t>(round_div::s(sum_cr, wt)),
			.a  = static_cast<int16_t>(round_div::u(sum_a,  wt)),
		};
	}
};
template<> struct pixel_sum<ExEdit::PixelYC, ExEdit::PixelYCA> {
	using src_pixel = ExEdit::PixelYC;
	using dst_pixel = ExEdit::PixelYCA;
	using weight_type = int;

	weight_type wt, sum_y, sum_cb, sum_cr, sum_a;
	void reset() { sum_y = sum_cb = sum_cr = sum_a = wt = 0; }
	void add(weight_type weight, src_pixel const& px) {
		wt += weight;

		sum_y  += weight * px.y;
		sum_cb += weight * px.cb;
		sum_cr += weight * px.cr;
		sum_a  += weight;
	}
	void add(weight_type weight) { wt += weight; }
	dst_pixel mean() const
	{
		if (sum_a <= 0) return { .a = 0 };
		return {
			.y  = static_cast<int16_t>(round_div::u(sum_y,  wt)),
			.cb = static_cast<int16_t>(round_div::s(sum_cb, wt)),
			.cr = static_cast<int16_t>(round_div::s(sum_cr, wt)),
			.a  = static_cast<int16_t>(round_div::u(sum_a << max_alpha_bits, wt)),
		};
	}
};
template<> struct pixel_sum<ExEdit::PixelYC> {
	using src_pixel = ExEdit::PixelYC;
	using dst_pixel = src_pixel;
	using weight_type = int;

	weight_type wt, sum_y, sum_cb, sum_cr;
	void reset() { sum_y = sum_cb = sum_cr = wt = 0; }
	void add(weight_type weight, src_pixel const& px) {
		wt += weight;
		sum_y += weight * px.y;
		sum_cb += weight * px.cb;
		sum_cr += weight * px.cr;
	}
	dst_pixel mean() const
	{
		return {
			.y  = static_cast<int16_t>(round_div::u(sum_y, wt)),
			.cb = static_cast<int16_t>(round_div::s(sum_cb, wt)),
			.cr = static_cast<int16_t>(round_div::s(sum_cr, wt)),
		};
	}
};
template<> struct pixel_sum<PixelYCA_fbbs> {
	using src_pixel = PixelYCA_fbbs;
	using dst_pixel = src_pixel;
	using weight_type = float;

	weight_type wt, sum_y, sum_cb, sum_cr, sum_a;
	void reset() { sum_y = sum_cb = sum_cr = sum_a = wt = 0; }
	void add(weight_type weight, src_pixel const& px) {
		wt += weight;
		if (px.a <= 0) return;

		sum_y  += weight * px.y;
		sum_cb += weight * px.cb;
		sum_cr += weight * px.cr;
		sum_a  += weight * px.a;
	}
	void add(weight_type weight) { wt += weight; }
	dst_pixel mean() const
	{
		if (sum_a <= 0) return { .a = 0 };
		return {
			.y  = sum_y / wt,
			.cb = static_cast<int8_t>(std::lroundf(sum_cb / wt)),
			.cr = static_cast<int8_t>(std::lroundf(sum_cr / wt)),
			.a  = static_cast<int16_t>(std::lroundf(sum_a / wt)),
		};
	}
};
template<> struct pixel_sum<PixelYC_fbb, PixelYCA_fbbs> {
	using src_pixel = PixelYC_fbb;
	using dst_pixel = PixelYCA_fbbs;
	using weight_type = float;

	weight_type wt, sum_y, sum_cb, sum_cr, sum_a;
	void reset() { sum_y = sum_cb = sum_cr = sum_a = wt = 0; }
	void add(weight_type weight, src_pixel const& px) {
		wt += weight;
		sum_y  += weight * px.y;
		sum_cb += weight * px.cb;
		sum_cr += weight * px.cr;
		sum_a  += weight;
	}
	void add(weight_type weight) { wt += weight; }
	dst_pixel mean() const
	{
		if (sum_a <= 0) return { .a = 0 };
		return {
			.y  = sum_y / wt,
			.cb = static_cast<int8_t>(std::lroundf(sum_cb / wt)),
			.cr = static_cast<int8_t>(std::lroundf(sum_cr / wt)),
			.a  = static_cast<int16_t>(std::lroundf(sum_a * max_alpha / wt)),
		};
	}
};
template<> struct pixel_sum<PixelYC_fbb> {
	using src_pixel = PixelYC_fbb;
	using dst_pixel = src_pixel;
	using weight_type = float;

	weight_type wt, sum_y, sum_cb, sum_cr;
	void reset() { sum_y = sum_cb = sum_cr = wt = 0; }
	void add(weight_type weight, src_pixel const& px) {
		wt += weight;
		sum_y  += weight * px.y;
		sum_cb += weight * px.cb;
		sum_cr += weight * px.cr;
	}
	dst_pixel mean() const
	{
		return {
			.y  = sum_y / wt,
			.cb = static_cast<int8_t>(std::lroundf(sum_cb / wt)),
			.cr = static_cast<int8_t>(std::lroundf(sum_cr / wt)),
		};
	}
};

// 平均計算の事前処理 / 事後処理．
struct preproc {
	static void apply(ExEdit::PixelYCA& px) {
		// アルファ値荷重の付与．
		if (px.a <= 0) { px = { .a = 0 }; return; }
		if (px.a >= max_alpha) return;

		px.y  = static_cast<int16_t>((px.y  * px.a + max_alpha / 2) >> max_alpha_bits);
		px.cb = static_cast<int16_t>((px.cb * px.a + max_alpha / 2) >> max_alpha_bits);
		px.cr = static_cast<int16_t>((px.cr * px.a + max_alpha / 2) >> max_alpha_bits);
	}
	static void apply(ExEdit::PixelYCA& px, float gamma) {
		// アルファ値荷重の付与 + 輝度のガンマ補正．
		ExEdit::PixelYCA src = px;
		auto& dst = reinterpret_cast<PixelYCA_fbbs&>(px);
		if (src.a <= 0) { dst = { .a = 0 }; return; }

		dst.y  = std::powf(src.y / max_luma_f, gamma) * src.a / max_alpha;
		dst.cb = static_cast<int8_t>(std::clamp((src.cb * src.a + (max_alpha << 3)) >> (max_alpha_bits + 4), -128, 127));
		dst.cr = static_cast<int8_t>(std::clamp((src.cr * src.a + (max_alpha << 3)) >> (max_alpha_bits + 4), -128, 127));
		//dst.a  = src.a;
	}
	static void apply(ExEdit::PixelYC& px, float gamma) {
		// 輝度のガンマ補正．
		ExEdit::PixelYC src = px;
		auto& dst = reinterpret_cast<PixelYC_fbb&>(px);

		dst.y = std::powf(src.y / max_luma_f, gamma);
		dst.cb = static_cast<int8_t>(std::clamp((src.cb + (1 << 3)) >> 4, -128, 127));
		dst.cr = static_cast<int8_t>(std::clamp((src.cr + (1 << 3)) >> 4, -128, 127));
	}
};
struct postproc {
	static void apply(ExEdit::PixelYCA& px) {
		// アルファ値荷重の除去．
		if (px.a <= 0) return;
		if (px.a >= max_alpha) return;

		px.y  = static_cast<int16_t>(round_div::u(px.y  << max_alpha_bits, px.a));
		px.cb = static_cast<int16_t>(round_div::s(px.cb << max_alpha_bits, px.a));
		px.cr = static_cast<int16_t>(round_div::s(px.cr << max_alpha_bits, px.a));
	}
	static void apply(PixelYCA_fbbs& px, float inv_gamma) {
		// アルファ値荷重の除去 + 輝度の逆ガンマ補正．
		PixelYCA_fbbs src = px;
		auto& dst = reinterpret_cast<ExEdit::PixelYCA&>(px);
		if (src.a <= 0) { dst = { .a = 0 }; return; }

		dst.y  = static_cast<int16_t>(std::lroundf(max_luma_f * std::powf(src.y / src.a * max_alpha, inv_gamma)));
		dst.cb = static_cast<int16_t>(round_div::s(src.cb << (max_alpha_bits + 4), src.a));
		dst.cr = static_cast<int16_t>(round_div::s(src.cr << (max_alpha_bits + 4), src.a));
		//dst.a  = src.a;
	}
	static void apply(PixelYC_fbb& px, float inv_gamma) {
		// 輝度の逆ガンマ補正．
		PixelYC_fbb src = px;
		auto& dst = reinterpret_cast<ExEdit::PixelYC&>(px);

		dst.y  = static_cast<int16_t>(std::lroundf(max_luma_f * std::powf(src.y, inv_gamma)));
		dst.cb = static_cast<int16_t>(src.cb << 4);
		dst.cr = static_cast<int16_t>(src.cr << 4);
	}
};

template<class proc>
static inline void process_pixels(int w, int h, pixel_type auto* buffer, int stride, auto... params)
{
	multi_thread(h, [&](int thread_id, int thread_num)
	{
		auto p = &buffer[(h * thread_id / thread_num) * stride];
		auto const p1 = &buffer[(h * (thread_id + 1) / thread_num) * stride];
		for (; p < p1; p += stride) {
			auto p_y = p; auto const p_y1 = p_y + w;
			for (; p_y < p_y1; p_y++) proc::apply(*p_y, params...);
		}
	});
}


////////////////////////////////
// 畳み込み積計算．
////////////////////////////////

// swap x and y axis.
static inline void flip_xy(int w, int h,
	pixel_type auto const* src, int src_stride, pixel_type auto* dst, int dst_stride)
{
	multi_thread(h, [&](int thread_id, int thread_num)
	{
		int y0 = h * thread_id / thread_num, y1 = h * (thread_id + 1) / thread_num;
		auto s_y = &src[y0 * src_stride];
		auto d_x = &dst[y0];
		for (; y0 < y1; y0++, s_y += src_stride, d_x++) {
			auto s = s_y; auto d = d_x;
			for (int x = 0; x < w; x++, s++, d += dst_stride) *d = *s;
		}
	});
}

// 元ピクセルを横方向に走査して読み取り，縦方向に記録．
// 2 回繰り返せば縦横に畳み込みができ，関数 1 つのみで完結する．
template<bool fixed_size>
static inline void calc_conv(float sigma, int ker_size, int w, int h,
	pixel_type auto const* src, int src_stride, pixel_type auto* dst, int dst_stride,
	bool reuse_kernel = false)
{
	using src_pixel = std::decay_t<decltype(*src)>;
	using dst_pixel = std::decay_t<decltype(*dst)>;
	using pixel_sum = pixel_sum<src_pixel, dst_pixel>;
	using scalar = pixel_sum::weight_type;

	if (ker_size <= 1) {
		// nothing to do but to flip the axis.
		flip_xy(w, h, src, src_stride, dst, dst_stride);
		return;
	}

	// ker_size is always odd, and hereafter > 1.
	auto const kernel = reuse_kernel ? reinterpret_cast<scalar const*>(*exedit.memory_ptr) :
		calc_normal_dist<!fixed_size>(sigma, ker_size, reinterpret_cast<scalar*>(*exedit.memory_ptr));
	auto const ker_sum = kernel + ker_size;
	int const side = (ker_size - 1) >> 1;

	int const st = fixed_size ? -side : -2 * side, ed = fixed_size ? w - side : w, mid = w - ker_size;
	int const ed0 = fixed_size ? std::min(ed, 0) : 0, mid1 = std::min(mid, 0);
	multi_thread(h, [&](int thread_id, int thread_num)
	{
		int const y0 = h * thread_id / thread_num, y1 = h * (thread_id + 1) / thread_num;
		auto s_y = &src[y0 * src_stride]; auto const s1 = &src[y1 * src_stride];
		auto d_y = &dst[y0];

		// perform convolution.
		pixel_sum sum;
		for (; s_y < s1; s_y += src_stride, d_y++) {
			int x = st;
			auto s = s_y + x; auto d = d_y;
			for (; x < mid1; x++, s++, d += dst_stride) {
				sum.reset();
				if constexpr (!fixed_size) sum.add(ker_sum[-x]);
				for (int i = -x; i < ker_size; i++)
					sum.add(kernel[i], s[i]);
				*d = sum.mean();
			}
			for (; x < ed0; x++, s++, d += dst_stride) {
				sum.reset();
				if constexpr (!fixed_size) sum.add(ker_sum[-x]);
				for (int i = -x; i < w - x; i++)
					sum.add(kernel[i], s[i]);
				if constexpr (!fixed_size) sum.add(ker_sum[ker_size - (w - x)]);
				*d = sum.mean();
			}
			for (; x < mid; x++, s++, d += dst_stride) {
				sum.reset();
				for (int i = 0; i < ker_size; i++)
					sum.add(kernel[i], s[i]);
				*d = sum.mean();
			}
			for (; x < ed; x++, s++, d += dst_stride) {
				sum.reset();
				for (int i = 0; i < w - x; i++)
					sum.add(kernel[i], s[i]);
				if constexpr (!fixed_size) sum.add(ker_sum[ker_size - (w - x)]);
				*d = sum.mean();
			}
		}
	});
}


////////////////////////////////
// フィルタ処理．
////////////////////////////////
template<bool is_effect>
BOOL func_proc(ExEdit::Filter* efp, ExEdit::FilterProcInfo* efpip)
{
	int const
		src_w = is_effect ? efpip->obj_w : efpip->scene_w,
		src_h = is_effect ? efpip->obj_h : efpip->scene_h;
	if (src_w <= 0 || src_h <= 0) return TRUE;

	constexpr int
		den_sigma	= track_den[idx_track::sigma],
		min_sigma	= track_min[idx_track::sigma],
		max_sigma	= track_max[idx_track::sigma],
		den_range	= track_den[idx_track::range],
		min_range	= track_min[idx_track::range],
		max_range	= track_max[idx_track::range],
		den_aspect	= track_den[idx_track::aspect],
		min_aspect	= track_min[idx_track::aspect],
		max_aspect	= track_max[idx_track::aspect],
		den_gamma	= track_den[idx_track::gamma],
		min_gamma	= track_min[idx_track::gamma],
		max_gamma	= track_max[idx_track::gamma],
		def_gamma	= track_default[idx_track::gamma];

	// pick values of trackbars and checkboxes.
	int const
		sigma_raw	= std::clamp(efp->track[idx_track::sigma],	min_sigma,	max_sigma),
		aspect_raw	= std::clamp(efp->track[idx_track::aspect],	min_aspect,	max_aspect),
		range_raw	= std::clamp(efp->track[idx_track::range],	min_range,	max_range),
		gamma_raw	= std::clamp(efp->track[idx_track::gamma],	min_gamma,	max_gamma);
	float const sigma = sigma_raw / static_cast<float>(den_sigma);
	bool const fixed_size = !is_effect || (efp->check[idx_check::fixed_size] != check_data::unchecked);

	// convert the picked values for further use.
	float const
		sigma_x = sigma * (max_aspect - std::max(aspect_raw, 0)) / max_aspect,
		sigma_y = sigma * (max_aspect + std::min(aspect_raw, 0)) / max_aspect;
	constexpr auto den_ker_size = max_aspect * den_sigma * 100 * den_range;
	int ker_size_x = static_cast<int>(
			((max_aspect - std::max(aspect_raw, 0)) * static_cast<int64_t>(sigma_raw * range_raw)
				+ (den_ker_size - 1)) / den_ker_size),
		ker_size_y = aspect_raw == 0 ? ker_size_x : static_cast<int>(
			((max_aspect + std::min(aspect_raw, 0)) * static_cast<int64_t>(sigma_raw * range_raw)
				+ (den_ker_size - 1)) / den_ker_size);

	// capping by the maximum size.
	if (!fixed_size) {
		// note that the result is always of YCA format in this case.
		ker_size_x = std::min(ker_size_x, (exedit.yca_max_w - src_w) >> 1);
		ker_size_y = std::min(ker_size_y, (exedit.yca_max_h - src_h) >> 1);

		efpip->obj_w += 2 * ker_size_x;
		efpip->obj_h += 2 * ker_size_y;
	}
	if (ker_size_x <= 0 && ker_size_y <= 0) return TRUE; // quit on trivial cases.
	ker_size_x = 1 + 2 * ker_size_x;
	ker_size_y = 1 + 2 * ker_size_y;

	// perform blur operations.
	// split into cases by pixel formats.
	bool const reuse_kernel = sigma_x == sigma_y && ker_size_x == ker_size_y;
	if (gamma_raw == def_gamma) {
		// ガンマ補正なしのパターン．
		if (!is_effect || efpip->no_alpha != 0) {
			if (fixed_size) {
				// ExEdit::PixelYC のまま計算．
				auto const
					src = reinterpret_cast<ExEdit::PixelYC*>(is_effect ? efpip->obj_edit : efpip->frame_edit),
					tmp = reinterpret_cast<ExEdit::PixelYC*>(is_effect ? efpip->obj_temp : efpip->frame_temp);

				calc_conv<true>(sigma_x, ker_size_x, src_w, src_h,
					src, efpip->scene_line, tmp, (src_h + 1) & (-2));
				calc_conv<true>(sigma_y, ker_size_y, src_h, src_w,
					tmp, (src_h + 1) & (-2), src, efpip->scene_line, reuse_kernel);
			}
			else {
				// ExEdit::PixelYC -> ExEdit::PixelYCA.
				calc_conv<false>(sigma_x, ker_size_x, src_w, src_h,
					reinterpret_cast<ExEdit::PixelYC*>(efpip->obj_edit), efpip->scene_line,
					efpip->obj_temp, src_h);
				calc_conv<false>(sigma_y, ker_size_y, src_h, efpip->obj_w,
					efpip->obj_temp, src_h, efpip->obj_edit, efpip->obj_line, reuse_kernel);
				process_pixels<postproc>(efpip->obj_w, efpip->obj_h, efpip->obj_edit, efpip->obj_line);

				efpip->no_alpha = 0;
			}
		}
		else {
			// ExEdit::PixelYCA のまま計算．
			process_pixels<preproc>(src_w, src_h, efpip->obj_edit, efpip->obj_line);
			if (fixed_size) {
				calc_conv<true>(sigma_x, ker_size_x, src_w, src_h,
					efpip->obj_edit, efpip->obj_line, efpip->obj_temp, src_h);
				calc_conv<true>(sigma_y, ker_size_y, src_h, src_w,
					efpip->obj_temp, src_h, efpip->obj_edit, efpip->obj_line, reuse_kernel);
			}
			else {
				calc_conv<false>(sigma_x, ker_size_x, src_w, src_h,
					efpip->obj_edit, efpip->obj_line, efpip->obj_temp, src_h);
				calc_conv<false>(sigma_y, ker_size_y, src_h, efpip->obj_w,
					efpip->obj_temp, src_h, efpip->obj_edit, efpip->obj_line, reuse_kernel);
			}
			process_pixels<postproc>(efpip->obj_w, efpip->obj_h, efpip->obj_edit, efpip->obj_line);
		}
	}
	else {
		// ガンマ補正ありのパターン．
		float gamma = gamma_raw / (100.0f * den_gamma);
		if (!is_effect || efpip->no_alpha != 0) {
			if (fixed_size) {
				// ExEdit::PixelYC -> PixelYC_fbb と変換，処理後に ExEdit::PixelYC に戻す．
				auto const
					med = reinterpret_cast<PixelYC_fbb*>(is_effect ? efpip->obj_edit : efpip->frame_edit),
					tmp = reinterpret_cast<PixelYC_fbb*>(is_effect ? efpip->obj_temp : efpip->frame_temp);

				process_pixels<preproc>(src_w, src_h, reinterpret_cast<ExEdit::PixelYC*>(med), efpip->scene_line, gamma);
				calc_conv<true>(sigma_x, ker_size_x, src_w, src_h,
					med, efpip->scene_line, tmp, (src_h + 1) & (-2));
				calc_conv<true>(sigma_y, ker_size_y, src_h, src_w,
					tmp, (src_h + 1) & (-2), med, efpip->scene_line, reuse_kernel);
				process_pixels<postproc>(src_w, src_h, med, efpip->scene_line, 1 / gamma);
			}
			else {
				// ExEdit::PixelYC -> PixelYC_fbb と変換，
				// 途中から PixelYCA_fbbs で最後に ExEdit::PixelYCA に戻す．
				auto const
					med = reinterpret_cast<PixelYCA_fbbs*>(efpip->obj_edit),
					tmp = reinterpret_cast<PixelYCA_fbbs*>(efpip->obj_temp);
				process_pixels<preproc>(src_w, src_h, reinterpret_cast<ExEdit::PixelYC*>(efpip->obj_edit), efpip->scene_line, gamma);
				calc_conv<false>(sigma_x, ker_size_x, src_w, src_h,
					reinterpret_cast<PixelYC_fbb*>(efpip->obj_edit), efpip->scene_line, tmp, src_h);
				calc_conv<false>(sigma_y, ker_size_y, src_h, efpip->obj_w,
					tmp, src_h, med, efpip->obj_line, reuse_kernel);
				process_pixels<postproc>(efpip->obj_w, efpip->obj_h, med, efpip->obj_line, 1 / gamma);

				efpip->no_alpha = 0;
			}
		}
		else {
			// ExEdit::PixelYCA -> PixelYC_fbbs と変換，処理後に ExEdit::PixelYCA に戻す．
			process_pixels<preproc>(src_w, src_h, efpip->obj_edit, efpip->obj_line, gamma);
			auto const
				med = reinterpret_cast<PixelYCA_fbbs*>(efpip->obj_edit),
				tmp = reinterpret_cast<PixelYCA_fbbs*>(efpip->obj_temp);
			if (fixed_size) {
				calc_conv<true>(sigma_x, ker_size_x, src_w, src_h,
					med, efpip->obj_line, tmp, src_h);
				calc_conv<true>(sigma_y, ker_size_y, src_h, src_w,
					tmp, src_h, med, efpip->obj_line, reuse_kernel);
			}
			else {
				calc_conv<false>(sigma_x, ker_size_x, src_w, src_h,
					med, efpip->obj_line, tmp, src_h);
				calc_conv<false>(sigma_y, ker_size_y, src_h, efpip->obj_w,
					tmp, src_h, med, efpip->obj_line, reuse_kernel);
			}
			process_pixels<postproc>(efpip->obj_w, efpip->obj_h, med, efpip->obj_line, 1 / gamma);
		}
	}

	return TRUE;
}


////////////////////////////////
// DLL 初期化．
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// エントリポイント．
////////////////////////////////
EXTERN_C __declspec(dllexport) ExEdit::Filter* const* __stdcall GetFilterTableList() {
	constexpr static ExEdit::Filter* filter_list[] = {
		&filter,
		&effect,
		nullptr,
	};

	return filter_list;
}
