import { useEffect, useRef } from 'react';
import { createChart, IChartApi, ISeriesApi, UTCTimestamp } from 'lightweight-charts';
import { fmtUsd } from '../lib/format';

interface PriceChartProps {
  symbol: string;
  lastTick: { price: number; bid: number; ask: number; ts: number } | null;
}

// Trace le prix en direct sous forme de ligne. lightweight-charts (TradingView)
// est conçu pour ce cas d'usage — mise à jour incrémentale point par point,
// sans redessiner tout le graphique à chaque tick.
export default function PriceChart({ symbol, lastTick }: PriceChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const seriesRef = useRef<ISeriesApi<'Line'> | null>(null);

  useEffect(() => {
    if (!containerRef.current) return;

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height: 260,
      layout: { background: { color: 'transparent' }, textColor: '#6f7887', fontFamily: 'IBM Plex Mono' },
      grid: {
        vertLines: { color: '#1a1f29' },
        horzLines: { color: '#1a1f29' }
      },
      rightPriceScale: { borderColor: '#232935' },
      timeScale: { borderColor: '#232935', timeVisible: true, secondsVisible: true }
    });
    const series = chart.addLineSeries({ color: '#4fb8c9', lineWidth: 2 });

    chartRef.current = chart;
    seriesRef.current = series;

    const handleResize = () => {
      if (containerRef.current) chart.applyOptions({ width: containerRef.current.clientWidth });
    };
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, []);

  useEffect(() => {
    if (!lastTick || !seriesRef.current) return;
    seriesRef.current.update({
      time: Math.floor(lastTick.ts / 1000) as UTCTimestamp,
      value: lastTick.price
    });
  }, [lastTick]);

  return (
    <div className="bg-panel px-6 py-5">
      <div className="flex items-baseline justify-between">
        <p className="font-mono text-[11px] uppercase tracking-widest text-muted">
          {symbol} — prix en direct
        </p>
        {lastTick && (
          <div className="flex gap-4 font-mono text-xs text-muted">
            <span>
              bid <span className="text-sell">{fmtUsd(lastTick.bid)}</span>
            </span>
            <span>
              ask <span className="text-buy">{fmtUsd(lastTick.ask)}</span>
            </span>
          </div>
        )}
      </div>
      <div ref={containerRef} className="mt-3" />
    </div>
  );
}
