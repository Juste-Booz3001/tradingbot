# TradingBot — moteur C++ multi-marchés + dashboard web

Squelette de projet : moteur de trading en C++ (crypto / actions / forex),
API REST + WebSocket, dashboard React, base TimescaleDB.

## Architecture

```
tradingbot/
├── backend/         # Moteur C++ (stratégie, risque, exécution, API)
├── frontend/        # Dashboard React + TypeScript
├── db/               # Schéma PostgreSQL/TimescaleDB
├── config/           # Fichiers de configuration
└── scripts/          # Scripts d'installation
```

## 1. Installation — Ubuntu / Debian

```bash
chmod +x scripts/install_ubuntu.sh
./scripts/install_ubuntu.sh
```

Ce script installe :
- `build-essential`, `cmake`, `git`
- `libboost-all-dev` (I/O asynchrone)
- `libssl-dev` (TLS pour les connexions WebSocket vers les exchanges)
- `postgresql`, `libpqxx-dev` (base de données)
- `nodejs`, `npm` (frontend)

## 1bis. Installation — Kali Linux

```bash
chmod +x scripts/install_kali.sh
./scripts/install_kali.sh
```

Différences à connaître sur Kali :
- Si vous êtes déjà connecté en `root`, le script détecte l'absence de `sudo` et l'omet automatiquement.
- PostgreSQL ne démarre pas au boot par défaut sur Kali (contrairement à Ubuntu) — le script le démarre explicitement avec `service postgresql start`.
- Vérifiez `node --version` après coup : visez v18 minimum.

## 1bis. Installation — macOS

```bash
brew install cmake boost openssl libpqxx postgresql node
brew services start postgresql
```

## 2. Base de données

```bash
sudo -u postgres createdb tradingbot
sudo -u postgres psql -d tradingbot -f db/schema.sql
```

Si vous voulez l'extension TimescaleDB (recommandé pour les séries temporelles) :
```bash
sudo apt install timescaledb-2-postgresql-16
sudo timescaledb-tune
sudo systemctl restart postgresql
sudo -u postgres psql -d tradingbot -c "CREATE EXTENSION IF NOT EXISTS timescaledb;"
```

## 3. Configuration

```bash
cp config/config.example.json config/config.json
```

Éditez `config/config.json` avec vos clés API d'exchange (jamais en dur dans le code,
jamais commit dans git — le fichier est dans `.gitignore`).

## 4. Compiler le backend

```bash
cd backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./tradingbot_engine ../../config/config.json
```

Le serveur API démarre sur `http://localhost:8080` et le WebSocket sur `ws://localhost:8080/ws`.

## 5. Lancer le dashboard (Streamlit)

```bash
cd dashboard
pip install -r requirements.txt --break-system-packages   # ou dans un venv
streamlit run streamlit_app.py
```

Dashboard accessible sur `http://localhost:8501`. Il interroge l'API C++
toutes les 2 secondes (polling, réglable dans la barre latérale) — pas un
vrai push WebSocket. Suffisant pour superviser le bot, pas pour du visuel
tick-par-tick.

> Un frontend React alternatif existe dans `frontend/` (vrai WebSocket temps
> réel, plus de contrôle sur l'UX) si vous voulez comparer les deux approches
> plus tard. Les deux consomment la même API C++, aucun conflit.

## 6. Ordre de mise en route recommandé

1. Compiler et lancer le backend en mode `paper_trading: true` (voir config.json)
2. Lancer le frontend, vérifier que les prix arrivent en temps réel
3. Activer une stratégie de test sur un seul marché (crypto conseillé, API la plus simple)
4. Observer le Risk Manager en paper trading pendant plusieurs semaines avant d'envisager le réel
5. Ne jamais passer en argent réel sans avoir vérifié les logs de reconnexion et de réconciliation d'état après un crash simulé

## Prochaines étapes de développement

- [x] ~~Implémenter un vrai connecteur Binance~~ — fait (WebSocket + REST signé HMAC), voir `backend/src/market/BinanceConnector.cpp`
- [x] ~~Persister les trades en base~~ — fait, `Database::insertTrade` appelé à chaque clôture de position
- [x] ~~Réconciliation au redémarrage~~ — fait, `OrderExecutor::reconcileFromDatabase` compare la DB au solde réel de l'exchange et alerte en cas de divergence
- [x] ~~Authentification JWT sur l'API~~ — fait, voir la section "Authentification" ci-dessous
- [ ] Ajouter une vraie stratégie dans `backend/include/IStrategy.hpp` (celle fournie, croisement de moyennes mobiles, est un exemple pédagogique — non backtestée)
- [ ] Implémenter `cancelOrder` (actuellement un stub, voir le TODO dans `BinanceConnector.cpp`)
- [ ] Write-ahead log des ordres avant envoi (persister l'intention *avant* l'appel réseau, pas seulement à la clôture) pour une réconciliation encore plus fine après un crash en plein envoi d'ordre
- [ ] Écrire les tests unitaires du Risk Manager (le module le plus critique)

## Authentification

Toutes les routes de l'API, sauf `/api/login`, exigent un JWT valide
(`Authorization: Bearer <token>`, ou `?token=<token>` pour le WebSocket).

1. Générez le hash de votre mot de passe admin :
   ```bash
   echo -n "votre_mot_de_passe" | sha256sum
   ```
2. Générez un secret de signature JWT aléatoire :
   ```bash
   openssl rand -hex 32
   ```
3. Renseignez les deux dans `config/config.json`, bloc `auth` (voir `config.example.json`).
4. Le dashboard (frontend React) affiche un écran de connexion au premier
   chargement et conserve le token dans le stockage local du navigateur.
