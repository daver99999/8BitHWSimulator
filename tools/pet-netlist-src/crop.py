"""crop.py - crop a region of a schematic scan to read it at full size.

    python3 crop.py SHEET X0 Y0 X1 Y1 OUT.png

SHEET is the sheet number of the logic diagram 8032081 (a PNG named 8032081-SHEET.png in the
folder PET_SHEETS, default ./sheets: the scans are Commodore's and are not in this repository).
The box is in display coordinates of a picture 2000 pixels wide; OUT.png goes in PET_CROPS
(default ./crops)."""
import os, sys
from PIL import Image
sheet, x0, y0, x1, y1, out = sys.argv[1], *map(int, sys.argv[2:6]), sys.argv[6]
im = Image.open(os.path.join(os.environ.get('PET_SHEETS', 'sheets'), '8032081-%s.png' % sheet)).convert('L')
k = 3296 / 2000.0                      # display coords -> original pixels
c = im.crop((int(x0 * k), int(y0 * k), int(x1 * k), int(y1 * k)))
os.makedirs(os.environ.get('PET_CROPS', 'crops'), exist_ok=True)
c.save(os.path.join(os.environ.get('PET_CROPS', 'crops'), out))
print(out, c.size)
