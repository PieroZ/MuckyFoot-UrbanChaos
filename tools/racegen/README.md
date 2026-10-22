# racegen – generator toru wyścigowego (mapa 256×256)

Skrypt tworzy kompletną misję na dużej mapie:

| plik | co to jest |
|---|---|
| `data/<nazwa>.iam` | mapa 256×256 (trawa, pagórki, asfaltowy tor z liniami, drzewa) |
| `data/Lighting/<nazwa>.lgt` | oświetlenie (lampy co ~22 pola wzdłuż toru) |
| `levels/<nazwa>.ucm` | misja: `testdrive1a.ucm` (3 okrążenia radiowozem na czas) przeniesiona na nowy tor |
| `clumps/<nazwa>.txc` | tekstury (kopia `testdrive1a.txc` – mapa używa tylko tych tekstur) |

## Użycie

```
python racegen.py --game "C:\Urban Chaos Retail" --install            # wprost do gry + wpis w data\urban.sty
python racegen.py --game "C:\Urban Chaos Retail" --out out            # tylko wygeneruj do .\out
python racegen.py --game "C:\Urban Chaos Retail" --install --name tor2 --title "Tor 2" --seed 12
```

Opcje: `--seed` (inny kształt toru), `--width` (szerokość toru w polach, domyślnie 9),
`--name`/`--title` (nazwa pliku / nazwa w menu). Potrzebny Python 3; Pillow tylko do podglądu PNG.

`--install` robi kopię `data\urban.sty.bak` przed dopisaniem misji (dystrykt „Oval Circuit”,
dostępna od początku).

## Ograniczenia dużych map

* Grać tylko exe zbudowanym z tego forka (oryginalny `fallen.exe` nie wczyta mapy 256×256).
* Na dużych mapach nie ma budynków z ukrytymi dachami / magazynów / kanałów poza obszarem 128×128
  (kodowanie `ROOF_HIDDEN` zostało 7-bitowe) – generator w ogóle nie stawia budynków.
* Strefy misji (`.ucm`) dalej obejmują tylko 128×128.
* Stare zapisy quicksave (wersja 1) nie są wczytywane – format ma teraz wersję 2.
