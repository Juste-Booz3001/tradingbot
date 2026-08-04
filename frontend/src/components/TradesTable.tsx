import { useEffect, useState } from 'react';

interface Trade {
  symbol: string;
  side: string;
  quantity: number;
  fill_price: number;
  pnl: number;
  ts: string;
}

export default function TradesTable() {
  const [trades, setTrades] = useState<Trade[]>([]);

  useEffect(() => {
    const fetchTrades = () => {
      fetch('/api/trades')
        .then((r) => r.json())
        .then((data) => setTrades(data.trades ?? []))
        .catch(() => {});
    };
    fetchTrades();
    const interval = setInterval(fetchTrades, 5000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div style={{ background: '#f5f5f5', borderRadius: 8, padding: 16 }}>
      <p style={{ fontSize: 13, color: '#666', marginBottom: 12 }}>Trades récents</p>
      {trades.length === 0 ? (
        <p style={{ color: '#999', fontSize: 14 }}>Aucun trade pour l'instant.</p>
      ) : (
        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 14 }}>
          <thead>
            <tr style={{ textAlign: 'left', color: '#666', borderBottom: '1px solid #ddd' }}>
              <th style={{ padding: '6px 8px' }}>Symbole</th>
              <th style={{ padding: '6px 8px' }}>Sens</th>
              <th style={{ padding: '6px 8px' }}>Quantité</th>
              <th style={{ padding: '6px 8px' }}>Prix</th>
              <th style={{ padding: '6px 8px' }}>PnL</th>
              <th style={{ padding: '6px 8px' }}>Date</th>
            </tr>
          </thead>
          <tbody>
            {trades.map((t, i) => (
              <tr key={i} style={{ borderBottom: '1px solid #eee' }}>
                <td style={{ padding: '6px 8px' }}>{t.symbol}</td>
                <td style={{ padding: '6px 8px', color: t.side === 'buy' ? '#16a34a' : '#dc2626' }}>
                  {t.side === 'buy' ? 'Achat' : 'Vente'}
                </td>
                <td style={{ padding: '6px 8px' }}>{t.quantity.toFixed(6)}</td>
                <td style={{ padding: '6px 8px' }}>{t.fill_price.toFixed(2)} $</td>
                <td style={{ padding: '6px 8px', color: t.pnl >= 0 ? '#16a34a' : '#dc2626' }}>
                  {t.pnl >= 0 ? '+' : ''}{t.pnl.toFixed(2)} $
                </td>
                <td style={{ padding: '6px 8px', color: '#666' }}>
                  {new Date(t.ts).toLocaleString('fr-FR')}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  );
}
