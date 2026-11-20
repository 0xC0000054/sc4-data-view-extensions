#!/usr/bin/env python3
# Builds the two DAT files of SC4DataViewExtensions:
#
#   python tools/make_data_view_ui.py <SimCity_1.dat> "dat/source/ExtendedDataViewUI-1.0.dat" \
#       "dat/source/z_DataView - TransientAura.dat" dat
#
# DataViews.dat     every data view of the game (the exemplars of SimCity_1.dat with a DataView: Data source) and
#                   the ones of the DLL (Moisture, Park Aura, Landmark Aura, Transient Aura), each listed one with the
#                   property Data View Button Cell (0x5C4DF150: column, row) from tools/data_view_layout.py. The zone
#                   desirability data views also get the legend of the Maxis desirability sub views (LEGENDS).
# ExtendedDataViewUI.dat
#                   the Data Views panel of the game (UI 0x96A006B0 / 0x2BC9060F) with a button for every data view
#                   that has a cell: the Maxis buttons keep their IDs and cells (page 1), the others are buttons
#                   0x5200 + view mode, hidden when they are not on page 1. Row 0 holds All Off and the page controls.
#                   Also the button template UI 0x96A006B0 / 0x5C4DF300 (one radio button, ID 0x5C4DF301): the DLL
#                   creates from it the button of a data view that has a cell but no button in the UI.
#
# The DLL reads the cells when the panel opens, so a cell changed in DataViews.dat (or in a later plugin) moves the
# button without a new UI, and a data view added by another plugin gets a button created at run time. See README.md,
# "Data View Button Cell".
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dbpf2
from data_view_layout import LAYOUT, MAXIS_BUTTONS, CUSTOM_BUTTON_BASE, CAPTIONS, CAPTION_1_0, LEGENDS, LEGEND_TEMPLATE

UI_KEY = (0x00000000, 0x96A006B0, 0x2BC9060F)
LTEXT = 0x2026960B
PNG = 0x856DDBAC
EXEMPLAR, DATA_VIEW_GROUP, DATA_VIEW_BASE = 0x6534284A, 0x690F693F, 0x4A0B6800
PROP_DATA_SOURCE = 0x4A0B47E5
PROP_BUTTON_CELL = 0x5C4DF150
BUTTON_TEXT_GROUP = 0x6A554AFD
CAPTION_BASE = 0x5C4DF200          # LTEXT of a new button: CAPTION_BASE + view mode
BUTTON_TEMPLATE_UI, BUTTON_TEMPLATE_ID = 0x5C4DF300, 0x5C4DF301

GRID = 0x8A2871D6
NAV_PREV, NAV_NEXT, NAV_LABEL = 0x5C4DF1F0, 0x5C4DF1F1, 0x5C4DF1F2
ROW, PAGE_ROWS = 18, 4
COLUMN_X = ((4, 124), (128, 212), (212, 352), (356, 502))
TRANSIENT_MODE = 0x0E

def ltext(s):
    return struct.pack('<HH', len(s), 0x1000) + s.encode('utf-16-le')

def cell_area(column, row):
    top = 1 + ROW * (row % PAGE_ROWS + 1)
    return COLUMN_X[column][0], top, COLUMN_X[column][1], top + ROW

def set_area(line, a):
    out, n = re.subn(r'area=\([^)]*\)', 'area=(%d,%d,%d,%d)' % a, line, count=1)
    assert n == 1
    return out

def wid(line):
    m = re.search(r' id=0x([0-9a-fA-F]{8})', line)
    return int(m.group(1), 16) if m else None

# Page buttons: the Maxis "Show previous / next picture" arrows (UI 0x4A8CC5EA, images 0x1ABE787D / 0x14416272 and
# 0x14416273, 4 states of 20 x 24), scaled to the 18-pixel rows of the grid (4 states of 15 x 18).
NAV_IMAGE_GROUP = 0x1ABE787D
NAV_IMAGES = {NAV_PREV: (0x14416272, 0x5C4DF1E0), NAV_NEXT: (0x14416273, 0x5C4DF1E1)}   # button: (Maxis, scaled)
NAV_W, NAV_H = 15, 18
NAV_LABEL_W = 50
NAV_BUTTON = ('<LEGACY clsid=GZWinBtn iid=IGZWinBtn id=0x%08x area=(%d,%d,%d,%d) fillcolor=(192,192,192) '
    'winflag_visible=no winflag_enabled=yes winflag_moveable=yes winflag_sizeable=no winflag_sortable=no winflag_pbuff=no '
    'winflag_pbufftrans=yes winflag_pbufferase=yes winflag_pbuffvid=no winflag_alphablend=yes winflag_acceptfocus=yes '
    'winflag_mousetrans=no winflag_ignoremouse=no image={%08x,%08x} colorfontnormal=(0,0,0) colorfontdisabled=(0,0,0) '
    'colorfonthilited=(0,0,0) colorfontnormalbkg=(0,0,0) colorfontdisabledbkg=(0,0,0) colorfonthilitedbkg=(0,0,0) '
    'toggle=off triggerondown=off showcaption=no fill=yes autosize=no wrapcaption=no shiftcaption=yes tips=yes '
    'tipsdelay=no tipstimeout=no style=standard gutters=(0,0,0,0) tiptext="%s" tipoffsets=(0,0) tipflag=0x01000000 '
    'align=center btnclicksnd={ca47efd9,4a5c31d7} >')

def nav_images(game_entries):
    """The two arrow strips scaled to NAV_W x NAV_H per state, as PNG entries."""
    import io
    from PIL import Image
    out = []
    for maxis, scaled in NAV_IMAGES.values():
        e = next(e for e in game_entries if (e['t'], e['g'], e['i']) == (PNG, NAV_IMAGE_GROUP, maxis))
        src = Image.open(io.BytesIO(e['data'])).convert('RGBA')
        w, h = src.width // 4, src.height
        strip = Image.new('RGBA', (4 * NAV_W, NAV_H))
        for state in range(4):
            strip.paste(src.crop((w * state, 0, w * state + w, h)).resize((NAV_W, NAV_H), Image.LANCZOS), (NAV_W * state, 0))
        buf = io.BytesIO()
        strip.save(buf, 'PNG')
        out.append(dict(t=PNG, g=NAV_IMAGE_GROUP, i=scaled, data=buf.getvalue()))
    return out

NAV_TEXT = ('<LEGACY clsid=GZWinText iid=IGZWinText id=0x%08x area=(%d,%d,%d,%d) fillcolor=(0,136,0) caption="%s" '
    'winflag_visible=no winflag_enabled=yes winflag_moveable=yes winflag_sizeable=no winflag_sortable=no winflag_pbuff=no '
    'winflag_pbufftrans=yes winflag_pbufferase=yes winflag_pbuffvid=no winflag_alphablend=no winflag_acceptfocus=yes '
    'winflag_mousetrans=no winflag_ignoremouse=yes font=GenBodyLight align=center notify=no wrapped=no opaque=no '
    'forecolor=(63,73,103) bkgcolor=(0,0,0) gutters=(0,0) textoffsets=(0,0) >')

def caption_key(mode):
    return CAPTION_1_0.get(mode, CAPTION_BASE + mode)

def build_ui(text, pages):
    lines = text.split('\r\n')
    start = next(i for i, l in enumerate(lines) if 'id=0x%08x' % GRID in l)
    assert lines[start + 1].strip() == '<CHILDREN>'
    end = start + 2
    while lines[end].strip() != '</CHILDREN>':
        end += 1
    children = lines[start + 2:end]
    indent = re.match(r'\s*', children[0]).group(0)
    ids = [wid(l) for l in children]
    assert ids.count(0x5001) == 1 and all(0x5001 <= i <= 0x51FF for i in ids), 'not the Maxis button grid'

    # The Maxis buttons must already be in the cells of page 1.
    for mode, button in MAXIS_BUTTONS.items():
        line = children[ids.index(button)]
        a = tuple(int(v) for v in re.search(r'area=\(([^)]*)\)', line).group(1).split(','))
        assert a == cell_area(*LAYOUT[mode]), (hex(button), a)

    template = children[ids.index(0x5002)].strip()   # Fire Hazard: a plain radio button
    new = []
    for mode, (column, row) in sorted(LAYOUT.items(), key=lambda kv: (kv[1][1], kv[1][0])):
        if mode in MAXIS_BUTTONS:
            continue
        line = template.replace(' id=0x00005002', ' id=0x%08x' % (CUSTOM_BUTTON_BASE + mode), 1)
        line = re.sub(r'caption="[^"]*" captionres=\{[^}]*\}',
                      'caption="%s" captionres={%08x,%08x}' % (CAPTIONS.get(mode, 'Data View'), BUTTON_TEXT_GROUP,
                                                                caption_key(mode)), line, count=1)
        line = set_area(line, cell_area(column, row))
        if row >= PAGE_ROWS:
            line = line.replace('winflag_visible=yes', 'winflag_visible=no', 1)
        new.append(line)

    right = COLUMN_X[3][1]                 # arrows and page number as one group at the right end of row 0
    left = right - 2 * NAV_W - NAV_LABEL_W
    nav = [NAV_BUTTON % ((NAV_PREV, left, 1, left + NAV_W, 1 + NAV_H, NAV_IMAGE_GROUP, NAV_IMAGES[NAV_PREV][1], 'Previous page')),
           NAV_TEXT % (NAV_LABEL, left + NAV_W, 1, right - NAV_W, 1 + ROW, '1 / %d' % pages),
           NAV_BUTTON % ((NAV_NEXT, right - NAV_W, 1, right, 1 + NAV_H, NAV_IMAGE_GROUP, NAV_IMAGES[NAV_NEXT][1], 'Next page'))]
    lines[end:end] = [indent + l for l in new + nav]
    return '\r\n'.join(lines)

def button_template(text):
    """The Maxis Fire Hazard button (0x5002) as a one-window UI: ID BUTTON_TEMPLATE_ID, no caption, at the origin."""
    line = next(l for l in text.split('\r\n') if ' id=0x00005002 ' in l).strip()
    line = line.replace(' id=0x00005002 ', ' id=0x%08x ' % BUTTON_TEMPLATE_ID, 1)
    line = re.sub(r'caption="[^"]*" captionres=\{[^}]*\} ', 'caption="" ', line, count=1)
    line = set_area(line, (0, 0, COLUMN_X[0][1] - COLUMN_X[0][0], ROW))
    assert 'style=radiocheck' in line and 'captionres' not in line and 'winflag_visible=yes' in line
    return ('# SC4DataViewExtensions button template (copy of the Maxis Fire Hazard button)\r\n' + line + '\r\n').encode('latin1')

LEGEND_PROPS = (0x4A0B47ED, 0x4A0B47F2, 0x4A0B47F3)   # legend style, colours, texts (copied)
PROP_LEGEND_HEADER = 0x4A0B47F4
LEGEND_TEXT_GROUP = 0x6A231EAA

def properties(data):
    """Binary exemplar -> [(id, raw record)]."""
    assert data[:8] == b'EQZB1###'
    size = {1: 1, 2: 2, 3: 4, 7: 4, 8: 8, 9: 4, 0xB: 1, 0xC: 1}
    n, p, out = struct.unpack_from('<I', data, 20)[0], 24, []
    for _ in range(n):
        start = p
        pid, typ, kt = struct.unpack_from('<IHH', data, p)
        p += 9
        p += 4 + size[typ >> 8] * struct.unpack_from('<I', data, p)[0] if kt & 0x80 else size[typ >> 8]
        out.append((pid, data[start:p]))
    assert p == len(data)
    return out

def add_legend(data, template, header):
    """Copies the legend of template into data (a data view without one) with its own header LTEXT instance."""
    props = [(pid, raw) for pid, raw in properties(data) if pid not in LEGEND_PROPS + (PROP_LEGEND_HEADER,)]
    legend = [raw for pid, raw in properties(template) if pid in LEGEND_PROPS]
    assert len(legend) == len(LEGEND_PROPS), 'template without a legend'
    legend.append(struct.pack('<IHHBI', PROP_LEGEND_HEADER, 0x0300, 0, 0, header))
    out = [raw for _, raw in props] + legend
    return data[:20] + struct.pack('<I', len(out)) + b''.join(out)

def add_cell(data, column, row):
    """Appends Data View Button Cell (Uint32 x 2) to a binary exemplar (EQZB1###)."""
    assert data[:8] == b'EQZB1###'
    count = struct.unpack_from('<I', data, 20)[0]
    assert struct.pack('<I', PROP_BUTTON_CELL) not in data[24:]
    prop = struct.pack('<IHHBI', PROP_BUTTON_CELL, 0x0300, 0x80, 0, 2) + struct.pack('<II', column, row)
    return data[:20] + struct.pack('<I', count + 1) + data[24:] + prop

def rename_exemplar(data, name):
    key = struct.pack('<IHH', 0x20, 0x0C00, 0x80)
    p = data.index(key) + 8 + 1
    n = struct.unpack_from('<I', data, p)[0]
    return data[:p] + struct.pack('<I', len(name)) + name.encode('latin1') + data[p + 4 + n:]

def main(game, ui_1_0, transient, out_dir):
    game_entries = dbpf2.read(game)[1]
    v10 = dbpf2.read(ui_1_0)[1]
    tr = dbpf2.read(transient)[1]

    # Data views: the game's, then the 1.0 ones over them (Moisture with a colour ramp, Park and Landmark Aura), then
    # Transient Aura as its own data view (view mode 14) instead of a replacement of Garbage Pollution.
    views = {}
    for e in game_entries:
        if e['t'] == EXEMPLAR and e['g'] == DATA_VIEW_GROUP and DATA_VIEW_BASE <= e['i'] <= DATA_VIEW_BASE + 0xFF \
                and struct.pack('<I', PROP_DATA_SOURCE) in e['data']:
            views[e['i']] = e['data']
    texts = []
    for e in v10 + tr:
        if e['t'] == EXEMPLAR and e['g'] == DATA_VIEW_GROUP:
            if e['i'] == 0x4A0B6819:
                views[DATA_VIEW_BASE + TRANSIENT_MODE] = rename_exemplar(e['data'], 'DataView: Transient Aura')
            else:
                views[e['i']] = e['data']
        elif e['t'] == LTEXT and e['g'] != BUTTON_TEXT_GROUP:
            texts.append(e)                               # legend texts
    for mode, (header, text) in LEGENDS.items():
        views[DATA_VIEW_BASE + mode] = add_legend(views[DATA_VIEW_BASE + mode], views[DATA_VIEW_BASE + LEGEND_TEMPLATE], header)
        texts.append(dict(t=LTEXT, g=LEGEND_TEXT_GROUP, i=header, data=ltext(text)))
    for mode, cell in LAYOUT.items():
        assert DATA_VIEW_BASE + mode in views, hex(mode)
        views[DATA_VIEW_BASE + mode] = add_cell(views[DATA_VIEW_BASE + mode], *cell)
    data_views = [dict(t=EXEMPLAR, g=DATA_VIEW_GROUP, i=i, data=d) for i, d in sorted(views.items())] + texts

    pages = max(row for _, row in LAYOUT.values()) // PAGE_ROWS + 1
    ui = next(e for e in game_entries if (e['t'], e['g'], e['i']) == UI_KEY)
    ui_entries = [dict(t=UI_KEY[0], g=UI_KEY[1], i=UI_KEY[2],
                       data=build_ui(ui['data'].decode('latin1'), pages).encode('latin1'))]
    ui_entries.append(dict(t=UI_KEY[0], g=UI_KEY[1], i=BUTTON_TEMPLATE_UI, data=button_template(ui['data'].decode('latin1'))))
    ui_entries += [e for e in v10 if e['t'] == LTEXT and e['g'] == BUTTON_TEXT_GROUP]
    ui_entries += nav_images(game_entries)
    ui_entries += [dict(t=LTEXT, g=BUTTON_TEXT_GROUP, i=CAPTION_BASE + m, data=ltext(c)) for m, c in sorted(CAPTIONS.items())]

    os.makedirs(out_dir, exist_ok=True)
    for name, entries in (('DataViews.dat', data_views), ('ExtendedDataViewUI.dat', ui_entries)):
        keys = [(e['t'], e['g'], e['i']) for e in entries]
        assert len(keys) == len(set(keys)), name
        dbpf2.write(os.path.join(out_dir, name), entries)
        print('%s: %d entries' % (name, len(entries)))
    print('%d data views, %d with a button cell, %d pages' % (len(views), len(LAYOUT), pages))

if __name__ == '__main__':
    main(*sys.argv[1:5])
