#!/usr/bin/env bash
set -e

# Détecte si on tourne déjà en root (courant sur Kali)
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

echo "== Installation des dépendances système =="
$SUDO apt update
$SUDO apt install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    postgresql \
    postgresql-contrib \
    libpqxx-dev \
    nlohmann-json3-dev \
    curl

echo "== Démarrage de PostgreSQL (pas automatique par défaut sur Kali) =="
$SUDO service postgresql start || $SUDO systemctl start postgresql
$SUDO systemctl enable postgresql 2>/dev/null || true

echo "== Installation de Node.js (frontend) =="
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | $SUDO -E bash -
    $SUDO apt install -y nodejs
fi

echo "== Vérification des versions =="
cmake --version
g++ --version
node --version
npm --version
pg_lsclusters 2>/dev/null || service postgresql status

echo "== Terminé. Voir README.md pour la suite (base de données, config, compilation). =="
