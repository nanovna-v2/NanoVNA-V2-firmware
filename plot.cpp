#include <math.h>
#include <string.h>
#include "common.hpp"
#include "globals.hpp"
#include "plot.hpp"
#include "ili9341.hpp"
#include "Font5x7.h"
#include <board.hpp>
#include <mculib/printf.hpp>

#define TRUE true
#define FALSE false
#define SWAP(x,y) do { int z=x; x = y; y = z; } while(0)
#define PULSE board::ledPulse()


static void cell_draw_marker_info(int m, int n, int w, int h);
void frequency_string(char *buf, size_t len, freqHz_t freq);
void frequency_string_short(char *buf, size_t len, freqHz_t freq, char prefix);
void markmap_all_markers(void);

//#define GRID_COLOR 0x0863
//uint16_t grid_color = 0x1084;

uint16_t redraw_request = 0;

/* indicate dirty cells */
uint16_t markmap[2][8];
uint16_t current_mappage = 0;

int32_t fgrid = 50000000;
int16_t grid_offset;
int16_t grid_width;

int area_width = AREA_WIDTH_NORMAL;
int area_height = HEIGHT;

bool plot_canceled = false;

small_function<freqHz_t(int)> plot_getFrequencyAt;
small_function<void()> plot_tick;

// alias for plot_getFrequencyAt
static inline freqHz_t freqAt(int i) {
	return plot_getFrequencyAt(i);
}

#define GRID_RECTANGULAR (1<<0)
#define GRID_SMITH       (1<<1)
#define GRID_ADMIT       (1<<2)
#define GRID_POLAR       (1<<3)


#define CELLWIDTH 32
#define CELLHEIGHT 32

/*
 * CELL_X0[27:31] cell position
 * CELL_Y0[22:26]
 * CELL_N[10:21] original order
 * CELL_X[5:9] position in the cell
 * CELL_Y[0:4]
 */
uint32_t trace_index[TRACES_MAX][SWEEP_POINTS_MAX];

#define INDEX(x, y, n) \
	((((x)&0x03e0UL)<<22) | (((y)&0x03e0UL)<<17) | (((n)&0x0fffUL)<<10)  \
 | (((x)&0x1fUL)<<5) | ((y)&0x1fUL))

#define CELL_X(i) (int)((((i)>>5)&0x1f) | (((i)>>22)&0x03e0))
#define CELL_Y(i) (int)(((i)&0x1f) | (((i)>>17)&0x03e0))
#define CELL_N(i) (int)(((i)>>10)&0xfff)

#define CELL_X0(i) (int)(((i)>>22)&0x03e0)
#define CELL_Y0(i) (int)(((i)>>17)&0x03e0)

#define CELL_P(i, x, y) (((((x)&0x03e0UL)<<22) | (((y)&0x03e0UL)<<17)) == ((i)&0xffc00000UL))


void update_grid(void)
{
	freqHz_t gdigit = 100000000;
	freqHz_t fstart, fspan;
	freqHz_t grid;
	if (frequency1 > 0) {
		fstart = frequency0;
		fspan = frequency1 - frequency0;
	} else {
		fspan = -frequency1;
		fstart = frequency0 - fspan/2;
	}

	while (gdigit > 100) {
		grid = 5 * gdigit;
		if (fspan / grid >= 4)
			break;
		grid = 2 * gdigit;
		if (fspan / grid >= 4)
			break;
		grid = gdigit;
		if (fspan / grid >= 4)
			break;
		gdigit /= 10;
	}
	fgrid = grid;

	grid_offset = (WIDTH-1) * ((fstart % fgrid) / 100) / (fspan / 100);
	grid_width = (WIDTH-1) * (fgrid / 100) / (fspan / 1000);

	force_set_markmap();
	redraw_request |= REDRAW_FREQUENCY;
}

static inline int
circle_inout(int x, int y, int r)
{
	int d = x*x + y*y - r*r;
	if (d <= -r)
		return 1;
	if (d > r)
		return -1;
	return 0;
}


#define P_CENTER_X 146
#define P_CENTER_Y 116
#define P_RADIUS 116

static int
polar_grid(int x, int y)
{
	int c = config.grid_color;
	int d;

	// offset to center
	x -= P_CENTER_X;
	y -= P_CENTER_Y;

	// outer circle
	d = circle_inout(x, y, P_RADIUS);
	if (d < 0) return 0;
	if (d == 0) return c;

	// vertical and horizontal axis
	if (x == 0 || y == 0)
		return c;

	d = circle_inout(x, y, P_RADIUS / 5);
	if (d == 0) return c;
	if (d > 0) return 0;

	d = circle_inout(x, y, P_RADIUS * 2 / 5);
	if (d == 0) return c;
	if (d > 0) return 0;

	// cross sloping lines
	if (x == y || x == -y)
		return c;

	d = circle_inout(x, y, P_RADIUS * 3 / 5);
	if (d == 0) return c;
	if (d > 0) return 0;

	d = circle_inout(x, y, P_RADIUS * 4 / 5);
	if (d == 0) return c;
	return 0;
}

/*
 * Constant Resistance circle: (u - r/(r+1))^2 + v^2 = 1/(r+1)^2
 * Constant Reactance circle:  (u - 1)^2 + (v-1/x)^2 = 1/x^2
 */
int
smith_grid(int x, int y)
{
	int c = config.grid_color;
	int d;

	// offset to center
	x -= P_CENTER_X;
	y -= P_CENTER_Y;

	// outer circle
	d = circle_inout(x, y, P_RADIUS);
	if (d < 0)
		return 0;
	if (d == 0)
		return c;

	// horizontal axis
	if (y == 0)
		return c;

	// shift circle center to right origin
	x -= P_RADIUS;

	// Constant Reactance Circle: 2j : R/2 = 58
	if (circle_inout(x, y+58, 58) == 0)
		return c;
	if (circle_inout(x, y-58, 58) == 0)
		return c;

	// Constant Resistance Circle: 3 : R/4 = 29
	d = circle_inout(x+29, y, 29);
	if (d > 0) return 0;
	if (d == 0) return c;

	// Constant Reactance Circle: 1j : R = 116
	if (circle_inout(x, y+116, 116) == 0)
		return c;
	if (circle_inout(x, y-116, 116) == 0)
		return c;

	// Constant Resistance Circle: 1 : R/2 = 58
	d = circle_inout(x+58, y, 58);
	if (d > 0) return 0;
	if (d == 0) return c;

	// Constant Reactance Circle: 1/2j : R*2 = 232
	if (circle_inout(x, y+232, 232) == 0)
		return c;
	if (circle_inout(x, y-232, 232) == 0)
		return c;

	// Constant Resistance Circle: 1/3 : R*3/4 = 87
	if (circle_inout(x+87, y, 87) == 0)
		return c;
	return 0;
}

int
smith_grid2(int x, int y, float scale)
{
	int c = config.grid_color;
	int d;

	// offset to center
	x -= P_CENTER_X;
	y -= P_CENTER_Y;

	// outer circle
	d = circle_inout(x, y, P_RADIUS);
	if (d < 0)
		return 0;
	if (d == 0)
		return c;

	// shift circle center to right origin
	x -= P_RADIUS * scale;

	// Constant Reactance Circle: 2j : R/2 = 58
	if (circle_inout(x, y+58*scale, 58*scale) == 0)
		return c;
	if (circle_inout(x, y-58*scale, 58*scale) == 0)
		return c;
#if 0
	// Constant Resistance Circle: 3 : R/4 = 29
	d = circle_inout(x+29*scale, y, 29*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
	d = circle_inout(x-29*scale, y, 29*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
#endif

	// Constant Reactance Circle: 1j : R = 116
	if (circle_inout(x, y+116*scale, 116*scale) == 0)
		return c;
	if (circle_inout(x, y-116*scale, 116*scale) == 0)
		return c;

	// Constant Resistance Circle: 1 : R/2 = 58
	d = circle_inout(x+58*scale, y, 58*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
	d = circle_inout(x-58*scale, y, 58*scale);
	if (d > 0) return 0;
	if (d == 0) return c;

	// Constant Reactance Circle: 1/2j : R*2 = 232
	if (circle_inout(x, y+232*scale, 232*scale) == 0)
		return c;
	if (circle_inout(x, y-232*scale, 232*scale) == 0)
		return c;

#if 0
	// Constant Resistance Circle: 1/3 : R*3/4 = 87
	d = circle_inout(x+87*scale, y, 87*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
	d = circle_inout(x+87*scale, y, 87*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
#endif

	// Constant Resistance Circle: 0 : R
	d = circle_inout(x+P_RADIUS*scale, y, P_RADIUS*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
	d = circle_inout(x-P_RADIUS*scale, y, P_RADIUS*scale);
	if (d > 0) return 0;
	if (d == 0) return c;

	// Constant Resistance Circle: -1/3 : R*3/2 = 174
	d = circle_inout(x+174*scale, y, 174*scale);
	if (d > 0) return 0;
	if (d == 0) return c;
	d = circle_inout(x-174*scale, y, 174*scale);
	//if (d > 0) return 0;
	if (d == 0) return c;
	return 0;
}


const int cirs[][4] = {
	{ 0, 58/2, 58/2, 0 },    // Constant Reactance Circle: 2j : R/2 = 58
	{ 29/2, 0, 29/2, 1 },    // Constant Resistance Circle: 3 : R/4 = 29
	{ 0, 116/2, 116/2, 0 },  // Constant Reactance Circle: 1j : R = 116
	{ 58/2, 0, 58/2, 1 },    // Constant Resistance Circle: 1 : R/2 = 58
	{ 0, 232/2, 232/2, 0 },  // Constant Reactance Circle: 1/2j : R*2 = 232
	{ 87/2, 0, 87/2, 1 },    // Constant Resistance Circle: 1/3 : R*3/4 = 87
	{ 0, 464/2, 464/2, 0 },  // Constant Reactance Circle: 1/4j : R*4 = 464
	{ 116/2, 0, 116/2, 1 },  // Constant Resistance Circle: 0 : R
	{ 174/2, 0, 174/2, 1 },  // Constant Resistance Circle: -1/3 : R*3/2 = 174
	{ 0, 0, 0, 0 } // sentinel
};

int
smith_grid3(int x, int y)
{
	int c = config.grid_color;
	int d;

	// offset to center
	x -= P_CENTER_X;
	y -= P_CENTER_Y;

	// outer circle
	d = circle_inout(x, y, P_RADIUS);
	if (d < 0)
		return 0;
	if (d == 0)
		return c;

	// shift circle center to right origin
	x -= P_RADIUS /2;

	int i;
	for (i = 0; cirs[i][2]; i++) {
		d = circle_inout(x+cirs[i][0], y+cirs[i][1], cirs[i][2]);
		if (d == 0)
			return c;
		if (d > 0 && cirs[i][3])
			return 0;
		d = circle_inout(x-cirs[i][0], y-cirs[i][1], cirs[i][2]);
		if (d == 0)
			return c;
		if (d > 0 && cirs[i][3])
			return 0;
	}
	return 0;
}

#if 0
int
rectangular_grid(int x, int y)
{
	int c = config.grid_color;
	//#define FREQ(x) (((x) * (fspan / 1000) / (WIDTH-1)) * 1000 + fstart)
	//int32_t n = FREQ(x-1) / fgrid;
	//int32_t m = FREQ(x) / fgrid;
	//if ((m - n) > 0)
	//if (((x * 6) % (WIDTH-1)) < 6)
	//if (((x - grid_offset) % grid_width) == 0)
	if (x == 0 || x == WIDTH-1)
		return c;
	if ((y % GRIDY) == 0)
		return c;
	if ((((x + grid_offset) * 10) % grid_width) < 10)
		return c;
	return 0;
}
#endif

int
rectangular_grid_x(int x, int bg = 0)
{
	int c = config.grid_color;
	if (x < 0)
		return bg;
	if (x == 0 || x == WIDTH)
		return c;
	if ((((x + grid_offset) * 10) % grid_width) < 10)
		return c;
	return bg;
}

int
rectangular_grid_y(int y, int bg = 0)
{
	int c = config.grid_color;
	if (y < 0)
		return bg;
	if ((y % GRIDY) == 0)
		return c;
	return bg;
}

/*
 * calculate log10(abs(gamma))
 */
float logmag(complexf v) {
	float re = v.real(), im = v.imag();
	return log10f(re*re + im*im) * 10;
}

/*
 * calculate phase[-2:2] of coefficient
 */
float phase(complexf v) {
	float re = v.real(), im = v.imag();
	return 2 * atan2f(im, re) / M_PI * 90;
}

/*
 * calculate groupdelay
 */
float groupdelay(complexf v, complexf w, float deltaf) {
	// calculate atan(w)-atan(v)
	complexf q = w / v;
	return arg(q) / (2 * M_PI * deltaf);
}

/*
 * calculate abs(gamma)
 */
float linear(complexf v) {
	float re = v.real(), im = v.imag();
	return - sqrtf(re*re + im*im);
}

/*
 * calculate vswr; (1+gamma)/(1-gamma)
 */
float swr(complexf v) {
	float re = v.real(), im = v.imag();
	float x = sqrtf(re*re + im*im);
	if (x > 1)
		return INFINITY;
	return (1 + x)/(1 - x);
}

float resistance(complexf v) {
	float re = v.real(), im = v.imag();
	float z0 = 50;
	float d = z0 / ((1-re)*(1-re)+im*im);
	float zr = ((1+re)*(1-re) - im*im) * d;
	return zr;
}

float reactance(complexf v) {
	float re = v.real(), im = v.imag();
	float z0 = 50;
	float d = z0 / ((1-re)*(1-re)+im*im);
	float zi = 2*im * d;
	return zi;
}

#define RADIUS ((HEIGHT-1)/2)
void
cartesian_scale(float re, float im, int *xp, int *yp, float scale)
{
	//float scale = 4e-3;
	int x = re * RADIUS * scale;
	int y = im * RADIUS * scale;
	if (x < -RADIUS) x = -RADIUS;
	if (y < -RADIUS) y = -RADIUS;
	if (x > RADIUS) x = RADIUS;
	if (y > RADIUS) y = RADIUS;
	*xp = WIDTH/2 + x;
	*yp = HEIGHT/2 - y;
}

float
groupdelay_from_array(int i, complexf array[SWEEP_POINTS_MAX])
{
	if (i == 0) {
		float deltaf = freqAt(1) - freqAt(0);
		return groupdelay(array[0], array[1], deltaf);
	} else if (i == (SWEEP_POINTS_MAX - 1)) {
		float deltaf = freqAt(i) - freqAt(i-1);
		return groupdelay(array[i-1], array[i], deltaf);
	} else {
		float deltaf = freqAt(i+1) - freqAt(i-1);
		return groupdelay(array[i-1], array[i+1], deltaf);
	}
}

uint32_t
trace_into_index(int x, int t, int i, complexf array[SWEEP_POINTS_MAX])
{
	int y = 0;
	float v = 0;
	float refpos = 8 - get_trace_refpos(t);
	float scale = 1 / get_trace_scale(t);
	complexf coeff = array[i];
	switch (trace[t].type) {
	case TRC_LOGMAG:
		v = refpos - logmag(coeff) * scale;
		break;
	case TRC_PHASE:
		v = refpos - phase(coeff) * scale;
		break;
	case TRC_DELAY:
		v = refpos - groupdelay_from_array(i, array) * scale;
		break;
	case TRC_LINEAR:
		v = refpos + linear(coeff) * scale;
		break;
	case TRC_SWR:
		v = refpos+ (1 - swr(coeff)) * scale;
		break;
	case TRC_REAL:
		v = refpos - coeff.real() * scale;
		break;
	case TRC_IMAG:
		v = refpos - coeff.imag() * scale;
		break;
	case TRC_R:
		v = refpos - resistance(coeff) * scale;
		break;
	case TRC_X:
		v = refpos - reactance(coeff) * scale;
		break;
	case TRC_SMITH:
	//case TRC_ADMIT:
	case TRC_POLAR:
		cartesian_scale(coeff.real(), coeff.imag(), &x, &y, scale);
		return INDEX(x +CELLOFFSETX, y, i);
		break;
	}
	if (v < 0) v = 0;
	if (v > 8) v = 8;
	y = v * GRIDY;
	return INDEX(x +CELLOFFSETX, y, i);
}

static int
string_value_with_prefix(char *buf, int len, float val, char unit)
{
	char prefix;
	int n = 0;
	if (val < 0) {
		val = -val;
		*buf = '-';
		n++;
		len--;
	}
	if (val < 1e-12) {
		prefix = 'f';
		val *= 1e15;
	} else if (val < 1e-9) {
		prefix = 'p';
		val *= 1e12;
	} else if (val < 1e-6) {
		prefix = 'n';
		val *= 1e9;
	} else if (val < 1e-3) {
		prefix = S_MICRO[0];
		val *= 1e6;
	} else if (val < 1) {
		prefix = 'm';
		val *= 1e3;
	} else if (val < 1e3) {
		prefix = 0;
	} else if (val < 1e6) {
		prefix = 'k';
		val /= 1e3;
	} else if (val < 1e9) {
		prefix = 'M';
		val /= 1e6;
	} else {
		prefix = 'G';
		val /= 1e9;
	}

	if (val < 10) {
		n += chsnprintf(&buf[n], len, "%.2f", val);
	} else if (val < 100) {
		n += chsnprintf(&buf[n], len, "%.1f", val);
	} else {
		n += chsnprintf(&buf[n], len, "%d", (int)val);
	}

	if (prefix)
		buf[n++] = prefix;
	if (unit)
		buf[n++] = unit;
	buf[n] = '\0';
	return n;
}


#define PI2 6.283184

static void
format_smith_value(char *buf, int len, complexf coeff, freqHz_t frequency)
{
	float re = coeff.real(), im = coeff.imag();
	// z = (gamma+1)/(gamma-1) * z0
	float z0 = 50;
	float d = z0 / ((1-re)*(1-re)+im*im);
	float zr = ((1+re)*(1-re) - im*im) * d;
	float zi = 2*im * d;
	int n;

	switch (marker_smith_format) {
	case MS_LIN:
		chsnprintf(buf, len, "%.2f %.1f" S_DEGREE, linear(coeff), phase(coeff));
		break;
		case MS_LOG: {
			float v = logmag(coeff);
			if (v == -INFINITY)
				chsnprintf(buf, len, "-INF dB");
			else
				chsnprintf(buf, len, "%.1fdB %.1f" S_DEGREE, v, phase(coeff));
		}
		break;

	case MS_REIM:
		n = string_value_with_prefix(buf, len, coeff.real(), '\0');
		if (coeff.imag() >= 0) buf[n++] = '+';
		string_value_with_prefix(buf+n, len-n, coeff.imag(), 'j');
		break;

	case MS_RX:
		n = string_value_with_prefix(buf, len, zr, S_OHM[0]);
		if (zi >= 0)
			buf[n++] = ' ';
		string_value_with_prefix(buf+n, len-n, zi, 'j');
		break;

	case MS_RLC:
		n = string_value_with_prefix(buf, len, zr, S_OHM[0]);
		buf[n++] = ' ';

		if (zi < 0) {
			float c = -1 / (PI2 * frequency * zi);
			string_value_with_prefix(buf+n, len-n, c, 'F');
		} else {
			float l = zi / (PI2 * frequency);
			string_value_with_prefix(buf+n, len-n, l, 'H');
		}
		break;
	}
}

static void
gamma2resistance(char *buf, int len, complexf coeff)
{
	float re = coeff.real(), im = coeff.imag();
	float z0 = 50;
	float d = z0 / ((1-re)*(1-re)+im*im);
	float zr = ((1+re)*(1-re) - im*im) * d;
	string_value_with_prefix(buf, len, zr, S_OHM[0]);
}

static void
gamma2reactance(char *buf, int len, complexf coeff)
{
	float re = coeff.real(), im = coeff.imag();
	float z0 = 50;
	float d = z0 / ((1-re)*(1-re)+im*im);
	float zi = 2*im * d;
	string_value_with_prefix(buf, len, zi, S_OHM[0]);
}

static void
trace_get_value_string(int t, char *buf, int len, complexf array[SWEEP_POINTS_MAX], int i)
{
	float v;
	complexf coeff = array[i];
	switch (trace[t].type) {
	case TRC_LOGMAG:
		v = logmag(coeff);
		if (v == -INFINITY)
			chsnprintf(buf, len, "-INF dB");
		else
			chsnprintf(buf, len, "%.2fdB", v);
		break;
	case TRC_PHASE:
		v = phase(coeff);
		chsnprintf(buf, len, "%.2f" S_DEGREE, v);
		break;
	case TRC_DELAY:
		v = groupdelay_from_array(i, array);
		string_value_with_prefix(buf, len, v, 's');
		break;
	case TRC_LINEAR:
		v = linear(coeff);
		chsnprintf(buf, len, "%.2f", v);
		break;
	case TRC_SWR:
		v = swr(coeff);
		chsnprintf(buf, len, "%.2f", v);
		break;
	case TRC_SMITH:
		format_smith_value(buf, len, coeff, freqAt(i));
		break;
	case TRC_REAL:
		chsnprintf(buf, len, "%.2f", coeff.real());
		break;
	case TRC_IMAG:
		chsnprintf(buf, len, "%.2fj", coeff.imag());
		break;
	case TRC_R:
		gamma2resistance(buf, len, coeff);
		break;
	case TRC_X:
		gamma2reactance(buf, len, coeff);
		break;
	//case TRC_ADMIT:
	case TRC_POLAR:
		chsnprintf(buf, len, "%.2f %.2fj", coeff.real(), coeff.imag());
		break;
	}
}


static void
trace_get_value_string_delta(int t, char *buf, int len, complexf array[SWEEP_POINTS_MAX], int index, int index_ref)
{
	complexf coeff = array[index];
	complexf coeff_ref = array[index_ref];
	float v;
	switch (trace[t].type) {
	case TRC_LOGMAG:
		v = logmag(coeff) - logmag(coeff_ref);
		if (v == -INFINITY)
			chsnprintf(buf, len, S_DELTA "-INF dB");
		else
			chsnprintf(buf, len, S_DELTA "%.2fdB", v);
		break;
	case TRC_PHASE:
		v = phase(coeff) - phase(coeff_ref);
		chsnprintf(buf, len, S_DELTA "%.2f" S_DEGREE, v);
		break;
	case TRC_DELAY:
		v = groupdelay_from_array(index, array) - groupdelay_from_array(index_ref, array);
		string_value_with_prefix(buf, len, v, 's');
		break;
	case TRC_LINEAR:
		v = linear(coeff) - linear(coeff_ref);
		chsnprintf(buf, len, S_DELTA "%.2f", v);
		break;
	case TRC_SWR:
		v = swr(coeff) - swr(coeff_ref);
		chsnprintf(buf, len, S_DELTA "%.2f", v);
		break;
	case TRC_SMITH:
		format_smith_value(buf, len, coeff, freqAt(index));
		break;
	case TRC_REAL:
		chsnprintf(buf, len, S_DELTA "%.2f", coeff.real() - coeff_ref.real());
		break;
	case TRC_IMAG:
		chsnprintf(buf, len, S_DELTA "%.2fj", coeff.imag() - coeff_ref.imag());
		break;
	case TRC_R:
		gamma2resistance(buf, len, coeff);
		break;
	case TRC_X:
		gamma2reactance(buf, len, coeff);
		break;
	//case TRC_ADMIT:
	case TRC_POLAR:
		chsnprintf(buf, len, "%.2f %.2fj", coeff.real(), coeff.imag());
		break;
	}
}


void
trace_get_info(int t, char *buf, int len)
{
	const char *type = get_trace_typename(t);
	int n;
	switch (trace[t].type) {
	case TRC_LOGMAG:
		chsnprintf(buf, len, "%s %ddB/", type, (int)get_trace_scale(t));
		break;
	case TRC_PHASE:
		chsnprintf(buf, len, "%s %d" S_DEGREE "/", type, (int)get_trace_scale(t));
		break;
	case TRC_SMITH:
	//case TRC_ADMIT:
	case TRC_POLAR:
		chsnprintf(buf, len, "%s %.1fFS", type, get_trace_scale(t));
		break;
	default:
		n = chsnprintf(buf, len, "%s ", type);
		string_value_with_prefix(buf+n, len-n, get_trace_scale(t), '/');
		break;
	}
}

static float time_of_index(int idx) {
	 return 1.0 / (float)(plot_getFrequencyAt(1) - plot_getFrequencyAt(0)) / (float)FFT_SIZE * idx;
}

static float distance_of_index(int idx) {
#define SPEED_OF_LIGHT 299792458
	 float distance = ((float)idx * (float)SPEED_OF_LIGHT) / ( (float)(plot_getFrequencyAt(1) - plot_getFrequencyAt(0)) * (float)FFT_SIZE * 2.0);
	 return distance * velocity_factor;
}


void
mark_map(int x, int y)
{
	if (y >= 0 && y < 8 && x >= 0 && x < 16)
		markmap[current_mappage][y] |= 1<<x;
}

static inline int
is_mapmarked(int x, int y)
{
	uint16_t bit = 1<<x;
	return (markmap[0][y] & bit) || (markmap[1][y] & bit);
}

static inline void
markmap_upperarea(void)
{
	markmap[current_mappage][0] |= 0xffff;
}

static inline void
swap_markmap(void)
{
	current_mappage = 1 - current_mappage;
}

static inline void
clear_markmap(void)
{
	memset(markmap[current_mappage], 0, sizeof markmap[current_mappage]);
}

void
force_set_markmap(void)
{
	memset(markmap[current_mappage], 0xff, sizeof markmap[current_mappage]);
}

struct Line2d {
	int dx, dy;
};
inline bool operator<=(const Line2d& a, const Line2d& b) {
	return a.dy*b.dx <= b.dy*a.dx;
}
inline bool operator>=(const Line2d& a, const Line2d& b) {
	return a.dy*b.dx >= b.dy*a.dx;
}

void
mark_cells_from_index(void)
{
	int t;
	constexpr int cellW = 1 << 5;
	constexpr int cellH = 1 << 5;

	/* mark cells between each neighber points */
	for (t = 0; t < TRACES_MAX; t++) {
		if (!trace[t].enabled)
			continue;
		int x0 = CELL_X(trace_index[t][0]);
		int y0 = CELL_Y(trace_index[t][0]);
		int m0 = x0 >> 5;
		int n0 = y0 >> 5;
		int i;
		mark_map(m0, n0);
		for (i = 1; i < sweep_points; i++) {
			int x1 = CELL_X(trace_index[t][i]);
			int y1 = CELL_Y(trace_index[t][i]);
			int m1 = x1 >> 5;
			int n1 = y1 >> 5;
			while (m0 != m1 || n0 != n1) {
				if (m0 == m1) {
					if (n0 < n1) n0++; else n0--;
				} else if (n0 == n1) {
					if (m0 < m1) m0++; else m0--;
				} else {
					// figure out which neighboring cell the line starting at
					// x0,y0 and ending at x1,y1 exits into.

					// the bounding rect when drawing a cell seems to start at
					// cellX - 0.5, cellY - 0.5, and end at
					// cellX + W - 0.5, cellY + H - 0.5
					// scale everything by 2 and subtract 1.
					int xLower = (m0 << 5) * 2 - 1;
					int xUpper = ((m0+1) << 5) * 2 - 1;
					int yLower = (n0 << 5) * 2 - 1;
					int yUpper = ((n0+1) << 5) * 2 - 1;
					int srcX = x0 * 2, srcY = y0 * 2;
					int mOrig = m0, nOrig = n0;

					Line2d tanDst = {x1 - x0, y1 - y0};

					// lines from the src point to the 4 corners of the current cell
					Line2d tanTL = {xLower - srcX, yLower - srcY};
					Line2d tanTR = {xUpper - srcX, yLower - srcY};
					Line2d tanBR = {xUpper - srcX, yUpper - srcY};
					Line2d tanBL = {xLower - srcX, yUpper - srcY};

					// compare line slope/tangents to see which sides of the
					// current cell the line intersects (only one direction counts).
					if(tanDst >= tanTL && tanDst <= tanTR) {
						n0--; // top side
					}
					if(tanDst >= tanTR && tanDst <= tanBR) {
						m0++; // right side
					}
					if(tanDst >= tanBR && tanDst <= tanBL) {
						n0++; // bottom side
					}
					if(tanDst >= tanBL && tanDst <= tanTL) {
						m0--; // left side
					}
					if(mOrig == m0 && nOrig == n0) {
						// if we reach here there is a bug
						break;
					}
				}
				mark_map(m0, n0);
				if(m0 < 0 || m0 >= 16 || n0 < 0 || n0 >= 8)
					break;
			}
			x0 = x1;
			y0 = y1;
			m0 = m1;
			n0 = n1;
		}
	}
}

void plot_into_index(complexf measured[2][SWEEP_POINTS_MAX])
{
	mark_cells_from_index();
	int i, t;
	for (i = 0; i < sweep_points; i++) {
		int x = i * (WIDTH-1) / (sweep_points-1);
		for (t = 0; t < TRACES_MAX; t++) {
			if (!trace[t].enabled)
				continue;
			int n = trace[t].channel;
			trace_index[t][i] = trace_into_index(x, t, i, measured[n]);
		}
	}

	mark_cells_from_index();
	markmap_all_markers();
	redraw_request |= REDRAW_CELLS;
}

const uint8_t INSIDE = 0b0000;
const uint8_t LEFT   = 0b0001;
const uint8_t RIGHT  = 0b0010;
const uint8_t BOTTOM = 0b0100;
const uint8_t TOP    = 0b1000;

inline static uint8_t
_compute_outcode(int w, int h, int x, int y)
{
		uint8_t code = 0;
		if (x < 0) {
				code |= LEFT;
		} else
		if (x > w) {
				code |= RIGHT;
		}
		if (y < 0) {
				code |= BOTTOM;
		} else
		if (y > h) {
				code |= TOP;
		}
		return code;
}

void
cell_drawline(int w, int h, int x0, int y0, int x1, int y1, int c)
{
	uint8_t outcode0 = _compute_outcode(w, h, x0, y0);
	uint8_t outcode1 = _compute_outcode(w, h, x1, y1);

	if (outcode0 & outcode1) {
			// this line is out of requested area. early return
			return;
	}

	if (x0 > x1) {
		SWAP(x0, x1);
		SWAP(y0, y1);
	}

	int dx = x1 - x0;
	int dy = y1 - y0;
	int sy = dy > 0 ? 1 : -1;
	int e = 0;

	dy *= sy;

	if (dx >= dy) {
			e = dy * 2 - dx;
			while (x0 != x1) {
					if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  ili9341_spi_buffer[y0*w+x0] |= c;
					x0++;
					e += dy * 2;
					if (e >= 0) {
							e -= dx * 2;
							y0 += sy;
					}
			}
			if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  ili9341_spi_buffer[y0*w+x0] |= c;
	} else {
			e = dx * 2 - dy;
			while (y0 != y1) {
					if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  ili9341_spi_buffer[y0*w+x0] |= c;
					y0 += sy;
					e += dx * 2;
					if (e >= 0) {
							e -= dy * 2;
							x0++;
					}
			}
			if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)  ili9341_spi_buffer[y0*w+x0] |= c;
	}
}

int
search_index_range(int x, int y, uint32_t* index, int *i0, int *i1)
{
	int i, j;
	int head = 0;
	int tail = sweep_points;
	i = 0;
	x &= 0x03e0;
	y &= 0x03e0;
	while (head < tail) {
		i = (head + tail) / 2;
		if (x < CELL_X0(index[i]))
			tail = i+1;
		else if (x > CELL_X0(index[i]))
			head = i;
		else if (y < CELL_Y0(index[i]))
			tail = i+1;
		else if (y > CELL_Y0(index[i]))
			head = i;
		else
			break;
	}

	if (x != CELL_X0(index[i]) || y != CELL_Y0(index[i]))
		return FALSE;

	j = i;
	while (j > 0 && x == CELL_X0(index[j-1]) && y == CELL_Y0(index[j-1]))
		j--;
	*i0 = j;
	j = i;
	while (j < (sweep_points - 1) && x == CELL_X0(index[j+1]) && y == CELL_Y0(index[j+1]))
		j++;
	*i1 = j;
	return TRUE;
}

int
search_index_range_x(int x, uint32_t* index, int *i0, int *i1)
{
	int i, j;
	int head = 0;
	int tail = sweep_points;
	x &= 0x03e0;
	i = 0;
	while (head < tail) {
		i = (head + tail) / 2;
		if (x == CELL_X0(index[i]))
			break;
		else if (x < CELL_X0(index[i])) {
			if (tail == i+1)
				break;
			tail = i+1;
		} else {
			if (head == i)
				break;
			head = i;
		}
	}

	if (x != CELL_X0(index[i]))
		return FALSE;

	j = i;
	while (j > 0 && x == CELL_X0(index[j-1]))
		j--;
	*i0 = j;
	j = i;
	while (j < (sweep_points - 1) && x == CELL_X0(index[j+1]))
		j++;
	*i1 = j;
	return TRUE;
}

void
draw_refpos(int w, int h, int x, int y, int c)
{
	// draw triangle
	int i, j;
	if (y < -3 || y > 32 + 3)
		return;
	for (j = 0; j < 3; j++) {
		int j0 = 6 - j*2;
		for (i = 0; i < j0; i++) {
			int x0 = x + i-5;
			int y0 = y - j;
			int y1 = y + j;
			if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
				ili9341_spi_buffer[y0*w+x0] = c;
			if (j != 0 && y1 >= 0 && y1 < h && x0 >= 0 && x0 < w)
				ili9341_spi_buffer[y1*w+x0] = c;
		}
	}
}


void
cell_draw_refpos(int m, int n, int w, int h)
{
	int x0 = m * CELLWIDTH;
	int y0 = n * CELLHEIGHT;
	int t;
	for (t = 0; t < TRACES_MAX; t++) {
		if (!trace[t].enabled)
			continue;
		if (trace[t].type == TRC_SMITH || trace[t].type == TRC_POLAR)
			continue;
		int x = 0 - x0 +CELLOFFSETX;
		int y = 8*GRIDY - (int)(get_trace_refpos(t) * GRIDY) - y0;
		if (x > -5 && x < w && y >= -3 && y < h+3)
			draw_refpos(w, h, x, y, config.trace_color[t]);
	}
}

void
draw_marker(int w, int h, int x, int y, int c, int ch)
{
	int i, j;
	for (j = 10; j >= 0; j--) {
		int j0 = j / 2;
		for (i = -j0; i <= j0; i++) {
			int x0 = x + i;
			int y0 = y - j;
			int cc = c;
			if (j <= 9 && j > 2 && i >= -1 && i <= 3) {
				uint16_t bits = x5x7_bits[(ch * 7) + (9-j)];
				if (bits & (0x80>>(i+1)))
					cc = 0;
			}
			if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
				ili9341_spi_buffer[y0*w+x0] = cc;
		}
	}
}

void
marker_position(int m, int t, int *x, int *y)
{
	uint32_t index = trace_index[t][markers[m].index];
	*x = CELL_X(index);
	*y = CELL_Y(index);
}

typedef int (*compare_t)(int x, int y);
static int greater(int x, int y) { return x > y; }
static int lesser(int x, int y) { return x < y; }

compare_t marker_comparator(MarkerSearchModes mode) {
	if (mode == MarkerSearchModes::Max)
		return ::greater;
	else
		return ::lesser;
}


int
marker_search(MarkerSearchModes mode)
{
	compare_t compare = marker_comparator(mode);
	int i;
	int found = 0;

	if (uistat.current_trace == -1)
		return -1;

	int value = CELL_Y(trace_index[uistat.current_trace][0]);
	for (i = 0; i < sweep_points; i++) {
		uint32_t index = trace_index[uistat.current_trace][i];
		if ((*compare)(value, CELL_Y(index))) {
			value = CELL_Y(index);
			found = i;
		}
	}

	return found;
}

// TODO: merge marker_search_left and marker_search_right into one function

int
marker_search_left(MarkerSearchModes mode, int from)
{
	compare_t compare = marker_comparator(mode);
	int i;
	int found = -1;

	if (uistat.current_trace == -1)
		return -1;

	int value = CELL_Y(trace_index[uistat.current_trace][from]);
	for (i = from - 1; i >= 0; i--) {
		uint32_t index = trace_index[uistat.current_trace][i];
		if ((*compare)(value, CELL_Y(index)))
			break;
		value = CELL_Y(index);
	}

	for (; i >= 0; i--) {
		uint32_t index = trace_index[uistat.current_trace][i];
		if ((*compare)(CELL_Y(index), value)) {
			break;
		}
		found = i;
		value = CELL_Y(index);
	}
	return found;
}

int
marker_search_right(MarkerSearchModes mode, int from)
{
	compare_t compare = marker_comparator(mode);
	int i;
	int found = -1;

	if (uistat.current_trace == -1)
		return -1;

	int value = CELL_Y(trace_index[uistat.current_trace][from]);
	for (i = from + 1; i < sweep_points; i++) {
		uint32_t index = trace_index[uistat.current_trace][i];
		if ((*compare)(value, CELL_Y(index)))
			break;
		value = CELL_Y(index);
	}

	for (; i < sweep_points; i++) {
		uint32_t index = trace_index[uistat.current_trace][i];
		if ((*compare)(CELL_Y(index), value)) {
			break;
		}
		found = i;
		value = CELL_Y(index);
	}
	return found;
}


int
search_nearest_index(int x, int y, int t)
{
	uint32_t *index = trace_index[t];
	int min_i = -1;
	int min_d = 1000;
	int i;
	for (i = 0; i < sweep_points; i++) {
		int16_t dx = x - CELL_X(index[i]) - OFFSETX;
		int16_t dy = y - CELL_Y(index[i]) - OFFSETY;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		if (dx > 20 || dy > 20)
			continue;
		int d = dx*dx + dy*dy;
		if (d < min_d) {
			min_i = i;
		}
	}

	return min_i;
}

void
cell_draw_markers(int m, int n, int w, int h)
{
	int x0 = m * CELLWIDTH;
	int y0 = n * CELLHEIGHT;
	int t, i;
	for (i = 0; i < MARKERS_MAX; i++) {
		if (!markers[i].enabled)
			continue;
		for (t = 0; t < TRACES_MAX; t++) {
			if (!trace[t].enabled)
				continue;
			uint32_t index = trace_index[t][markers[i].index];
			int x = CELL_X(index) - x0;
			int y = CELL_Y(index) - y0;
			if (x > -6 && x < w+6 && y >= 0 && y < h+12)
				draw_marker(w, h, x, y, config.trace_color[t], '1' + i);
		}
	}
}

void
markmap_marker(int marker)
{
	int t;
	if (!markers[marker].enabled)
		return;
	for (t = 0; t < TRACES_MAX; t++) {
		if (!trace[t].enabled)
			continue;
		uint32_t index = trace_index[t][markers[marker].index];
		int x = CELL_X(index);
		int y = CELL_Y(index);
		int m = x>>5;
		int n = y>>5;
		mark_map(m, n);
		if ((x&31) < 6)
			mark_map(m-1, n);
		if ((x&31) > 32-6)
			mark_map(m+1, n);
		if ((y&31) < 12) {
			mark_map(m, n-1);
			if ((x&31) < 6)
				mark_map(m-1, n-1);
			if ((x&31) > 32-6)
				mark_map(m+1, n-1);
		}
	}
}

void
markmap_all_markers(void)
{
	int i;
	for (i = 0; i < MARKERS_MAX; i++) {
		if (!markers[i].enabled)
			continue;
		markmap_marker(i);
	}
	markmap_upperarea();
}
bool plot_checkerBoard = false;
bool plot_shadeCells = false;
static void
draw_cell(int m, int n)
{
	int x0 = m * CELLWIDTH;
	int y0 = n * CELLHEIGHT;
	int x0off = x0 - CELLOFFSETX;
	int w = CELLWIDTH;
	int h = CELLHEIGHT;
	int x, y;
	int i0, i1;
	int i;
	int t;
	bool shade = plot_shadeCells;
	if(plot_checkerBoard)
		shade |= (((m + n) % 2) == 0);

	int bg = shade ? RGB565(40,40,40) : 0;

	if (x0off + w > area_width)
		w = area_width - x0off;
	if (y0 + h > area_height)
		h = area_height - y0;
	if (w <= 0 || h <= 0)
		return;

	uint16_t grid_mode = 0;
	for (t = 0; t < TRACES_MAX; t++) {
		if (!trace[t].enabled)
			continue;

		if (trace[t].type == TRC_SMITH)
			grid_mode |= GRID_SMITH;
		//else if (trace[t].type == TRC_ADMIT)
		//  grid_mode |= GRID_ADMIT;
		else if (trace[t].type == TRC_POLAR)
			grid_mode |= GRID_POLAR;
		else
			grid_mode |= GRID_RECTANGULAR;
	}

	PULSE;
	/* draw grid */
	if (grid_mode & GRID_RECTANGULAR) {
		for (x = 0; x < w; x++) {
			uint16_t c = rectangular_grid_x(x+x0off, bg);
			for (y = 0; y < h; y++)
				ili9341_spi_buffer[y * w + x] = c;
		}
		for (y = 0; y < h; y++) {
			uint16_t c = rectangular_grid_y(y+y0, bg);
			for (x = 0; x < w; x++)
				if (x+x0off >= 0 && x+x0off <= WIDTH)
					ili9341_spi_buffer[y * w + x] |= c;
		}
	} else {
		memset(ili9341_spi_buffer, 0, ili9341_bufferSize * sizeof(*ili9341_spi_buffer));
	}
	if (grid_mode & (GRID_SMITH|GRID_ADMIT|GRID_POLAR)) {
		for (y = 0; y < h; y++) {
			for (x = 0; x < w; x++) {
				uint16_t c = 0;
				if (grid_mode & GRID_SMITH)
					c = smith_grid(x+x0off, y+y0);
				else if (grid_mode & GRID_ADMIT)
					c = smith_grid3(x+x0off, y+y0);
				//c = smith_grid2(x+x0, y+y0, 0.5);
				else if (grid_mode & GRID_POLAR)
					c = polar_grid(x+x0off, y+y0);
				ili9341_spi_buffer[y * w + x] |= c;
			}
		}
	}
	PULSE;

#if 1
	/* draw rectanglar plot */
	for (t = 0; t < TRACES_MAX; t++) {
		if (!trace[t].enabled)
			continue;
		if (trace[t].type == TRC_SMITH || trace[t].type == TRC_POLAR)
			continue;

		if (search_index_range_x(x0, trace_index[t], &i0, &i1)) {
			if (i0 > 0)
				i0--;
			if (i1 < current_props._sweep_points-1)
				i1++;
			for (i = i0; i < i1; i++) {
				int x1 = CELL_X(trace_index[t][i]);
				int x2 = CELL_X(trace_index[t][i+1]);
				int y1 = CELL_Y(trace_index[t][i]);
				int y2 = CELL_Y(trace_index[t][i+1]);
				int c = config.trace_color[t];
				cell_drawline(w, h, x1 - x0, y1 - y0, x2 - x0, y2 - y0, c);
			}
		}
	}
#endif
#if 1
	/* draw polar plot */
	for (t = 0; t < TRACES_MAX; t++) {
		int c = config.trace_color[t];
		if (!trace[t].enabled)
			continue;
		if (trace[t].type != TRC_SMITH && trace[t].type != TRC_POLAR)
			continue;

		for (i = 1; i < sweep_points; i++) {
			//uint32_t index = trace_index[t][i];
			//uint32_t pindex = trace_index[t][i-1];
			//if (!CELL_P(index, x0, y0) && !CELL_P(pindex, x0, y0))
			//  continue;
			int x1 = CELL_X(trace_index[t][i-1]);
			int x2 = CELL_X(trace_index[t][i]);
			int y1 = CELL_Y(trace_index[t][i-1]);
			int y2 = CELL_Y(trace_index[t][i]);
			cell_drawline(w, h, x1 - x0, y1 - y0, x2 - x0, y2 - y0, c);
		}
	}
#endif

	PULSE;
	//draw marker symbols on each trace
	cell_draw_markers(m, n, w, h);
	// draw trace and marker info on the top
	cell_draw_marker_info(m, n, w, h);
	PULSE;

	if (m == 0)
		cell_draw_refpos(m, n, w, h);

	ili9341_bulk(OFFSETX + x0off, OFFSETY + y0, w, h);
}

void
draw_all_cells(bool flush_markmap)
{
	int m, n;
	for (m = 0; m < (area_width+CELLWIDTH-1) / CELLWIDTH; m++)
		for (n = 0; n < (area_height+CELLHEIGHT-1) / CELLHEIGHT; n++) {
			if (is_mapmarked(m, n)) {
				draw_cell(m, n);
				plot_tick();
				if(plot_canceled) return;
			}
		}

	if (flush_markmap) {
		// keep current map for update
		swap_markmap();
		// clear map for next plotting
		clear_markmap();
	}
}

void
draw_all(bool flush)
{
	plot_canceled = false;
	if (redraw_request & REDRAW_CELLS)
		draw_all_cells(flush);
	if (redraw_request & REDRAW_FREQUENCY)
		draw_frequencies();
	if (redraw_request & REDRAW_CAL_STATUS)
		draw_cal_status();
	redraw_request = 0;
}

void
request_to_redraw_marker(int marker, int update_info)
{
	// mark map on new position of marker
	redraw_request |= REDRAW_CELLS;
	markmap_marker(marker);

	// mark cells on marker info
	if (update_info)
		markmap[current_mappage][0] = 0xffff;
}


void
redraw_marker(int marker, int update_info)
{
	request_to_redraw_marker(marker, update_info);
	draw_all_cells(true);
}

void
request_to_draw_cells_behind_menu(void)
{
	int n, m;
	for (m = 7; m <= 9; m++)
		for (n = 0; n < 8; n++)
			mark_map(m, n);
	redraw_request |= REDRAW_CELLS;
}

void
request_to_draw_cells_behind_numeric_input(void)
{
	int n, m;
	for (m = 0; m <= 9; m++)
		for (n = 6; n < 8; n++)
			mark_map(m, n);
	redraw_request |= REDRAW_CELLS;
}


void
cell_drawchar_5x7(int w, int h, uint8_t ch, int x, int y, uint16_t fg, int invert)
{
	uint8_t bits;
	int c, r;
	if (y <= -7 || y >= h || x <= -5 || x >= w)
		return;
	for(c = 0; c < 7; c++) {
		if ((y + c) < 0 || (y + c) >= h)
			continue;
		bits = x5x7_bits[(ch * 7) + c];
		if (invert)
			bits = ~bits;
		for (r = 0; r < 5; r++) {
			if ((x+r) >= 0 && (x+r) < w && (0x80 & bits))
				ili9341_spi_buffer[(y+c)*w + (x+r)] = fg;
			bits <<= 1;
		}
	}
}

void
cell_drawstring_5x7(int w, int h, char *str, int x, int y, uint16_t fg)
{
	while (*str) {
		cell_drawchar_5x7(w, h, *str, x, y, fg, FALSE);
		x += 5;
		str++;
	}
}

void
cell_drawstring_invert_5x7(int w, int h, char *str, int x, int y, uint16_t fg, int invert)
{
	while (*str) {
		cell_drawchar_5x7(w, h, *str, x, y, fg, invert);
		x += 5;
		str++;
	}
}

static void
cell_draw_marker_info(int m, int n, int w, int h)
{
	char buf[24];
	int t;
	if (n != 0)
		return;
	if (active_marker < 0)
		return;
	int idx = markers[active_marker].index;
	int j = 0;
	if (active_marker != -1 && previous_marker != -1 && uistat.current_trace != -1) {
		int t = uistat.current_trace;
		int mk;
		for (mk = 0; mk < MARKERS_MAX; mk++) {
			if (!markers[mk].enabled)
				continue;
			int xpos = 1 + (j%2)*146;
			int ypos = 1 + (j/2)*7;
			xpos -= m * CELLWIDTH -CELLOFFSETX;
			ypos -= n * CELLHEIGHT;
			strcpy(buf, "MK1");
			buf[2] += mk;
			cell_drawstring_invert_5x7(w, h, buf, xpos, ypos, config.trace_color[t], mk == active_marker);
			xpos += 20;
			//trace_get_info(t, buf, sizeof buf);
			int32_t freq = freqAt(markers[mk].index);
			if (uistat.marker_delta && mk != active_marker) {
				freq -= freqAt(markers[active_marker].index);
				frequency_string_short(buf, sizeof buf, freq, S_DELTA[0]);
			} else {
				frequency_string_short(buf, sizeof buf, freq, 0);
			}
			cell_drawstring_5x7(w, h, buf, xpos, ypos, config.trace_color[t]);
			xpos += 64;
			if (uistat.marker_delta && mk != active_marker)
				trace_get_value_string_delta(t, buf, sizeof buf, measured[trace[t].channel], markers[mk].index, markers[active_marker].index);
			else
				trace_get_value_string(t, buf, sizeof buf, measured[trace[t].channel], markers[mk].index);
			cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
			j++;
		}

		// draw marker delta
		if (!uistat.marker_delta && previous_marker >= 0 && active_marker != previous_marker && markers[previous_marker].enabled) {
			int idx0 = markers[previous_marker].index;
			int xpos = 192;
			int ypos = 1 + (j/2)*7;
			xpos -= m * CELLWIDTH -CELLOFFSETX;
			ypos -= n * CELLHEIGHT;
			strcpy(buf, S_DELTA "1:");
			buf[1] += previous_marker;
			cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
			xpos += 19;
			if ((domain_mode & DOMAIN_MODE) == DOMAIN_FREQ) {
				frequency_string(buf, sizeof buf, freqAt(idx) - freqAt(idx0));
			} else {
				//chsnprintf(buf, sizeof buf, "%d ns %.1f m", (uint16_t)(time_of_index(idx) * 1e9 - time_of_index(idx0) * 1e9),
				//                                            distance_of_index(idx) - distance_of_index(idx0));
				int n = string_value_with_prefix(buf, sizeof buf, time_of_index(idx) - time_of_index(idx0), 's');
				buf[n++] = ' ';
				string_value_with_prefix(&buf[n], sizeof buf - n, distance_of_index(idx) - distance_of_index(idx0), 'm');
			}
			cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
		}
	} else {
		for (t = 0; t < TRACES_MAX; t++) {
			if (!trace[t].enabled)
				continue;
			int xpos = 1 + (j%2)*146;
			int ypos = 1 + (j/2)*7;
			xpos -= m * CELLWIDTH -CELLOFFSETX;
			ypos -= n * CELLHEIGHT;
			strcpy(buf, "CH0");
			buf[2] += trace[t].channel;
			//chsnprintf(buf, sizeof buf, "CH%d", trace[t].channel);
			cell_drawstring_invert_5x7(w, h, buf, xpos, ypos, config.trace_color[t], t == uistat.current_trace);
			xpos += 20;
			trace_get_info(t, buf, sizeof buf);
			cell_drawstring_5x7(w, h, buf, xpos, ypos, config.trace_color[t]);
			xpos += 64;
			trace_get_value_string(t, buf, sizeof buf, measured[trace[t].channel], idx);
			cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
			j++;
		}

		// draw marker frequency
		int xpos = 192;
		int ypos = 1 + (j/2)*7;
		xpos -= m * CELLWIDTH -CELLOFFSETX;
		ypos -= n * CELLHEIGHT;
		strcpy(buf, "1:");
		buf[0] += active_marker;
		xpos += 5;
		cell_drawstring_invert_5x7(w, h, buf, xpos, ypos, 0xffff, uistat.lever_mode == LM_MARKER);
		xpos += 14;
		if ((domain_mode & DOMAIN_MODE) == DOMAIN_FREQ) {
			frequency_string(buf, sizeof buf, plot_getFrequencyAt(idx));
		} else {
			//chsnprintf(buf, sizeof buf, "%d ns %.1f m", (uint16_t)(time_of_index(idx) * 1e9), distance_of_index(idx));
			int n = string_value_with_prefix(buf, sizeof buf, time_of_index(idx), 's');
			buf[n++] = ' ';
			string_value_with_prefix(&buf[n], sizeof buf-n, distance_of_index(idx), 'm');
		}
		cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
	}
	if (electrical_delay != 0) {
		// draw electrical delay
		int xpos = 21;
		int ypos = 1 + ((j+1)/2)*7;
		xpos -= m * CELLWIDTH -CELLOFFSETX;
		ypos -= n * CELLHEIGHT;
		chsnprintf(buf, sizeof buf, "Edelay");
		cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
		xpos += 7 * 5;
		int n = string_value_with_prefix(buf, sizeof buf, electrical_delay * 1e-12, 's');
		cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
		xpos += n * 5 + 5;
		float light_speed_ps = 299792458e-12; //(m/ps)
		string_value_with_prefix(buf, sizeof buf, electrical_delay * light_speed_ps * velocity_factor, 'm');
		cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
	}
}

void
frequency_string(char *buf, size_t len, freqHz_t freq)
{
	if (freq < 0) {
		freq = -freq;
		*buf++ = '-';
		len -= 1;
	}
	if (freq < 1000) {
		chsnprintf(buf, len, "%d Hz", (int)freq);
	} else if (freq < 1000000) {
		chsnprintf(buf, len, "%d.%03d kHz",
						 (int)(freq / 1000),
						 (int)(freq % 1000));
	} else {
		chsnprintf(buf, len, "%d.%03d %03d MHz",
						 (int)(freq / 1000000),
						 (int)((freq / 1000) % 1000),
						 (int)(freq % 1000));
	}
}

void
frequency_string_short(char *b, size_t len, freqHz_t freq, char prefix)
{
	char *buf = b;
	if (prefix) {
		*buf++ = prefix;
		len -= 1;
	}
	if (freq < 0) {
		freq = -freq;
		*buf++ = '-';
		len -= 1;
	}
	if (freq < 1000) {
		chsnprintf(buf, len, "%d Hz", (int)freq);
	} else if (freq < 1000000) {
		chsnprintf(buf, len, "%d.%03dkHz",
						 (int)(freq / 1000),
						 (int)(freq % 1000));
	} else {
		chsnprintf(buf, len, "%d.%06d",
						 (int)(freq / 1000000),
						 (int)(freq % 1000000));
		strcpy(b+9, "MHz");
	}
}

void padString(char* s, int len, char c = ' ') {
	int oldLen = strlen(s);
	for(int i = oldLen; i < (len - 1); i++)
		s[i] = c;
	s[len - 1] = 0;
}

void
draw_frequencies(void)
{
	char buf[24];

	// draw sweep points
	chsnprintf(buf, sizeof(buf), "%3d P", (int)sweep_points);
	ili9341_drawstring_5x7(buf, WIDTH / 2, 233, 0xffff, 0x0000);

	if ((domain_mode & DOMAIN_MODE) == DOMAIN_FREQ) {
		if (frequency1 > 0) {
			auto start = frequency0;
			auto stop = frequency1;

			strcpy(buf, "START ");
			frequency_string(buf+6, 24-6, start);
			padString(buf, sizeof(buf));
			ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);

			strcpy(buf, "STOP ");
			frequency_string(buf+5, 24-5, stop);
			padString(buf, sizeof(buf));
			ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
		} else if (frequency1 < 0) {
			auto fcenter = frequency0;
			auto fspan = -frequency1;
			int x = OFFSETX;
			strcpy(buf, "CENTER");
			ili9341_drawstring_5x7_inv(buf, x, 233, 0xffff, 0x0000, uistat.lever_mode == LM_CENTER);
			x += 5 * 6;
			strcpy(buf, " ");
			frequency_string(buf+1, 24-1, fcenter);
			padString(buf, sizeof(buf));
			ili9341_drawstring_5x7(buf, x, 233, 0xffff, 0x0000);
			x = 205;
			strcpy(buf, "SPAN");
			ili9341_drawstring_5x7_inv(buf, x, 233, 0xffff, 0x0000, uistat.lever_mode == LM_SPAN);
			x += 5 * 4;
			strcpy(buf, " ");
			frequency_string(buf+1, 24-1, fspan);
			padString(buf, sizeof(buf));
			ili9341_drawstring_5x7(buf, x, 233, 0xffff, 0x0000);
		} else {
			int x = OFFSETX;
			strcpy(buf, "CW");
			ili9341_drawstring_5x7_inv(buf, x, 233, 0xffff, 0x0000, uistat.lever_mode == LM_CENTER);
			x += 5 * 2;
			strcpy(buf, " ");
			frequency_string(buf+1, 24-1, frequency0);
			padString(buf, sizeof(buf));
			ili9341_drawstring_5x7(buf, x, 233, 0xffff, 0x0000);
		}
	} else {
		strcpy(buf, "START 0s");
		padString(buf, sizeof(buf));
		ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);

		strcpy(buf, "STOP ");
		chsnprintf(buf+5, 24-5, "%d ns", (uint16_t)(time_of_index(current_props._sweep_points) * 1e9));
		padString(buf, sizeof(buf));
		ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
	}
}

void
draw_cal_status(void)
{
	int x = 0;
	int y = 100;
#define YSTEP 7
	ili9341_fill(0, y, 10, 6*YSTEP, 0x0000);
	if (cal_status & CALSTAT_APPLY) {
		char c[3] = "C0";
		c[1] += lastsaveid;
		if (cal_status & CALSTAT_INTERPOLATED)
			c[0] = 'c';
		else if (active_props == &current_props)
			c[1] = '*';
		ili9341_drawstring_5x7(c, x, y, 0xffff, 0x0000);
		y += YSTEP;
	}

	if (cal_status & CALSTAT_ED) {
		ili9341_drawstring_5x7("D", x, y, 0xffff, 0x0000);
		y += YSTEP;
	}
	if (cal_status & CALSTAT_ER) {
		ili9341_drawstring_5x7("R", x, y, 0xffff, 0x0000);
		y += YSTEP;
	}
	if (cal_status & CALSTAT_ES) {
		ili9341_drawstring_5x7("S", x, y, 0xffff, 0x0000);
		y += YSTEP;
	}
	if (cal_status & CALSTAT_ET) {
		ili9341_drawstring_5x7("T", x, y, 0xffff, 0x0000);
		y += YSTEP;
	}
	if (cal_status & CALSTAT_EX) {
		ili9341_drawstring_5x7("X", x, y, 0xffff, 0x0000);
		y += YSTEP;
	}
}

void
draw_battery_status(void)
{
		int w = 10, h = 14;
		int x = 0, y = 0;
		int i, c;
		uint16_t *buf = ili9341_spi_buffer;
		uint8_t vbati = vbat2bati(vbat);
		uint16_t col = vbati == 0 ? RGB565(0, 255, 0) : RGB565(0, 0, 240);
		memset(ili9341_spi_buffer, 0, w * h * 2);

		// battery head
		x = 3;
		buf[y * w + x++] = col;
		buf[y * w + x++] = col;
		buf[y * w + x++] = col;
		buf[y * w + x++] = col;

		y++;
		x = 3;
		buf[y * w + x++] = col;
		x++; x++;
		buf[y * w + x++] = col;

		y++;
		x = 1;
		for (i = 0; i < 8; i++)
				buf[y * w + x++] = col;

		for (c = 0; c < 3; c++) {
				y++;
				x = 1;
				buf[y * w + x++] = col;
				x++; x++; x++; x++; x++; x++;
				buf[y * w + x++] = col;

				y++;
				x = 1;
				buf[y * w + x++] = col;
				x++;
				for (i = 0; i < 4; i++)
						buf[y * w + x++] = ( ((c+1) * 25) >= (100 - vbati)) ? col : 0;
				x++;
				buf[y * w + x++] = col;

				y++;
				x = 1;
				buf[y * w + x++] = col;
				x++;
				for (i = 0; i < 4; i++)
						buf[y * w + x++] = ( ((c+1) * 25) >= (100 - vbati)) ? col : 0;
				x++;
				buf[y * w + x++] = col;
		}

		// battery foot
		y++;
		x = 1;
		buf[y * w + x++] = col;
		x++; x++; x++; x++; x++; x++;
		buf[y * w + x++] = col;

		y++;
		x = 1;
		for (i = 0; i < 8; i++)
				buf[y * w + x++] = col;

		ili9341_bulk(0, 1, w, h);
}

void
request_to_redraw_grid(void)
{
	force_set_markmap();
	redraw_request |= REDRAW_CELLS;
}

void
redraw_frame(void)
{
	ili9341_clear_screen();
	draw_frequencies();
	draw_cal_status();
}

void
plot_init(void)
{
	force_set_markmap();
}

void plot_cancel() {
	plot_canceled = true;
}
