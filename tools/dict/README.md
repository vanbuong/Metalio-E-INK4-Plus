# Reader dictionaries

Long-press a word while reading to see its English→Vietnamese and English→English meanings.
The dictionaries live on the SD card; no dictionary data ships with the firmware.

## Prepare

Any StarDict dictionary works (`.ifo` + `.idx`/`.idx.gz` + `.dict`/`.dict.dz`). Convert it once
on a PC (Python 3, standard library only):

```
python tools/dict/prepare_stardict.py path/to/en-vi.ifo --lang en-vi --out dict-out
python tools/dict/prepare_stardict.py path/to/wordnet.ifo --lang en-en --out dict-out
```

The script unpacks the files, turns HTML/markup entries into plain text, drops phonetic and media
fields, sorts the index and writes a `.pos` offset table the device uses for fast lookup.

## Copy to the SD card

```
/dict/<name>/<name>.ifo
/dict/<name>/<name>.idx
/dict/<name>/<name>.dict
/dict/<name>/<name>.pos
```

Copy each folder from `dict-out/` into `/dict/` on the SD card (USB mass storage works).
English→Vietnamese dictionaries are shown first, then English→English.

## Licences

Check the licence of every dictionary you use. WordNet-based English dictionaries are under the
permissive WordNet licence. English–Vietnamese dictionaries come from many sources with
different terms; using them is your responsibility. Do not commit dictionary data to this repo.
