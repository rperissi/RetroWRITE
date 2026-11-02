/*
** RetroWrite II - resident user interface (included by core.c):
** drawing helpers, dialogs, menus, commands, overlays
*/

char progdir[65], browsedir[65];

/* ---------------------------------------------------------------------- */
/* Drawing helpers                                                         */

void __fastcall__ ui_text(unsigned x, unsigned char base, unsigned char fmt, const char *s)
{
    gx = x;
    gbase = base;
    gfmt = fmt;
    while (*s) draw_char(*s++);
}

unsigned __fastcall__ text_w(unsigned char fmt, const char *s)
{
    unsigned w = 0;
    gfmt = fmt;
    while (*s) w += char_width(*s++);
    return w;
}

/* frame in the line buffer: 2-pixel verticals, 1-row horizontals */
void __fastcall__ frame_rect(unsigned x0, unsigned x1, unsigned char r0, unsigned char r1)
{
    lb_fill(x0, x1, r0, r0, LB_SET);
    lb_fill(x0, x1, r1, r1, LB_SET);
    lb_fill(x0, x0 + 2, r0, r1, LB_SET);
    lb_fill(x1 - 2, x1, r0, r1, LB_SET);
}

/* box with shadow over byte columns c0..c1, rows y0..y1 */
void __fastcall__ draw_box(unsigned char c0, unsigned char c1, unsigned char y0, unsigned char y1)
{
    unsigned x0 = c0 * 7, x1 = c1 * 7 + 7;
    unsigned char y, n, r;
    for (y = y0; y <= y1; y += n) {
        n = y1 + 1 - y > 32 ? 32 : y1 + 1 - y;
        lb_clear(n);
        lb_fill(x0, x0 + 2, 0, n - 1, LB_SET);
        lb_fill(x1 - 4, x1 - 2, 0, n - 1, LB_SET);
        r = y < y0 + 3 ? y0 + 3 - y : 0;
        if (r < n) lb_fill(x1 - 2, x1, r, n - 1, LB_SET);
        if (y == y0) lb_fill(x0, x1 - 2, 0, 0, LB_SET);
        if (y + n > y1 - 1) {
            r = y1 - 1 - y;
            lb_fill(x0, x1 - 2, r, r, LB_SET);
            lb_fill(x0 + 4, x1, r + 1, r + 1, LB_SET);
            lb_fill(x0, x0 + 4, r + 1, r + 1, LB_CLR);
        }
        present(y, n, c0, c1);
    }
}

/* rounded button; default button gets a heavier outline. The corner and
   side pieces are lb_cmds records relative to the left or right edge. */
#define NEG(n) (unsigned char)(-(n)), 0xFF
static const unsigned char btn_l[] = {
    1, 2, 0, 4, 0, 2, 2,   1, 2, 0, 4, 0, 12, 12,   1, 0, 0, 2, 0, 3, 11,  0xFF };
static const unsigned char btn_ld[] = {
    1, 2, 0, 4, 0, 1, 1,   1, 2, 0, 4, 0, 13, 13,   1, 0, 0, 4, 0, 3, 11,  0xFF };
static const unsigned char btn_r[] = {
    1, NEG(4), NEG(2), 2, 2,   1, NEG(4), NEG(2), 12, 12,   1, NEG(2), 0, 0, 3, 11,  0xFF };
static const unsigned char btn_rd[] = {
    1, NEG(4), NEG(2), 1, 1,   1, NEG(4), NEG(2), 13, 13,   1, NEG(4), 0, 0, 3, 11,  0xFF };

void __fastcall__ draw_button(unsigned x0, unsigned x1, unsigned char y, const char *label, unsigned char dflt)
{
    unsigned w;
    lb_clear(15);
    lb_fill(x0 + 4, x1 - 4, dflt ? 0 : 1, 1, LB_SET);
    lb_fill(x0 + 4, x1 - 4, 13, dflt ? 14 : 13, LB_SET);
    lbx = x0;
    lb_cmds(dflt ? btn_ld : btn_l);
    lb_cmds(btn_l);
    lbx = x1;
    lb_cmds(dflt ? btn_rd : btn_r);
    lb_cmds(btn_r);
    w = text_w(0, label);
    w = (w < x1 - x0) ? (x1 - x0 - w) / 2 : 1;
    ui_text(x0 + w, 10, 0, label);
    present(y, 15, x0 / 7, (x1 - 1) / 7);
}

/* ---------------------------------------------------------------------- */
/* Alerts and dialogs                                                      */

#define MAXB 6
unsigned bx0[MAXB], bx1[MAXB];
unsigned char by[MAXB], nbtn;
const char *blabel[MAXB];

unsigned char hit_button(void)
{
    unsigned char i;
    for (i = 0; i < nbtn; ++i)
        if (mx >= bx0[i] && mx < bx1[i] && my >= by[i] && my < by[i] + 15) return i;
    return 0xFF;
}

/* Return = button 0, Esc = last button, or a label's first letter.
   With extra_keys, other keys return 0xFE and other clicks 0xFD. */
unsigned char __fastcall__ run_buttons(unsigned char extra_keys)
{
    unsigned char b, i, k;
    for (;;) {
        if (wait_event() == 1) {
            k = key & 0xDF;
            if (key == K_RET) return 0;
            if (key == K_ESC && nbtn > 1) return nbtn - 1;
            if (extra_keys != 2)        /* 2: letters go into a text box */
                for (i = 0; i < nbtn; ++i)
                    if ((blabel[i][0] & 0xDF) == k) return i;
            if (extra_keys) return 0xFE;
        } else {
            b = hit_button();
            wait_up();
            if (b != 0xFF && hit_button() == b) return b;
            if (extra_keys) return 0xFD;
        }
    }
}

#define DC0 14
#define DC1 65
#define DX0 (DC0 * 7)
#define DX1 (DC1 * 7 + 7)

static unsigned char __fastcall__ text_lines(const char *msg, unsigned char y)
{
    char *line = (char *)0x1D00;    /* line buffer rows past the 11 a dialog line uses */
    unsigned char n;
    while (*msg) {
        n = 0;
        while (*msg && *msg != '\n' && n < 63) line[n++] = *msg++;
        line[n] = 0;
        if (*msg == '\n') ++msg;
        lb_clear(11);
        ui_text(DX0 + 16, 8, 0, line);
        present(y, 11, DC0 + 1, DC1 - 1);
        y += 11;
    }
    return y;
}

static void __fastcall__ dialog_open(unsigned char y1)
{
    ptr_hide();
    caret_hide();
    scr_save(DC0, DC1, 56, y1);
    draw_box(DC0, DC1, 56, y1);
}

/* buttons right-aligned on row y */
static void __fastcall__ place_buttons(unsigned char y)
{
    unsigned char i;
    unsigned x = DX1 - 16;
    for (i = 0; i < nbtn; ++i) {
        x -= 92;
        bx0[i] = x;
        bx1[i] = x + 84;
        by[i] = y;
        draw_button(x, x + 84, y, blabel[i], i == 0);
    }
}

unsigned char __fastcall__ alert(const char *msg, const char *b1, const char *b2, const char *b3)
{
    unsigned char nlines = 1, r;
    const char *p;
    for (p = msg; *p; ++p) if (*p == '\n') ++nlines;
    dialog_open(56 + nlines * 11 + 40);
    blabel[0] = b1; blabel[1] = b2; blabel[2] = b3;
    nbtn = b3 ? 3 : (b2 ? 2 : 1);
    place_buttons(text_lines(msg, 66) + 8);
    beep();
    r = run_buttons(0);
    ptr_hide();
    scr_restore();
    return r;
}

void __fastcall__ show_msg(const char *s)
{
    alert(s, "OK", 0, 0);
}

static void __fastcall__ draw_field(unsigned char y, const char *buf)
{
    lb_clear(15);
    frame_rect(DX0 + 14, DX1 - 14, 0, 14);
    ui_text(DX0 + 20, 10, 0, buf);
    lb_fill(gx + 1, gx + 2, 2, 12, LB_SET);
    present(y, 15, DC0 + 1, DC1 - 1);
}

unsigned char __fastcall__ ask_text(const char *prompt, char *buf, unsigned char max)
{
    unsigned char n, r, fy;
    dialog_open(146);
    fy = text_lines(prompt, 66) + 4;
    n = (unsigned char)strlen(buf);
    draw_field(fy, buf);
    blabel[0] = "OK"; blabel[1] = "Cancel";
    nbtn = 2;
    place_buttons(124);
    for (;;) {
        r = run_buttons(2);         /* typing goes into the box */
        if (r < 2) break;
        if (r == 0xFE) {
            if (key == K_DEL || key == K_LEFT) { if (n) buf[--n] = 0; }
            else if (key >= 0x20 && key < 0x7F && n < max && !oa) { buf[n++] = key; buf[n] = 0; }
            ptr_hide();
            draw_field(fy, buf);
        }
    }
    ptr_hide();
    scr_restore();
    return r == 0;
}

/* ---------------------------------------------------------------------- */
/* Menus                                                                   */



#define NMENU 9
const char *const mtitle[NMENU] = {
    "\x80", "File", "Edit", "Search", "Format", "Font", "Size", "Style", "Windows"
};
unsigned mtx[NMENU + 1];

void draw_menubar(void)
{
    unsigned char i;
    unsigned x = 14;
    lb_clear(MENUH);
    lb_fill(0, 560, 10, 10, LB_SET);
    for (i = 0; i < NMENU; ++i) {
        mtx[i] = x;
        ui_text(x, 7, F_BOLD, mtitle[i]);
        x = gx + 14;
    }
    mtx[NMENU] = x;
    present(0, MENUH, 0, 79);
}

/* Open-Apple keys that are not on a menu */
static unsigned char __fastcall__ oa_special(unsigned char k)
{
    switch (k) {
    case K_UP: return CM_PGUP;
    case K_DOWN: return CM_PGDN;
    case K_LEFT: return CM_WLEFT;
    case K_RIGHT: return CM_WRIGHT;
    case K_DEL: return CM_DELFWD;
    case '1': return CM_TOP;
    case '9': return CM_END;
    }
    return CM_NONE;
}

/* menus live in the edit overlay: how 0 = mouse, 1 = keyboard, 2 = OA key */
static unsigned char __fastcall__ menus(unsigned char how)
{
    if (!load_ovl(OV_MENU)) return CM_NONE;
    return menu_run(how);
}

/* ---------------------------------------------------------------------- */
/* Overlays (loaded from the RetroWrite II folder on the hard disk)        */

static unsigned char cur_ovl;

unsigned char __fastcall__ load_ovl(unsigned char n)
{
    int fd, r;
    char *op = (char *)0x1D80;          /* own buffer: pathbuf may hold a chosen name */
    if (cur_ovl == n) return 1;
    strcpy(op, progdir);
    strcat(op, "/RW2.OVL1");
    op[strlen(op) - 1] = n < 10 ? '0' + n : 'A' - 10 + n;
    fd = open(op, O_RDONLY);
    if (fd < 0) { cur_ovl = 0; alert("Can't load part of RetroWrite II\n(RW2.OVLn is missing).", "OK", 0, 0); return 0; }
    r = read(fd, (void *)OVLADDR, OVLSIZE);
    close(fd);
    if (r < 0) r = 0;
    memset((unsigned char *)OVLADDR + r, 0, OVLSIZE - r);
    cur_ovl = n;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* Commands                                                                */

/* ---------------------------------------------------------------------- */
/* File commands: the file overlay loads and saves, the dialog overlay     */
/* picks names; the steps are strung together here                         */

unsigned char ftype;

static unsigned char __fastcall__ is_odt(const char *s)
{
    unsigned char n = (unsigned char)strlen(s);
    return n > 4 && !strcmp(s + n - 4, ".ODT");
}

/* ODT files go through overlays 8 (save) and 9 + 10 (open) */
static unsigned char __fastcall__ fop(unsigned char op)
{
    unsigned char r;
    if (op == FO_LOAD && ftype == FT_ODT)
        return load_ovl(OV_ODTIN) && odt_unzip() && load_ovl(OV_ODTRD) && odt_read();
    if (op == FO_SAVETO && is_odt(pathbuf)) r = 3;
    else r = load_ovl(OV_FILE) ? file_op(op) : 0;
    if (r == 3) return load_ovl(OV_ODTOUT) && odt_content() && load_ovl(OV_ODTZIP) && odt_save();
    return r;
}

static unsigned char __fastcall__ pick(unsigned char mode)
{
    return load_ovl(OV_DLG) ? file_dialog(mode) : 0;
}

static unsigned char __fastcall__ save_as(unsigned char text)
{
    if (text && !fop(FO_TEXTOK)) return 0;
    return pick(text ? 2 : 1) && fop(text ? FO_SAVETEXT : FO_SAVETO);
}

static unsigned char save(void)
{
    unsigned char r = fop(FO_SAVE);
    return r == 2 ? save_as(0) : r;
}

/* ask to save a changed document; 0 if the user cancels */
unsigned char confirm(void)
{
    unsigned char r;
    if (!modified) return 1;
    r = alert("Save changes to this document\nbefore closing it?", "Save", "Don't Save", "Cancel");
    if (r == 2) return 0;
    if (r == 1) return 1;
    save();
    return !modified;
}

/* Windows: up to 4 documents. Only the one on the screen is in memory;
   the others wait in RW2.WINn files in the RetroWrite II folder. */
unsigned char cur_doc, wlive = 1, wmod, fo_arg;
char wname[4][16];

static unsigned char doc_blank(void)
{
    return !modified && !docpath[0] && tlen <= 2;
}

/* put the current document away and show window k (a new one if unused) */
static unsigned char __fastcall__ win_go(unsigned char k)
{
    if (!fop(FO_SWAPOUT)) return 0;
    fo_arg = k;
    return fop(FO_SWAPIN);
}

static void __fastcall__ file_cmd(unsigned char c)
{
    unsigned char k;
    switch (c) {
    case CM_SAVE: save(); return;
    case CM_SAVEAS: save_as(0); return;
    case CM_SAVETEXT: save_as(1); return;
    }
    if (c >= CM_WIN1 && c <= CM_WIN4) {
        k = c - CM_WIN1;
        if (k != cur_doc && (wlive & (1 << k))) win_go(k);
        return;
    }
    if ((c == CM_NEW || c == CM_OPEN) && !doc_blank()) {
        for (k = 0; k < 4 && (wlive & (1 << k)); ++k) ;
        if (k < 4) {                    /* a free window */
            if (c == CM_OPEN && !pick(0)) return;
            if (win_go(k) && c == CM_OPEN) fop(FO_LOAD);
            return;
        }
    }
    if (!confirm()) return;
    switch (c) {
    case CM_NEW: fop(FO_NEW); break;
    case CM_OPEN: if (pick(0)) fop(FO_LOAD); break;
    case CM_CLOSE: fop(FO_CLOSEWIN); break;
    case CM_QUIT: case CM_BASIC:
        for (k = 0; k < 4; ++k)         /* the other changed documents */
            if (k != cur_doc && (wmod & (1 << k))) {
                if (!win_go(k) || !confirm()) return;
            }
        quit_now = c == CM_QUIT ? 1 : 2;    /* 2: run BASIC.SYSTEM */
        break;
    case CM_UDICT: fop(FO_UDICT); break;
    case CM_GLOSSEDIT: fop(FO_GLOSS); break;
    case CM_TUTOR: fop(FO_TUTOR); break;
    }
}

/* Print Several: put the document away, print the chosen ones, bring it back */
static void print_many(void)
{
    if (!fop(FO_SWAPOUT)) return;
    while (pick(3) && fop(FO_LOAD) && load_ovl(OV_PRINT) && print_command(CM_PRINTNOW)) ;
    fo_arg = cur_doc;
    fop(FO_SWAPIN);
}

static void __fastcall__ command(unsigned char c)
{
    unsigned olo = sel_lo(), ohi = sel_hi();
    caret_hide();
    if (c == CM_NONE || c >= CM_SEP) return;
    if (c <= CM_FILE_LAST) {                    /* these redraw for themselves */
        file_cmd(c);
        return;
    }
    if (c == CM_PRINTMANY) { print_many(); return; }
    if (c == CM_BASIC) { if (load_ovl(OV_ACC) && acc_command(c)) file_cmd(c); return; }
    if (c < CM_RES_FIRST) {
        if (c == CM_SPELL) { if (load_ovl(OV_SPELL)) cmd_spell(); }
        else if (c < CM_EDIT_FIRST) { if (load_ovl(OV_MENU)) menu_command(c); }
        else if (c < CM_PRINT_FIRST) { if (load_ovl(OV_EDIT)) edit_command(c); }
        else if (c < CM_ACC_FIRST) { if (load_ovl(OV_PRINT)) print_command(c); }
        else if (c < CM_NET) { if (load_ovl(OV_ACC)) acc_command(c); }
        else if (load_ovl(OV_NET) && net_command(c)) fop(FO_LOAD);
        goto done;
    }
    switch (c) {
    case CM_SELALL: anc = 1; cur = tlen; sel_changed(olo, ohi); return;
    case CM_PGUP: page(-1); break;
    case CM_PGDN: page(1); break;
    }
done:
    if (!layout_ok) set_all_dirty();
    c = (c >= CM_SANS && c <= CM_SUB) ? tf : 0xFF;
    after_move(olo, ohi, 0);
    if (cur == anc && c != 0xFF) tf = c;    /* a style chosen with nothing selected
                                               is kept for what is typed next */
}
