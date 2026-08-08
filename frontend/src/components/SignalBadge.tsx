import { fmtUsd } from '../lib/format';

interface SignalBadgeProps {
  fastMa?: number;
  slowMa?: number;
  rsi?: number;
}

// Seuils utilisés par les stratégies côté backend — dupliqués ici
// uniquement pour l'affichage. Si ces valeurs changent dans IStrategy.hpp,
// penser à les répercuter ici aussi.
const CROSS_THRESHOLD_PCT = 0.1;
const RSI_OVERSOLD = 30;
const RSI_OVERBOUGHT = 70;

export default function SignalBadge({ fastMa, slowMa, rsi }: SignalBadgeProps) {
  if (fastMa === undefined && slowMa === undefined && rsi === undefined) {
    return (
      <div className="flex items-center gap-2 border border-hair bg-panel2 px-3 py-1.5 font-mono text-xs text-muted">
        <span className="h-1.5 w-1.5 rounded-full bg-muted" />
        Aucune stratégie active — historique insuffisant ou tout est désactivé
      </div>
    );
  }

  return (
    <div className="flex flex-wrap items-center gap-3">
      {fastMa !== undefined && slowMa !== undefined && (
        <MaBadge fastMa={fastMa} slowMa={slowMa} />
      )}
      {rsi !== undefined && <RsiBadge rsi={rsi} />}
    </div>
  );
}

function MaBadge({ fastMa, slowMa }: { fastMa: number; slowMa: number }) {
  const gapPct = ((fastMa - slowMa) / slowMa) * 100;
  const isBullish = gapPct > CROSS_THRESHOLD_PCT;
  const isBearish = gapPct < -CROSS_THRESHOLD_PCT;
  const proximity = Math.min(Math.abs(gapPct) / CROSS_THRESHOLD_PCT, 1) * 100;

  const label = isBullish ? 'MA : signal ACHAT' : isBearish ? 'MA : signal VENTE' : 'MA : neutre';
  const color = isBullish ? 'text-buy' : isBearish ? 'text-sell' : 'text-muted';
  const dotColor = isBullish ? 'bg-buy' : isBearish ? 'bg-sell' : 'bg-amber';

  return (
    <div className="flex flex-wrap items-center gap-3 border border-hair bg-panel2 px-3 py-1.5 font-mono text-xs">
      <span className="flex items-center gap-2">
        <span className={`h-1.5 w-1.5 rounded-full ${dotColor} ${isBullish || isBearish ? 'animate-led' : ''}`} />
        <span className={color}>{label}</span>
      </span>
      <span className="text-muted">
        MA9 {fmtUsd(fastMa)} · MA21 {fmtUsd(slowMa)} · écart{' '}
        <span className={color}>
          {gapPct >= 0 ? '+' : ''}
          {gapPct.toFixed(3)}%
        </span>
      </span>
      {!isBullish && !isBearish && (
        <span className="flex items-center gap-1.5 text-muted">
          proche du croisement
          <span className="h-1 w-16 overflow-hidden rounded-full bg-hair">
            <span
              className="block h-full bg-amber transition-all duration-300"
              style={{ width: `${proximity}%` }}
            />
          </span>
        </span>
      )}
    </div>
  );
}

function RsiBadge({ rsi }: { rsi: number }) {
  const isOversold = rsi < RSI_OVERSOLD;
  const isOverbought = rsi > RSI_OVERBOUGHT;
  const label = isOversold ? 'RSI : signal ACHAT (survente)' : isOverbought ? 'RSI : signal VENTE (sur-achat)' : 'RSI : neutre';
  const color = isOversold ? 'text-buy' : isOverbought ? 'text-sell' : 'text-muted';
  const dotColor = isOversold ? 'bg-buy' : isOverbought ? 'bg-sell' : 'bg-amber';
  const gaugePct = Math.min(Math.max(rsi, 0), 100);

  return (
    <div className="flex flex-wrap items-center gap-3 border border-hair bg-panel2 px-3 py-1.5 font-mono text-xs">
      <span className="flex items-center gap-2">
        <span className={`h-1.5 w-1.5 rounded-full ${dotColor} ${isOversold || isOverbought ? 'animate-led' : ''}`} />
        <span className={color}>{label}</span>
      </span>
      <span className="flex items-center gap-2 text-muted">
        RSI(14) <span className={color}>{rsi.toFixed(1)}</span>
        <span className="h-1 w-16 overflow-hidden rounded-full bg-hair">
          <span
            className={`block h-full transition-all duration-300 ${
              isOversold ? 'bg-buy' : isOverbought ? 'bg-sell' : 'bg-amber'
            }`}
            style={{ width: `${gaugePct}%` }}
          />
        </span>
      </span>
    </div>
  );
}
