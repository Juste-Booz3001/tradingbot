import { useEffect, useRef } from 'react';
import { createChart, IChartApi, ISeriesApi, IPriceLine, UTCTimestamp } from 'lightweight-charts';
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

const POSITIVE = { line: '#35c48a', top: 'rgba(53, 196, 138, 0.28)', bottom: 'rgba(53, 196, 138, 0.02)' };
const NEGATIVE = { line: '#e2503f', top: 'rgba(226, 80, 63, 0.28)', bottom: 'rgba(226, 80, 63, 0.02)' };
const NEUTRAL = { line: '#e2a63b', top: 'rgba(226, 166, 59, 0.28)', bottom: 'rgba(226, 166, 59, 0.02)' };

export default function EquityChart({ liveUpdate, onUnauthorized }: EquityChartProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const seriesRef = useRef<ISeriesApi<'Area'> | null>(null);
  const baselineRef = useRef<number | null>(null); // premier point connu = capital de départ
  const baselineLineRef = useRef<IPriceLine | null>(null);

  // Ajuste la couleur de la courbe selon qu'on est au-dessus ou en dessous
  // du capital de départ — un simple dégradé ambre ne dit rien sur la
  // performance réelle, contrairement à vert/rouge.
  const applyColorForValue = (value: number) => {
    if (!seriesRef.current || baselineRef.current === null) return;
    const palette = value > baselineRef.current ? POSITIVE : value < baselineRef.current ? NEGATIVE : NEUTRAL;
    seriesRef.current.applyOptions({
      lineColor: palette.line,
      topColor: palette.top,
      bottomColor: palette.bottom
    });
  };

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
      lineColor: NEUTRAL.line,
      topColor: NEUTRAL.top,
      bottomColor: NEUTRAL.bottom,
      lineWidth: 2
    });

    chartRef.current = chart;
    seriesRef.current = series;

    authFetch('/api/equity_history', {}, onUnauthorized)
      .then((r) => r.json())
      .then((data: { points?: EquityPoint[] }) => {
        const points = data.points ?? [];
        if (points.length > 0) {
          baselineRef.current = points[0].equity;
          baselineLineRef.current = series.createPriceLine({
            price: points[0].equity,
            color: '#6f7887',
            lineWidth: 1,
            lineStyle: 3,
            axisLabelVisible: true,
            title: 'départ'
          });
        }
        series.setData(
          points.map((p) => ({
            time: Math.floor(p.ts / 1000) as UTCTimestamp,
            value: p.equity
          }))
        );
        if (points.length > 0) applyColorForValue(points[points.length - 1].equity);
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
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!liveUpdate || !seriesRef.current) return;
    if (baselineRef.current === null) baselineRef.current = liveUpdate.equity;
    seriesRef.current.update({
      time: Math.floor(liveUpdate.ts / 1000) as UTCTimestamp,
      value: liveUpdate.equity
    });
    applyColorForValue(liveUpdate.equity);
  }, [liveUpdate]);

  return (
    <div className="border-b border-hair bg-panel px-6 py-5 md:border-b-0 md:border-r">
      <p className="font-mono text-[11px] uppercase tracking-widest text-muted">Courbe d'équité</p>
      <div ref={containerRef} className="mt-3" />
    </div>
  );
}
