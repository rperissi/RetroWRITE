/*
** RetroWrite II - overlay 12: CoGS Share
**
**   Apple menu > CoGS Share... sends the saved document to the "files"
**   folder of a CoGS LAN share, or opens an .ODT document from it. The
**   share is CoGS Image Manager on a Mac or cogs_share.py (both in the
**   CoGS repo, netstorage/); the Mac sees the documents as ordinary files
**   and LibreOffice opens them as they are.
**
**   The card does the networking. Each request is one HTTP command frame
**   to the card (method, URL, headers, body) answered by DATA events and
**   an HTTP_DONE event; a document goes up in 1 KB pieces with a
**   Content-Range header the share joins back together, because one
**   frame carries under 2 KB. See the CoGS Technical Reference, ch. 9.
*/
#pragma code-name (push, "OVLC")
#pragma rodata-name (push, "OVLC")
#pragma data-name (push, "OVLC")
#pragma bss-name (push, "OVLCBSS")

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <apple2enh.h>
#include "rw.h"

/* net.s */
void __fastcall__ cg_set(unsigned char slot);
unsigned char cg_rd(void);
void __fastcall__ cg_wr(unsigned char b);
void __fastcall__ cg_rdn(void *dst, unsigned n);
void __fastcall__ cg_wrn(const void *src, unsigned n);
void __fastcall__ cg_skip(unsigned n);
extern unsigned char cg_err, cg_tmo;

/* The 1 KB piece buffer is the line buffer, so nothing may be drawn while
   it holds data: load or receive a piece, move it, then draw. */
#define NETBUF  ((unsigned char *)0x1400)
#define PIECE   1024
#define NMAX    48
#define SRVMAX  48

#define OP_HTTP     0x20
#define OP_CATURL   0x5A
#define EV_DATA     0x91
#define EV_ERROR    0x93
#define EV_DONE     0x95
#define HF_KEEP     0x08

static char server[SRVMAX + 4];         /* "http://host:port/" (RW2.COGS) */
static char names[NMAX][16];            /* .ODT files on the share */
static unsigned char nnames;
static unsigned char http_err;          /* card error code from an ERROR event */
static unsigned http_status;
static int fd;
static unsigned got;                    /* bytes received into fd */
static unsigned char sink;              /* DATA goes: 0 nowhere, 1 fd, 2 names */
static unsigned char lpos, lskip;       /* listing parse state */
static char msg[128];

/* ---------------------------------------------------------------------- */
/* Box                                                                     */

#define PC0 11
#define PC1 68
#define PX0 (PC0 * 7)
#define PX1 (PC1 * 7 + 7)
#define PY0 34
#define PY1 186
#define LX0 (PX0 + 14)
#define LX1 (PX0 + 266)
#define LY0 (PY0 + 44)
#define LROWS 7
#define LY1 (LY0 + LROWS * 11 + 2)
#define BY  (PY1 - 26)

static void __fastcall__ line(unsigned char y, unsigned char fmt, const char *s)
{
    lb_clear(11);
    ui_text(PX0 + 14, 8, fmt, s);
    present(y, 11, PC0 + 1, PC1 - 1);
}

static void __fastcall__ buttons(const char *b0, const char *b1, const char *b2, const char *b3)
{
    unsigned char b;
    unsigned x = PX1 - 16;
    lb_clear(15);                           /* erase the previous row */
    present(BY, 15, PC0 + 1, PC1 - 1);
    blabel[0] = b0; blabel[1] = b1; blabel[2] = b2; blabel[3] = b3;
    nbtn = b3 ? 4 : (b2 ? 3 : (b1 ? 2 : 1));
    for (b = 0; b < nbtn; ++b) {
        x -= 92;
        bx0[b] = x;
        bx1[b] = x + 84;
        by[b] = BY;
        draw_button(x, x + 84, BY, blabel[b], b == 0);
    }
}

static void __fastcall__ box(const char *title)
{
    caret_hide();
    ptr_hide();
    draw_box(PC0, PC1, PY0, PY1);
    line(PY0 + 6, F_BOLD, title);
}

static void unbox(void)
{
    ptr_hide();
    redraw_all();
    refresh();
}

/* "Share: http://..." on the second line of the box */
static void show_server(void)
{
    strcpy(msg, "Share: ");
    strcat(msg, server[0] ? server : "(not set)");
    line(PY0 + 20, 0, msg);
}

/* ---------------------------------------------------------------------- */
/* The card                                                                */

static unsigned char find_card(void)
{
    unsigned char s;
    unsigned b;
    for (s = 1; s <= 7; ++s) {
        b = 0xC080 + s * 16;
        if (PEEK(b + 5) == 0xA2 && PEEK(b + 6) != 0 && PEEK(b + 1) != 0xFF) {
            cg_set(s);
            cg_tmo = 20;
            return 1;
        }
    }
    return 0;
}

/* report a failed transaction */
static void net_fail(void)
{
    if (cg_err == 2) { show_msg("Cancelled."); return; }
    if (cg_err) { show_msg("The CoGS card did not answer.\nIs it joined to Wi-Fi?"); return; }
    if (http_err) {
        strcpy(msg, "The card could not reach the share\n(network error ");
        utoa(http_err, msg + strlen(msg), 10);
        strcat(msg, "). Check the address.");
        show_msg(msg);
        return;
    }
    strcpy(msg, "The share answered HTTP ");
    utoa(http_status, msg + strlen(msg), 10);
    if (http_status == 403) strcat(msg, ".\nIt is read-only: start it with --writable,\nor turn on Writeable in Image Manager.");
    else if (http_status == 404) strcat(msg, ".\nNo files folder there. Is the address right?");
    else strcat(msg, ".");
    show_msg(msg);
}

/* a DATA event's bytes have arrived in NETBUF */
static void __fastcall__ deliver(unsigned n)
{
    unsigned char *p = NETBUF, c;
    if (sink == 1) {
        if (write(fd, NETBUF, n) != (int)n) cg_err = 1;
        got += n;
        return;
    }
    if (sink != 2) return;
    /* listing: "ALIAS<tab>name<tab>bytes\n" per file; keep the .ODT aliases.
       lskip: 0 in the alias, 1 past it, 2 line rejected */
    while (n--) {
        c = *p++;
        if (c == '\n') {
            if (lskip != 2 && lpos > 4 && nnames < NMAX) {
                names[nnames][lpos] = 0;
                if (!strcmp(names[nnames] + lpos - 4, ".ODT")) ++nnames;
            }
            lpos = 0; lskip = 0;
        } else if (c == '\t') {
            if (!lskip) lskip = 1;
        } else if (!lskip && nnames < NMAX) {
            if (lpos < 15) names[nnames][lpos++] = c;
            else lskip = 2;                 /* not a ProDOS name after all */
        }
    }
}

/* One HTTP command: method, path under the share, extra header lines, body
   of blen bytes already in NETBUF. 1 with http_status set on HTTP_DONE. */
static unsigned char __fastcall__ http(const char *method, const char *path, const char *hdrs,
                                       unsigned char flags, unsigned blen)
{
    unsigned len, n;
    unsigned char op, ml = (unsigned char)strlen(method), hl = (unsigned char)strlen(hdrs);
    unsigned char sl = (unsigned char)strlen(server), pl = (unsigned char)strlen(path);

    cg_err = 0; http_err = 0; http_status = 0;
    len = 3 + ml + 1 + sl + pl + 2 + hl + 2 + blen;
    cg_wr(OP_HTTP); cg_wr(len & 0xFF); cg_wr(len >> 8);
    cg_wr(0);                               /* socket 0 */
    cg_wr(flags);
    cg_wr(ml); cg_wrn(method, ml);
    cg_wr(sl + pl); cg_wrn(server, sl); cg_wrn(path, pl);
    cg_wr(hl); cg_wr(0); cg_wrn(hdrs, hl);
    cg_wr(blen & 0xFF); cg_wr(blen >> 8);
    if (blen) cg_wrn(NETBUF, blen);
    if (cg_err) return 0;

    for (;;) {
        op = cg_rd();
        len = cg_rd();
        len |= cg_rd() << 8;
        if (cg_err) return 0;
        switch (op) {
        case EV_DATA:
            cg_rd(); --len;                 /* socket */
            while (len) {
                n = len > PIECE ? PIECE : len;
                cg_rdn(NETBUF, n);
                if (cg_err) return 0;
                deliver(n);
                if (cg_err) return 0;
                len -= n;
            }
            break;
        case EV_DONE:
            cg_rd();
            http_status = cg_rd();
            http_status |= cg_rd() << 8;
            cg_skip(len - 3);
            return !cg_err;
        case EV_ERROR:
            cg_rd();
            http_err = cg_rd();
            cg_skip(len - 2);
            return 0;
        default:
            cg_skip(len);                   /* Wi-Fi state and the like */
        }
    }
}

/* ---------------------------------------------------------------------- */
/* The share's address: RW2.COGS next to the program, else the card's      */
/* Image Source (a LAN share's base URL), else typed in                    */

static void srv_path(void)
{
    strcpy(tmppath, progdir);
    strcat(tmppath, "/RW2.COGS");
}

static void srv_load(void)
{
    int n;
    server[0] = 0;
    srv_path();
    fd = open(tmppath, O_RDONLY);
    if (fd < 0) return;
    n = read(fd, server, SRVMAX);
    close(fd);
    if (n < 0) n = 0;
    server[n] = 0;
    while (n && server[n - 1] < ' ') server[--n] = 0;
}

static void srv_save(void)
{
    srv_path();
    _filetype = PRODOS_T_BIN;         /* not TXT: keeps it out of the Open dialog */
    _auxtype = 0;
    fd = open(tmppath, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) return;
    write(fd, server, strlen(server));
    close(fd);
}

/* does s contain t */
static unsigned char __fastcall__ has(const char *s, const char *t)
{
    unsigned char n = (unsigned char)strlen(t), l = (unsigned char)strlen(s);
    for (; l >= n; ++s, --l) if (!memcmp(s, t, n)) return 1;
    return 0;
}

/* CAT_URL_GET: the card's Image Source. A LAN share's base URL is the
   right default; the a2cogs.com catalog has no files folder. */
static void srv_default(void)
{
    unsigned len;
    unsigned char op, r;
    char *p;
    strcpy(server, "http://");
    cg_err = 0;
    cg_wr(OP_CATURL); cg_wr(0); cg_wr(0);
    op = cg_rd();
    len = cg_rd();
    len |= cg_rd() << 8;
    if (cg_err || op != (OP_CATURL | 0x80) || len < 2) return;
    r = cg_rd(); --len;
    if (len > SRVMAX) { cg_rdn(msg, SRVMAX); cg_skip(len - SRVMAX); len = SRVMAX; }
    else cg_rdn(msg, len);
    msg[len] = 0;
    if (cg_err || r || has(msg, "a2cogs.com")) return;
    for (p = msg; *p && *p != ':'; ++p) ;         /* scheme:// */
    if (p[0] != ':' || p[1] != '/' || p[2] != '/') return;
    for (p += 3; *p && *p != '/'; ++p) ;          /* host:port */
    p[0] = '/'; p[1] = 0;
    strcpy(server, msg);
}

static unsigned char srv_ask(void)
{
    unsigned char n;
    if (!server[0]) srv_default();
    if (!ask_text("Address of the CoGS share, as Image Manager\nor cogs_share shows it (http://host:port/):", server, SRVMAX)) return 0;
    n = (unsigned char)strlen(server);
    if (n && server[n - 1] != '/') strcat(server, "/");
    srv_save();
    return 1;
}

/* ---------------------------------------------------------------------- */
/* Send                                                                    */

static void __fastcall__ progress(const char *what, unsigned done, unsigned total)
{
    strcpy(msg, what);
    utoa(done, msg + strlen(msg), 10);
    strcat(msg, " of ");
    utoa(total, msg + strlen(msg), 10);
    strcat(msg, " bytes.  Esc stops.");
    line(LY0, 0, msg);
}

static void do_send(void)
{
    long sz;
    unsigned size, off, n;
    unsigned char *b = (unsigned char *)&sz;
    const char *name = strrchr(docpath, '/') + 1;

    fd = open(docpath, O_RDONLY);
    if (fd < 0) { show_msg("Can't read the document's file."); return; }
    sz = lseek(fd, 0, SEEK_END);
    if (b[2] | b[3]) { close(fd); show_msg("The file is over 64K; too big to send."); return; }
    size = *(unsigned *)&sz;
    lseek(fd, 0, SEEK_SET);

    strcpy(tmppath, "files/");
    strcat(tmppath, name);
    sink = 0;
    for (off = 0; off < size; off += n) {
        n = size - off;
        if (n > PIECE) n = PIECE;
        if (read(fd, NETBUF, n) != (int)n) { cg_err = 1; break; }
        strcpy(msg, "Content-Range: bytes ");
        utoa(off, msg + strlen(msg), 10);
        strcat(msg, "-");
        utoa(off + n - 1, msg + strlen(msg), 10);
        strcat(msg, "/");
        utoa(size, msg + strlen(msg), 10);
        if (!http("PUT", tmppath, msg, HF_KEEP, n)) break;
        if (http_status != 200 && http_status != 201) break;
        progress("Sent ", off + n, size);
    }
    close(fd);
    if (off < size) { net_fail(); return; }
    strcpy(msg, name);
    strcat(msg, " is on the share, in its files folder.");
    show_msg(msg);
}

/* ---------------------------------------------------------------------- */
/* Open                                                                    */

static unsigned char sel, scr;

static void draw_list(void)
{
    unsigned char i, n, y = LY0;
    ptr_hide();
    lb_clear(1);
    lb_fill(LX0, LX1, 0, 0, LB_SET);
    present(y - 1, 1, LX0 / 7, (LX1 - 1) / 7);
    for (i = 0; i < LROWS; ++i, y += 11) {
        lb_clear(11);
        lb_fill(LX0, LX0 + 2, 0, 10, LB_SET);
        lb_fill(LX1 - 2, LX1, 0, 10, LB_SET);
        n = scr + i;
        if (n < nnames) {
            ui_text(LX0 + 6, 8, 0, "\x83");
            ui_text(LX0 + 22, 8, F_BOLD, names[n]);
            if (n == sel) lb_fill(LX0 + 2, LX1 - 2, 0, 10, LB_XOR);
        } else if (n == 0) {
            ui_text(LX0 + 22, 8, F_ITAL, "(no .ODT files on the share)");
        }
        present(y, 11, LX0 / 7, (LX1 - 1) / 7);
    }
    lb_clear(1);
    lb_fill(LX0, LX1, 0, 0, LB_SET);
    present(y, 1, LX0 / 7, (LX1 - 1) / 7);
}

/* 1 with the chosen name in names[sel] */
static unsigned char pick_name(void)
{
    unsigned char b, i;
    sel = scr = 0;
    draw_list();
    buttons("Open", "Cancel", 0, 0);
    for (;;) {
        b = run_buttons(1);
        if (b == 0) return nnames != 0;
        if (b == 1) return 0;
        if (b == 0xFE) {
            if (key == K_UP && sel) --sel;
            else if (key == K_DOWN && sel + 1 < nnames) ++sel;
            else continue;
        } else if (b == 0xFD) {
            if (mx < LX0 || mx >= LX1 || my < LY0 || my >= LY0 + LROWS * 11) continue;
            i = scr + (my - LY0) / 11;
            if (i >= nnames) continue;
            sel = i;
        }
        if (sel < scr) scr = sel;
        if (sel >= scr + LROWS) scr = sel - LROWS + 1;
        draw_list();
    }
}

/* 1 with pathbuf and ftype set for FO_LOAD */
static unsigned char do_open(void)
{
    unsigned char ok;
    line(LY0, 0, "Asking the share for its files...");
    nnames = 0; lpos = 0; lskip = 0; sink = 2;
    if (!http("GET", "files/?fmt=txt", "", 0, 0) || http_status != 200) { net_fail(); return 0; }
    line(PY0 + 32, 0, "Open from the share:");
    if (!pick_name()) return 0;
    if (modified && alert("This document has changes that are not saved.\nOpening from the share replaces it.", "Discard", "Cancel", 0)) return 0;

    strcpy(pathbuf, browsedir[0] ? browsedir : progdir);
    strcat(pathbuf, "/");
    strcat(pathbuf, names[sel]);
    strcpy(tmppath, "files/");
    strcat(tmppath, names[sel]);
    strcpy(msg, "Getting ");
    strcat(msg, names[sel]);
    strcat(msg, "...  Esc stops.");
    line(LY0 - 12, 0, msg);
    _filetype = PRODOS_T_BIN;
    _auxtype = 0;
    fd = open(pathbuf, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { show_msg("Can't create the file here.\nSet a documents folder in Startup Preferences."); return 0; }
    sink = 1; got = 0;
    ok = http("GET", tmppath, "", 0, 0) && http_status == 200;
    close(fd);
    if (!ok) { remove(pathbuf); net_fail(); return 0; }
    ftype = FT_ODT;
    return 1;
}

/* ---------------------------------------------------------------------- */

unsigned char __fastcall__ net_command(unsigned char c)
{
    unsigned char b, r = 0, saved;
    (void)c;
    if (!find_card()) { show_msg("No CoGS card found in slots 1 to 7."); return 0; }
    srv_load();
    saved = docpath[0] && !modified;
    box("CoGS Share");
    for (;;) {
        show_server();
        if (saved) {
            strcpy(msg, "Document: ");
            strcat(msg, strrchr(docpath, '/') + 1);
        } else strcpy(msg, docpath[0] ? "Document: unsaved changes (Save it first to send)" : "Document: not saved yet (Save As first to send)");
        line(PY0 + 32, 0, msg);
        line(LY0, 0, "");
        buttons("Send", "Open", "Address", "Cancel");
        b = run_buttons(0);
        if (b == 2) { srv_ask(); continue; }
        if (b == 3) break;
        if (!server[0] && !srv_ask()) continue;
        if (b == 0) {
            if (!saved) { show_msg("Save the document first (Save or Save As),\nthen send it."); continue; }
            do_send();
            break;
        }
        r = do_open();
        break;
    }
    unbox();
    return r;
}
