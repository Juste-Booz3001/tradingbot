import { useEffect, useRef, useState } from 'react';

export interface BotMessage {
  type: 'tick' | 'position_update' | 'equity_update';
  [key: string]: unknown;
}

// Se connecte au WebSocket du moteur C++ et reconnecte automatiquement
// en cas de coupure (backoff simple).
export function useBotSocket() {
  const [connected, setConnected] = useState(false);
  const [lastMessage, setLastMessage] = useState<BotMessage | null>(null);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    let retryDelay = 1000;
    let cancelled = false;

    function connect() {
      const ws = new WebSocket(`ws://${window.location.hostname}:8080/ws`);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        retryDelay = 1000;
      };

      ws.onmessage = (event) => {
        try {
          setLastMessage(JSON.parse(event.data));
        } catch {
          // message non-JSON ignoré
        }
      };

      ws.onclose = () => {
        setConnected(false);
        if (!cancelled) {
          setTimeout(connect, retryDelay);
          retryDelay = Math.min(retryDelay * 2, 30000);
        }
      };

      ws.onerror = () => ws.close();
    }

    connect();
    return () => {
      cancelled = true;
      wsRef.current?.close();
    };
  }, []);

  return { connected, lastMessage };
}
