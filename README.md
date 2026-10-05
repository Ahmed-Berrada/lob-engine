# LOB Engine — Limit Order Book Matching Engine

[![CI](https://github.com/Ahmed-Berrada/lob-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/Ahmed-Berrada/lob-engine/actions/workflows/ci.yml)

*[English version](README.en.md)*

> Moteur d'appariement haute performance en C++17 — Price-Time Priority FIFO
> **3.46M ops/sec | 163ns médiane | O(log n) matching**

---

## Qu'est-ce qu'un Matching Engine ?

Un **moteur d'appariement** est le composant central de toute bourse ou marché électronique. Son rôle : mettre en correspondance un acheteur et un vendeur qui s'accordent sur un prix, puis générer une transaction.

### Le problème fondamental

Sur un marché, des milliers de participants émettent simultanément des ordres d'achat et de vente. Sans système déterministe :
- Qui est servi en premier quand deux acheteurs veulent le même prix ?
- Comment exécuter un ordre de 200 actions quand un seul vendeur n'en propose que 50 ?
- Comment garantir l'équité entre un institutionnel et un particulier ?

### Pourquoi c'est impératif

| Sans matching engine | Avec matching engine |
|---------------------|---------------------|
| Négociation bilatérale (téléphone) | Exécution en <1μs |
| Opacité sur le prix | Transparence (carnet visible) |
| Risque de contrepartie | Exécution garantie |
| Fragmentation de la liquidité | Centralisation |
| Favoritisme possible | Règles FIFO impartiales |

### Les 3 fonctions essentielles

1. **Découverte du prix (Price Discovery)** — Le prix juste émerge du point de rencontre offre/demande, matérialisé en temps réel dans le carnet d'ordres.

2. **Équité et déterminisme (Fairness)** — Règle Price-Time Priority : le meilleur prix gagne toujours, à prix égal le premier arrivé est servi (FIFO).

3. **Liquidité et efficience** — Centralisation des ordres → spread serré → exécution immédiate → attire plus de participants → cercle vertueux.

### Représentation du carnet d'ordres

```
           ASKS (vendeurs)
           152€  ████  300 actions
           151€  ██    100 actions
           150€  █      50 actions     ← Best Ask
    spread ─────────────────────────
           149€  ███   200 actions     ← Best Bid
           148€  █████ 500 actions
           147€  ██    150 actions
           BIDS (acheteurs)
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     MatchingEngine                        │
│                                                          │
│   ┌─────────────────────────────────────────────────┐   │
│   │                  OrderBook                       │   │
│   │                                                  │   │
│   │   Bids: std::map<Price, Level, std::greater>     │   │  ← prix décroissant
│   │   Asks: std::map<Price, Level, std::less>        │   │  ← prix croissant
│   │                                                  │   │
│   │   orders_: unordered_map<ID, Order*>             │   │  ← cancel O(1)
│   │   pool_: PoolAllocator<Order, 65536>             │   │  ← arena alloc
│   │                                                  │   │
│   │   Level = intrusive doubly-linked list (FIFO)    │   │
│   └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---

## Structure du projet

```
lob-engine/
├── CMakeLists.txt                 # Build system (C++17, -O3 -march=native)
├── README.md
├── include/lob/
│   ├── types.h                    # Price, OrderId, Side, Trade, OrderType
│   ├── order.h                    # Order struct (noeud intrusive list)
│   ├── level.h                    # Price Level (FIFO doubly-linked list)
│   ├── pool_allocator.h           # Arena allocator — blocs de 64K
│   ├── order_book.h               # Carnet d'ordres (maps + hashmap)
│   └── matching_engine.h          # Interface publique
├── src/
│   ├── order_book.cpp             # Logique de matching price-time FIFO
│   └── matching_engine.cpp        # Wrapper (extensible multi-symbole)
├── tests/
│   └── test_orderbook.cpp         # 8 tests unitaires
└── bench/
    └── benchmark.cpp              # Benchmark 5M ops (latence + throughput)
```

---

## Choix de conception

### 1. Prix en entiers (`int64_t`) — pas de `double`

```cpp
using Price = int64_t;  // 1 tick = 0.01€ → 150.25€ = 15025
```

**Pourquoi :**
- `0.1 + 0.2 ≠ 0.3` en floating-point → bugs de matching
- Comparaison d'entiers = 1 cycle CPU
- En finance, les prix sont discrets (pas de cotation) — c'est naturellement un entier

### 2. `std::map` (Red-Black Tree) pour l'arbre de prix

**Pourquoi pas `unordered_map` :** On a besoin de l'**ordre trié** des prix. `begin()` donne le meilleur prix en O(1). Avec un hash map il faudrait un scan O(n).

**Pourquoi pas un tableau :** Le range de prix est potentiellement grand et sparse. Un tableau indexé par prix gaspillerait de la mémoire.

**Alternative considérée — `boost::flat_map` :** Meilleure localité cache mais O(n) pour l'insertion (shift). Viable si peu de niveaux.

### 3. Liste doublement chaînée intrusive pour les ordres par niveau

```cpp
struct Order {
    // ... données ...
    Order* prev = nullptr;  // pointeurs intrusifs
    Order* next = nullptr;
};
```

**Pourquoi pas `std::list` :** `std::list` alloue un nœud séparé par élément via `malloc`. Avec une liste intrusive, les pointeurs vivent dans la struct Order elle-même → une seule allocation, meilleure localité cache.

**Avantage FIFO :** Push en tail O(1), pop du head O(1), cancel en O(1) depuis n'importe où.

### 4. Pool Allocator (Arena)

```cpp
PoolAllocator<Order, 65536>  // blocs de 64K ordres pré-alloués
```

**Pourquoi :**
- `new`/`delete` → appel système potentiel, 50-200ns de latence non-déterministe
- Le pool pré-alloue et maintient une free-list → alloc/dealloc en ~5ns
- Pas de fragmentation mémoire
- Latence **déterministe** (pas de worst-case malloc)

### 5. `unordered_map<OrderId, Order*>` pour le cancel

Permet de retrouver n'importe quel ordre par ID en O(1) amorti, puis de l'unlink de sa liste en O(1). Cancel total = O(1).

### 6. Single-threaded par carnet

**Pourquoi pas de multi-threading :**
- Un carnet est **séquentiel par nature** — l'ordre d'arrivée détermine la priorité
- Les locks détruisent la latence P99
- Un thread par symbole est le pattern standard (LMAX Disruptor, Nasdaq ITCH)

---

## Algorithme de matching

```cpp
// Incoming BID arrive à prix P pour quantité Q
while (Q > 0 && !asks.empty()) {
    Level& best_ask = asks.begin();      // meilleur ask (prix le plus bas)
    
    if (P < best_ask.price) break;       // pas de croisement → stop
    
    Order* resting = best_ask.head;      // FIFO: plus ancien en premier
    uint32_t fill = min(Q, resting->qty);
    
    emit_trade(buyer, seller, best_ask.price, fill);  // trade au prix resting
    
    Q -= fill;
    resting->qty -= fill;
    
    if (resting->qty == 0) remove(resting);  // fully filled
}
if (Q > 0) insert_in_book(order);  // résidu → resting order
```

**Points clés :**
- Le trade se fait au **prix de l'ordre resting** (celui qui était déjà dans le book)
- Le fill est le minimum des deux quantités (partial fill)
- Si l'incoming est entièrement rempli, il ne rentre jamais dans le book

---

## Complexité

| Opération | Complexité | Explication |
|-----------|-----------|-------------|
| Add (pas de match) | O(log n) | Insertion dans le map |
| Add (match même level) | O(1) | `begin()` + head de la liste |
| Add (sweep K levels) | O(K + log n) | K fills + suppression de levels vides |
| Cancel | O(1) | Hashmap lookup + unlink |
| Best bid/ask | O(1) | `begin()` du map |

Avec $n$ = nombre de niveaux de prix actifs (typiquement <1000 sur un instrument liquide).

---

## Performance

```
=== LOB Matching Engine Benchmark ===

Operations:    5.00M
Throughput:    3.46 M ops/sec
Mean latency:  253.16 ns
Median (P50):  163.00 ns
P99:           996.00 ns
P99.9:         5517.00 ns

--- Targets ---
Throughput > 2M ops/sec: PASS ✓
Median < 500ns:          PASS ✓
```

**Méthodologie :** 5 millions d'opérations avec distribution réaliste (60% ajouts, 25% cancels, 15% ordres agressifs). Seed déterministe pour la reproductibilité. Warm-up de 100K ops avant mesure.

---

## Build & Run

```bash
# Build en Release (-O3 -march=native)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Tests unitaires
./build/tests

# Benchmark
./build/bench
```

---

## Axes d'amélioration

### Court terme

| Feature | Description |
|---------|-------------|
| **IOC / FOK** | Immediate-or-Cancel (fill max puis cancel le reste), Fill-or-Kill (tout ou rien) |
| **Market Orders** | Pas de prix limite — exécution immédiate au meilleur prix disponible |
| **Modify Order** | Actuellement naïf (cancel + re-add) — devrait conserver la priorité si même prix / qty réduite |
| **Event callbacks** | Système de callbacks/listeners pour notifier les trades, book updates |

### Moyen terme

| Feature | Description |
|---------|-------------|
| **Multi-symbole** | Un `MatchingEngine` qui gère N `OrderBook` (un par instrument/ISIN) |
| **Protocole FIX** | Interface standard pour recevoir les ordres (FIX 4.4) |
| **Market data feed** | Multicast des updates du book (L2/L3 data) |
| **Persistance** | Write-ahead log pour recovery après crash |
| **Audit trail** | Logging de chaque événement avec timestamp nanoseconde |

### Long terme — Optimisations extrêmes

| Optimisation | Gain attendu |
|-------------|-------------|
| **Kernel bypass (DPDK/RDMA)** | Élimine la couche réseau du kernel → -10μs en network latency |
| **Huge pages** | Réduit les TLB misses pour le pool allocator |
| **CPU pinning + isolcpus** | Élimine les context switches sur le core du matching |
| **Template `<Side>`** | Élimine les branches bid/ask à la compilation (zero-cost) |
| **`boost::intrusive::rbtree`** | Remplacement du `std::map` avec contrôle total de l'allocation |
| **Lock-free SPSC queue** | Pour le pipeline input → engine → output sans lock |
| **Prefetch** | `__builtin_prefetch` sur le prochain level pendant le fill courant |

### Alternatives architecturales à explorer

| Alternative | Trade-off |
|------------|-----------|
| **Array-indexed book** (prix = index) | O(1) lookup mais gaspille mémoire si range large |
| **Skip list** au lieu de `std::map` | Potentiellement meilleur cache behavior, même complexité |
| **Custom red-black tree** | Contrôle de l'allocation, nœuds intrusifs pour éviter le double pointeur |
| **Flat sorted vector** | Excellent pour read-heavy (localité), mauvais pour insert-heavy |

---

## Vocabulaire technique

| Anglais | Français |
|---------|----------|
| Limit Order Book | Carnet d'ordres à cours limité |
| Matching Engine | Moteur d'appariement |
| Best Bid / Ask | Meilleure offre / demande |
| Spread | Fourchette (bid-ask) |
| Fill / Partial fill | Exécution / Exécution partielle |
| Price-Time Priority | Priorité prix-temps |
| Resting order | Ordre au repos (dans le carnet) |
| Aggressive order | Ordre agressif (crossing the spread) |
| Tick | Pas de cotation |
| Throughput / Latency | Débit / Latence |

---

## Licence

MIT. Voir [LICENSE](LICENSE).