import { useEffect, useRef } from 'react';
import { createChart, IChartApi, ISeriesApi, UTCTimestamp } from 'lightweight-charts';

interface PriceChartProps {
  symbol: string;
  lastTick: { price: number; ts: number } | null;
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
      height: 300,
      layout: { background: { color: '#ffffff' }, textColor: '#333' },
      grid: { vertLines: { color: '#f0f0f0' }, horzLines: { color: '#f0f0f0' } },
      timeScale: { timeVisible: true, secondsVisible: true }
    });
    const series = chart.addLineSeries({ color: '#2a78d6', lineWidth: 2 });

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
    <div style={{ background: '#f5f5f5', borderRadius: 8, padding: 16, marginBottom: 24 }}>
      <p style={{ fontSize: 13, color: '#666', marginBottom: 8 }}>
        {symbol} — prix en direct
      </p>
      <div ref={containerRef} />
    </div>
  );
}
