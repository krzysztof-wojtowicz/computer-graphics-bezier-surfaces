# Powierzchnie Béziera – pomoc

## O programie

Program wyświetla powierzchnie Béziera trzeciego stopnia wczytane z pliku tekstowego. Każdą powierzchnię dzieli na siatkę trójkątów, obraca całą scenę myszą, wypełnia trójkąty z buforem głębokości (Z-buffer) i oświetla je modelem Lamberta ze składową zwierciadlaną. Źródło światła krąży po zamkniętej, edytowalnej krzywej Béziera. Kolor obiektu może pochodzić z koloru jednolitego albo z tekstury, a wektor normalny można zmodyfikować mapą wektorów normalnych (normal mapą) – osobno dla każdej powierzchni.

## Szybki start

1. Przeciągnij myszą w obszarze rysowania (lewy przycisk), żeby obrócić scenę.
2. `Space` zatrzymuje i wznawia ruch światła.
3. `T` włącza teksturę, `N` włącza normal mapę – na powierzchni z otwartej zakładki „Surface 1”–„Surface 3”.
4. `R` przywraca początkowy obrót sceny.
5. `F1` otwiera to okno; przycisk „English” na dole przełącza język pomocy.

## Okno programu

Po lewej jest obszar rysowania, po prawej panel sterowania. Początek układu współrzędnych leży w środku obszaru rysowania; scena zawsze mieści się w oknie, niezależnie od obrotu.

### Triangulation

- „Resolution n” – dokładność triangulacji (od 1 do 50). Każda powierzchnia ma `(n+1)²` wierzchołków i `2n²` trójkątów.

### Display

Pola wyboru decydują, co jest rysowane:

| Pole | Co rysuje |
|---|---|
| „Bezier polygon” | wielobok Béziera (siatka 4×4 punktów kontrolnych) i punkty kontrolne |
| „Triangle mesh” | krawędzie trójkątów siatki |
| „Mesh points” | wierzchołki siatki |
| „Triangle fill” | wypełnione, oświetlone trójkąty |
| „Light path” | krzywą światła, jej wielobok kontrolny, punkty kontrolne i aktualne położenie światła |

### Zakładki „Surface 1” – „Surface 3”

Każda powierzchnia ma własną zakładkę i własne ustawienia, niezależne od pozostałych powierzchni. Powierzchnia numer `k` (licząc od 1) należy do zakładki `((k−1) mod 3) + 1`: zakładka „Surface 1” dotyczy powierzchni 1, 4, 7…, „Surface 2” – 2, 5, 8… W każdej zakładce:

- „Visible” – pokazuje lub ukrywa powierzchnię. Ukryta powierzchnia nie zasłania tego, co jest za nią.
- „Solid color” / „Texture” – kolor obiektu `IO` we wzorze oświetlenia: kolor jednolity albo tekstura.
- Próbka koloru obok „Solid color” – kliknięcie otwiera wybór jednolitego koloru powierzchni (ten sam wybór jest w menu „Surface”).
- Lista tekstur i przycisk „Load...” obok „Texture” – wybór wbudowanej tekstury albo wczytanie obrazu z pliku. Wybranie tekstury od razu ją włącza; wczytany plik dopisuje się do listy tej powierzchni.
- „Normal map” – włącza modyfikację wektora normalnego normal mapą, niezależnie od koloru obiektu.
- Lista normal map i przycisk „Load...” obok „Normal map” – wybór wbudowanej normal mapy albo wczytanie jej z pliku.

Na starcie każda powierzchnia ma kolor jednolity, wybraną pierwszą wbudowaną teksturę („Floor”) i normal mapę („Floor”), a normal mapa jest wyłączona.

### Lighting

- „Light path:” „Bezier” / „B-spline” – rodzaj krzywej, po której porusza się światło: jednosegmentowa zamknięta krzywa Béziera albo zamknięta krzywa B-sklejana (B-spline) stopnia 3 (segmenty w różnych kolorach). Obie są zbudowane na tych samych punktach kontrolnych, edytowanych myszą; przełączenie zmienia tylko krzywą. Na starcie „B-spline”.
- „Lights:” „1 light” / „3 lights” – jedno źródło światła albo trzy źródła rozłożone równo na tej samej krzywej (co 1/3 okrążenia), poruszające się razem i oświetlające scenę z trzech stron. Przy trzech źródłach są to reflektory skierowane zawsze w punkt (0, 0, 0) sceny, o stałych kolorach: czerwony, zielony i niebieski (każdy z pełną składową); strzałka przy każdym reflektorze pokazuje, w którą stronę świeci (im krótsza, tym bardziej w dół). Pojedyncze źródło świeci jak dotąd we wszystkie strony, w wybranym kolorze. Wszystkie mają ten sam kolor `IL`, a ich wkłady się sumują. Na starcie „1 light”.
- „kd”, „ks” (od 0 do 1) – udział składowej rozproszonej i zwierciadlanej.
- „m” (od 1 do 100) – im większe, tym mniejszy i ostrzejszy odblask.
- „z” (od 0 do 20) – wysokość płaszczyzny, na której leży krzywa światła.
- „spot” (od 1 do 100, domyślnie 10) – wykładnik `k` wiązki reflektorów `I = IL · cos^k(α)`: im większy, tym węższa i bardziej skupiona plama światła. Aktywny tylko przy „3 lights”.
- „Light color...” – kolor światła `IL` (domyślnie biały); dotyczy tylko pojedynczego źródła, przy „3 lights” jest nieaktywny.
- „Stop animation” / „Start animation” – zatrzymuje i wznawia ruch światła.

## Menu

| Menu | Pozycja | Działanie |
|---|---|---|
| „File” | „Quit” | zamyka program |
| „Scene” | „Reset rotation” | przywraca początkowy obrót sceny |
| „Scene” | „Full screen” | przełącza tryb pełnoekranowy |
| „Surface” | „Texture” | przełącza kolor obiektu powierzchni z otwartej zakładki: tekstura / kolor jednolity |
| „Surface” | „Normal map” | włącza i wyłącza normal mapę powierzchni z otwartej zakładki |
| „Surface” | „Load texture...” | wczytuje z pliku teksturę powierzchni z otwartej zakładki |
| „Surface” | „Load normal map...” | wczytuje z pliku normal mapę powierzchni z otwartej zakładki |
| „Surface” | „Surface color 1...” – „Surface color 3...” | wybór jednolitego koloru grupy powierzchni |
| „Lighting” | „Animate light” | zatrzymuje i wznawia ruch światła |
| „Lighting” | „Light color...” | wybór koloru światła |
| „Lighting” | „Reset light path” | przywraca domyślny kształt krzywej światła (wysokość `z` zostaje) |
| „Help” | „Documentation” | otwiera to okno |

Zaznaczenie pozycji „Texture” i „Normal map” pokazuje stan powierzchni z otwartej zakładki i zmienia się po przełączeniu zakładki.

## Sterowanie myszą

- **Obrót sceny:** wciśnij lewy przycisk w obszarze rysowania i przeciągnij. Kursor zmienia się w zaciśniętą dłoń. Ruch w prawo i w lewo obraca scenę wokół pionowej osi ekranu, ruch w górę i w dół – wokół poziomej. Przednia część sceny podąża za kursorem. Kąty nie mają ograniczeń. Szczegóły – sekcja „Opis techniczny: zamiana ruchu myszy na obrót”.
- **Edycja krzywej światła:** gdy pole „Light path” jest zaznaczone, wciśnięcie lewego przycisku bliżej niż 8 px od punktu kontrolnego krzywej chwyta ten punkt i przeciąganie go przesuwa (zamiast obracać scenę). Chwytany jest punkt najbliższy kursorowi; nie przeskakuje do kursora i nie da się go wyciągnąć poza okno. Krzywa jest związana z kamerą – obrót sceny jej nie obraca.
- Prawy przycisk myszy nic nie robi.

## Skróty klawiaturowe

Na macOS `Ctrl` oznacza klawisz `⌘` (Command), a klawisze funkcyjne mogą wymagać przytrzymania `fn`.

| Skrót | Działanie |
|---|---|
| `Space` | zatrzymanie / wznowienie ruchu światła |
| `T` | tekstura / kolor jednolity (powierzchnia z otwartej zakładki) |
| `N` | normal mapa wł. / wył. (powierzchnia z otwartej zakładki) |
| `R` | przywrócenie początkowego obrotu sceny |
| `F11` | pełny ekran (na macOS także `Ctrl+⌘+F`) |
| `Ctrl+T` | wczytanie tekstury z pliku (powierzchnia z otwartej zakładki) |
| `Ctrl+Shift+N` | wczytanie normal mapy z pliku (powierzchnia z otwartej zakładki) |
| `Ctrl+1`, `Ctrl+2`, `Ctrl+3` | kolor jednolity powierzchni 1, 2, 3 |
| `Ctrl+L` | kolor światła |
| `Ctrl+R` | przywrócenie domyślnej krzywej światła |
| `F1` | pomoc (na macOS także `⌘?`) |
| `Ctrl+Q` | zamknięcie programu |

## Pliki

### Powierzchnie

Przy starcie program wczytuje plik `surfaces.txt` wbudowany w program (trzy powierzchnie: kopuła, fala, siodło). Każda powierzchnia to 16 kolejnych wierszy, po jednym na punkt kontrolny, każdy z trzema liczbami `x y z` (kropka dziesiętna):

```
X00 Y00 Z00
X01 Y01 Z01
...
X33 Y33 Z33
```

Kolejność to `P00, P01, …, P03, P10, …, P33`; w punkcie `Pij` indeks `i` biegnie wzdłuż parametru `u`, a `j` wzdłuż `v`. Kolejne powierzchnie to kolejne grupy po 16 wierszy. Przy błędzie program podaje numer i treść błędnego wiersza.

### Tekstury i normal mapy

- Formaty: PNG, JPG, BMP, dowolny rozmiar. Kanał alfa jest pomijany.
- Obraz jest rozciągany na całą powierzchnię: `u` od 0 do 1 odpowiada kolumnom od lewej do prawej, `v` od 0 do 1 – wierszom od góry do dołu. Każdy piksel ekranu bierze najbliższy teksel.
- Normal mapy są w konwencji OpenGL: `RGB(128, 128, 255)` oznacza wektor `[0, 0, 1]` (płasko), składowa zielona rośnie w górę obrazu.

## Opis techniczny: zamiana ruchu myszy na obrót

### Konwencje

- Wektory są kolumnowe, punkt po obrocie to `P' = R · P`.
- Obserwator patrzy z `+z`; rysowany jest rzut prostokątny na płaszczyznę `xy`: `x_ekran = w/2 + x·s`, `y_ekran = h/2 − y·s`, gdzie `s = 0.5·min(w, h) / 5.2` (5.2 to promień kuli, w której mieści się cała scena). Oś `y` obrazu rośnie w dół, stąd minus.
- Orientacja sceny to jedna macierz obrotu `R` (3×3). Program nie przechowuje kątów Eulera.

Macierze obrotu wokół osi `x` i `y`:

```
         | 1    0       0    |            |  cos θ   0   sin θ |
Rx(θ) =  | 0  cos θ  −sin θ  |    Ry(θ) = |    0     1     0   |
         | 0  sin θ   cos θ  |            | −sin θ   0   cos θ |
```

### Przyrost obrotu z ruchu myszy

Każde zdarzenie ruchu myszy przy wciśniętym lewym przycisku daje przesunięcie `dx`, `dy` w pikselach **od poprzedniej pozycji kursora** (nie od miejsca, w którym zaczęło się przeciąganie). Z przesunięcia powstają dwa kąty:

```
β = dx · k      (obrót wokół pionowej osi ekranu)
α = dy · k      (obrót wokół poziomej osi ekranu)
k = 0.01 rad/px (100 px ≈ 57°)
```

Nowa orientacja to złożenie przyrostu z dotychczasową:

```
R ← orthonormalize( Ry(dx·k) · Rx(dy·k) · R )
```

Obrót jest więc **narastający**: każde małe przesunięcie dokłada swój przyrost do aktualnej orientacji.

### Dlaczego przyrost mnożony z lewej

Iloczyn `Rprzyrost · R` najpierw obraca scenę do aktualnej orientacji, a potem obraca wynik wokół osi **ekranu**. Dzięki temu ruch myszy w prawo zawsze kręci sceną wokół pionowej osi ekranu, niezależnie od tego, jak scena jest już obrócona. Mnożenie z prawej (`R · Rprzyrost`) obracałoby wokół osi **obiektu** – po obróceniu sceny „do góry nogami” ruch w prawo kręciłby ją w lewo i sterowanie przestałoby być intuicyjne.

### Dlaczego kąty bez minusów

Sprawdzenie na przednim punkcie sceny `P = (0, 0, 1)` (najbliżej obserwatora):

- `Ry(β) · P = (sin β, 0, cos β)` – dla ruchu w prawo (`dx > 0`, `β > 0`) punkt idzie w stronę `+x`, czyli w prawo na ekranie;
- `Rx(α) · P = (0, −sin α, cos α)` – dla ruchu w dół (`dy > 0`, bo `y` ekranu rośnie w dół) punkt idzie w stronę `−y` sceny, czyli w dół na ekranie.

Przód sceny podąża więc za kursorem bez dodatkowych zmian znaku:

| Ruch myszy | Przód sceny |
|---|---|
| w prawo | w prawo |
| w lewo | w lewo |
| w górę | w górę |
| w dół | w dół |

Przykład: przeciągnięcie o 100 px w prawo (`dx = 100`, `dy = 0`) daje `β = 1 rad ≈ 57°` i `R ← Ry(1) · R`.

### Ortonormalizacja

Po tysiącach mnożeń błędy zaokrągleń sprawiłyby, że macierz przestałaby być obrotem (scena zaczęłaby się rozciągać i ścinać). Dlatego po każdym kroku kolumny macierzy są ortonormalizowane metodą Grama–Schmidta: `c0 ← c0 / |c0|`, `c1 ← normalize(c1 − (c1·c0)·c0)`, `c2 ← c0 × c1`.

### Obracanie geometrii

- Każdy wierzchołek pamięta punkt `P`, wektory styczne `Pu`, `Pv` i wektor normalny `N` **przed obrotem** i **po obrocie**. Wersja po obrocie jest zawsze liczona od wersji przed obrotem: `P_po = R · P_przed` – błędy nie kumulują się w geometrii.
- `Pu`, `Pv` i `N` są obracane tą samą macierzą jako wektory kierunkowe (bez przesunięcia), a `N` jest potem normalizowany.
- Kąty nie mają ograniczeń i nie ma blokady przegubu (gimbal lock), bo orientacja jest macierzą, a nie trzema kątami.

## Opis techniczny: renderowanie

### Triangulacja

Wierzchołek `(i, j)` siatki ma parametry `u = i/n`, `v = j/n` i indeks `i·(n+1) + j`. Dla każdego wierzchołka liczone są punkt `P(u, v)`, pochodne `Pu`, `Pv` (wielomiany Bernsteina) i wektor normalny `N = normalize(Pu × Pv)`; gdy `Pu × Pv ≈ 0`, przyjmowane jest `N = [0, 0, 1]`. Każda komórka siatki to dwa trójkąty.

### Wypełnianie i Z-buffer

- Trójkąty są wypełniane algorytmem z **sortowaniem wierzchołków**: wierzchołki są sortowane według `y`, a linia skanowania przechodzi od góry do dołu, dodając i usuwając krawędzie na kolejnych wierzchołkach. Piksel jest wypełniany, gdy jego środek leży w trójkącie (krawędzie dolne i prawe nie należą do trójkąta, więc sąsiednie trójkąty się nie nakładają i nie zostawiają szczelin).
- Dla każdego piksela liczone są współrzędne barycentryczne i nimi interpolowane są `z`, `N`, `P` oraz `u`, `v`.
- Bufor głębokości: piksel jest rysowany tylko wtedy, gdy jego `z` jest większe (bliżej obserwatora) niż dotychczas zapisane w tym miejscu.

### Oświetlenie

Kolor każdego piksela liczony jest osobno dla R, G i B (wartości od 0 do 1, wynik przeliczany na 0–255):

```
I = kd · IL · IO · cos(N, L) + ks · IL · IO · cos^m(V, R)
L = normalize(Pświatła − P),  V = [0, 0, 1],  R = 2·⟨N, L⟩·N − L
```

Przy trzech źródłach światła składniki rozproszony i zwierciadlany są liczone dla każdego źródła osobno (każde ma własne `L` i `R`) i sumowane; wynik powyżej 1 jest obcinany do 1.

Trzy źródła są reflektorami (model Warna): każdy ma oś `D = normalize((0, 0, 0) − Preflektora)`, a do punktu `P` dociera tylko część jego światła:

```
IL(P) = IL · cos^k(α),  cos(α) = ⟨D, normalize(P − Preflektora)⟩
```

Na osi reflektora (`α = 0`) światło jest pełne, im dalej od osi, tym słabsze, a od 90° wcale nie dociera (ujemny cosinus → 0). `IL` to tu stały kolor reflektora (czerwony, zielony albo niebieski), a `IL(P)` zastępuje `IL` w obu składnikach wzoru dla danego reflektora; wykładnik `k` ustawia suwak „spot”.

Ujemne cosinusy są zastępowane zerem. Powierzchnie są otwarte, więc oświetlenie jest dwustronne: gdy `N` wskazuje od obserwatora (`N.z < 0`), program używa `−N`.

### Źródło światła

Światło jest punktowe i porusza się po krzywej zbudowanej na wspólnych punktach kontrolnych – domyślnie 12 punktach okręgu Béziera o promieniu 4.3 z 4 segmentów trzeciego stopnia. Przełącznik w grupie „Lighting” zmienia tylko rodzaj krzywej, punkty zostają (przesunięcie punktu działa na obie krzywe):

- **Krzywa Béziera** – jeden segment stopnia `n` na `n` punktach: `V0, …, Vn`, gdzie `Vn = V0` (dla 12 punktów stopień 12). Krzywa przechodzi tylko przez `V0` (rysowany jako kwadrat), pozostałe punkty ją przyciągają (kółka). Rysowana jedną przerywaną linią.
- **Krzywa B-sklejana (B-spline)** – zamknięta, jednorodna, stopnia 3. `n` punktów daje `n` segmentów; segment `i` zależy od punktów `i−1, i, i+1, i+2` (modulo `n`), więc przesunięcie punktu zmienia tylko 4 segmenty, a krzywa ma ciągłą drugą pochodną także w miejscu zamknięcia. Nie przechodzi przez punkty kontrolne (wszystkie rysowane jako kółka). Każdy segment ma inny kolor – kolejne segmenty mają odcienie odległe na kole barw i na przemian jaśniejsze i ciemniejsze, żeby było widać, gdzie się łączą.

Krzywe leżą na wysokości `z` (domyślnie `z = 8`). Położenie światła to część okrążenia `t ∈ [0, 1)`, rosnąca o 1/8 na sekundę (pełne okrążenie w 8 s na obu krzywych, niezależnie od liczby segmentów), liczona z rzeczywistego czasu, a nie z liczby klatek. Krzywe są związane z kamerą.

### Tekstura i normal mapa

- Każda powierzchnia ma własną teksturę, normal mapę i własne przełączniki; przy wypełnianiu trójkątów powierzchni `k` używane są ustawienia jej zakładki.
- Tekstura: kolor obiektu `IO` w pikselu to teksel w miejscu interpolowanych `(u, v)`.
- Normal mapa: kolor teksela `c` daje wektor `Nmapy = 2c − 1` (ze zmienionym znakiem składowej `y`, bo mapy mają `y` w górę obrazu, a `v` rośnie w dół). Zmodyfikowany wektor normalny to `N' = normalize(M · Nmapy)`, gdzie `M = [Pu, Pv, N]` to macierz, której kolumnami są znormalizowane wektory `Pu`, `Pv` i `N`.

## Architektura

| Katalog | Zawartość |
|---|---|
| `src/math/` | `Vec3`, `Mat3` – wektory, macierze, obroty |
| `src/geometry/` | wielomiany Bernsteina, powierzchnia i krzywa Béziera, siatka trójkątów, wczytywanie pliku |
| `src/renderer/` | rzut, obrót myszą, wypełnianie, współrzędne barycentryczne, Z-buffer, oświetlenie, tekstury, normal mapy |
| `src/ui/` | okno programu, obszar rysowania, panel, wczytywanie obrazów, okno pomocy (Qt) |

Katalogi `math/`, `geometry/` i `renderer/` to czysty C++ bez Qt; Qt jest tylko w `ui/`. Obszar rysowania rysuje własny obraz piksel po pikselu, bez OpenGL.
