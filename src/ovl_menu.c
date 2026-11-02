/*
** RetroWrite II - overlay 3: menus, ruler, about, help
*/
#pragma code-name (push, "OVL3")
#pragma rodata-name (push, "OVL3")
#pragma data-name (push, "OVL3")
#pragma bss-name (push, "OVL3BSS")

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <apple2enh.h>
#include "rw.h"

/* ---------------------------------------------------------------------- */

static const char about_msg[] =
    "RetroWrite II   version 2.2\n"
    "A word processor for the enhanced Apple IIe\n"
    "Fonts: Sans, Serif and Mono, three sizes,\n"
    "styles, ruler, printing, glossary and ODT.\n"
    "6502 Retro Computing";

void about(void)
{
    alert(about_msg, "OK", 0, 0);
}

static const char help_msg[] =
    "Esc opens the menus. Click and drag to select.\n"
    "\x7F-X/C/V cut, copy, paste   \x7F-B/I/U styles\n"
    "\x7F-arrows page and word moves   \x7F-1/9 top/end\n"
    "\x7F-F find   \x7F-G again   \x7F-R replace   \x7F-K spell";

void help(void)
{
    alert(help_msg, "OK", 0, 0);
}

/* ---------------------------------------------------------------------- */
/* Menus (moved here from the resident part to save memory)                */

struct mitem {
    const char *label;
    unsigned char cmd;
    unsigned char key;
};

static const struct mitem m_apple[] = {
    { "About RetroWrite II...", CM_ABOUT, 0 },
    { "Keyboard Help", CM_HELP, '?' },
    { "Mouse Tutorial", CM_TUTOR, 0 },
    { 0, CM_SEP, 0 },
    { "Startup Preferences...", CM_PREFS, 0 },
    { 0, CM_SEP, 0 },
    { "Load DOS 3.3 File...", CM_DOSLOAD, 0 },
    { "Communications...", CM_COMM, 0 },
    { "CoGS Share...", CM_NET, 0 },
    { "BASIC.SYSTEM", CM_BASIC, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_file[] = {
    { 0, CM_TITLE, 0 },
    { 0, CM_SEP, 0 },
    { "New", CM_NEW, 'N' },
    { "Open...", CM_OPEN, 'O' },
    { "Close", CM_CLOSE, 0 },
    { "Save", CM_SAVE, 'S' },
    { "Save As...", CM_SAVEAS, 'A' },
    { "Save As Text...", CM_SAVETEXT, 0 },
    { 0, CM_SEP, 0 },
    { "Page Setup...", CM_PSETUP, 0 },
    { "Print...", CM_PRINT, 'P' },
    { "Print Several...", CM_PRINTMANY, 0 },
    { 0, CM_SEP, 0 },
    { "Quit", CM_QUIT, 'Q' },
    { 0, 0, 0 }
};
static const struct mitem m_edit[] = {
    { "Cut", CM_CUT, 'X' },
    { "Copy", CM_COPY, 'C' },
    { "Paste", CM_PASTE, 'V' },
    { "Clear", CM_CLEAR, 0 },
    { 0, CM_SEP, 0 },
    { "Select All", CM_SELALL, 'W' },
    { 0, CM_SEP, 0 },
    { "Glossary  (Ctrl-G)", CM_GLOSSARY, 0 },
    { "Edit Glossary", CM_GLOSSEDIT, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_search[] = {
    { "Find...", CM_FIND, 'F' },
    { "Find Next", CM_FINDNEXT, 'G' },
    { "Replace...", CM_REPLACE, 'R' },
    { 0, CM_SEP, 0 },
    { "Check Spelling", CM_SPELL, 'K' },
    { "Edit User Dictionary", CM_UDICT, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_format[] = {
    { "Show Ruler", CM_RULER, 'H' },
    { 0, CM_SEP, 0 },
    { "Align Left", CM_ALEFT, 'L' },
    { "Center", CM_ACENTER, 'E' },
    { "Align Right", CM_ARIGHT, 0 },
    { "Justify", CM_AJUST, 'J' },
    { 0, CM_SEP, 0 },
    { "Single Space", CM_SP1, 0 },
    { "1 1/2 Space", CM_SP15, 0 },
    { "Double Space", CM_SP2, 0 },
    { 0, CM_SEP, 0 },
    { "Insert Page Break", CM_PGBREAK, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_font[] = {
    { "Sans", CM_SANS, 0 },
    { "Serif", CM_SERIF, 0 },
    { "Mono", CM_MONO, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_size[] = {
    { "9 Point", CM_S9, 0 },
    { "18 Point", CM_S18, 0 },
    { "27 Point", CM_S27, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_style[] = {
    { "Plain Text", CM_PLAIN, 'T' },
    { "Bold", CM_BOLD, 'B' },
    { "Italic", CM_ITALIC, 'I' },
    { "Underline", CM_UNDER, 'U' },
    { "Superscript", CM_SUPER, 0 },
    { "Subscript", CM_SUB, 0 },
    { 0, 0, 0 }
};
static const struct mitem m_windows[] = {
    { 0, CM_WIN1, 0 },
    { 0, CM_WIN2, 0 },
    { 0, CM_WIN3, 0 },
    { 0, CM_WIN4, 0 },
    { 0, 0, 0 }
};

#define NMENU 9
static const struct mitem *const menus[NMENU] = {
    m_apple, m_file, m_edit, m_search, m_format, m_font, m_size, m_style, m_windows
};
extern const char *const mtitle[NMENU];
extern unsigned mtx[NMENU + 1];

static const char *docname(void);

/* label of an item: window items are named after their documents */
static const char *__fastcall__ item_label(const struct mitem *it)
{
    unsigned char k;
    if (it->cmd >= CM_WIN1 && it->cmd <= CM_WIN4) {
        k = it->cmd - CM_WIN1;
        if (!(wlive & (1 << k))) return "";
        return k == cur_doc ? docname() : wname[k];
    }
    return it->label;
}

static void __fastcall__ hilite_title(unsigned char m)
{
    unsigned char i;
    lb_clear(MENUH);
    lb_fill(0, 560, 10, 10, LB_SET);
    for (i = 0; i < NMENU; ++i) ui_text(mtx[i], 7, F_BOLD, mtitle[i]);
    if (m < NMENU) lb_fill(mtx[m] - 7, mtx[m + 1] - 7, 0, 9, LB_XOR);
    present(0, MENUH, 0, 79);
}

static unsigned char fmt_sel(void)
{
    return (cur != anc) ? fmt_at(skip_fwd(sel_lo()) + 1) : tf;
}

static unsigned char __fastcall__ item_checked(unsigned char cmd)
{
    unsigned char f = fmt_sel(), pa = caret_pa();
    if (cmd >= CM_SANS && cmd <= CM_MONO) return (f & F_FONT) == cmd - CM_SANS;
    if (cmd >= CM_S9 && cmd <= CM_S27) return ((f & F_SIZE) >> 2) == cmd - CM_S9;
    if (cmd >= CM_ALEFT && cmd <= CM_AJUST) return ((pa >> 2) & 3) == cmd - CM_ALEFT;
    if (cmd >= CM_SP1 && cmd <= CM_SP2) return ((pa >> 4) & 3) == cmd - CM_SP1;
    if (cmd >= CM_WIN1 && cmd <= CM_WIN4) return cmd - CM_WIN1 == cur_doc;
    switch (cmd) {
    case CM_PLAIN: return !(f & (F_BOLD | F_ITAL | F_UND));
    case CM_SUPER: return (f & (F_SIZE | F_BOLD)) == F_SIZE;
    case CM_SUB: return (f & (F_SIZE | F_BOLD)) == (F_SIZE | F_BOLD);
    case CM_BOLD: return (f & (F_SIZE | F_BOLD)) == F_BOLD || (f & (F_SIZE | F_BOLD)) == (F_BOLD | 4) || (f & (F_SIZE | F_BOLD)) == (F_BOLD | 8);
    case CM_ITALIC: return f & F_ITAL;
    case CM_UNDER: return f & F_UND;
    case CM_RULER: return ruler_on;
    }
    return 0;
}

static unsigned char __fastcall__ item_enabled(unsigned char cmd)
{
    if (cmd >= CM_TITLE) return 0;
    if (cmd == CM_CUT || cmd == CM_COPY || cmd == CM_CLEAR) return cur != anc;
    if (cmd >= CM_WIN1 && cmd <= CM_WIN4) return (wlive >> (cmd - CM_WIN1)) & 1;
    return 1;
}

static unsigned char dd_c0, dd_c1, dd_n, dd_m;
static unsigned dd_x0, dd_x1;
static unsigned char dd_y[20];

static const char *base_name(const char *p)
{
    const char *b = p;
    while (*p) { if (*p == '/') b = p + 1; ++p; }
    return b;
}

static const char *docname(void)
{
    return docpath[0] ? base_name(docpath) : "Untitled";
}

static void __fastcall__ dd_geometry(unsigned char m)
{
    const struct mitem *it = menus[m];
    unsigned w = 0, t;
    unsigned char i, y = MENUH;
    dd_m = m;
    for (i = 0; it[i].cmd; ++i) {
        if (it[i].cmd >= CM_WIN1 && it[i].cmd <= CM_WIN4 && !item_label(&it[i])[0]) break;
        dd_y[i] = y;
        y += (it[i].cmd == CM_SEP) ? 5 : 11;
        t = item_label(&it[i]) ? text_w(0, item_label(&it[i])) : (it[i].cmd == CM_TITLE ? text_w(F_BOLD, docname()) : 0);
        if (t > w) w = t;
    }
    dd_y[i] = y;
    dd_n = i;
    w += 56;
    dd_c0 = (mtx[m] - 7) / 7;
    dd_c1 = dd_c0 + (unsigned char)((w + 6) / 7);
    if (dd_c1 > 78) { dd_c0 -= dd_c1 - 78; dd_c1 = 78; }
    dd_x0 = dd_c0 * 7;
    dd_x1 = dd_c1 * 7 + 7;
}

static void __fastcall__ draw_item(unsigned char i, unsigned char sel)
{
    const struct mitem *it = &menus[dd_m][i];
    unsigned char h = dd_y[i + 1] - dd_y[i], last = (i + 1 == dd_n);
    static char sc[3];
    lb_clear(h + 3);
    lb_fill(dd_x0, dd_x0 + 2, 0, h - 1, LB_SET);
    lb_fill(dd_x1 - 4, dd_x1 - 2, 0, h - 1, LB_SET);
    lb_fill(dd_x1 - 2, dd_x1, i ? 0 : 2, h - 1 + (last ? 3 : 0), LB_SET);
    if (last) {                         /* bottom edge and shadow */
        lb_fill(dd_x0, dd_x1 - 2, h, h, LB_SET);
        lb_fill(dd_x0 + 4, dd_x1, h + 1, h + 2, LB_SET);
    }
    if (it->cmd == CM_SEP) {
        lb_fill(dd_x0 + 2, dd_x1 - 4, 2, 2, LB_GRAY);
    } else if (it->cmd == CM_TITLE) {
        ui_text(dd_x0 + 8, 8, F_BOLD, docname());
    } else {
        if (item_checked(it->cmd)) ui_text(dd_x0 + 5, 8, 0, "\x81");
        ui_text(dd_x0 + 16, 8, 0, item_label(it));
        if (it->key) {
            sc[0] = G_OAPPLE;
            sc[1] = it->key;
            ui_text(dd_x1 - 30, 8, 0, sc);
        }
        if (!item_enabled(it->cmd)) lb_fill(dd_x0 + 3, dd_x1 - 4, 0, h - 1, LB_DIM);
        else if (sel) lb_fill(dd_x0 + 2, dd_x1 - 4, 0, h - 1, LB_XOR);
    }
    present(dd_y[i], h + (last ? 3 : 0), dd_c0, dd_c1);
}

static void __fastcall__ open_menu(unsigned char m)
{
    unsigned char i;
    ptr_hide();
    dd_geometry(m);
    scr_save(dd_c0, dd_c1, MENUH, dd_y[dd_n] + 2);
    hilite_title(m);
    for (i = 0; i < dd_n; ++i) draw_item(i, 0);
}

static void close_menu(void)
{
    ptr_hide();
    scr_restore();
    draw_menubar();
}

static unsigned char __fastcall__ item_at(unsigned char y)
{
    unsigned char i;
    if (mx < dd_x0 || mx >= dd_x1) return 0xFF;
    for (i = 0; i < dd_n; ++i)
        if (y >= dd_y[i] && y < dd_y[i + 1])
            return item_enabled(menus[dd_m][i].cmd) ? i : 0xFF;
    return 0xFF;
}

static unsigned char __fastcall__ title_at(unsigned x)
{
    unsigned char i;
    for (i = 0; i < NMENU; ++i)
        if (x >= mtx[i] - 7 && x < mtx[i + 1] - 7) return i;
    return 0xFF;
}

/* press on the bar, drag, release on an item; releasing on the title keeps
   the menu open until the next click */
static unsigned char menu_mouse(void)
{
    unsigned char m, nm, it = 0xFF, ni, sticky = 0, cmd = CM_NONE, oy, moved = 1;
    unsigned ox;
    m = title_at(mx);
    if (m == 0xFF) { wait_up(); return CM_NONE; }
    open_menu(m);
    for (;;) {
        ox = mx;
        oy = my;
        mouse_poll();
        if (mx != ox || my != oy) moved = 1;
        if (PEEK(KBD) & 0x80) { POKE(KBDSTRB, 0); it = 0xFF; break; }
        if (!mbtn) {
            if (!sticky) {              /* released: on the title it stays open */
                if (my < MENUH && title_at(mx) == m) sticky = 1;
                else break;
            }
        } else if (sticky) {            /* a click while the menu stays open */
            wait_up();
            if (my >= MENUH) { it = item_at(my); break; }
            sticky = 0;
            nm = title_at(mx);
            if (nm == m || nm == 0xFF) { it = 0xFF; break; }
            moved = 1;
        }
        if (!moved) continue;
        moved = 0;
        ptr_hide();                     /* follow the pointer (dragging or not) */
        if (my < MENUH) {
            nm = title_at(mx);
            if (nm != 0xFF && nm != m) {
                scr_restore();
                open_menu(m = nm);
                it = 0xFF;
            } else if (it != 0xFF) {
                draw_item(it, 0);
                it = 0xFF;
            }
        } else {
            ni = item_at(my);
            if (ni != it) {
                if (it != 0xFF) draw_item(it, 0);
                if (ni != 0xFF) draw_item(ni, 1);
                it = ni;
            }
        }
        ptr_show();
    }
    if (it != 0xFF) {
        cmd = menus[m][it].cmd;
        for (ni = 0; ni < 4; ++ni) { ptr_hide(); draw_item(it, ni & 1); }
    }
    close_menu();
    return cmd;
}

/* Esc: keyboard menus. Left/Right switch, Up/Down move, Return chooses */
static unsigned char menu_keyboard(void)
{
    unsigned char m = 1, it = 0xFF, k, i;
    open_menu(m);
    for (;;) {
        if (wait_event() == 2) {
            close_menu();
            return menu_mouse();
        }
        k = key;
        ptr_hide();
        if (k == K_ESC) break;
        if (k == K_LEFT || k == K_RIGHT) {
            scr_restore();
            m = (k == K_LEFT) ? (m + NMENU - 1) % NMENU : (m + 1) % NMENU;
            open_menu(m);
            it = 0xFF;
        } else if (k == K_UP || k == K_DOWN) {
            if (it != 0xFF) draw_item(it, 0);
            for (i = 0; i < dd_n; ++i) {
                if (k == K_DOWN) it = (it == 0xFF || it + 1 >= dd_n) ? 0 : it + 1;
                else it = (it == 0xFF || it == 0) ? dd_n - 1 : it - 1;
                if (item_enabled(menus[m][it].cmd)) break;
            }
            draw_item(it, 1);
        } else if (k == K_RET) {
            if (it != 0xFF) { k = menus[m][it].cmd; close_menu(); return k; }
        } else {
            k &= 0xDF;
            for (i = 0; i < dd_n; ++i) {
                const char *l = item_label(&menus[m][i]);
                if (l && (l[0] & 0xDF) == k && item_enabled(menus[m][i].cmd)) {
                    close_menu();
                    return menus[m][i].cmd;
                }
            }
        }
    }
    close_menu();
    return CM_NONE;
}

static unsigned char __fastcall__ oa_command(unsigned char k)
{
    unsigned char m, i;
    if (k >= 'a' && k <= 'z') k -= 32;
    if (k == '/') k = '?';
    for (m = 0; m < NMENU; ++m)
        for (i = 0; menus[m][i].cmd; ++i)
            if (menus[m][i].key == k) {
                if (item_enabled(menus[m][i].cmd)) {
                    hilite_title(m);    /* flash the title, as on the Mac */
                    draw_menubar();
                    return menus[m][i].cmd;
                }
                return CM_NONE;
            }
    return CM_NONE;
}

unsigned char __fastcall__ menu_run(unsigned char how)
{
    if (how == 0) return menu_mouse();
    if (how == 1) return menu_keyboard();
    return oa_command(key);
}


/* ---------------------------------------------------------------------- */
/* Ruler and paragraph formats                                             */

/* change paragraph attributes (bits in mask) of every paragraph that the
   selection touches. Each paragraph keeps them in a marker at its start. */
static void __fastcall__ set_para(unsigned char mask, unsigned char val)
{
    unsigned p, q, hi = sel_hi();
    unsigned char b;
    p = para_start(sel_lo());
    if (!p) p = 1;
    for (;;) {
        for (q = p; q < tlen && ((b = tch(q)) & 0x80); ++q)
            if ((b & 3) == 3) break;
        if (q < tlen && (b & 0x83) == 0x83) {
            aux_poke(phys(q), (b & ~mask) | val);
            disp_shift(q, 0);
        } else {
            b = 0x83 | val;
            if (!raw_insert(p, &b, 1)) break;
            disp_shift(p, 1);
            if (cur >= p) ++cur;
            if (anc >= p) ++anc;
            ++hi;
        }
        for (q = p; q < tlen && tch(q) != CR; ++q) ;
        if (q + 1 >= tlen || q + 1 > hi) break;
        p = q + 1;
    }
    modified = 1;
    text_moved();
    ruler_dirty = 1;
}

static unsigned __fastcall__ snapx(unsigned x)
{
    if (x < LEFTX) return 0;
    x = (x - LEFTX + 2) / 5 * 5;
    return x > 512 ? 512 : x;
}

static void sort_tabs(void)
{
    unsigned char i, j;
    unsigned t;
    for (i = 0; i < 8; ++i)
        for (j = i + 1; j < 8; ++j)
            if (dtabs[j] && (!dtabs[i] || dtabs[j] < dtabs[i])) {
                t = dtabs[i]; dtabs[i] = dtabs[j]; dtabs[j] = t;
            }
}

/* a press on the ruler: icons set the paragraph format; margin and tab
   markers can be dragged; a press on the scale adds a tab; dragging a tab
   down off the ruler removes it */
void ruler_click(void)
{
    unsigned x = mx, *v;
    unsigned char y = my - MENUH, k, i;
    if (y >= 16) {
        for (k = 0; k < 7; ++k) {
            v = (unsigned *)(LEFTX + k * 22 + (k > 3 ? 16 : 0));
            if (x >= (unsigned)v && x < (unsigned)v + 20) break;
        }
        wait_up();
        if (k < 4) set_para(0x0C, k << 2);
        else if (k < 7) set_para(0x30, (k - 4) << 4);
        return;
    }
    x = snapx(x);
    v = 0;
    if (x + 4 >= lmargin && x <= lmargin + 4) v = &lmargin;
    else if (x + 4 >= rmargin && x <= rmargin + 4) v = &rmargin;
    else for (i = 0; i < 8 && dtabs[i]; ++i)
        if (x + 4 >= dtabs[i] && x <= dtabs[i] + 4) { v = &dtabs[i]; break; }
    if (!v) {                           /* new tab */
        for (i = 0; i < 8 && dtabs[i]; ++i) ;
        if (i == 8 || !x) { beep(); wait_up(); return; }
        v = &dtabs[i];
        *v = x;
        draw_ruler();
    }
    while (mbtn) {
        mouse_poll();
        x = snapx(mx);
        if (v == &lmargin && x + 40 > rmargin) x = rmargin - 40;
        if (v == &rmargin && x < lmargin + 40) x = lmargin + 40;
        if (!x && v != &lmargin) x = 5;
        if (x != *v) { *v = x; draw_ruler(); }
        ptr_show();
    }
    if (v != &lmargin && v != &rmargin && my > MENUH + RULERH + 6) *v = 0;   /* dragged off */
    sort_tabs();
    modified = 1;
    ruler_apply();
    invalidate_layout();
    set_all_dirty();
    ruler_dirty = 1;
}

void __fastcall__ menu_command(unsigned char c)
{
    switch (c) {
    case CM_ABOUT: about(); break;
    case CM_HELP: help(); break;
    case CM_RULER:
        ruler_on = !ruler_on;
        redraw_all();
        break;
    case CM_ALEFT: case CM_ACENTER: case CM_ARIGHT: case CM_AJUST:
        set_para(0x0C, (c - CM_ALEFT) << 2);
        break;
    case CM_SP1: case CM_SP15: case CM_SP2:
        set_para(0x30, (c - CM_SP1) << 4);
        break;
    }
}
