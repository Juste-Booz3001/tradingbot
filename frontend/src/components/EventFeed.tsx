import { useEffect, useState } from 'react';
import type { SignalEventMessage } from '../hooks/useBotSocket';

interface EventFeedProps {
  lastSignalEvent: SignalEventMessage | null;
}

const MAX_EVENTS = 30;

function relativeTime(ts: number): string {
  const diffSec = Math.max(0, Math.floor((Date.now() - ts) / 1000));
  if (diffSec < 5) return "à l'instant";
  if (diffSec < 60) return `il y a ${diffSec}s`;
  const diffMin = Math.floor(diffSec / 60);
  if (diffMin < 60) return `il y a ${diffMin}min`;
  return `il y a ${Math.floor(diffMin / 60)}h`;
}

// Journal des signaux de la stratégie, acceptés ET rejetés — sans ça, un
// signal bloqué par le Risk Manager (drawdown, exposition, halt) est
// totalement invisible côté dashboard alors qu'il explique pourquoi le bot
// reste silencieux.
export default function EventFeed({ lastSignalEvent }: EventFeedProps) {
  const [events, setEvents] = useState<SignalEventMessage[]>([]);
  const [, forceTick] = useState(0);

  useEffect(() => {
    if (!lastSignalEvent) return;
    setEvents((prev) => [lastSignalEvent, ...prev].slice(0, MAX_EVENTS));
  }, [lastSignalEvent]);

  // Rafraîchit les libellés de temps relatif ("il y a 3s" -> "il y a 12s")
  // sans dépendre d'un nouvel événement.
  useEffect(() => {
    const interval = setInterval(() => forceTick((n) => n + 1), 5000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="bg-panel px-6 py-5">
      <p className="font-mono text-[11px] uppercase tracking-widest text-muted">
        Signaux de la stratégie
      </p>
      {events.length === 0 ? (
        <p className="mt-4 font-mono text-sm text-muted">
          Aucun signal détecté depuis l'ouverture du dashboard.
        </p>
      ) : (
        <ul className="mt-3 max-h-64 space-y-1.5 overflow-y-auto">
          {events.map((e, i) => (
            <li
              key={i}
              className="flex items-center justify-between gap-3 border-b border-hair/60 pb-1.5 font-mono text-xs last:border-b-0"
            >
              <div className="flex min-w-0 items-center gap-2">
                <span
                  className={`h-1.5 w-1.5 shrink-0 rounded-full ${
                    !e.accepted ? 'bg-muted' : e.side === 'buy' ? 'bg-buy' : 'bg-sell'
                  }`}
                />
                <span className={e.side === 'buy' ? 'text-buy' : 'text-sell'}>
                  {e.side === 'buy' ? 'ACHAT' : 'VENTE'}
                </span>
                <span className="text-ink">{e.symbol}</span>
                {e.accepted ? (
                  <span className="text-muted">exécuté</span>
                ) : (
                  <span className="truncate text-muted" title={e.reason}>
                    rejeté — {e.reason}
                  </span>
                )}
              </div>
              <span className="shrink-0 text-muted">{relativeTime(e.ts)}</span>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
