# Data View Button Cell of every data view exemplar (type 0x6534284A, group 0x690F693F, instance 0x4A0B6800 + view
# mode) in DataViews.dat: view mode -> (column 0-3, row). Rows 0-3 are page 1, 4-7 page 2 and so on; row 0 of the
# panel (All Off and the page controls) is not part of it. Page 1 is the Maxis layout.
LAYOUT = {
    # page 1: the Maxis buttons in their Maxis cells
    0x05: (0, 0),  # Fire (Fire Hazard button)
    0x03: (0, 1),  # Police (Crime button)
    0x1B: (0, 2),  # EQ (Education button)
    0x28: (0, 3),  # R$ Desirability (Desirability button, which opens the desirability sub views)
    0x0A: (1, 0),  # Water
    0x07: (1, 1),  # Power
    0x09: (1, 2),  # Traffic (Traffic button, which opens the traffic sub views)
    0x1E: (1, 3),  # Zones
    0x02: (2, 0),  # Land value
    0x01: (2, 1),  # Mayor rating
    0x20: (2, 2),  # Age (Resident Avg. Age button)
    0x1C: (2, 3),  # HQ (Health button)
    0x08: (3, 0),  # Air Pollution
    0x18: (3, 1),  # Water Pollution
    0x19: (3, 2),  # Garbage Pollution
    0x1A: (3, 3),  # Radiation Pollution
    # page 2: the data views of the game that had no button, and the ones of this DLL
    0x04: (0, 4),  # Crime (crime map without the police coverage)
    0x06: (0, 5),  # Flammability
    0x1D: (0, 6),  # Moisture
    0x14: (2, 4),  # Residential Desirability
    0x15: (2, 5),  # Commercial Service Desirability
    0x16: (2, 6),  # Commercial Office Desirability
    0x17: (2, 7),  # Industrial Desirability
    0x0C: (3, 4),  # Park Aura
    0x0D: (3, 5),  # Landmark Aura
    0x0E: (3, 6),  # Transient Aura
}

# Buttons of the Maxis UI (their IDs stay, so mods that re-purpose them keep working): view mode -> button ID.
MAXIS_BUTTONS = {
    0x05: 0x5002, 0x03: 0x5003, 0x1B: 0x5004, 0x28: 0x5005, 0x0A: 0x5006, 0x07: 0x5007, 0x09: 0x5008, 0x20: 0x5009,
    0x02: 0x500A, 0x01: 0x500B, 0x1E: 0x500C, 0x1C: 0x500D, 0x08: 0x500E, 0x18: 0x500F, 0x19: 0x5010, 0x1A: 0x5011,
}
CUSTOM_BUTTON_BASE = 0x5200  # any other data view: button 0x5200 + view mode

# Captions of the new buttons (LTEXT group 0x6A554AFD; Moisture, Park Aura and Landmark Aura keep their 1.0 texts).
CAPTIONS = {
    0x04: 'Crime Map', 0x06: 'Flammability', 0x0E: 'Transient Aura',
    0x14: 'Residential Desir.', 0x15: 'Comm. Service Desir.', 0x16: 'Comm. Office Desir.', 0x17: 'Industrial Desir.',
}
CAPTION_1_0 = {0x1D: 0x4AD045DD, 0x0C: 0x4AD045DE, 0x0D: 0x4AD045DF}
