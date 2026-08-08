import { useEffect, useRef, useState } from 'react';

export interface TickMessage {
  type: 'tick';
  symbol: string;
  price: number;
  bid: number;
  ask: number;
  volume: number;
  ts: number;
  fast_ma?: number;
  slow_ma?: number;
  rsi?: number;
}

export interface SignalEventMessage {
  type: 'signal_event';
  symbol: string;
  side: 'buy' | 'sell';
  confidence: number;
  strategy: string;
  accepted: boolean;
  reason?: string;
  ts: number;
}

export interface PositionUpdateMessage {
  type: 'position_update';
  symbol: string;
  side: 'buy' | 'sell';
  quantity: number;
  entry_price: number;
  unrealized_pnl: number;
  stop_loss: number;
  take_profit: number;
}

export interface PositionClosedMessage {
  type: 'position_closed';
  symbol: string;
}

export interface EquityUpdateMessage {
  type: 'equity_update';
  equity: number;
  drawdown_pct: number;
  ts: number;
}

export type BotMessage =
  | TickMessage
  | PositionUpdateMessage
  | PositionClosedMessage
  | EquityUpdateMessage
  | SignalEventMessage;

// Se connecte au WebSocket du moteur C++ et reconnecte automatiquement
// en cas de coupure (backoff simple). Le token JWT est requis dès l'ouverture
// (vérifié côté serveur dans onaccept, avant même la mise à niveau HTTP->WS).
export function useBotSocket(token: string | null) {
  const [connected, setConnected] = useState(false);
  const [lastMessage, setLastMessage] = useState<BotMessage | null>(null);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    if (!token) {
      setConnected(false);
      return;
    }

    let retryDelay = 1000;
    let cancelled = false;

    function connect() {
      const ws = new WebSocket(
        `ws://${window.location.hostname}:8080/ws?token=${encodeURIComponent(token!)}`
      );
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
  }, [token]);

  return { connected, lastMessage };
}
