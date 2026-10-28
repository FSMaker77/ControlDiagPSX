/*
 * TEST DIAG - DualShock controller tester for the original PlayStation
 * Built with PSn00bSDK - https://github.com/Lameguy64/PSn00bSDK
 *
 * Dark "diagnostics" themed UI:
 * - boot logo screen ("TEST DIAG" / FSmaker) with grid and radar decorations
 * - button boxes light up bright green while held
 * - circular analog stick wells with a cursor dot following the stick
 * - vibration motor level bars (hold L2 for the motor sweep, R2 for the
 *   small motor)
 *
 * Controllers are polled at high speed using the asynchronous SPI driver from
 * PSn00bSDK's io/pads example ((C) spicyjpeg - MPL licensed), extended with
 * vibration motor support. Analog mode is forced automatically on DualShock
 * pads that identify as digital.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psxetc.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxpad.h>

#include "spi.h"

#define SCREEN_XRES 320
#define SCREEN_YRES 240

#define OT_LENGTH     16
#define BUFFER_LENGTH 24576

/* Number of frames the boot logo screen is displayed for. */

#define SPLASH_FRAMES 170

/* Primitive Z layers. Lower indices are drawn later, i.e. on top of higher
 * indices. All UI elements are assigned to one of these layers to guarantee
 * a consistent draw order regardless of how many primitives each one uses. */

#define Z_TEXT  2
#define Z_FILL  3
#define Z_FRAME 4
#define Z_PLOT  5
#define Z_BG    6

#define PRESSED(btn, mask) (!((btn) & (mask)))

/* Render context (double buffering, following the beginner/hello example) */

typedef struct {
	DISPENV  disp;
	DRAWENV  draw;
	uint32_t ot[OT_LENGTH];
	uint8_t  buffer[BUFFER_LENGTH];
} RenderBuffer;

typedef struct {
	RenderBuffer db[2];
	uint8_t      *next_packet;
	int          active_buffer;
} RenderContext;

static RenderContext ctx;

static void setup_context(int r, int g, int b) {
	SetDefDispEnv(&(ctx.db[0].disp), 0,            0, SCREEN_XRES, SCREEN_YRES);
	SetDefDrawEnv(&(ctx.db[0].draw), SCREEN_XRES,  0, SCREEN_XRES, SCREEN_YRES);
	SetDefDispEnv(&(ctx.db[1].disp), SCREEN_XRES,  0, SCREEN_XRES, SCREEN_YRES);
	SetDefDrawEnv(&(ctx.db[1].draw), 0,            0, SCREEN_XRES, SCREEN_YRES);

	setRGB0(&(ctx.db[0].draw), r, g, b);
	setRGB0(&(ctx.db[1].draw), r, g, b);
	ctx.db[0].draw.isbg = 1;
	ctx.db[1].draw.isbg = 1;

	ctx.active_buffer = 0;
	ctx.next_packet    = ctx.db[0].buffer;
	ClearOTagR(ctx.db[0].ot, OT_LENGTH);

	SetDispMask(1);
}

static void flip_buffers(void) {
	DrawSync(0);
	VSync(0);

	RenderBuffer *draw_buffer = &(ctx.db[ctx.active_buffer]);
	RenderBuffer *disp_buffer = &(ctx.db[ctx.active_buffer ^ 1]);

	PutDispEnv(&(disp_buffer->disp));
	DrawOTagEnv(&(draw_buffer->ot[OT_LENGTH - 1]), &(draw_buffer->draw));

	ctx.active_buffer ^= 1;
	ctx.next_packet    = disp_buffer->buffer;
	ClearOTagR(disp_buffer->ot, OT_LENGTH);
}

/* Pad state */

typedef struct {
	uint8_t  connected;
	uint8_t  type;
	uint16_t buttons;     // Raw response (active low: 0 = pressed)
	uint8_t  ls_x, ls_y;  // Left stick
	uint8_t  rs_x, rs_y;  // Right stick
} PadState;

/* Simple 2D drawing helpers */

static void *alloc_prim(uint32_t z, size_t size) {
	RenderBuffer *buffer = &(ctx.db[ctx.active_buffer]);
	uint8_t      *prim   = ctx.next_packet;

	addPrim(&(buffer->ot[z]), prim);
	ctx.next_packet += size;

	return (void *) prim;
}

static void draw_rect(int x, int y, int w, int h, int r, int g, int b, uint32_t z) {
	TILE *tile = (TILE *) alloc_prim(z, sizeof(TILE));

	setTile(tile);
	setXY0(tile, x, y);
	setWH(tile, w, h);
	setRGB0(tile, r, g, b);
}

/* Draws a 1 pixel thick rectangular frame (4 solid rectangles). */

static void draw_frame(int x, int y, int w, int h, int r, int g, int b, uint32_t z) {
	draw_rect(x,         y,         w, 1, r, g, b, z);
	draw_rect(x,         y + h - 1, w, 1, r, g, b, z);
	draw_rect(x,         y,         1, h, r, g, b, z);
	draw_rect(x + w - 1, y,         1, h, r, g, b, z);
}

static void text_out(int x, int y, const char *text) {
	RenderBuffer *buffer = &(ctx.db[ctx.active_buffer]);

	ctx.next_packet = (uint8_t *)
		FntSort(&(buffer->ot[Z_TEXT]), ctx.next_packet, x, y, text);
}

/* Integer square root, used to rasterize circles with horizontal spans. */

static int isqrt_i(int v) {
	int x = 0;

	while ((x + 1) * (x + 1) <= v)
		x++;

	return x;
}

/* Filled disc, drawn as one horizontal tile per scanline. */

static void fill_disc(int cx, int cy, int radius, int r, int g, int b, uint32_t z) {
	for (int dy = -radius; dy <= radius; dy++) {
		int w = isqrt_i(radius * radius - dy * dy);

		draw_rect(cx - w, cy + dy, w * 2 + 1, 1, r, g, b, z);
	}
}

/* Thin circle outline built from per-scanline span pairs. When "dots" is set,
 * every other scanline is skipped for a dotted "radar display" look. */

static void draw_ring(int cx, int cy, int radius, int r, int g, int b, uint32_t z, int dots) {
	for (int dy = -radius; dy <= radius; dy++) {
		int w;

		if (dots && (dy & 1))
			continue;

		w = isqrt_i(radius * radius - dy * dy);

		draw_rect(cx - w,     cy + dy, 2, 1, r, g, b, z);
		draw_rect(cx + w - 1, cy + dy, 2, 1, r, g, b, z);
	}
}

/* The debug font's texture page and palette live in libpsxgpu's font.c. They
 * are reused here to draw scaled up text: the 8x8 glyph cells are mapped onto
 * textured quads, which unlike sprites can be stretched to any size. */

extern uint16_t _font_tpage;
extern uint16_t _font_clut;

static void text_big(int x, int y, const char *text, int scale, int dim, uint32_t z) {
	int sx   = x;
	int size = 8 * scale;

	for (; *text != 0; text++, sx += size) {
		int c = *text;
		int i, u, v;

		if ((c >= 'a') && (c <= 'z'))
			c -= 32;

		i = c - ' ';

		if (i <= 0)
			continue;
		i--;

		u = (i % 16) * 8;
		v = (i / 16) * 8;

		POLY_FT4 *poly = (POLY_FT4 *) alloc_prim(z, sizeof(POLY_FT4));

		setPolyFT4(poly);
		setShadeTex(poly, 1);
		setSemiTrans(poly, dim);
		setXY4(poly, sx, y, sx + size, y, sx, y + size, sx + size, y + size);
		setUV4(poly, u, v, u + 8, v, u, v + 8, u + 8, v + 8);
		poly->clut  = _font_clut;
		poly->tpage = _font_tpage;
	}
}

/* Button indicator box with centered label. Lights up bright green while the
 * corresponding bit of the raw button mask (active low) is cleared. The idle
 * border color can be customized (used for the colored face buttons). */

static void draw_button(int x, int y, int w, int h, const char *label, int on, int br, int bg, int bb) {
	if (on) {
		draw_rect(x, y, w, h, 0, 170, 70, Z_PLOT);
		draw_frame(x, y, w, h, 120, 255, 150, Z_FRAME);
	} else {
		draw_rect(x, y, w, h, 22, 22, 36, Z_PLOT);
		draw_frame(x, y, w, h, br, bg, bb, Z_FRAME);
	}

	text_out(x + (w - (int) strlen(label) * 8) / 2, y + (h - 8) / 2, label);
}

/* Horizontal level bar used to display vibration motor states. */

static void draw_level_bar(int x, int y, int w, int h, int value, int busy) {
	int fill;

	draw_rect(x, y, w, h, 14, 14, 26, Z_PLOT);
	draw_frame(x, y, w, h, 84, 84, 120, Z_FRAME);

	if (!busy)
		return;

	fill = (w - 2) * value / 255;
	if (fill > 0)
		draw_rect(x + 1, y + 1, fill, h - 2, 255, 128, 0, Z_FILL);
}

/* Circular analog stick well with a cursor dot that follows the stick. The
 * ring lights up green while the stick is pressed down (L3/R3). */

static void draw_stick(int cx, int cy, const PadState *pad, int right_stick, int clicked) {
	char    buf[24];
	int     is_analog;
	uint8_t ax = 128, ay = 128;

	is_analog = pad->connected && (
		(pad->type == PAD_ID_ANALOG) ||
		(pad->type == PAD_ID_ANALOG_STICK)
	);

	if (is_analog) {
		ax = right_stick ? pad->rs_x : pad->ls_x;
		ay = right_stick ? pad->rs_y : pad->ls_y;
	}

	/* Well, ring, compass ticks and inner crosshair */

	fill_disc(cx, cy, 26, 10, 10, 20, Z_PLOT);
	draw_ring(
		cx, cy, 28,
		clicked ?   0 :  86,
		clicked ? 230 :  86,
		clicked ? 100 : 126,
		Z_FRAME, 0
	);

	draw_rect(cx -  1, cy - 34, 2, 6, 86, 86, 126, Z_FRAME);
	draw_rect(cx -  1, cy + 28, 2, 6, 86, 86, 126, Z_FRAME);
	draw_rect(cx - 34, cy -  1, 6, 2, 86, 86, 126, Z_FRAME);
	draw_rect(cx + 28, cy -  1, 6, 2, 86, 86, 126, Z_FRAME);

	/* Cursor: follows the stick, stays centered when there is no analog data.
	 * Within a layer the first primitive added ends up on top (addPrim
	 * inserts at the list head), so the cursor is added before the crosshair
	 * and the black shadow after the orange square to sit underneath it. */

	if (is_analog) {
		int dx = ((int) ax - 128) * 22 / 128;
		int dy = ((int) ay - 128) * 22 / 128;

		draw_rect(cx + dx - 3, cy + dy - 3, 7, 7, 255, 168, 0, Z_FILL);
		draw_rect(cx + dx - 2, cy + dy - 2, 9, 9, 0, 0, 0, Z_FILL);
	} else {
		draw_rect(cx - 2, cy - 2, 5, 5, 60, 60, 90, Z_FILL);
	}

	/* Crosshair (added after the cursor so it renders underneath it) */

	draw_rect(cx - 24, cy -  1, 48, 2, 30, 60, 40, Z_FILL);
	draw_rect(cx -  1, cy - 24,  2, 48, 30, 60, 40, Z_FILL);
	draw_rect(cx -  1, cy -  1,  2,  2, 90, 90, 130, Z_FRAME);

	/* Axis readout under the stick */

	if (is_analog)
		sprintf(buf, "X%3d Y%3d", ax, ay);
	else
		strcpy(buf, "X--- Y---");

	text_out(cx - (int) strlen(buf) * 4, cy + 29, buf);
}

/* Pad state */

static volatile PadState  pad_state[2];
static volatile uint8_t   pad_buff[2][34];
static volatile uint32_t  pad_config_attempt[2];

/* Just a wrapper around SPI_CreateRequest(). This does not send the command
 * immediately but adds it to the driver's request queue. */

static void send_pad_cmd(
	uint32_t     port,
	PadCommand   cmd,
	uint8_t      arg1,
	uint8_t      arg2,
	SPI_Callback callback
) {
	SPI_Request *req = SPI_CreateRequest();

	req->len              = 9;
	req->port             = port;
	req->callback         = callback;
	req->pad_req.addr     = 0x01;
	req->pad_req.cmd      = cmd;
	req->pad_req.tap_mode = 0x00;
	req->pad_req.motor_r  = arg1;
	req->pad_req.motor_l  = arg2;

	// The padding bytes must be 0xff when unlocking vibration motors.
	memset(
		req->pad_req.dummy,
		(cmd == PAD_CMD_REQUEST_CONFIG) ? 0xff : 0x00,
		4
	);
}

// Called once the pad has responded to a config mode request. If it really is
// a DualShock, force it into analog mode, unlock the rumble motors and enable
// longer responses.
static void dualshock_init_cb(uint32_t port, const volatile uint8_t *buff, size_t rx_len) {
	PadResponse *pad = (PadResponse *) buff;

	if (
		(rx_len < 2) ||
		(pad->prefix != 0x5a) ||
		(pad->type != PAD_ID_CONFIG_MODE)
	) {
		pad_config_attempt[port]++;
		return;
	}

	send_pad_cmd(port, PAD_CMD_CONFIG_MODE,     0x01, 0x00, 0);
	send_pad_cmd(port, PAD_CMD_SET_ANALOG,      0x01, 0x02, 0);
	send_pad_cmd(port, PAD_CMD_INIT_PRESSURE,   0x00, 0x00, 0); // Ignored by DualShock 1
	send_pad_cmd(port, PAD_CMD_REQUEST_CONFIG,  0x00, 0x01, 0);
	send_pad_cmd(port, PAD_CMD_RESPONSE_CONFIG, 0xff, 0xff, 0); // Ignored by DualShock 1
	send_pad_cmd(port, PAD_CMD_CONFIG_MODE,     0x00, 0x00, 0);
}

// Called by the SPI driver each time a poll request receives a response. Only
// the state of the first port is mirrored into pad_state[] for display.
static void poll_cb(uint32_t port, const volatile uint8_t *buff, size_t rx_len) {
	PadResponse *pad = (PadResponse *) buff;
	PadState    *st  = (PadState *) &pad_state[port];

	if (rx_len > 34)
		rx_len = 34;

	if (rx_len)
		memcpy((void *) pad_buff[port], (void *) buff, rx_len);

	if (!rx_len || (pad->type == PAD_ID_NONE && pad->len == 0xf)) {
		st->connected = 0;
		st->buttons   = 0xffff;
		pad_config_attempt[port] = 0;
		return;
	}

	st->connected = 1;
	st->type      = pad->type;

	if (rx_len >= 4)
		st->buttons = pad->btn;
	else
		st->buttons = 0xffff;

	if (pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK) {
		st->rs_x = pad->rs_x;
		st->rs_y = pad->rs_y;
		st->ls_x = pad->ls_x;
		st->ls_y = pad->ls_y;
	}

	// If this pad identifies as a digital pad, attempt to put it into analog
	// mode up to 3 times by entering configuration mode (this is how a real
	// DualShock in digital mode is detected). Once the attempt counter
	// exceeds the threshold, it is treated as digital-only.
	if (
		rx_len >= 4 &&
		((pad->prefix == 0x5a) || !(pad->prefix)) &&
		(pad->type == PAD_ID_DIGITAL)
	) {
		if (pad_config_attempt[port] < 3) {
			send_pad_cmd(port, PAD_CMD_CONFIG_MODE, 0x01, 0x00, 0);
			send_pad_cmd(port, PAD_CMD_READ,        0x00, 0x00, &dualshock_init_cb);
		}
	} else {
		pad_config_attempt[port] = 0;
	}
}

/* UI */

static const char *pad_type_short(uint8_t type, int connected) {
	static const char *const NAMES[16] = {
		"NON", "MOU", "NEG", "GUN", "DIG", "STK", "GCN", "ANA",
		"MUL", "?",   "?",   "?",   "?",   "?",   "JOG", "CFG"
	};

	if (!connected)
		return "NON";

	return NAMES[type & 0xf];
}

/* Short names for every button, in display order, used for the "BTN:" line. */

static const struct {
	uint16_t    mask;
	const char *name;
} BUTTON_LIST[] = {
	{ PAD_L1,       "L1"  }, { PAD_L2,       "L2"  },
	{ PAD_R1,       "R1"  }, { PAD_R2,       "R2"  },
	{ PAD_UP,       "UP"  }, { PAD_DOWN,     "DN"  },
	{ PAD_LEFT,     "LT"  }, { PAD_RIGHT,    "RT"  },
	{ PAD_TRIANGLE, "TR"  }, { PAD_SQUARE,   "SQ"  },
	{ PAD_CIRCLE,   "CI"  }, { PAD_CROSS,    "X"   },
	{ PAD_SELECT,   "SEL" }, { PAD_START,    "STR" },
	{ PAD_L3,       "L3"  }, { PAD_R3,       "R3"  },
};

/* Builds a "BTN: L1 X .." string (at most 12 characters wide). */

static void build_btn_list(uint16_t buttons, char *buf) {
	int len = 4;

	strcpy(buf, "BTN:");

	for (int i = 0; i < (int) (sizeof(BUTTON_LIST) / sizeof(BUTTON_LIST[0])); i++) {
		int n = (int) strlen(BUTTON_LIST[i].name);

		if (buttons & BUTTON_LIST[i].mask)
			continue; // Active low: bit set = released

		if ((len + n + 1) > 12) {
			strcat(buf, " ..");
			return;
		}

		buf[len++] = ' ';
		strcpy(buf + len, BUTTON_LIST[i].name);
		len += n;
	}
}

/* Main test screen. Layout follows classic pad-test utilities: shoulders,
 * d-pad and face buttons around a center column, analog stick wells and the
 * vibration motor bars along the bottom. */

static void draw_ui(const PadState pads[2], const PadState *ctl, uint8_t motor_l, uint8_t motor_r, uint32_t frame) {
	char     buf[40];
	uint16_t b1 = pads[0].connected ? pads[0].buttons : 0xffff;
	int      is_analog = ctl->connected && (
		(ctl->type == PAD_ID_ANALOG) ||
		(ctl->type == PAD_ID_ANALOG_STICK)
	);

	/* Header: logo mark, port status line and frame counter. Everything is
	 * kept inside a ~12px margin as CRT televisions crop the outer edges of
	 * the picture (overscan). */

	draw_rect(0, 12, 320, 14, 12, 12, 20, Z_BG);
	draw_rect(0, 25, 320, 1, 0, 150, 70, Z_FRAME);

	draw_rect( 6, 19, 3,  6,   0, 120,  60, Z_FILL);
	draw_rect(11, 16, 3,  9,   0, 170,  80, Z_FILL);
	draw_rect(16, 13, 3, 12,   0, 230, 100, Z_FILL);

	text_out(24, 15, "TEST DIAG");

	sprintf(
		buf,
		"P1:%s P2:%s",
		pad_type_short(pads[0].type, pads[0].connected),
		pad_type_short(pads[1].type, pads[1].connected)
	);
	text_out(112, 15, buf);

	sprintf(buf, "FRT:%5d", (int) frame);
	text_out(314 - (int) strlen(buf) * 8, 15, buf);

	/* Shoulder buttons */

	draw_button(  6, 30, 72, 12, "L2", PRESSED(b1, PAD_L2), 84, 84, 120);
	draw_button(  6, 44, 72, 12, "L1", PRESSED(b1, PAD_L1), 84, 84, 120);
	draw_button(242, 30, 72, 12, "R2", PRESSED(b1, PAD_R2), 84, 84, 120);
	draw_button(242, 44, 72, 12, "R1", PRESSED(b1, PAD_R1), 84, 84, 120);

	/* Directional pad */

	draw_button(42,  68, 24, 24, "^", PRESSED(b1, PAD_UP),    84,  84, 120);
	draw_button(18,  92, 24, 24, "<", PRESSED(b1, PAD_LEFT),  84,  84, 120);
	draw_button(66,  92, 24, 24, ">", PRESSED(b1, PAD_RIGHT), 84,  84, 120);
	draw_button(42, 116, 24, 24, "v", PRESSED(b1, PAD_DOWN),  84,  84, 120);

	/* Face buttons (diamond layout, PS-style colored borders) */

	draw_button(254,  68, 24, 24, "/\\", PRESSED(b1, PAD_TRIANGLE),   0, 110,  50);
	draw_button(230,  92, 24, 24, "[]", PRESSED(b1, PAD_SQUARE),   130,  50, 100);
	draw_button(278,  92, 24, 24, "O",  PRESSED(b1, PAD_CIRCLE),   130,  35,  35);
	draw_button(254, 116, 24, 24, "X",  PRESSED(b1, PAD_CROSS),     40,  70, 140);

	/* Start and select, center column */

	draw_button(108, 84, 48, 13, "SELECT", PRESSED(b1, PAD_SELECT), 84, 84, 120);
	draw_button(164, 84, 48, 13, "START",  PRESSED(b1, PAD_START),  84, 84, 120);

	/* Currently pressed buttons and analog mode state */

	build_btn_list(b1, buf);
	text_out(112, 102, buf);
	text_out(112, 114, is_analog ? "ANA: ON" : "ANA: --");

	/* Analog stick wells (L3/R3 light up their rings when clicked) */

	draw_stick(84,  178, ctl, 0, PRESSED(b1, PAD_L3));
	draw_stick(236, 178, ctl, 1, PRESSED(b1, PAD_R3));

	/* Vibration motor indicator bars */

	text_out( 14, 220, "BIG");
	draw_level_bar( 46, 218, 102, 10, motor_l, ctl->connected);

	text_out(174, 220, "SML");
	draw_level_bar(206, 218, 102, 10, motor_r, ctl->connected);
}

/* Boot logo screen, shown for SPLASH_FRAMES frames while the disc image is
 * still settling: technical grid, radar rings, the TEST DIAG wordmark (drawn
 * with a stretched version of the debug font) and a loading bar. */

static void draw_splash(uint32_t frame) {
	/* Technical background grid and crosshair */

	for (int x = 0; x < SCREEN_XRES; x += 32)
		draw_rect(x, 0, 1, SCREEN_YRES, 0, 44, 22, Z_BG);
	for (int y = 0; y < SCREEN_YRES; y += 32)
		draw_rect(0, y, SCREEN_XRES, 1, 0, 44, 22, Z_BG);

	draw_rect(0, 119, SCREEN_XRES, 2, 0, 110, 55, Z_FRAME);
	draw_rect(159, 0, 2, SCREEN_YRES, 0, 110, 55, Z_FRAME);

	/* Radar rings with a few "target blips" on both sides */

	draw_ring( 48, 120, 44, 0, 80, 40, Z_FRAME, 1);
	draw_ring( 48, 120, 26, 0, 80, 40, Z_FRAME, 1);
	draw_ring(272, 120, 44, 0, 80, 40, Z_FRAME, 1);
	draw_ring(272, 120, 26, 0, 80, 40, Z_FRAME, 1);

	draw_rect( 79,  89, 3, 3, 0, 220, 90, Z_FILL);
	draw_rect( 28, 134, 3, 3, 0, 220, 90, Z_FILL);
	draw_rect(238,  92, 3, 3, 0, 220, 90, Z_FILL);
	draw_rect(286, 142, 3, 3, 0, 220, 90, Z_FILL);

	/* Wordmark with drop shadow and accent underlines */

	text_big( 52, 58, "TEST DIAG", 3, 0, Z_FILL);
	text_big( 54, 60, "TEST DIAG", 3, 1, Z_FILL);

	draw_rect( 52, 88, 216, 3, 0, 200, 200, Z_FRAME);
	draw_rect(120, 93,  80, 2, 0, 220,  90, Z_FRAME);

	text_big(104, 102, "FSmaker", 2, 0, Z_FILL);
	text_out( 68, 128, "CONTROLLER TEST UTILITY");

	/* Loading bar (kept away from the screen edges for CRT overscan) */

	draw_frame(60, 200, 200, 10, 84, 84, 120, Z_FRAME);
	draw_rect(62, 202, (int) (frame * 196 / (SPLASH_FRAMES - 1)), 6, 0, 200, 90, Z_FILL);

	text_out(60, 218, "FSmaker v1.0  -  PSn00bSDK");
}

/* Main */

int main(int argc, const char **argv) {
	(void) argc;
	(void) argv;

	// Initialize the GPU and load the debug font into VRAM.
	ResetGraph(0);
	FntLoad(960, 0);

	setup_context(3, 3, 8);

	// Show the boot logo screen before starting the controller driver.
	for (uint32_t f = 0; f < SPLASH_FRAMES; f++) {
		draw_splash(f);
		flip_buffers();
	}

	// Install the SPI polling driver. Controllers on both ports are polled at
	// 250 Hz in the background.
	SPI_Init(&poll_cb);

	uint32_t rumble_phase = 0;
	uint32_t frame        = 0;

	while (1) {
		PadState p[2];

		// Take a consistent snapshot of the state of both pads, then release
		// interrupts to minimize interference with the poll timer.
		EnterCriticalSection();
		memcpy(&p[0], (const void *) &pad_state[0], sizeof(PadState));
		memcpy(&p[1], (const void *) &pad_state[1], sizeof(PadState));
		ExitCriticalSection();

		const PadState *ctl = &p[0];

		if (!ctl->connected && p[1].connected)
			ctl = &p[1];

		// Rumble test: holding L2 drives the big motor through a sweep,
		// holding R2 turns the small motor on.
		uint8_t motor_l = 0;
		uint8_t motor_r = 0;

		if (ctl->connected) {
			if (!(ctl->buttons & PAD_L2)) {
				uint32_t phase = (rumble_phase += 4) & 0x1ff;

				motor_l = (uint8_t) (phase < 256 ? phase : 511 - phase);
			} else {
				rumble_phase = 0;
			}

			if (!(ctl->buttons & PAD_R2))
				motor_r = 0x01;
		}

		spi_motor_left  = motor_l;
		spi_motor_right = motor_r;

		draw_ui(p, ctl, motor_l, motor_r, frame);
		flip_buffers();

		frame++;
	}

	return 0;
}
