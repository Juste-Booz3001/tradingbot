#!/usr/bin/env bash
set -e

echo "== Installation des dépendances système =="
sudo apt update
sudo apt install -y \
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

echo "== Installation de Node.js (frontend) =="
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt install -y nodejs
fi

echo "== Vérification des versions =="
cmake --version
g++ --version
node --version
npm --version

echo "== Terminé. Voir README.md pour la suite (base de données, config, compilation). =="
