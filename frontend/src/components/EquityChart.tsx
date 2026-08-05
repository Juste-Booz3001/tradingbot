import { useEffect, useRef } from 'react';
import { createChart, IChartApi, ISeriesApi, UTCTimestamp } from 'lightweight-charts';
import type { EquityUpdateMessage } from '../hooks/useBotSocket';
import { authFetch } from '../lib/auth';

interface EquityPoint {
  ts: number;
  equity: number;
}

interface EquityChartProps {
  liveUpdate: EquityUpdateMessage | null;
  onUnauthorized: () => void;
}

export default function EquityChart({ liveUpdate, onUnauthorized }: EquityChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const seriesRef = useRef<ISeriesApi<'Area'> | null>(null);

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
      timeScale: { borderColor: '#232935', timeVisible: true, secondsVisible: false }
    });
    const series = chart.addAreaSeries({
      lineColor: '#e2a63b',
      topColor: 'rgba(226, 166, 59, 0.28)',
      bottomColor: 'rgba(226, 166, 59, 0.02)',
      lineWidth: 2
    });

    chartRef.current = chart;
    seriesRef.current = series;

    authFetch('/api/equity_history', {}, onUnauthorized)
      .then((r) => r.json())
      .then((data: { points?: EquityPoint[] }) => {
        const points = data.points ?? [];
        series.setData(
          points.map((p) => ({
            time: Math.floor(p.ts / 1000) as UTCTimestamp,
            value: p.equity
          }))
        );
        chart.timeScale().fitContent();
      })
      .catch(() => {});

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
    if (!liveUpdate || !seriesRef.current) return;
    seriesRef.current.update({
      time: Math.floor(liveUpdate.ts / 1000) as UTCTimestamp,
      value: liveUpdate.equity
    });
  }, [liveUpdate]);

  return (
    <div className="border-b border-hair bg-panel px-6 py-5 md:border-b-0 md:border-r">
      <p className="font-mono text-[11px] uppercase tracking-widest text-muted">Courbe d'équité</p>
      <div ref={containerRef} className="mt-3" />
    </div>
  );
}
