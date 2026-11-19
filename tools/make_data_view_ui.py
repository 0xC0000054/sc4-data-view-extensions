#!/usr/bin/env python3
# Builds ExtendedDataViewUI.dat with pages in the Data Views panel.
#
#   python make_data_view_ui.py "dat/source/ExtendedDataViewUI-1.0.dat" "dat/source/z_DataView - TransientAura.dat" dat
#
# The button grid of the panel (window 0x8A2871D6, 4 columns x 5 rows of 18 pixels) keeps the Maxis size and position.
# Row 0 is the same on every page: All Off, Moisture, Park Aura and the page controls (< / page number / >).
# Rows 1-4 are the pages: page 1 holds the 16 Maxis data views in their Maxis cells, page 2 Landmark Aura and the
# Transient Aura data view, which gets its own button (view mode 14, exemplar 0x4A0B680E) instead of replacing
# Garbage Pollution. Each page is a container window 0x5C4DF100 + page; the DLL shows one page at a time
# (see README.md, "Data view pages").
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dbpf2

UI_TYPE, UI_GROUP, UI_INSTANCE = 0x00000000, 0x96A006B0, 0x2BC9060F
LTEXT = 0x2026960B
EXEMPLAR, DATA_VIEW_GROUP = 0x6534284A, 0x690F693F
BUTTON_TEXT_GROUP = 0x6A554AFD

CONTAINER = 0x8A2871D6
PAGE_BASE = 0x5C4DF100          # page N (N >= 1) = PAGE_BASE + N
NAV_PREV, NAV_NEXT, NAV_LABEL = 0x5C4DF1F0, 0x5C4DF1F1, 0x5C4DF1F2
ROW = 18
COLUMN_X = ((4, 124), (128, 212), (212, 352), (356, 502))   # the Maxis cells of the four columns
ROW0_IDS = {0x5001, 0x5101, 0x5012, 0x5112, 0x5013, 0x5113}   # All Off, Moisture, Park Aura: on every page
LANDMARK, LANDMARK2 = 0x5014, 0x5114
TRANSIENT, TRANSIENT2 = 0x5015, 0x5115
TRANSIENT_MODE = 14             # view mode = data view exemplar instance - 0x4A0B6800
TRANSIENT_CAPTION = 0x4AD045E0  # "Transient Aura" in group 0x6A554AFD

def ltext(s):
    return struct.pack('<HH', len(s), 0x1000) + s.encode('utf-16-le')

def set_area(line, l, t, r, b):
    out, n = re.subn(r'area=\([^)]*\)', 'area=(%d,%d,%d,%d)' % (l, t, r, b), line, count=1)
    assert n == 1
    return out

def area(line):
    return tuple(int(v) for v in re.search(r'area=\(([^)]*)\)', line).group(1).split(','))

def wid(line):
    m = re.search(r' id=0x([0-9a-fA-F]{8})', line)
    return int(m.group(1), 16) if m else None

NAV_BUTTON = ('<LEGACY clsid=GZWinBtn iid=IGZWinBtn id=0x%08x area=(%d,%d,%d,%d) fillcolor=(156,178,194) caption="%s" '
    'winflag_visible=no winflag_enabled=yes winflag_moveable=yes winflag_sizeable=no winflag_sortable=no winflag_pbuff=no '
    'winflag_pbufftrans=yes winflag_pbufferase=yes winflag_pbuffvid=no winflag_alphablend=yes winflag_acceptfocus=yes '
    'winflag_mousetrans=no winflag_ignoremouse=no font=GenBodyLight colorfontnormal=(63,73,103) '
    'colorfontdisabled=(102,102,102) colorfonthilited=(255,255,255) colorfontnormalbkg=(0,0,0) '
    'colorfontdisabledbkg=(0,0,0) colorfonthilitedbkg=(0,0,0) toggle=off triggerondown=off showcaption=yes '
    'fill=no autosize=no wrapcaption=no shiftcaption=no tips=no tipsdelay=no tipstimeout=no style=standard '
    'gutters=(0,0,0,0) tiptext="" tipoffsets=(0,0) tipflag=0x01000000 align=center btnclicksnd={ca4d1943,2a5c322b} >')

NAV_TEXT = ('<LEGACY clsid=GZWinText iid=IGZWinText id=0x%08x area=(%d,%d,%d,%d) fillcolor=(0,136,0) caption="%s" '
    'winflag_visible=no winflag_enabled=yes winflag_moveable=yes winflag_sizeable=no winflag_sortable=no winflag_pbuff=no '
    'winflag_pbufftrans=yes winflag_pbufferase=yes winflag_pbuffvid=no winflag_alphablend=no winflag_acceptfocus=yes '
    'winflag_mousetrans=no winflag_ignoremouse=yes font=GenBodyLight align=center notify=no wrapped=no opaque=no '
    'forecolor=(63,73,103) bkgcolor=(0,0,0) gutters=(0,0) textoffsets=(0,0) >')

def page_container(lines, page, visible):
    """A transparent container over rows 1-4 (copy of the flags of the collapsed panel 0x8A2871B2)."""
    src = next(l for l in lines if 'id=0x8a2871b2' in l).strip()
    line = src.replace('id=0x8a2871b2', 'id=0x%08x' % (PAGE_BASE + page))
    line = set_area(line, 0, ROW, COLUMN_X[3][1], 5 * ROW + 1)
    line = line.replace('winflag_visible=yes', 'winflag_visible=%s' % ('yes' if visible else 'no'), 1)
    return line

def build_ui(text, pages):
    lines = text.split('\r\n')
    assert 'id=0x%08x' % (PAGE_BASE + 1) not in text, 'the UI already has pages'
    start = next(i for i, l in enumerate(lines) if 'id=0x%08x' % CONTAINER in l)
    assert lines[start + 1].strip() == '<CHILDREN>'
    end = start + 2
    while lines[end].strip() != '</CHILDREN>':
        end += 1
    children = lines[start + 2:end]
    assert all(l.strip().startswith('<LEGACY') for l in children), 'nested windows in the button grid'
    indent = re.match(r'\s*', children[0]).group(0)

    row0, page1 = [], []
    landmark = {}
    for l in children:
        i = wid(l)
        if i in (LANDMARK, LANDMARK2):
            landmark[i] = l.strip()
        elif i in ROW0_IDS or (i & 0xFF00) == 0x5100:   # alternates (0x51xx) sit below the grid, hidden
            row0.append(l.strip())
        else:
            assert 0x5002 <= i <= 0x5011, hex(i)
            x0, y0, x1, y1 = area(l)
            assert y0 >= ROW, (hex(i), area(l))
            page1.append(set_area(l.strip(), x0, y0 - ROW, x1, y1 - ROW))
    assert len(page1) == 16 and len(landmark) == 2

    left, right = COLUMN_X[3]
    nav = [NAV_BUTTON % (NAV_PREV, left, 1, left + 36, 1 + ROW, '<'),
           NAV_TEXT % (NAV_LABEL, left + 36, 1, right - 36, 1 + ROW, '1 / %d' % pages),
           NAV_BUTTON % (NAV_NEXT, right - 36, 1, right, 1 + ROW, '>')]

    def cell(line, col, row, new_id=None, caption=None):
        if new_id is not None:
            line = line.replace(' id=0x%08x' % wid(line), ' id=0x%08x' % new_id, 1)
        if caption is not None:
            line = re.sub(r'caption="[^"]*" captionres=\{[^}]*\}', caption, line, count=1)
        if line.find('winflag_visible=no') != -1 and (wid(line) & 0xFF00) == 0x5100:
            # alternate button: same column, hidden below the visible rows, as in the Maxis UI
            return set_area(line, COLUMN_X[col][0] + 17, 82 + ROW * row, COLUMN_X[col][0] + 120, 100 + ROW * row)
        return set_area(line, COLUMN_X[col][0], 1 + ROW * row, COLUMN_X[col][1], 1 + ROW * (row + 1))

    caption = 'caption="Transient" captionres={%08x,%08x}' % (BUTTON_TEXT_GROUP, TRANSIENT_CAPTION)
    page2 = [cell(landmark[LANDMARK], 0, 0), cell(landmark[LANDMARK2], 0, 0),
             cell(landmark[LANDMARK], 0, 1, TRANSIENT, caption), cell(landmark[LANDMARK2], 0, 1, TRANSIENT2, caption)]

    out = [indent + l for l in row0 + nav]
    for page, items in ((1, page1), (2, page2)):
        out.append(indent + page_container(lines, page, page == 1))
        out.append(indent + '<CHILDREN>')
        out += [indent + ' ' + l for l in items]
        out.append(indent + '</CHILDREN>')
    lines[start + 2:end] = out
    return '\r\n'.join(lines)

def rename_exemplar(data, name):
    """Replaces the Exemplar Name (0x00000020) string of a binary exemplar."""
    key = struct.pack('<IHH', 0x20, 0x0C00, 0x80)
    p = data.index(key) + 8 + 1
    n = struct.unpack_from('<I', data, p)[0]
    return data[:p] + struct.pack('<I', len(name)) + name.encode('latin1') + data[p + 4 + n:]

def main(src, transient_src, out_dir):
    entries = dbpf2.read(src)[1]
    ui = next(e for e in entries if (e['t'], e['g'], e['i']) == (UI_TYPE, UI_GROUP, UI_INSTANCE))
    ui['data'] = build_ui(ui['data'].decode('latin1'), 2).encode('latin1')
    entries.append(dict(t=LTEXT, g=BUTTON_TEXT_GROUP, i=TRANSIENT_CAPTION, data=ltext('Transient Aura')))

    for e in dbpf2.read(transient_src)[1]:
        if e['t'] == EXEMPLAR and e['g'] == DATA_VIEW_GROUP:
            assert e['i'] == 0x4A0B6819          # the optional DAT replaces Garbage Pollution
            e = dict(e, i=0x4A0B6800 + TRANSIENT_MODE, data=rename_exemplar(e['data'], 'DataView: Transient Aura'))
            assert b'\x0e\x00\x00\x00' in e['data']
        entries.append(e)
    keys = [(e['t'], e['g'], e['i']) for e in entries]
    assert len(keys) == len(set(keys))
    os.makedirs(out_dir, exist_ok=True)
    dbpf2.write(os.path.join(out_dir, 'ExtendedDataViewUI.dat'), entries)

if __name__ == '__main__':
    main(*sys.argv[1:4])
