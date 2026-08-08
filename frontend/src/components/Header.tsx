interface HeaderProps {
  connected: boolean;
  halted: boolean;
  paperTrading: boolean;
  symbols: string[];
  availableSymbols: string[];
  symbol: string;
  onSymbolChange: (s: string) => void;
  onHalt: () => void;
  onResume: () => void;
}

export default function Header({
  connected,
  halted,
  paperTrading,
  symbols,
  availableSymbols,
  symbol,
  onSymbolChange,
  onHalt,
  onResume
}: HeaderProps) {
  return (
    <header
      className={`sticky top-0 z-10 border-b border-hair bg-panel/95 backdrop-blur transition-colors ${
        halted ? 'animate-alarm' : ''
      }`}
    >
      <div className="mx-auto flex max-w-[1400px] items-center justify-between gap-4 px-6 py-3">
        <div className="flex items-center gap-3">
          <span
            className={`inline-block h-2.5 w-2.5 rounded-full ${
              connected ? 'animate-led text-amber shadow-led' : 'text-muted'
            } bg-current`}
            aria-hidden
          />
          <div>
            <h1 className="font-mono text-sm font-semibold tracking-[0.2em] text-ink">
              TRADINGBOT
            </h1>
            <p className="font-mono text-[11px] leading-none text-muted">
              {connected ? 'flux connecté' : 'flux hors ligne — reconnexion…'}
            </p>
          </div>
          <span
            className={`rounded-sm border px-2 py-1 font-mono text-[11px] font-bold uppercase tracking-widest ${
              paperTrading
                ? 'border-amber/60 bg-amber/10 text-amber'
                : 'border-sell bg-sell/20 text-sell'
            }`}
            title={
              paperTrading
                ? 'Mode simulation — testnet, aucun argent réel engagé'
                : 'ARGENT RÉEL — les ordres sont exécutés en production'
            }
          >
            {paperTrading ? 'Démo' : '⚠ Réel'}
          </span>
        </div>

        <div className="flex items-center gap-2">
          <label htmlFor="symbol-select" className="sr-only">
            Symbole
          </label>
          <select
            id="symbol-select"
            value={symbol}
            onChange={(e) => onSymbolChange(e.target.value)}
            className="rounded-sm border border-hair bg-panel2 px-3 py-1.5 font-mono text-sm text-ink outline-none focus:border-amber"
          >
            {symbols.map((s) => {
              const available = availableSymbols.includes(s);
              return (
                <option key={s} value={s} disabled={!available}>
                  {s}
                  {available ? '' : ' — non connecté'}
                </option>
              );
            })}
          </select>
        </div>

        <button
          onClick={halted ? onResume : onHalt}
          aria-pressed={halted}
          className={`group relative flex items-center gap-2 rounded-sm border px-4 py-2 font-mono text-xs font-semibold uppercase tracking-widest transition-colors ${
            halted
              ? 'border-buy/60 bg-buy/10 text-buy hover:bg-buy/20'
              : 'border-sell/60 bg-sell/10 text-sell hover:bg-sell/20'
          }`}
        >
          <span
            className={`h-2 w-2 rounded-full border ${
              halted ? 'border-buy' : 'border-sell bg-sell'
            }`}
          />
          {halted ? 'Reprendre le trading' : 'Arrêt d\u2019urgence'}
        </button>
      </div>
    </header>
  );
}
