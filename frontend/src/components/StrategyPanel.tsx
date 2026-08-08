import { useEffect, useState } from 'react';
import { authFetch } from '../lib/auth';

interface Strategy {
  name: string;
  description: string;
  enabled: boolean;
}

interface StrategyPanelProps {
  open: boolean;
  onClose: () => void;
  onUnauthorized: () => void;
}

export default function StrategyPanel({ open, onClose, onUnauthorized }: StrategyPanelProps) {
  const [strategies, setStrategies] = useState<Strategy[]>([]);
  const [pending, setPending] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const fetchStrategies = () => {
    authFetch('/api/strategies', {}, onUnauthorized)
      .then((r) => r.json())
      .then((data: { strategies?: Strategy[] }) => setStrategies(data.strategies ?? []))
      .catch(() => setError('Impossible de récupérer la liste des stratégies.'));
  };

  useEffect(() => {
    if (open) fetchStrategies();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [open]);

  const toggle = async (name: string, nextEnabled: boolean) => {
    setPending(name);
    setError(null);
    try {
      const res = await authFetch(
        `/api/strategies/${encodeURIComponent(name)}/${nextEnabled ? 'enable' : 'disable'}`,
        { method: 'POST' },
        onUnauthorized
      );
      if (!res.ok) throw new Error();
      // Optimiste : évite d'attendre un aller-retour réseau supplémentaire
      // pour voir le bouton changer d'état.
      setStrategies((prev) => prev.map((s) => (s.name === name ? { ...s, enabled: nextEnabled } : s)));
    } catch {
      setError(`Échec du changement d'état pour ${name}.`);
    } finally {
      setPending(null);
    }
  };

  if (!open) return null;

  return (
    <div
      className="fixed inset-0 z-20 flex items-start justify-center bg-black/60 px-4 pt-20"
      onClick={onClose}
    >
      <div
        className="w-full max-w-md border border-hair bg-panel px-6 py-5"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between">
          <p className="font-mono text-xs uppercase tracking-widest text-muted">
            Stratégies de trading
          </p>
          <button
            onClick={onClose}
            aria-label="Fermer"
            className="font-mono text-muted hover:text-ink"
          >
            ✕
          </button>
        </div>

        {error && <p className="mt-3 font-mono text-xs text-sell">{error}</p>}

        {strategies.length === 0 ? (
          <p className="mt-4 font-mono text-sm text-muted">Chargement…</p>
        ) : (
          <ul className="mt-4 space-y-3">
            {strategies.map((s) => (
              <li
                key={s.name}
                className="flex items-start justify-between gap-3 border-b border-hair/60 pb-3 last:border-b-0"
              >
                <div className="min-w-0">
                  <p className="font-mono text-sm text-ink">{s.name}</p>
                  <p className="mt-0.5 font-mono text-xs leading-relaxed text-muted">{s.description}</p>
                </div>
                <button
                  onClick={() => toggle(s.name, !s.enabled)}
                  disabled={pending === s.name}
                  className={`shrink-0 rounded-sm border px-3 py-1.5 font-mono text-[10px] font-semibold uppercase tracking-widest transition-colors disabled:opacity-50 ${
                    s.enabled
                      ? 'border-buy/60 bg-buy/10 text-buy hover:bg-buy/20'
                      : 'border-hair bg-panel2 text-muted hover:text-ink'
                  }`}
                >
                  {pending === s.name ? '…' : s.enabled ? 'Activée' : 'Désactivée'}
                </button>
              </li>
            ))}
          </ul>
        )}

        <p className="mt-5 font-mono text-[10px] text-muted/70">
          Une stratégie désactivée ne trade plus, mais reste en mémoire — la réactiver reprend
          l'analyse dès le prochain tick.
        </p>
      </div>
    </div>
  );
}
