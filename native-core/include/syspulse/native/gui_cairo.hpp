// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Cairo is a native system drawing library, not a GUI framework. Use its
// installed headers where available; the fallback below declares only the
// stable public C ABI used here, for minimal runtime-only build environments.
#if defined(SYSPULSE_GUI_CAIRO)
#if __has_include(<cairo/cairo-xlib.h>)
#include <cairo/cairo-xlib.h>
#else
#include <X11/Xlib.h>
extern "C" {
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef int cairo_status_t;
inline constexpr cairo_status_t CAIRO_STATUS_SUCCESS=0;
typedef enum _cairo_font_slant { CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_SLANT_OBLIQUE } cairo_font_slant_t;
typedef enum _cairo_font_weight { CAIRO_FONT_WEIGHT_NORMAL, CAIRO_FONT_WEIGHT_BOLD } cairo_font_weight_t;
typedef struct { double x_bearing,y_bearing,width,height,x_advance,y_advance; } cairo_text_extents_t;
cairo_surface_t* cairo_xlib_surface_create(Display*,Drawable,Visual*,int,int);
cairo_status_t cairo_surface_status(cairo_surface_t*);
void cairo_surface_destroy(cairo_surface_t*);
void cairo_surface_flush(cairo_surface_t*);
cairo_t* cairo_create(cairo_surface_t*);
void cairo_destroy(cairo_t*);
cairo_status_t cairo_status(cairo_t*);
void cairo_set_source_rgb(cairo_t*,double,double,double);
void cairo_new_path(cairo_t*);
void cairo_rectangle(cairo_t*,double,double,double,double);
void cairo_arc(cairo_t*,double,double,double,double,double);
void cairo_close_path(cairo_t*);
void cairo_fill(cairo_t*);
void cairo_stroke(cairo_t*);
void cairo_set_line_width(cairo_t*,double);
void cairo_move_to(cairo_t*,double,double);
void cairo_line_to(cairo_t*,double,double);
void cairo_select_font_face(cairo_t*,const char*,cairo_font_slant_t,cairo_font_weight_t);
void cairo_set_font_size(cairo_t*,double);
void cairo_text_extents(cairo_t*,const char*,cairo_text_extents_t*);
void cairo_show_text(cairo_t*,const char*);
}
#endif
#endif
