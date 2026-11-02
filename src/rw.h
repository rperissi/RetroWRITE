/*
** RetroWrite II (graphics edition) - shared definitions
*/
#ifndef RW_H
#define RW_H

#include "gfx.h"
#include "ll.h"

#define PEEK(a)    (*(volatile unsigned char *)(a))
#define POKE(a, v) (*(volatile unsigned char *)(a) = (v))
#define KBD      0xC000
#define KBDSTRB  0xC010
#define OAPPLE   0xC061
#define SAPPLE   0xC062

/* document text: aux $4000-$B6FF as a gap buffer */
#define TBASE   0x4000u
#define CAP     0x7700u         /* aux $B700-$BFFF holds the font bitmaps */
#define NOLINE  0xFFFFu
#define CR      0x0D
#define LF      0x0A
#define TAB     0x09

/* format byte (stored in the text as $80 | fmt) */
#define F_FONT  0x03
#define F_SIZE  0x0C
#define F_BOLD  0x10
#define F_ITAL  0x20
#define F_UND   0x40
#define ISMARK(c) ((c) & 0x80)

/* screen layout */
#define MENUH   11              /* menu bar rows 0..10 (row 10 = line) */
#define RULERH  26              /* ruler rows below the menu bar */
#define BOTY    192
#define LEFTX   12              /* text left edge (pixels) */
#define WRAPX   524             /* text must end before this x */
#define TC0     1               /* byte columns presented for text */
#define TC1     76
#define SBX     539             /* scroll bar left edge */

#define MAXL    24

/* keys */
#define K_LEFT  0x08
#define K_RIGHT 0x15
#define K_UP    0x0B
#define K_DOWN  0x0A
#define K_RET   0x0D
#define K_ESC   0x1B
#define K_DEL   0x7F
#define K_TAB   0x09

/* special glyphs in the fonts */
#define G_OAPPLE  0x7F
#define G_APPLE   0x80
#define G_CHECK   0x81
#define G_FOLDER  0x82
#define G_DOC     0x83

/* overlays: one region, loaded from RW2.OVL1..3 */
#define OVLADDR 0xA190u
#define OVLSIZE (0xBF00u - OVLADDR)
#define OV_FILE  1
#define OV_SPELL 2
#define OV_MENU  3
#define OV_EDIT  4
#define OV_PRINT 5
#define OV_ACC   6
#define OV_DLG   7
#define OV_ODTOUT 8
#define OV_ODTIN  9
#define OV_ODTRD  10
#define OV_ODTZIP 11
#define OV_NET    12          /* CoGS Share: send / open over the card */
#define FT_ODT   0xD0           /* pseudo file type: a name ending in .ODT */
unsigned char odt_content(void); /* overlay 8: the text -> RW2.XML2 */
unsigned char odt_save(void);   /* overlay 11: RW2.XML2 + styles -> pathbuf as ODT */
unsigned char odt_unzip(void);  /* overlay 9: pathbuf -> RW2.XML1/2 */
unsigned char odt_read(void);   /* overlay 10: RW2.XML1/2 -> document */
extern unsigned char iob_limit;         /* open files allowed: 2 borrows $0C00 */
extern unsigned char upool_loaded;
unsigned char __fastcall__ load_ovl(unsigned char n);
void __fastcall__ edit_command(unsigned char c);
void __fastcall__ menu_command(unsigned char c);
unsigned char __fastcall__ print_command(unsigned char c);
/* printing (settings are resident so Startup Preferences can save them) */
extern unsigned char prt_type, prt_slot, prt_quality, prt_pnum;
extern char doc_head[40], doc_foot[40];
extern unsigned char def_fmt;
void basic_launch(void);
unsigned char __fastcall__ acc_command(unsigned char c);
unsigned char __fastcall__ net_command(unsigned char c);   /* overlay 12; 1 = pathbuf/ftype hold a file to load */
unsigned char __fastcall__ file_op(unsigned char op);
unsigned char __fastcall__ file_dialog(unsigned char mode);
extern unsigned char ftype;
/* windows: hidden documents are swapped to RW2.WINn files */
extern unsigned char cur_doc, wlive, wmod, fo_arg;
extern char wname[4][16];
enum { FO_NEW = 1, FO_LOAD, FO_SAVETO, FO_SAVETEXT, FO_TEXTOK, FO_SAVE, FO_UDICT, FO_GLOSS, FO_TUTOR,
       FO_SWAPOUT, FO_SWAPIN, FO_CLOSEWIN };
unsigned char __fastcall__ menu_run(unsigned char how);

/* line engine (gfx.s) */
extern unsigned char lm_n, lm_s0, lm_s1, lm_i, lm_nf, lm_asc, lm_desc, lm_cr;
extern unsigned char lm_pa, lm_np, lm_ns, lm_jn, lm_jx, lm_jr;
extern unsigned lm_w, wrapx, tabbase, tabs[8];
extern unsigned lm_tx, lm_x0, lm_x1, lbx;
unsigned char __fastcall__ line_engine(unsigned char mode);
void __fastcall__ lb_cmds(const unsigned char *list);

/* buffers in low memory */
#define TB      ((unsigned char *)0x1E00)   /* 256-byte text scratch */
#define MAXENT  48                          /* file dialog entries (text page) */
#define UPOOL   1024                        /* learned words pool, at $0C00 */

extern unsigned gs, ge, tlen, cur, anc, top;
extern unsigned char topf, topp, modified, tf;
/* document ruler: margins and tab stops in pixels from LEFTX (80 per inch) */
extern unsigned lmargin, rmargin, dtabs[8];
extern unsigned char text_top, ruler_on, ruler_dirty;
void ruler_apply(void);
void draw_ruler(void);
unsigned char caret_pa(void);
void ruler_click(void);
unsigned __fastcall__ para_start(unsigned p);
extern char docpath[65];
extern unsigned char doctype;           /* 0 = RetroWrite document, 1 = text file, 2 = ODT */

/* fixed buffers in low main memory */
#define iob     ((unsigned char *)0x1000)  /* 512-byte I/O buffer; C stack is $1200-$13FF */
#define pathbuf ((char *)0x1F00)           /* 80 */
#define tmppath ((char *)0x1F50)           /* 80 */
#define findstr ((char *)0x1FA0)           /* 32 */
#define replstr ((char *)0x1FC0)           /* 32 */
extern char progdir[65], browsedir[65];
extern unsigned char key, oa, quit_now;
unsigned sel_lo(void);
unsigned sel_hi(void);
unsigned __fastcall__ skip_fwd(unsigned p);
unsigned char __fastcall__ isword(unsigned char c);
unsigned char delete_selection(void);
void __fastcall__ sel_changed(unsigned olo, unsigned ohi);
extern unsigned char layout_ok, topp;
void __fastcall__ delete_range(unsigned p, unsigned n);
void __fastcall__ insert_char(unsigned char c);
void __fastcall__ after_move(unsigned olo, unsigned ohi, unsigned char keepgoal);
unsigned __fastcall__ phys(unsigned p);
void __fastcall__ clear_rows(unsigned char y, unsigned char y1, unsigned char c0, unsigned char c1);

/* text */
unsigned char __fastcall__ tch(unsigned p);
void __fastcall__ fetch(unsigned p, unsigned char *buf, unsigned n);
unsigned char __fastcall__ raw_insert(unsigned p, const unsigned char *s, unsigned n);
void __fastcall__ raw_delete(unsigned p, unsigned n);
void text_clear(void);
unsigned char __fastcall__ fmt_at(unsigned p);

/* screen */
void redraw_all(void);
void draw_menubar(void);
void __fastcall__ ui_text(unsigned x, unsigned char base, unsigned char fmt, const char *s);
unsigned __fastcall__ text_w(unsigned char fmt, const char *s);
void __fastcall__ frame_rect(unsigned x0, unsigned x1, unsigned char r0, unsigned char r1);
void __fastcall__ draw_button(unsigned x0, unsigned x1, unsigned char y, const char *label, unsigned char dflt);
unsigned char __fastcall__ alert(const char *msg, const char *b1, const char *b2, const char *b3);
unsigned char __fastcall__ ask_text(const char *prompt, char *buf, unsigned char max);
void __fastcall__ show_msg(const char *s);
void scroll_to_caret(void);
void __fastcall__ scroll_into(unsigned char ybot);
void set_all_dirty(void);
void refresh(void);
void invalidate_layout(void);
void __fastcall__ disp_shift(unsigned p, int delta);
void text_moved(void);
void __fastcall__ draw_box(unsigned char c0, unsigned char c1, unsigned char y0, unsigned char y1);
extern unsigned bx0[], bx1[];
extern unsigned char by[], nbtn;
extern const char *blabel[];
unsigned char hit_button(void);
unsigned char __fastcall__ run_buttons(unsigned char extra_keys);
void caret_hide(void);
void __fastcall__ set_style(unsigned char kind, unsigned char val);
unsigned char wait_event(void);
void wait_up(void);
void beep(void);

/* commands provided by modules */
unsigned char confirm(void);
void cmd_find(unsigned char replace);
void find_next(void);
void cmd_spell(void);
void about(void);
void help(void);
unsigned char __fastcall__ load_file(const char *path, unsigned char type);
unsigned char __fastcall__ file_exists(const char *path);

/* commands (grouped by the overlay that carries them out) */
enum {
    CM_NONE,
    /* file overlay */
    CM_NEW, CM_OPEN, CM_CLOSE, CM_SAVE, CM_SAVEAS, CM_SAVETEXT, CM_QUIT,
    CM_UDICT, CM_GLOSSEDIT, CM_TUTOR, CM_WIN1, CM_WIN2, CM_WIN3, CM_WIN4,
    /* spelling overlay */
    CM_SPELL,
    /* menu overlay */
    CM_ABOUT, CM_HELP, CM_RULER, CM_ALEFT, CM_ACENTER, CM_ARIGHT, CM_AJUST,
    CM_SP1, CM_SP15, CM_SP2,
    /* edit overlay */
    CM_CUT, CM_COPY, CM_PASTE, CM_FIND, CM_FINDNEXT, CM_REPLACE, CM_GLOSSARY,
    CM_SANS, CM_SERIF, CM_MONO,
    CM_S9, CM_S18, CM_S27,
    CM_PLAIN, CM_BOLD, CM_ITALIC, CM_UNDER, CM_SUPER, CM_SUB,
    CM_CLEAR, CM_TOP, CM_END, CM_WLEFT, CM_WRIGHT, CM_DELFWD, CM_PGBREAK,
    /* print overlay */
    CM_PSETUP, CM_PRINT, CM_PRINTMANY, CM_PRINTNOW,
    /* accessories overlay */
    CM_PREFS, CM_DOSLOAD, CM_COMM, CM_BASIC,
    /* CoGS Share overlay */
    CM_NET,
    /* resident */
    CM_SELALL, CM_PGUP, CM_PGDN,
    CM_SEP = 0xFE, CM_TITLE = 0xFD
};
#define CM_FILE_LAST  CM_WIN4
#define CM_MENU_FIRST CM_ABOUT
#define CM_EDIT_FIRST CM_CUT
#define CM_PRINT_FIRST CM_PSETUP
#define CM_ACC_FIRST  CM_PREFS
#define CM_RES_FIRST  CM_SELALL

#endif
