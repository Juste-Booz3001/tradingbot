import { useEffect, useMemo, useState } from 'react';
import { fmtDateTime, fmtQty, fmtUsd } from '../lib/format';
import { authFetch } from '../lib/auth';

interface Trade {
  symbol: string;
  side: string;
  quantity: number;
  fill_price: number;
  pnl: number;
  ts: string;
}

type SideFilter = 'all' | 'buy' | 'sell';

interface TradesTableProps {
  onUnauthorized: () => void;
}

export default function TradesTable({ onUnauthorized }: TradesTableProps) {
  const [trades, setTrades] = useState<Trade[]>([]);
  const [sideFilter, setSideFilter] = useState<SideFilter>('all');
  const [symbolFilter, setSymbolFilter] = useState('all');

  useEffect(() => {
    const fetchTrades = () => {
      authFetch('/api/trades', {}, onUnauthorized)
        .then((r) => r.json())
        .then((data) => setTrades(data.trades ?? []))
        .catch(() => {});
    };
    fetchTrades();
    const interval = setInterval(fetchTrades, 5000);
    return () => clearInterval(interval);
  }, [onUnauthorized]);

  const symbols = useMemo(
    () => Array.from(new Set(trades.map((t) => t.symbol))).sort(),
    [trades]
  );

  const filtered = useMemo(
    () =>
      trades.filter(
        (t) =>
          (sideFilter === 'all' || t.side === sideFilter) &&
          (symbolFilter === 'all' || t.symbol === symbolFilter)
      ),
    [trades, sideFilter, symbolFilter]
  );

  return (
    <div className="bg-panel px-6 py-5">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <p className="font-mono text-[11px] uppercase tracking-widest text-muted">
          Trades récents
        </p>
        <div className="flex gap-2">
          <select
            value={symbolFilter}
            onChange={(e) => setSymbolFilter(e.target.value)}
            className="rounded-sm border border-hair bg-panel2 px-2 py-1 font-mono text-xs text-ink outline-none focus:border-amber"
          >
            <option value="all">Tous les symboles</option>
            {symbols.map((s) => (
              <option key={s} value={s}>
                {s}
              </option>
            ))}
          </select>
          <div className="flex overflow-hidden rounded-sm border border-hair font-mono text-xs">
            {(['all', 'buy', 'sell'] as SideFilter[]).map((f) => (
              <button
                key={f}
                onClick={() => setSideFilter(f)}
                className={`px-3 py-1 transition-colors ${
                  sideFilter === f ? 'bg-amber/20 text-amber' : 'bg-panel2 text-muted hover:text-ink'
                }`}
              >
                {f === 'all' ? 'Tout' : f === 'buy' ? 'Achats' : 'Ventes'}
              </button>
            ))}
          </div>
        </div>
      </div>

      {filtered.length === 0 ? (
        <p className="mt-4 font-mono text-sm text-muted">Aucun trade pour l'instant.</p>
      ) : (
        <div className="mt-3 overflow-x-auto">
          <table className="w-full border-collapse font-mono text-sm">
            <thead>
              <tr className="border-b border-hair text-left text-[11px] uppercase tracking-wide text-muted">
                <th className="py-2 pr-4 font-medium">Symbole</th>
                <th className="py-2 pr-4 font-medium">Sens</th>
                <th className="py-2 pr-4 font-medium">Quantité</th>
                <th className="py-2 pr-4 font-medium">Prix</th>
                <th className="py-2 pr-4 font-medium">PnL</th>
                <th className="py-2 pr-4 font-medium">Date</th>
              </tr>
            </thead>
            <tbody>
              {filtered.map((t, i) => (
                <tr key={i} className="border-b border-hair/60 text-ink">
                  <td className="py-2 pr-4">{t.symbol}</td>
                  <td className={`py-2 pr-4 ${t.side === 'buy' ? 'text-buy' : 'text-sell'}`}>
                    {t.side === 'buy' ? 'Achat' : 'Vente'}
                  </td>
                  <td className="py-2 pr-4 tabular">{fmtQty(t.quantity)}</td>
                  <td className="py-2 pr-4 tabular">{fmtUsd(t.fill_price)} $</td>
                  <td className={`py-2 pr-4 tabular ${t.pnl >= 0 ? 'text-buy' : 'text-sell'}`}>
                    {t.pnl >= 0 ? '+' : ''}
                    {fmtUsd(t.pnl)} $
                  </td>
                  <td className="py-2 pr-4 text-muted">{fmtDateTime(t.ts)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
