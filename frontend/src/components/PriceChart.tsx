import { useEffect, useRef, useState } from 'react';
import {
  createChart,
  IChartApi,
  ISeriesApi,
  IPriceLine,
  UTCTimestamp,
  SeriesMarker,
  Time
} from 'lightweight-charts';
import { fmtUsd } from '../lib/format';

export interface TradeMarker {
  ts: number; // epoch ms
  side: 'buy' | 'sell';
  price: number;
  pnl: number;
}

export interface OpenPositionLevels {
  entryPrice: number;
  stopLoss: number;
  takeProfit: number;
}

interface PriceChartProps {
  symbol: string;
  lastTick: {
    price: number;
    bid: number;
    ask: number;
    volume: number;
    ts: number;
    fastMa?: number;
    slowMa?: number;
  } | null;
  trades?: TradeMarker[];
  position?: OpenPositionLevels | null;
}

// Trace le prix en direct sous forme de ligne, avec les moyennes mobiles de
// la stratégie superposées — pour voir concrètement ce qui pilote le bot,
// pas juste le résultat de ses décisions. lightweight-charts (TradingView)
// gère nativement la mise à jour incrémentale point par point.
export default function PriceChart({ symbol, lastTick, trades = [], position = null }: PriceChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const priceSeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const fastMaSeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const slowMaSeriesRef = useRef<ISeriesApi<'Line'> | null>(null);
  const volumeSeriesRef = useRef<ISeriesApi<'Histogram'> | null>(null);
  const slPriceLineRef = useRef<IPriceLine | null>(null);
  const tpPriceLineRef = useRef<IPriceLine | null>(null);
  const entryPriceLineRef = useRef<IPriceLine | null>(null);
  const prevPriceRef = useRef<number | null>(null);
  const [flash, setFlash] = useState<'up' | 'down' | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height: 300,
      layout: { background: { color: 'transparent' }, textColor: '#6f7887', fontFamily: 'IBM Plex Mono' },
      grid: {
        vertLines: { color: '#1a1f29' },
        horzLines: { color: '#1a1f29' }
      },
      rightPriceScale: { borderColor: '#232935' },
      timeScale: { borderColor: '#232935', timeVisible: true, secondsVisible: true }
    });

    // Volume en histogramme discret, sur sa propre échelle tassée en bas
    // (30% de la hauteur) pour ne pas écraser la lecture du prix.
    const volumeSeries = chart.addHistogramSeries({
      color: '#2d3543',
      priceFormat: { type: 'volume' },
      priceScaleId: 'volume'
    });
    volumeSeries.priceScale().applyOptions({ scaleMargins: { top: 0.82, bottom: 0 } });

    const priceSeries = chart.addLineSeries({
      color: '#e7ebf1',
      lineWidth: 2,
      priceScaleId: 'right'
    });
    priceSeries.priceScale().applyOptions({ scaleMargins: { top: 0.05, bottom: 0.25 } });

    const fastMaSeries = chart.addLineSeries({
      color: '#4fb8c9',
      lineWidth: 1,
      priceLineVisible: false,
      lastValueVisible: false,
      crosshairMarkerVisible: false
    });
    const slowMaSeries = chart.addLineSeries({
      color: '#e2a63b',
      lineWidth: 1,
      priceLineVisible: false,
      lastValueVisible: false,
      crosshairMarkerVisible: false
    });

    chartRef.current = chart;
    priceSeriesRef.current = priceSeries;
    fastMaSeriesRef.current = fastMaSeries;
    slowMaSeriesRef.current = slowMaSeries;
    volumeSeriesRef.current = volumeSeries;

    const handleResize = () => {
      if (containerRef.current) chart.applyOptions({ width: containerRef.current.clientWidth });
    };
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [symbol]); // un chart neuf par symbole : évite de mélanger les séries BTC/ETH

  useEffect(() => {
    if (!lastTick) return;
    const t = Math.floor(lastTick.ts / 1000) as UTCTimestamp;

    priceSeriesRef.current?.update({ time: t, value: lastTick.price });
    volumeSeriesRef.current?.update({
      time: t,
      value: lastTick.volume,
      color: prevPriceRef.current !== null && lastTick.price < prevPriceRef.current ? '#4a2f2f' : '#2d3543'
    });
    if (lastTick.fastMa !== undefined) fastMaSeriesRef.current?.update({ time: t, value: lastTick.fastMa });
    if (lastTick.slowMa !== undefined) slowMaSeriesRef.current?.update({ time: t, value: lastTick.slowMa });

    // Flash vert/rouge bref sur variation de prix — retour visuel immédiat
    // sans avoir à fixer les yeux sur le graphique pour voir que ça bouge.
    if (prevPriceRef.current !== null && lastTick.price !== prevPriceRef.current) {
      setFlash(lastTick.price > prevPriceRef.current ? 'up' : 'down');
      const timeout = setTimeout(() => setFlash(null), 400);
      prevPriceRef.current = lastTick.price;
      return () => clearTimeout(timeout);
    }
    prevPriceRef.current = lastTick.price;
  }, [lastTick]);

  // Marqueurs de trades clôturés (flèche verte/rouge selon le PnL réalisé).
  useEffect(() => {
    if (!priceSeriesRef.current) return;
    const markers: SeriesMarker<Time>[] = trades
      .slice()
      .sort((a, b) => a.ts - b.ts)
      .map((t) => ({
        time: Math.floor(t.ts / 1000) as UTCTimestamp,
        position: t.pnl >= 0 ? 'aboveBar' : 'belowBar',
        color: t.pnl >= 0 ? '#4ade80' : '#f87171',
        shape: t.pnl >= 0 ? 'arrowUp' : 'arrowDown',
        text: `${t.side === 'buy' ? 'Achat' : 'Vente'} ${fmtUsd(t.price)}$`
      }));
    priceSeriesRef.current.setMarkers(markers);
  }, [trades]);

  // Lignes horizontales SL/TP/entrée pour la position ouverte sur ce symbole,
  // recréées à chaque changement (ouverture, ajustement, clôture -> retrait).
  useEffect(() => {
    const series = priceSeriesRef.current;
    if (!series) return;

    slPriceLineRef.current && series.removePriceLine(slPriceLineRef.current);
    tpPriceLineRef.current && series.removePriceLine(tpPriceLineRef.current);
    entryPriceLineRef.current && series.removePriceLine(entryPriceLineRef.current);
    slPriceLineRef.current = null;
    tpPriceLineRef.current = null;
    entryPriceLineRef.current = null;

    if (!position) return;

    entryPriceLineRef.current = series.createPriceLine({
      price: position.entryPrice,
      color: '#6f7887',
      lineWidth: 1,
      lineStyle: 3, // pointillé fin
      axisLabelVisible: true,
      title: 'entrée'
    });
    if (position.stopLoss > 0) {
      slPriceLineRef.current = series.createPriceLine({
        price: position.stopLoss,
        color: '#e2503f',
        lineWidth: 2,
        lineStyle: 2, // tirets
        axisLabelVisible: true,
        title: 'SL'
      });
    }
    if (position.takeProfit > 0) {
      tpPriceLineRef.current = series.createPriceLine({
        price: position.takeProfit,
        color: '#35c48a',
        lineWidth: 2,
        lineStyle: 2,
        axisLabelVisible: true,
        title: 'TP'
      });
    }
  }, [position]);

  return (
    <div className="bg-panel px-6 py-5">
      <div className="flex items-baseline justify-between">
        <p className="font-mono text-[11px] uppercase tracking-widest text-muted">
          {symbol} — prix en direct
          <span className="ml-3 text-[10px] normal-case text-muted/70">
            <span className="text-[#4fb8c9]">— MA9</span> <span className="text-amber">— MA21</span>
          </span>
        </p>
        {lastTick && (
          <div className="flex gap-4 font-mono text-xs text-muted">
            <span>
              bid <span className="text-sell">{fmtUsd(lastTick.bid)}</span>
            </span>
            <span>
              ask <span className="text-buy">{fmtUsd(lastTick.ask)}</span>
            </span>
            <span
              className={`rounded-sm px-1.5 transition-colors duration-300 ${
                flash === 'up' ? 'bg-buy/25 text-buy' : flash === 'down' ? 'bg-sell/25 text-sell' : 'text-ink'
              }`}
            >
              {fmtUsd(lastTick.price)}
            </span>
          </div>
        )}
      </div>
      <div ref={containerRef} className="mt-3" />
    </div>
  );
}
