import { useCallback, useEffect, useMemo, useState } from 'react';
import { useBotSocket } from '../hooks/useBotSocket';
import type { EquityUpdateMessage, PositionUpdateMessage } from '../hooks/useBotSocket';
import { authFetch, clearToken, getToken } from '../lib/auth';
import Header from './Header';
import StatsBar from './StatsBar';
import EquityChart from './EquityChart';
import PriceChart from './PriceChart';
import PositionsPanel from './PositionsPanel';
import TradesTable from './TradesTable';
import LoginScreen from './LoginScreen';

interface Status {
  equity: number;
  drawdown_pct: number;
  halted: boolean;
  paper_trading: boolean;
}

const SYMBOLS = ['BTCUSDT', 'ETHUSDT', 'EURUSD', 'AAPL'];

export default function Dashboard() {
  const [token, setTokenState] = useState<string | null>(() => getToken());
  const { connected, lastMessage } = useBotSocket(token);
  const [status, setStatus] = useState<Status | null>(null);
  const [symbol, setSymbol] = useState(SYMBOLS[0]);
  const [lastTick, setLastTick] = useState<{
    price: number;
    bid: number;
    ask: number;
    ts: number;
  } | null>(null);
  const [positions, setPositions] = useState<Record<string, PositionUpdateMessage>>({});
  const [lastEquityUpdate, setLastEquityUpdate] = useState<EquityUpdateMessage | null>(null);
  const [pnlToday, setPnlToday] = useState<number | null>(null);

  // Retour forcé à l'écran de login si le serveur répond 401 (token expiré/révoqué).
  const handleUnauthorized = useCallback(() => {
    clearToken();
    setTokenState(null);
  }, []);

  useEffect(() => {
    if (!token) return;
    const fetchStatus = () => {
      authFetch('/api/status', {}, handleUnauthorized)
        .then((r) => r.json())
        .then(setStatus)
        .catch(() => {});
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
        .then((data: { trades?: { pnl: number; ts: string }[] }) => {
          const cutoff = Date.now() - 24 * 60 * 60 * 1000;
          const total = (data.trades ?? [])
            .filter((t) => new Date(t.ts).getTime() >= cutoff)
            .reduce((sum, t) => sum + t.pnl, 0);
          setPnlToday(total);
        })
        .catch(() => {});
    };
    fetchTradesForPnl();
    const interval = setInterval(fetchTradesForPnl, 10000);
    return () => clearInterval(interval);
  }, [token, handleUnauthorized]);

  // Route chaque message WebSocket vers l'état concerné (prix, position, équité).
  useEffect(() => {
    if (!lastMessage) return;
    if (lastMessage.type === 'tick') {
      if (lastMessage.symbol !== symbol) return;
      setLastTick({
        price: lastMessage.price,
        bid: lastMessage.bid,
        ask: lastMessage.ask,
        ts: lastMessage.ts
      });
    } else if (lastMessage.type === 'position_update') {
      setPositions((prev) => ({ ...prev, [lastMessage.symbol]: lastMessage }));
    } else if (lastMessage.type === 'equity_update') {
      setLastEquityUpdate(lastMessage);
    }
  }, [lastMessage, symbol]);

  const emergencyStop = async () => {
    await authFetch('/api/halt', { method: 'POST' }, handleUnauthorized);
  };

  const resume = async () => {
    await authFetch('/api/resume', { method: 'POST' }, handleUnauthorized);
  };

  const halted = useMemo(() => status?.halted ?? false, [status]);

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
        symbol={symbol}
        onSymbolChange={(s) => {
          setSymbol(s);
          setLastTick(null);
        }}
        onHalt={emergencyStop}
        onResume={resume}
      />

      <main className="mx-auto max-w-[1400px]">
        <StatsBar status={status} pnlToday={pnlToday} />

        <div className="grid grid-cols-1 border-b border-hair md:grid-cols-2">
          <EquityChart liveUpdate={lastEquityUpdate} onUnauthorized={handleUnauthorized} />
          <PriceChart symbol={symbol} lastTick={lastTick} />
        </div>

        <div className="grid grid-cols-1 border-b border-hair md:grid-cols-[1fr_320px]">
          <div className="border-b border-hair md:border-b-0 md:border-r">
            <TradesTable onUnauthorized={handleUnauthorized} />
          </div>
          <PositionsPanel positions={positions} />
        </div>
      </main>
    </div>
  );
}
