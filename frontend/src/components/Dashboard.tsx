import { useCallback, useEffect, useMemo, useState } from 'react';
import { useBotSocket } from '../hooks/useBotSocket';
import type { EquityUpdateMessage, PositionUpdateMessage, SignalEventMessage } from '../hooks/useBotSocket';
import { authFetch, clearToken, getToken } from '../lib/auth';
import Header from './Header';
import StatsBar from './StatsBar';
import EquityChart from './EquityChart';
import PriceChart from './PriceChart';
import type { TradeMarker } from './PriceChart';
import CandlestickChart from './CandlestickChart';
import SignalBadge from './SignalBadge';
import EventFeed from './EventFeed';
import StrategyPanel from './StrategyPanel';
import PositionsPanel from './PositionsPanel';
import TradesTable from './TradesTable';
import LoginScreen from './LoginScreen';

interface Status {
  equity: number;
  drawdown_pct: number;
  halted: boolean;
  paper_trading: boolean;
}

interface LastTick {
  price: number;
  bid: number;
  ask: number;
  volume: number;
  ts: number;
  fastMa?: number;
  slowMa?: number;
  rsi?: number;
}

const SYMBOLS = ['BTCUSDT', 'ETHUSDT', 'EURUSD', 'AAPL'];
// Seuls les symboles crypto ont un connecteur réel (BinanceConnector) pour
// l'instant. EURUSD/AAPL restent dans le sélecteur pour montrer la direction
// du projet, mais sont désactivés tant que les connecteurs forex/actions
// n'existent pas.
const AVAILABLE_SYMBOLS = ['BTCUSDT', 'ETHUSDT'];

export default function Dashboard() {
  const [token, setTokenState] = useState<string | null>(() => getToken());
  const { connected, lastMessage } = useBotSocket(token);
  const [status, setStatus] = useState<Status | null>(null);
  const [symbol, setSymbol] = useState(SYMBOLS[0]);
  const [lastTick, setLastTick] = useState<LastTick | null>(null);
  const [positions, setPositions] = useState<Record<string, PositionUpdateMessage>>({});
  const [lastEquityUpdate, setLastEquityUpdate] = useState<EquityUpdateMessage | null>(null);
  const [lastSignalEvent, setLastSignalEvent] = useState<SignalEventMessage | null>(null);
  const [pnlToday, setPnlToday] = useState<number | null>(null);
  const [apiError, setApiError] = useState<string | null>(null);
  const [tradeMarkers, setTradeMarkers] = useState<TradeMarker[]>([]);
  const [strategyPanelOpen, setStrategyPanelOpen] = useState(false);

  // Retour forcé à l'écran de login si le serveur répond 401 (token expiré/révoqué).
  const handleUnauthorized = useCallback(() => {
    clearToken();
    setTokenState(null);
  }, []);

  useEffect(() => {
    if (!token) return;
    const fetchStatus = () => {
      authFetch('/api/status', {}, handleUnauthorized)
        .then((r) => {
          if (!r.ok) throw new Error(`HTTP ${r.status}`);
          return r.json();
        })
        .then((data) => {
          setStatus(data);
          setApiError(null);
        })
        .catch(() => {
          setApiError("Impossible de joindre le moteur — vérifiez qu'il tourne toujours.");
        });
    };
    fetchStatus();
    const interval = setInterval(fetchStatus, 3000);
    return () => clearInterval(interval);
  }, [token, handleUnauthorized]);

  useEffect(() => {
    if (!token) return;
    const fetchTradesForPnl = () => {
      authFetch('/api/trades', {}, handleUnauthorized)
        .then((r) => r.json())
        .then((data: { trades?: { symbol: string; side: 'buy' | 'sell'; pnl: number; fill_price: number; ts: string }[] }) => {
          const trades = data.trades ?? [];
          const cutoff = Date.now() - 24 * 60 * 60 * 1000;
          const total = trades
            .filter((t) => new Date(t.ts).getTime() >= cutoff)
            .reduce((sum, t) => sum + t.pnl, 0);
          setPnlToday(total);

          setTradeMarkers(
            trades
              .filter((t) => t.symbol === symbol)
              .map((t) => ({
                ts: new Date(t.ts).getTime(),
                side: t.side,
                price: t.fill_price,
                pnl: t.pnl
              }))
          );
        })
        .catch(() => {});
    };
    fetchTradesForPnl();
    const interval = setInterval(fetchTradesForPnl, 10000);
    return () => clearInterval(interval);
  }, [token, handleUnauthorized, symbol]);

  // Route chaque message WebSocket vers l'état concerné (prix, position, équité, signaux).
  useEffect(() => {
    if (!lastMessage) return;
    if (lastMessage.type === 'tick') {
      if (lastMessage.symbol !== symbol) return;
      setLastTick({
        price: lastMessage.price,
        bid: lastMessage.bid,
        ask: lastMessage.ask,
        volume: lastMessage.volume,
        ts: lastMessage.ts,
        fastMa: lastMessage.fast_ma,
        slowMa: lastMessage.slow_ma,
        rsi: lastMessage.rsi
      });
    } else if (lastMessage.type === 'position_update') {
      setPositions((prev) => ({ ...prev, [lastMessage.symbol]: lastMessage }));
    } else if (lastMessage.type === 'position_closed') {
      setPositions((prev) => {
        const next = { ...prev };
        delete next[lastMessage.symbol];
        return next;
      });
    } else if (lastMessage.type === 'equity_update') {
      setLastEquityUpdate(lastMessage);
    } else if (lastMessage.type === 'signal_event') {
      setLastSignalEvent(lastMessage);
    }
  }, [lastMessage, symbol]);

  const emergencyStop = async () => {
    await authFetch('/api/halt', { method: 'POST' }, handleUnauthorized);
  };

  const resume = async () => {
    await authFetch('/api/resume', { method: 'POST' }, handleUnauthorized);
  };

  const halted = useMemo(() => status?.halted ?? false, [status]);
  const currentPosition = positions[symbol] ?? null;

  if (!token) {
    return <LoginScreen onSuccess={() => setTokenState(getToken())} />;
  }

  return (
    <div className="min-h-screen bg-void">
      <Header
        connected={connected}
        halted={halted}
        paperTrading={status?.paper_trading ?? true}
        symbols={SYMBOLS}
        availableSymbols={AVAILABLE_SYMBOLS}
        symbol={symbol}
        onSymbolChange={(s) => {
          setSymbol(s);
          setLastTick(null);
        }}
        onHalt={emergencyStop}
        onResume={resume}
        onOpenStrategies={() => setStrategyPanelOpen(true)}
      />

      <StrategyPanel
        open={strategyPanelOpen}
        onClose={() => setStrategyPanelOpen(false)}
        onUnauthorized={handleUnauthorized}
      />

      {apiError && (
        <div className="border-b border-sell/40 bg-sell/10 px-6 py-2 text-center font-mono text-xs text-sell">
          ⚠ {apiError}
        </div>
      )}

      <main className="mx-auto max-w-[1400px]">
        <StatsBar status={status} pnlToday={pnlToday} />

        <div className="grid grid-cols-1 border-b border-hair md:grid-cols-2">
          <EquityChart liveUpdate={lastEquityUpdate} onUnauthorized={handleUnauthorized} />
          <PriceChart
            symbol={symbol}
            lastTick={lastTick}
            trades={tradeMarkers}
            position={
              currentPosition
                ? {
                    entryPrice: currentPosition.entry_price,
                    stopLoss: currentPosition.stop_loss,
                    takeProfit: currentPosition.take_profit
                  }
                : null
            }
          />
        </div>

        <CandlestickChart symbol={symbol} lastTick={lastTick} />

        <div className="border-b border-hair bg-panel px-6 py-3">
          <SignalBadge fastMa={lastTick?.fastMa} slowMa={lastTick?.slowMa} rsi={lastTick?.rsi} />
        </div>

        <div className="grid grid-cols-1 border-b border-hair md:grid-cols-[1fr_280px_320px]">
          <div className="border-b border-hair md:border-b-0 md:border-r">
            <TradesTable onUnauthorized={handleUnauthorized} />
          </div>
          <div className="border-b border-hair md:border-b-0 md:border-r">
            <PositionsPanel positions={positions} />
          </div>
          <EventFeed lastSignalEvent={lastSignalEvent} />
        </div>
      </main>
    </div>
  );
}
