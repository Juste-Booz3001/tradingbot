import { useEffect, useState } from 'react';
import { useBotSocket } from '../hooks/useBotSocket';
import PriceChart from './PriceChart';
import TradesTable from './TradesTable';

interface Status {
  equity: number;
  drawdown_pct: number;
  halted: boolean;
}

interface TickMessage {
  type: 'tick';
  symbol: string;
  price: number;
  ts: number;
}

export default function Dashboard() {
  const { connected, lastMessage } = useBotSocket();
  const [status, setStatus] = useState<Status | null>(null);
  const [lastTick, setLastTick] = useState<{ price: number; ts: number } | null>(null);
  const [symbol, setSymbol] = useState('BTCUSDT');

  useEffect(() => {
    const fetchStatus = () => {
      fetch('/api/status')
        .then((r) => r.json())
        .then(setStatus)
        .catch(() => {});
    };
    fetchStatus();
    const interval = setInterval(fetchStatus, 3000);
    return () => clearInterval(interval);
  }, []);

  // Chaque tick reçu via WebSocket met à jour le graphique de prix en direct.
  useEffect(() => {
    if (lastMessage?.type === 'tick') {
      const tick = lastMessage as unknown as TickMessage;
      setLastTick({ price: tick.price, ts: tick.ts });
      setSymbol(tick.symbol);
    }
  }, [lastMessage]);

  const emergencyStop = async () => {
    await fetch('/api/halt', { method: 'POST' });
  };

  const resume = async () => {
    await fetch('/api/resume', { method: 'POST' });
  };

  return (
    <div style={{ fontFamily: 'sans-serif', padding: '2rem', maxWidth: 900, margin: '0 auto' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <h1>TradingBot</h1>
        <span style={{ color: connected ? 'green' : 'crimson' }}>
          {connected ? '● connecté' : '○ déconnecté'}
        </span>
      </div>

      {status && (
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 16, margin: '2rem 0' }}>
          <div style={{ background: '#f5f5f5', borderRadius: 8, padding: 16 }}>
            <p style={{ fontSize: 13, color: '#666' }}>Équité</p>
            <p style={{ fontSize: 24, fontWeight: 600 }}>{status.equity.toFixed(2)} $</p>
          </div>
          <div style={{ background: '#f5f5f5', borderRadius: 8, padding: 16 }}>
            <p style={{ fontSize: 13, color: '#666' }}>Drawdown</p>
            <p style={{ fontSize: 24, fontWeight: 600 }}>{status.drawdown_pct.toFixed(2)}%</p>
          </div>
          <div style={{ background: '#f5f5f5', borderRadius: 8, padding: 16 }}>
            <p style={{ fontSize: 13, color: '#666' }}>État</p>
            <p style={{ fontSize: 24, fontWeight: 600, color: status.halted ? 'crimson' : 'green' }}>
              {status.halted ? 'Arrêté' : 'Actif'}
            </p>
          </div>
        </div>
      )}

      <button
        onClick={status?.halted ? resume : emergencyStop}
        style={{
          padding: '12px 24px',
          borderRadius: 8,
          border: 'none',
          background: status?.halted ? '#16a34a' : '#dc2626',
          color: 'white',
          fontWeight: 600,
          cursor: 'pointer',
          marginBottom: 24
        }}
      >
        {status?.halted ? 'Reprendre le trading' : 'Arrêt d\'urgence'}
      </button>

      <PriceChart symbol={symbol} lastTick={lastTick} />
      <TradesTable />
    </div>
  );
}
