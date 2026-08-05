import { fmtQty, fmtUsd } from '../lib/format';
import type { PositionUpdateMessage } from '../hooks/useBotSocket';

interface PositionsPanelProps {
  positions: Record<string, PositionUpdateMessage>;
}

export default function PositionsPanel({ positions }: PositionsPanelProps) {
  const list = Object.values(positions);

  return (
    <div className="bg-panel px-6 py-5">
      <p className="font-mono text-[11px] uppercase tracking-widest text-muted">
        Positions ouvertes
      </p>
      {list.length === 0 ? (
        <p className="mt-4 font-mono text-sm text-muted">Aucune position ouverte.</p>
      ) : (
        <ul className="mt-3 divide-y divide-hair">
          {list.map((p) => (
            <li key={p.symbol} className="flex items-center justify-between py-2.5">
              <div>
                <p className="font-mono text-sm font-medium text-ink">{p.symbol}</p>
                <p
                  className={`font-mono text-[11px] uppercase tracking-wide ${
                    p.side === 'buy' ? 'text-buy' : 'text-sell'
                  }`}
                >
                  {p.side === 'buy' ? 'long' : 'short'} · {fmtQty(p.quantity)} @ {fmtUsd(p.entry_price)} $
                </p>
              </div>
              <p
                className={`font-mono text-sm font-semibold tabular ${
                  p.unrealized_pnl >= 0 ? 'text-buy' : 'text-sell'
                }`}
              >
                {p.unrealized_pnl >= 0 ? '+' : ''}
                {fmtUsd(p.unrealized_pnl)} $
              </p>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
