-- Schéma TradingBot. Compatible PostgreSQL seul (sans hypertables) ou
-- avec l'extension TimescaleDB pour de meilleures perfs sur les séries temporelles.

CREATE TABLE IF NOT EXISTS trades (
    id BIGSERIAL PRIMARY KEY,
    order_id TEXT NOT NULL,
    symbol TEXT NOT NULL,
    market SMALLINT NOT NULL,      -- 0=Crypto, 1=Stocks, 2=Forex
    side TEXT NOT NULL,            -- buy / sell
    quantity DOUBLE PRECISION NOT NULL,
    fill_price DOUBLE PRECISION NOT NULL,
    pnl DOUBLE PRECISION NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_trades_symbol ON trades(symbol);
CREATE INDEX IF NOT EXISTS idx_trades_created_at ON trades(created_at);

CREATE TABLE IF NOT EXISTS positions (
    id BIGSERIAL PRIMARY KEY,
    symbol TEXT NOT NULL,
    market SMALLINT NOT NULL,
    side TEXT NOT NULL,
    quantity DOUBLE PRECISION NOT NULL,
    entry_price DOUBLE PRECISION NOT NULL,
    stop_loss DOUBLE PRECISION,
    take_profit DOUBLE PRECISION,
    status TEXT NOT NULL DEFAULT 'open', -- open / closed
    opened_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    closed_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS market_data (
    id BIGSERIAL PRIMARY KEY,
    symbol TEXT NOT NULL,
    market SMALLINT NOT NULL,
    bid DOUBLE PRECISION,
    ask DOUBLE PRECISION,
    last DOUBLE PRECISION,
    volume DOUBLE PRECISION,
    ts TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_market_data_symbol_ts ON market_data(symbol, ts DESC);

CREATE TABLE IF NOT EXISTS equity_curve (
    id BIGSERIAL PRIMARY KEY,
    equity DOUBLE PRECISION NOT NULL,
    drawdown_pct DOUBLE PRECISION NOT NULL,
    ts TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS alerts_log (
    id BIGSERIAL PRIMARY KEY,
    level TEXT NOT NULL,           -- info / warning / critical
    message TEXT NOT NULL,
    ts TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS bot_state (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Si l'extension TimescaleDB est disponible, décommentez ces lignes pour
-- convertir les tables de séries temporelles en hypertables (perfs++) :
-- SELECT create_hypertable('market_data', 'ts', if_not_exists => TRUE);
-- SELECT create_hypertable('equity_curve', 'ts', if_not_exists => TRUE);
