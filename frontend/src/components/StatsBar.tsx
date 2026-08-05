import { fmtPct, fmtUsd } from '../lib/format';

interface Status {
  equity: number;
  drawdown_pct: number;
  halted: boolean;
}

interface StatsBarProps {
  status: Status | null;
  pnlToday: number | null;
}

function Cell({
  label,
  value,
  valueClassName = 'text-ink'
}: {
  label: string;
  value: string;
  valueClassName?: string;
}) {
  return (
    <div className="border-r border-hair px-6 py-4 last:border-r-0">
      <p className="font-mono text-[11px] uppercase tracking-widest text-muted">{label}</p>
      <p className={`mt-1 font-mono text-2xl font-semibold tabular ${valueClassName}`}>{value}</p>
    </div>
  );
}

export default function StatsBar({ status, pnlToday }: StatsBarProps) {
  return (
    <div className="grid grid-cols-2 border-b border-hair bg-panel md:grid-cols-4">
      <Cell label="Équité" value={status ? `${fmtUsd(status.equity)} $` : '—'} />
      <Cell
        label="P&L (24h)"
        value={pnlToday !== null ? `${pnlToday >= 0 ? '+' : ''}${fmtUsd(pnlToday)} $` : '—'}
        valueClassName={pnlToday === null ? 'text-ink' : pnlToday >= 0 ? 'text-buy' : 'text-sell'}
      />
      <Cell
        label="Drawdown"
        value={status ? fmtPct(-Math.abs(status.drawdown_pct)) : '—'}
        valueClassName={status && status.drawdown_pct > 0 ? 'text-sell' : 'text-ink'}
      />
      <Cell
        label="Statut"
        value={status ? (status.halted ? 'Arrêté' : 'Actif') : '—'}
        valueClassName={status?.halted ? 'text-sell' : 'text-buy'}
      />
    </div>
  );
}
